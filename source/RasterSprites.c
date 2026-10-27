#include <nds.h>
#include <stdint.h>
#include <string.h>
#if defined(__arm__)
#pragma GCC target("arm")
#pragma GCC optimize("O3")
#endif

/* Sparse software OBJ rasterization.
 * Guest position registers and changed sprite attributes are sampled at the
 * same visible-line boundary as the accepted backdrop/scroll implementation.
 * Unchanged frames keep the original native OBJ path. Changed frames are
 * binned by source Y so only intersecting sprite rows need software work.
 * Patterns use the existing K2GE mid-frame VRAM snapshot; arbitrary raster
 * pattern rewrites are outside this position/attribute fix.
 *
 * No HBlank IRQ or DMA channel is used.
 * Each populated 32x32 block becomes one 8bpp hardware OBJ, retaining the
 * three NGPC sprite priorities and the existing OBJ palette raster streams. */
#define LINES 152
#define BLOCKS 75
#define BLOCK_BYTES 1024
#define ATLAS_BASE 0x4000

typedef struct { uint8_t bytes[320]; unsigned line; } Snapshot;
typedef struct { uint16_t attr, x, y; uint8_t palette; } Sprite;
typedef struct { int16_t next; uint8_t sprite, row; } RowEntry;
typedef struct {
    uint8_t pixels[BLOCKS][BLOCK_BYTES];
    uint32_t oam[256];
    uint8_t used[BLOCKS];
    unsigned active, blocks, guestFrame;
    uint32_t revision[BLOCKS];
} Frame;

uint32_t spriteRasterLines[LINES];
uint32_t spriteRasterWindows[LINES];
uint32_t spriteRasterDirty = 1;
static Snapshot snapshots[LINES];
static unsigned snapshotCount, displayBank, pendingBank;
extern uint32_t frameTotal;
uint32_t spriteRasterPresentedFrame, spriteRasterShownFrame;
volatile uint32_t spriteRasterStage;
static Frame frames[2] __attribute__((aligned(32)));
static Sprite sprites[64];
static int16_t rowHeads[256];
static RowEntry entries[512];
static uint32_t expanded[16][256], masks[256];
static uint8_t reversed[256];
/* The SDK's size-optimized memcmp loops over bytes. Compare aligned state
 * in full words without hashes, and fall back for genuinely unaligned input. */
static inline int exactEqual(const void *a,const void *b,unsigned bytes) {
    if(!(((uintptr_t)a|(uintptr_t)b|bytes)&3)) {
        typedef uint32_t AliasWord __attribute__((__may_alias__));
        const AliasWord *p=a,*q=b;
        while(bytes>=16) {
            if((p[0]^q[0])|(p[1]^q[1])|(p[2]^q[2])|(p[3]^q[3]))return 0;
            p+=4;q+=4;bytes-=16;
        }
        while(bytes) {if(*p++!=*q++)return 0;bytes-=4;}
        return 1;
    }
    return !memcmp(a,b,bytes);
}
static unsigned lookupReady;
/* Exact inputs, not hashes: no collision can suppress a changed sprite frame. */
static Snapshot cachedSnapshots[LINES];
static uint32_t cachedLines[LINES], cachedWindows[LINES];
static uint8_t cachedChars[8192] __attribute__((aligned(4)));
static uint16_t cacheTiles[512];
static unsigned cacheTileCount;
static unsigned cachedCount, cacheValid, generation;
static unsigned uploadedRevision[BLOCKS];
uint32_t spriteRasterCacheHits, spriteRasterSkippedBlocks;
static unsigned pendingGuestFrame;
static inline unsigned pixelOffset(unsigned x,unsigned y) {
    return (y/8)*256+(x/8)*64+(y&7)*8+(x&7);
}
static inline uint32_t *pixelWord(Frame *f,unsigned b,unsigned x,unsigned y) {
    return (uint32_t *)(f->pixels[b]+pixelOffset(x,y));
}
static void initializeLookup(void) {
    if(lookupReady) return;
    for(unsigned b=0;b<256;b++) {
        unsigned reverse=0;
        for(unsigned i=0;i<4;i++) reverse|=((b>>(i*2))&3)<<((3-i)*2);
        reversed[b]=reverse;
        for(unsigned pal=0;pal<16;pal++) {
            uint32_t word=0, mask=0;
            for(unsigned x=0;x<4;x++) {
                unsigned pen=(b>>((3-x)*2))&3;
                if(pen) {word|=(pal*16+pen)<<(x*8); mask|=255u<<(x*8);}
            }
            expanded[pal][b]=word; masks[b]=mask;
        }
    }
    lookupReady=1;
}
static inline void merge(uint32_t *dst,uint32_t pixels,uint32_t mask) {
    *dst=(*dst&~mask)|pixels;
}
uint32_t spriteRasterFrames, spriteRasterMaxBlocks, spriteRasterUploadBytes;
uint32_t spriteRasterMaxSnapshots;

