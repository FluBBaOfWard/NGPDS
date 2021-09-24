#ifndef NGPFLASH_HEADER
#define NGPFLASH_HEADER

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_BLOCKS (35)

extern u32 flashSize;
extern u8 g_paletteBank;

bool isBlockDirty(int chip, int block);
void markBlockDirty(int chip, int block);

/**
 * Get the offset of the block in the flash chip memory.
 */
int getBlockOffset(int block);
int getBlockSize(int block);
int getBlockFromAddress(int address);
int getSaveStartAddress(void);
void *getFlashLOBlocksAddress(void);
void *getFlashHIBlocksAddress(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NGPFLASH_HEADER
