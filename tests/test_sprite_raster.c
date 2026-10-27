/* Independent slow pixel oracle versus the row-binned, packed-word renderer. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../source/RasterSprites.c"
uint32_t frameTotal;
volatile uint32_t test_dispcnt;
static uint8_t expected[3][152][160], raw[320], chars[8192];
static uint32_t seed=0x19641026;
static unsigned randomValue(void) {seed=seed*1664525+1013904223;return seed;}
static void oracle(unsigned line,const uint8_t *r,unsigned input,unsigned window) {
    int lx=0,ly=0;unsigned mode=input>>16;
    for(unsigned i=0;i<64;i++) {
        unsigned a=r[i*4]|(r[i*4+1]<<8),pri=(a>>11)&3;
        int x=(a&0x400?lx:0)+r[i*4+2],y=(a&0x200?ly:0)+r[i*4+3];lx=x;ly=y;
        if(!pri)continue;
        x+=(input&255);y+=(input>>8)&255;
        if(y>248 && y<256)y-=256;else y&=255;
        if((int)line<y || (int)line>y+7)continue;
        unsigned row=(line-y)^((a&0x4000)?7:0);
        unsigned bits=chars[(a&511)*16+row*2]|(chars[(a&511)*16+row*2+1]<<8);
        x&=255;if(x>248)x-=256;
        for(int j=0;j<8;j++) {
            int at=x+j;
            unsigned pen=(bits>>((a&0x8000?j:7-j)*2))&3;
            unsigned wx=window&255,wy=(window>>8)&255,ww=(window>>16)&255,wh=window>>24;
            if(!pen||at<0||at>=160||at<(int)wx||at>=(int)(wx+ww)||line<wy||line>=wy+wh)continue;
            unsigned palette=mode?((a>>13)&1):(r[256+i]&15);
            if(!expected[3-pri][line][at])expected[3-pri][line][at]=palette*16+pen;
        }
    }
}
int main(void) {
#if 1
    uint8_t aa[100],bb[100];
    for(unsigned i=0;i<100;i++)aa[i]=bb[i]=i;
    for(unsigned off=0;off<4;off++)for(unsigned n=0;n<80;n++) {
        assert(exactEqual(aa+off,bb+off,n));
        if(n) {bb[off+n-1]^=1;assert(!exactEqual(aa+off,bb+off,n));bb[off+n-1]^=1;}
    }
#endif
    for(unsigned scenario=0;scenario<80;scenario++) {
        rasterSpritesReset();memset(expected,0,sizeof(expected));
        for(unsigned i=0;i<8192;i++)chars[i]=randomValue()>>24;
        for(unsigned i=0;i<64;i++) {
            unsigned a=(randomValue()&511)|((1+randomValue()%3)<<11);
            if(i%7==0)a|=0x200;if(i%5==0)a|=0x400;if(i%3==0)a|=0x4000;if(i%2==0)a|=0x8000;
            raw[i*4]=a;raw[i*4+1]=a>>8;raw[i*4+2]=randomValue()>>24;raw[i*4+3]=randomValue()>>24;raw[256+i]=randomValue()&15;
        }
        for(unsigned y=0;y<152;y++) {
            if(y==0||y==51||y==93) {
                if(y) {raw[2]=(raw[2]+37)&255;raw[7]=(raw[7]+63)&255;}
                rasterSpritesSnapshot(y,raw);
            }
            unsigned off=((scenario*17+y*3)&255)|(((scenario*29+y)&255)<<8);
            if(scenario&1)off|=0x800000;
            unsigned win=(scenario%3?0x98A00000:0x7A800B07);
            spriteRasterLines[y]=off;spriteRasterWindows[y]=win;oracle(y,raw,off,win);
        }
        rasterSpritesFinish(chars);rasterSpritesPublish();
#if 1
        unsigned hits=spriteRasterCacheHits;frameTotal++;rasterSpritesFinish(chars);rasterSpritesPublish();assert(spriteRasterCacheHits==hits+1);
#endif
        Frame *f=&frames[displayBank];assert(f->active&&f->blocks<=75);
        for(unsigned pri=0;pri<3;pri++)for(unsigned y=0;y<152;y++)for(unsigned x=0;x<160;x++) {
            unsigned block=pri*25+(y/32)*5+x/32;
            unsigned actual=f->used[block]?f->pixels[block][pixelOffset(x&31,y&31)]:0;
            if(actual!=expected[pri][y][x]) {fprintf(stderr,"scenario=%u p=%u x=%u y=%u got=%u expected=%u\n",scenario,pri,x,y,actual,expected[pri][y][x]);return 1;}
        }
    }
#if 1
    /* Correctness of cache invalidation, independent of frame number. */
    for(unsigned mutation=0;mutation<5;mutation++) {
        if(mutation==0) chars[(snapshots[0].bytes[0]|(snapshots[0].bytes[1]<<8))%512*16]^=3;
        if(mutation==1) spriteRasterLines[70]^=1;
        if(mutation==2) spriteRasterWindows[50]^=1;
        if(mutation==3) snapshots[0].bytes[2]^=1;
        if(mutation==4) snapshots[0].bytes[256]^=1;
        memset(expected,0,sizeof(expected));unsigned si=0;
        for(unsigned y=0;y<152;y++) {
            while(si+1<snapshotCount && snapshots[si+1].line<=y)si++;
            oracle(y,snapshots[si].bytes,spriteRasterLines[y],spriteRasterWindows[y]);
        }
        unsigned hits=spriteRasterCacheHits;
        rasterSpritesFinish(chars);rasterSpritesPublish();
        /* The mutated tile can be unused; an exact hit is then legitimate. */
        if(mutation)assert(spriteRasterCacheHits==hits);
        Frame *f=&frames[displayBank];
        for(unsigned pri=0;pri<3;pri++)for(unsigned y=0;y<152;y++)for(unsigned x=0;x<160;x++) {
            unsigned b=pri*25+(y/32)*5+x/32;
            unsigned got=f->used[b]?f->pixels[b][pixelOffset(x&31,y&31)]:0;
            assert(got==expected[pri][y][x]);
        }
    }
    rasterSpritesReset();assert(!cacheValid);
    for(unsigned b=0;b<BLOCKS;b++)assert(!uploadedRevision[b]);
#endif
    puts("PASS 80 deterministic scenes plus exact cache-hit and invalidation checks: chains, flips, XY wrapping, palette modes, transparency, clipping, priorities, mid-frame attributes");
}