void rasterSpritesReset(void) {
    snapshotCount=0;
    cacheValid=0;
    memset(uploadedRevision,0,sizeof(uploadedRevision));
    frames[0].active=frames[1].active=0;
    frames[0].guestFrame=frames[1].guestFrame=0;
    displayBank=pendingBank=0;
    spriteRasterDirty=1;
}

void rasterSpritesSnapshot(unsigned line, const void *spr) {
    if (!line) snapshotCount = 0;
    if (snapshotCount && exactEqual(snapshots[snapshotCount-1].bytes,spr,320)) {
        spriteRasterDirty=0;
        return;
    }
    if (snapshotCount < LINES) {
        Snapshot *s = &snapshots[snapshotCount++];
        memcpy(s->bytes, spr, 320);
        s->line = line;
    }
    spriteRasterDirty = 0;
}

static void buildRows(const uint8_t *raw, unsigned mode) {
    unsigned x=0, y=0;
    memset(rowHeads, 0xFF, sizeof(rowHeads));
    uint8_t ys[64];
    for (unsigned i=0; i<64; i++) {
        unsigned a=raw[i*4] | (raw[i*4+1]<<8);
        x=((a&0x400)?x:0)+raw[i*4+2];
        y=((a&0x200)?y:0)+raw[i*4+3];
        sprites[i]=(Sprite){a,x&255,y,mode?((a>>13)&1):(raw[256+i]&15)};
        ys[i]=y;
    }
    unsigned n=0;
    /* Prepend in increasing OAM order: draw backwards, so the earlier
     * sprite overwrites later sprites of the same priority. */
    for (unsigned i=0; i<64; i++) {
        if (!(sprites[i].attr & 0x1800)) continue;
        for (unsigned row=0; row<8; row++) {
            unsigned at=(ys[i]+row)&255;
            entries[n]=(RowEntry){rowHeads[at],i,row};
            rowHeads[at]=n++;
        }
    }
}

