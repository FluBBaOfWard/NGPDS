#include "FlashStore.h"
#include "SafeFile.h"
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#define BLOCKS 35
typedef struct {uint16_t version,count;uint32_t length;} Header;
typedef struct {uint32_t address,length;} Block;
uint32_t flashStoreWrites,flashStoreUnchanged,flashStoreFailures;
static uint8_t scratch[1024];
static bool decode(FlashStore *c,Block b,unsigned *chip,unsigned *index,unsigned *offset) {
    unsigned k,off;
    if(b.address>=0x200000 && b.address<0x400000){k=0;off=b.address-0x200000;}
    else if(b.address>=0x800000 && b.address<0xA00000){k=1;off=b.address-0x800000;}
    else return false;
    if(k>=c->chips || off>=c->chipSize)return false;
    int i=c->block(off);
    if(i<0||i>=BLOCKS || !(c->permissions[k][i]&2) || c->offset(i)!=(int)off ||
       c->size(i)!=(int)b.length || b.length>c->chipSize-off ||
       k*0x200000u+off>c->bytes || b.length>c->bytes-(k*0x200000u+off))return false;
    *chip=k;*index=i;*offset=k*0x200000u+off;return true;
}
bool flashStoreValidate(FILE *f,void *opaque) {
    FlashStore *c=opaque;Header h;Block b;uint32_t seen[2][2]={{0}},length=sizeof(h);
    if(fread(&h,1,sizeof(h),f)!=sizeof(h)||h.version!=0x53||h.count>BLOCKS*c->chips)return false;
    for(unsigned n=0;n<h.count;n++) {
        unsigned chip,i,off;
        if(fread(&b,1,sizeof(b),f)!=sizeof(b)||!decode(c,b,&chip,&i,&off)||
           (seen[chip][i/32]&(1u<<(i%32))))return false;
        seen[chip][i/32]|=1u<<(i%32);length+=sizeof(b)+b.length;
        for(unsigned left=b.length;left;) {
            unsigned z=left>sizeof(scratch)?sizeof(scratch):left;
            if(fread(scratch,1,z,f)!=z)return false;
            left-=z;
        }
    }
    return length==h.length && fgetc(f)==EOF && !ferror(f);
}
/* Zero blocks is a valid legacy no-op only with the exact header/EOF.
 * A nonempty recovery file takes precedence over that placeholder. */
