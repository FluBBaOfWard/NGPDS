#ifndef FILEHANDLING_HEADER
#define FILEHANDLING_HEADER

#ifdef __cplusplus
extern "C" {
#endif

#include "Emubase.h"
#include "NeoGeoPocket.h"

#define FILEEXTENSIONS ".ngp.ngc"
#define NGPF_MAGIC (0x4650474e)

typedef struct
{
	unsigned short version;		// Always 0x53?
	unsigned short blockCount;	// How many blocks are in the file
	unsigned long fileLen;		// Length of the file in bytes
}  NgfHeader;

typedef struct
{
	unsigned long ngpAddr;	// Where this block starts (in NGP memory map)
	unsigned long len;		// Length of following data in bytes.
}  NgfBlock;

/// NgpFlashFile
typedef struct
{
	u32 magic;				// NGPF
	u8  blocksLOInfo[36];	// Flash low blocks
	u8  blocksHIInfo[36];	// Flash high blocks
	u32 addressLO;			// Start address of flash low blocks in bytes
	u32 sizeLO;				// Length of flash low blocks in bytes
	u32 addressHI;			// Start address of flash high blocks in bytes
	u32 sizeHI;				// Length of flash high blocks in bytes
} NgpFlashFile;


extern ConfigData cfg;

int initSettings(void);
bool updateSettingsFromNGP(void);
int loadSettings(void);
void saveSettings(void);
bool loadGame(const char *gameName);
void loadNVRAM(void);
void saveNVRAM(void);
void loadState(void);
void saveState(void);
void ejectCart(void);
void selectGame(void);
void selectColorBios(void);
void selectBWBios(void);
int loadColorBIOS(void);
int loadBWBIOS(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // FILEHANDLING_HEADER