void rasterSpritesFinish(const uint8_t *chars) {
    /* A diagnostic RAM marker: the headless runner measures emulated cycles
     * without allocating hardware timers or modifying guest CPU semantics. */
    spriteRasterStage=1;
    unsigned next=displayBank^1;
    Frame *f=&frames[next];
    f->active=f->blocks=0;
    f->guestFrame=frameTotal+1;
    pendingGuestFrame=f->guestFrame;
    if (!snapshotCount) goto publish;
    unsigned varying=snapshotCount>1;
    for (unsigned y=1; y<LINES && !varying; y++)
        varying=spriteRasterLines[y]!=spriteRasterLines[0];
    if (!varying) goto publish;
    unsigned same=cacheValid && cachedCount==snapshotCount && frames[displayBank].active &&
       exactEqual(cachedLines,spriteRasterLines,sizeof(cachedLines)) &&
       exactEqual(cachedWindows,spriteRasterWindows,sizeof(cachedWindows)) &&
       exactEqual(cachedSnapshots,snapshots,snapshotCount*sizeof(Snapshot));
    if(same) for(unsigned i=0;i<cacheTileCount;i++) {
        unsigned at=cacheTiles[i]*16;
        if(!exactEqual(cachedChars+at,chars+at,16)) {same=0;break;}
    }
    if(same) {
        spriteRasterCacheHits++;
        next=displayBank;
        goto publish;
    }
    memcpy(cachedLines,spriteRasterLines,sizeof(cachedLines));
    memcpy(cachedWindows,spriteRasterWindows,sizeof(cachedWindows));
    memcpy(cachedSnapshots,snapshots,snapshotCount*sizeof(Snapshot));
    /* Only patterns referenced by enabled sprites can affect this frame.
     * Deduplicate tile indices across all mid-frame attribute snapshots. */
    uint32_t tileBits[16]={0};
    cacheTileCount=0;
    for(unsigned si=0;si<snapshotCount;si++) for(unsigned i=0;i<64;i++) {
        unsigned attr=snapshots[si].bytes[i*4]|(snapshots[si].bytes[i*4+1]<<8);
        unsigned tile=attr&511,bit=1u<<(tile&31);
        if(!(attr&0x1800) || (tileBits[tile/32]&bit))continue;
        tileBits[tile/32]|=bit;cacheTiles[cacheTileCount++]=tile;
        memcpy(cachedChars+tile*16,chars+tile*16,16);
    }
    cachedCount=snapshotCount;cacheValid=1;
    generation++;
    initializeLookup();
    memset(f->used,0,sizeof(f->used));
    for (unsigned i=0;i<128;i++) { f->oam[i*2]=0x200; f->oam[i*2+1]=0; }
    unsigned snap=0, lastMode=~0u;
    for (unsigned y=0; y<LINES; y++) {
        unsigned input=spriteRasterLines[y], mode=(input>>16)&255;
        unsigned changed=y==0 || mode!=lastMode;
        while (snap+1<snapshotCount && snapshots[snap+1].line<=y) { snap++; changed=1; }
        if (changed) buildRows(snapshots[snap].bytes,mode);
        lastMode=mode;
        unsigned window=spriteRasterWindows[y];
        unsigned wy=(window>>8)&255, wh=window>>24;
        if (y<wy || y>=wy+wh) continue;
        unsigned left=window&255, right=left+((window>>16)&255);
        if (right>160) right=160;
        unsigned lookup=(y-(input>>8))&255;
        for (int e=rowHeads[lookup];e>=0;e=entries[e].next) {
            RowEntry *r=&entries[e]; Sprite *s=&sprites[r->sprite];
            unsigned rawY=s->y+((input>>8)&255);
            if(rawY>=256 && (rawY&255)>248) continue;
            unsigned row=r->row ^ ((s->attr&0x4000)?7:0);
            const uint8_t *p=chars+(s->attr&511)*16+row*2;
            unsigned pattern=p[0]|(p[1]<<8), priority=3-((s->attr>>11)&3);
            unsigned base=(s->x+(input&255))&255;
            if(!pattern || (base>=160 && base<=248)) continue;
            unsigned block=priority*25+(y>>5)*5+(base>>5);
            if(base>=left && base+8<=right && (base&31)<=24) {
                if(!f->used[block]) {memset(f->pixels[block],0,BLOCK_BYTES);f->used[block]=1;}
                unsigned b0=pattern>>8,b1=pattern&255;
                if(s->attr&0x8000) {b0=reversed[pattern&255];b1=reversed[pattern>>8];}
                uint32_t lo=expanded[s->palette][b0],hi=expanded[s->palette][b1];
                uint32_t ml=masks[b0],mh=masks[b1];
                unsigned xword=base&28, yword=y&31;
                uint32_t *dst=pixelWord(f,block,xword,yword);
                unsigned shift=(base&3)*8;
                if(!shift) {merge(dst,lo,ml);merge(pixelWord(f,block,xword+4,yword),hi,mh);}
                else {
                    merge(dst,lo<<shift,ml<<shift);
                    merge(pixelWord(f,block,xword+4,yword),(lo>>(32-shift))|(hi<<shift),(ml>>(32-shift))|(mh<<shift));
                    merge(pixelWord(f,block,xword+8,yword),hi>>(32-shift),mh>>(32-shift));
                }
            } else for (unsigned px=0;px<8;px++) {
                unsigned pen=(pattern>>(((s->attr&0x8000)?px:7-px)*2))&3;
                unsigned x=(base+px)&255;
                if (!pen || x<left || x>=right) continue;
                unsigned at=pixelOffset(x&31,y&31);
                unsigned target=priority*25+(y>>5)*5+(x>>5);
                if (!f->used[target]) { memset(f->pixels[target],0,BLOCK_BYTES); f->used[target]=1; }
                f->pixels[target][at]=s->palette*16+pen;
            }
        }
    }
        for (unsigned block=0;block<BLOCKS;block++) {
        if (!f->used[block]) continue;
        unsigned n=f->blocks++, cell=block%25;
        unsigned x=48+(cell%5)*32, y=20+(cell/5)*32;
        unsigned tile=(ATLAS_BASE+n*BLOCK_BYTES)/128;
        f->oam[n*2]=(y|0x2000)|((x|0x8000)<<16);
        f->oam[n*2+1]=tile|((block/25)<<10);
        f->revision[n]=generation;
    }
    f->active=1;
    spriteRasterFrames++;
    if(f->blocks>spriteRasterMaxBlocks) spriteRasterMaxBlocks=f->blocks;
    if(snapshotCount>spriteRasterMaxSnapshots) spriteRasterMaxSnapshots=snapshotCount;
publish:
    __asm__ volatile("" ::: "memory");
    pendingBank=next;
    spriteRasterStage=2;
}