static int flashRank(FILE *f, void *opaque) {
    Header h;
    if (fread(&h, 1, sizeof(h), f) != sizeof(h) || fseek(f, 0, SEEK_SET) ||
        !flashStoreValidate(f, opaque)) return -1;
    return h.count ? 1 : 0;
}
static bool nonemptyFlash(FILE *f, void *opaque) {
    return flashRank(f, opaque) > 0;
}
static Header current(FlashStore *c) {
    Header h={0x53,0,sizeof(Header)};
    for(unsigned chip=0;chip<c->chips;chip++)for(unsigned i=0;i<BLOCKS;i++)if(c->dirty(chip,i)) {
        unsigned off=c->offset(i),len=c->size(i);
        if(!(c->permissions[chip][i]&2)||off>=c->chipSize||len>c->chipSize-off)continue;
        h.count++;h.length+=sizeof(Block)+len;
    }
    return h;
}
static bool emit(FILE *f,void *opaque) {
    FlashStore *c=opaque;Header h=current(c);
    if(fwrite(&h,1,sizeof(h),f)!=sizeof(h))return false;
    for(unsigned chip=0;chip<c->chips;chip++)for(unsigned i=0;i<BLOCKS;i++)if(c->dirty(chip,i)) {
        Block b={c->offset(i)+(chip?0x800000:0x200000),c->size(i)};unsigned cc,ii,off;
        if(!decode(c,b,&cc,&ii,&off))return false;
        if(fwrite(&b,1,sizeof(b),f)!=sizeof(b)||fwrite(c->rom+off,1,b.length,f)!=b.length)return false;
    }
    return true;
}
static bool matches(FILE *f,void *opaque) {
    FlashStore *c=opaque;Header a,b=current(c);bool ok=fread(&a,1,sizeof(a),f)==sizeof(a)&&!memcmp(&a,&b,sizeof(a));
    for(unsigned chip=0;ok&&chip<c->chips;chip++)for(unsigned i=0;ok&&i<BLOCKS;i++)if(c->dirty(chip,i)) {
        Block got,want={c->offset(i)+(chip?0x800000:0x200000),c->size(i)};
        ok=fread(&got,1,sizeof(got),f)==sizeof(got)&&!memcmp(&got,&want,sizeof(got));
        for(unsigned at=0;ok&&at<want.length;) {
            unsigned n=want.length-at;if(n>sizeof(scratch))n=sizeof(scratch);
            ok=fread(scratch,1,n,f)==n&&!memcmp(scratch,c->rom+chip*0x200000u+c->offset(i)+at,n);at+=n;
        }
    }
    return ok&&fgetc(f)==EOF&&!ferror(f);
}
int flashStoreSave(const char *p,FlashStore *c) {
    if(!c->rom||!c->bytes||!current(c).count){flashStoreUnchanged++;return 0;}
    if(safeFileValid(p,matches,c)){flashStoreUnchanged++;return 0;}
    if(!safeFileReplaceRanked(p,emit,flashStoreValidate,matches,flashRank,c)){flashStoreFailures++;return -1;}
    flashStoreWrites++;return 1;
}
int flashStoreLoad(const char *p,FlashStore *c) {
    char selected[512], paths[3][512];
    int n = snprintf(paths[0], sizeof(paths[0]), "%s", p);
    int b = snprintf(paths[1], sizeof(paths[1]), "%s.bak", p);
    int t = snprintf(paths[2], sizeof(paths[2]), "%s.tmp", p);
    if (n < 0 || n >= sizeof(paths[0]) || b < 0 || b >= sizeof(paths[1]) ||
        t < 0 || t >= sizeof(paths[2])) return -1;
    int empty = -1;
    bool invalid = false, found = false;
    for (unsigned i = 0; i < 3; i++) {
        FILE *candidate = fopen(paths[i], "rb");
        if (!candidate) {
            if (access(paths[i], F_OK) == 0 || errno != ENOENT) invalid = true;
            continue;
        }
        int rank = flashRank(candidate, c);
        if (fclose(candidate)) rank = -1;
        if (rank > 0) {
            if (i == 2) {
                /* Keep the sole valid temporary generation protected if
                 * promotion fails, as in the original recovery path. */
                if (!safeFileSelect(p, selected, sizeof(selected), nonemptyFlash, c)) return -1;
            }
            else strcpy(selected, paths[i]);
            found = true;
            break;
        }
        if (rank == 0 && empty < 0) empty = i;
        if (rank < 0) invalid = true;
    }
    if (!found) {
        /* A placeholder may not hide an unreadable or damaged potential
         * progress file. Only wholly valid empty/absent sets are no-ops. */
        if (invalid) return -1;
        if (empty < 0) return 0;
        strcpy(selected, paths[empty]);
    }
    FILE *f=fopen(selected,"rb");if(!f)return -1;
    Header h;bool ok=fread(&h,1,sizeof(h),f)==sizeof(h);
    if(ok)ok=h.version==0x53&&h.count<=BLOCKS*c->chips&&
        h.length>=sizeof(h)&&h.length<=c->bytes+sizeof(Header)+BLOCKS*c->chips*sizeof(Block);
    uint8_t *buffer=ok?malloc(h.length):NULL;
    if(!buffer)ok=false;
    if(ok) {
        memcpy(buffer,&h,sizeof(h));
        ok=fread(buffer+sizeof(h),1,h.length-sizeof(h),f)==h.length-sizeof(h)&&fgetc(f)==EOF&&!ferror(f);
    }
    if(fclose(f))ok=false;
    /* A late read/close failure must not leave a partially applied save in ROM
     * RAM. Stage first, then validate the staged geometry before any mutation. */
    unsigned pos=sizeof(h);uint32_t seen[2][2]={{0}};
    for(unsigned n=0;ok&&n<h.count;n++) {
        Block b;unsigned chip,i,off;
        if(pos>h.length || h.length-pos<sizeof(b)){ok=false;break;}
        memcpy(&b,buffer+pos,sizeof(b));pos+=sizeof(b);
        ok=decode(c,b,&chip,&i,&off)&&b.length<=h.length-pos&&!(seen[chip][i/32]&(1u<<(i%32)));
        if(ok){seen[chip][i/32]|=1u<<(i%32);pos+=b.length;}
    }
    if(ok)ok=pos==h.length;
    /* Mark before copying; callbacks cannot trigger file I/O or emulation. */
    for(unsigned chip=0;ok&&chip<c->chips;chip++)for(unsigned i=0;i<BLOCKS;i++)
        if(seen[chip][i/32]&(1u<<(i%32)))if(!c->mark(chip,i)){ok=false;break;}
    pos=sizeof(h);
    for(unsigned n=0;ok&&n<h.count;n++) {
        Block b;unsigned chip,i,off;memcpy(&b,buffer+pos,sizeof(b));pos+=sizeof(b);
        decode(c,b,&chip,&i,&off);memcpy(c->rom+off,buffer+pos,b.length);pos+=b.length;
    }
    free(buffer);return ok?1:-1;
}
