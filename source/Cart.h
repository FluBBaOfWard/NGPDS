#ifndef CART_HEADER
#define CART_HEADER

#ifdef __cplusplus
extern "C" {
#endif

#include "NGPHeader.h"

extern u32 gRomSize;
extern u32 maxRomSize;
extern u32 allocatedRomMemSize;
extern u32 emuFlags;
extern u8 gConfig;
extern u8 gMachineSet;
extern u8 gMachine;
extern u8 gSOC;
extern u8 gLang;
extern u8 gPaletteBank;

extern u8 ngpRAM[0x4000];
extern u8 biosSpace[0x10000];
extern u8 biosSpaceColor[0x10000];
extern u8 *romSpacePtr;
extern u8 *allocatedRomMem;
extern const NgpHeader *ngpHeader;
extern const void *g_BIOSBASE_COLOR;
extern const void *g_BIOSBASE_BNW;

void machineInit(void);
void loadCart(int emuFlags);
void tlcs9000MemInit(u8 *romPtr);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // CART_HEADER