void rasterSpritesPublish(void) {
    displayBank=pendingBank;
    frames[displayBank].guestFrame=pendingGuestFrame;
}

/* Explicit word stores: ARM9 byte stores cannot populate sprite VRAM. */
static void copyWords(volatile uint32_t *dst,const uint32_t *src,unsigned count) {
    #if defined(__arm__)
    while(count>=4) {
        __asm__ volatile("ldmia %1!, {r2-r5}\n\tstmia %0!, {r2-r5}"
            : "+r"(dst), "+r"(src) : : "r2","r3","r4","r5","memory");
        count-=4;
    }
#endif
    while(count--) *dst++=*src++;
}

void rasterSpritesVBlank(void) {
    spriteRasterStage=3;
    const Frame *f=&frames[displayBank];
    spriteRasterShownFrame=spriteRasterPresentedFrame;
    spriteRasterPresentedFrame=f->guestFrame;
    static unsigned wasActive;
    if(f->active) {
        unsigned n=0;
        for(unsigned block=0;block<BLOCKS;block++) if(f->used[block]) {
            volatile uint32_t *dst=(volatile uint32_t *)(0x06400000+ATLAS_BASE+n*BLOCK_BYTES);
            const uint32_t *src=(const uint32_t*)f->pixels[block];
            if(uploadedRevision[n]!=f->revision[n]) {
                copyWords(dst,src,BLOCK_BYTES/4);
                uploadedRevision[n]=f->revision[n];
                spriteRasterUploadBytes+=BLOCK_BYTES;
            } else spriteRasterSkippedBlocks++;
            n++;
        }
        copyWords((volatile uint32_t*)0x07000000,f->oam,256);
        REG_DISPCNT=(REG_DISPCNT&~(3u<<20))|(1u<<4)|(2u<<20);
    } else if(wasActive) {
        for(unsigned i=64;i<128;i++) ((volatile uint32_t*)0x07000000)[i*2]=0x200;
        REG_DISPCNT&=~((1u<<4)|(3u<<20));
    }
    wasActive=f->active;
    spriteRasterStage=4;
}
