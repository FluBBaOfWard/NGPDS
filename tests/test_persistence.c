#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>
static int faultKind,faultAt,calls,hits;
static bool fail(int k){if(k!=faultKind)return false;if(++calls==faultAt){hits++;errno=ENOSPC;return true;}return false;}
static void *badMalloc(size_t n){if(fail(7)){errno=ENOMEM;return NULL;}return malloc(n);}
static size_t badWrite(const void *p,size_t s,size_t n,FILE *f){if(fail(1)){size_t z=n/2;fwrite(p,s,z,f);return z;}return fwrite(p,s,n,f);}
static int badFlush(FILE *f){int z=fflush(f);return fail(2)?EOF:z;}
static int badClose(FILE *f){int z=fclose(f);return fail(3)?EOF:z;}
static int badRename(const char *a,const char *b){return fail(4)?-1:rename(a,b);}
static int badRemove(const char *p){return fail(5)?-1:remove(p);}
static size_t badRead(void *p,size_t s,size_t n,FILE *f){return fail(6)?0:fread(p,s,n,f);}
#define malloc badMalloc
#define fwrite badWrite
#define fflush badFlush
#define fclose badClose
#define rename badRename
#define remove badRemove
#define fread badRead
#include "../source/SafeFile.c"
#include "../source/FlashStore.c"
#undef malloc
#undef fwrite
#undef fflush
#undef fclose
#undef rename
#undef remove
#undef fread
static uint8_t data[0x80000],perms[35];static bool dirty[35];
static bool dirtyFn(int c,int i){return !c&&dirty[i];}
static bool markFn(int c,int i){if(c||i>=35||!(perms[i]&2))return false;dirty[i]=true;return true;}
static int offFn(int i){return i*8192;}
static int sizeFn(int i){(void)i;return 8192;}
static int blockFn(int a){return a/8192;}
static FlashStore ctx={data,sizeof(data),sizeof(data),1,dirtyFn,markFn,offFn,sizeFn,blockFn,{perms,perms}};
static unsigned char saved[8208];
static void clean(void){faultKind=0;remove("game.fla");remove("game.fla.tmp");remove("game.fla.bak");remove("game.fla.bad");}
static bool equalFile(const char *name,const void *p,size_t n){unsigned char b[8208];FILE *f=fopen(name,"rb");if(!f)return false;bool z=fread(b,1,sizeof(b),f)==n&&!memcmp(b,p,n);fclose(f);return z;}
static void fixture(void){clean();memset(data,0x5a,sizeof(data));memset(dirty,0,sizeof(dirty));dirty[6]=true;assert(flashStoreSave("game.fla",&ctx)==1);FILE *f=fopen("game.fla","rb");assert(fread(saved,1,sizeof(saved),f)==sizeof(saved));fclose(f);assert(flashStoreSave("game.fla",&ctx)==0);data[offFn(6)+3]^=1;assert(flashStoreSave("game.fla",&ctx)==1);f=fopen("game.fla","rb");assert(fread(saved,1,sizeof(saved),f)==sizeof(saved));fclose(f);data[offFn(6)+4]^=1;}
int main(void){char dir[]="/tmp/ngpds-safe-XXXXXX";assert(mkdtemp(dir));assert(!chdir(dir));for(unsigned i=6;i<35;i++)perms[i]=2;
 clean();memset(dirty,0,sizeof(dirty));assert(!flashStoreSave("game.fla",&ctx));assert(access("game.fla",F_OK));
 unsigned scenarios=0,triggered=0;
 for(int kind=1;kind<=6;kind++)for(int at=1;at<=12;at++) {
  fixture();faultKind=kind;faultAt=at;calls=hits=0;int status=flashStoreSave("game.fla",&ctx);int h=hits;faultKind=0;
  assert(equalFile("game.fla",saved,sizeof(saved))||equalFile("game.fla.bak",saved,sizeof(saved))||equalFile("game.fla.bad",saved,sizeof(saved)));
  if(status>0)assert(safeFileValid("game.fla",matches,&ctx));scenarios++;triggered+=h!=0;
 }
 fixture();faultKind=0;assert(flashStoreSave("game.fla",&ctx)==1);memset(dirty,0,sizeof(dirty));memset(data,0,sizeof(data));assert(flashStoreLoad("game.fla",&ctx)==1);unsigned writes=flashStoreWrites;assert(flashStoreSave("game.fla",&ctx)==0);assert(writes==flashStoreWrites);
 /* Truncated/bad geometry must be rejected before touching any cartridge bytes. */
 for(unsigned mutation=0;mutation<6;mutation++) {
  fixture();FILE *f=fopen("game.fla","wb");unsigned char corrupt[8208];memcpy(corrupt,saved,sizeof(saved));
  if(mutation==0)corrupt[0]=0;if(mutation==1)corrupt[2]=71;if(mutation==2)memset(corrupt+8,0xff,4);if(mutation==3)corrupt[12]=1;if(mutation==4)corrupt[4]=0;
  fwrite(corrupt,1,mutation==5?100:sizeof(corrupt),f);fclose(f);remove("game.fla.bak");uint8_t before[sizeof(data)];memcpy(before,data,sizeof(data));assert(flashStoreLoad("game.fla",&ctx)==-1);assert(!memcmp(before,data,sizeof(data)));
 }
 /* Two individually valid blocks must still be rejected when duplicated. */
 fixture();faultKind=0;unsigned char duplicate[16408];Header dh={0x53,2,sizeof(duplicate)};
 memcpy(duplicate,&dh,sizeof(dh));memcpy(duplicate+8,saved+8,8200);memcpy(duplicate+8208,saved+8,8200);
 FILE *df=fopen("game.fla","wb");assert(fwrite(duplicate,1,sizeof(duplicate),df)==sizeof(duplicate));assert(!fclose(df));remove("game.fla.bak");remove("game.fla.tmp");
 unsigned char unchanged[sizeof(data)];memcpy(unchanged,data,sizeof(data));assert(flashStoreLoad("game.fla",&ctx)==-1);assert(!memcmp(unchanged,data,sizeof(data)));
 unsigned loadSchedules=0;
 for(int kind=3;kind<=7;kind++) {
  if(kind!=3&&kind!=6&&kind!=7)continue;
  for(int at=1;at<=20;at++) {
   fixture();faultKind=0;memset(data,0xa7,sizeof(data));uint8_t before[sizeof(data)];memcpy(before,data,sizeof(data));
   faultKind=kind;faultAt=at;calls=hits=0;int z=flashStoreLoad("game.fla",&ctx);faultKind=0;
   if(z<0)assert(!memcmp(before,data,sizeof(data)));
   loadSchedules++;
  }
 }
 fixture();assert(!rename("game.fla","game.fla.tmp"));remove("game.fla.bak");faultKind=4;faultAt=1;calls=hits=0;assert(flashStoreLoad("game.fla",&ctx)==1);faultKind=0;data[offFn(6)+7]^=1;assert(flashStoreSave("game.fla",&ctx)==-1);assert(!access("game.fla.tmp",F_OK));assert(flashStoreLoad("game.fla",&ctx)==1);assert(!access("game.fla",F_OK));
 fixture();assert(!remove("game.fla"));assert(flashStoreLoad("game.fla",&ctx)==1);assert(!access("game.fla.bak",F_OK));
 clean();chdir("/");rmdir(dir);printf("PASS: %u load schedules (including OOM/late read/close, failed loads leave ROM unchanged); %u save failure schedules (%u triggered); unchanged/no-dirty, partial writes, flush/close/read/rename/remove failures, malformed geometry, backup and sole-temp recovery\n",loadSchedules,scenarios,triggered);
 return 0;
}
