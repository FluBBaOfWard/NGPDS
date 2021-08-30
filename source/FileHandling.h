#ifndef FILEHANDLING_HEADER
#define FILEHANDLING_HEADER

#ifdef __cplusplus
extern "C" {
#endif

#include "Shared/FileHelper.h"
#include "Emubase.h"

#define FILEEXTENSIONS ".ngp.ngc"

extern ConfigData cfg;

int initSettings(void);
int updateSettingsFromNGP(void);
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