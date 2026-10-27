#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include "SafeFile.h"
#include "FlashStore.h"

#include "FileHandling.h"
#include "Shared/EmuMenu.h"
#include "Shared/EmuSettings.h"
#include "Shared/FileHelper.h"
#include "Shared/AsmExtra.h"
#include "Shared/CartridgeRAM.h"
#include "Main.h"
#include "Gui.h"
#include "Cart.h"
#include "cpu.h"
#include "Gfx.h"
#include "io.h"
#include "Memory.h"
#include "NGPHeader.h"
#include "NGPFlash/NGPFlash.h"

static const char *const settingName = "settings.cfg";

ConfigData cfg;
static char storageFolder[32], loadedFilename[FILENAME_MAX_LENGTH], loadedROMPath[512];
static bool flashLoadBlocked, settingsLoadBlocked;
uint32_t persistenceFailures, settingsWrites;
static bool storageEnter(char *previous) {
    if(!fatAvailable || !getcwd(previous,FILEPATH_MAX_LENGTH))return false;
    if(!storageFolder[0]) {
        const char *dirs[]={"/data/ngpds","/ngpds"};
        struct stat st;
        for(unsigned i=0;i<2;i++) {
            char p[64];snprintf(p,sizeof(p),"%s/settings.cfg",dirs[i]);
            if(!access(p,F_OK)){strcpy(storageFolder,dirs[i]);break;}
            strcat(p,".bak");if(!access(p,F_OK)){strcpy(storageFolder,dirs[i]);break;}
            snprintf(p,sizeof(p),"%s/settings.cfg.tmp",dirs[i]);
            if(!access(p,F_OK)){strcpy(storageFolder,dirs[i]);break;}
        }
        if(!storageFolder[0])strcpy(storageFolder,(!stat(dirs[0],&st)&&S_ISDIR(st.st_mode))?dirs[0]:dirs[1]);
        if(mkdir(storageFolder,0777) && errno!=EEXIST)return false;
    }
    return chdir(storageFolder)==0;
}
extern int flashDirtyResult(int,int) __asm__("isBlockDirty");
extern int flashMarkResult(int,int) __asm__("markBlockDirty");
static bool dirtyBlock(int c,int b) {return flashDirtyResult(c,b)!=0;}
static bool markBlock(int c,int b) {return flashMarkResult(c,b)!=0;}
static FlashStore flashContext(void) {
    FlashStore c={romSpacePtr,flashSize,flashSize==0x400000?0x200000:flashSize,
        flashSize==0x400000?2:1,dirtyBlock,markBlock,getBlockOffset,getBlockSize,getBlockFromAddress,
        {getFlashLOBlocksAddress(),getFlashHIBlocksAddress()}};
    if(c.bytes>maxRomSize)c.bytes=maxRomSize;
    return c;
}
/* Dirty means different from the original cartridge, not "not yet saved".
 * A soft reset must not forget blocks that are still overlaid in ROM RAM. */
void resetCartridgePreservingFlash(bool wholeMachine) {
    bool modified[2][MAX_BLOCKS]={{false}};
    bool preserve=gameInserted&&loadedFilename[0];
    unsigned chips=flashSize==0x400000?2:1;
    if(preserve)for(unsigned c=0;c<chips;c++)for(unsigned b=0;b<MAX_BLOCKS;b++)modified[c][b]=dirtyBlock(c,b);
    if(wholeMachine)machineInit();else loadCart(0);
    if(preserve)for(unsigned c=0;c<chips;c++)for(unsigned b=0;b<MAX_BLOCKS;b++)if(modified[c][b])markBlock(c,b);
}
static bool configDataValid(const ConfigData *value) {
	return !memcmp(value->magic, "cfg", 4) && value->gammaValue <= 4 &&
		value->language <= 1 && value->palette <= 7 &&
		memchr(value->currentPath, 0, sizeof(value->currentPath)) &&
		memchr(value->biosPath, 0, sizeof(value->biosPath)) &&
		memchr(value->biosPathColor, 0, sizeof(value->biosPathColor));
}
static bool configValid(FILE *f,void *unused) {
	(void)unused;
	ConfigData value;
	return fread(&value, 1, sizeof(value), f) == sizeof(value) &&
		fgetc(f) == EOF && !ferror(f) && configDataValid(&value);
}
static bool configEqual(FILE *f,void *unused) {
    (void)unused;ConfigData v;
    return fread(&v,1,sizeof(v),f)==sizeof(v)&&!memcmp(&v,&cfg,sizeof(v))&&fgetc(f)==EOF&&!ferror(f);
}
static bool configWrite(FILE *f,void *unused) {(void)unused;return fwrite(&cfg,1,sizeof(cfg),f)==sizeof(cfg);}


//---------------------------------------------------------------------------------
void applyConfigData(void) {
	emuSettings  = cfg.emuSettings & ~EMUSPEED_MASK;	// Clear speed setting.
	gGammaValue  = cfg.gammaValue;
	gLang        = cfg.language;
	gPaletteBank = cfg.palette;
	gConfig      = cfg.config;
	int mach     = gConfig & 3;
	if (mach == 3) mach = 0;
	gMachineSet  = mach;
	joyCfg       = (joyCfg & ~0x400) | ((cfg.controller & 1) << 10);
	strlcpy(currentDir, cfg.currentPath, sizeof(currentDir));
	pauseEmulation = (emuSettings & AUTOPAUSE_EMULATION);
}

void updateConfigData(void) {
	strcpy(cfg.magic, "cfg");
	cfg.emuSettings = emuSettings & ~EMUSPEED_MASK;		// Clear speed setting.
	cfg.gammaValue  = gGammaValue;
	cfg.language    = gLang;
	cfg.palette     = gPaletteBank;
	cfg.config      = (cfg.config & ~3) | gMachineSet;
	cfg.controller  = (joyCfg >> 10) & 1;
	strlcpy(cfg.currentPath, currentDir, sizeof(cfg.currentPath));
}

void initSettings() {
	memset(&cfg, 0, sizeof(ConfigData));
	cfg.emuSettings = AUTOPAUSE_EMULATION | AUTOLOAD_NVRAM | AUTOSAVE_NVRAM | AUTOSAVE_SETTINGS | ALLOW_SPEED_HACKS | AUTOSLEEP_OFF;
	cfg.alarmHour   = PersonalData->alarmHour;
	cfg.alarmMinute = PersonalData->alarmMinute;
	cfg.birthDay    = PersonalData->birthDay;
	cfg.birthMonth  = PersonalData->birthMonth;
	cfg.birthYear   = 99;
	cfg.language    = (PersonalData->language == 0) ? 0 : 1;
	int col = 0;		// Black n White
	switch (PersonalData->theme & 0xF) {
		case 1:
		case 4:
			col = 4;	// Brown
			break;
		case 2:
		case 3:
		case 15:
			col = 1;	// Red
			break;
		case 6:
		case 7:
		case 8:
			col = 2;	// Green
			break;
		case 10:
		case 11:
		case 12:
			col = 3;	// Blue
			break;
		default:
			break;
	}
	cfg.palette = col;

	applyConfigData();
}

bool updateSettingsFromNGP() {
	if (g_BIOSBASE_COLOR == NULL && g_BIOSBASE_BNW == NULL) {
		return false;
	}

	bool changed = false;
	int val;
	val = t9LoadBX(0x6F8B);
	if (cfg.birthYear != val) {
		cfg.birthYear = val;
		changed = true;
	}
	val = t9LoadBX(0x6F8C);
	if (cfg.birthMonth != val) {
		cfg.birthMonth = val;
		changed = true;
	}
	val = t9LoadBX(0x6F8D);
	if (cfg.birthDay != val) {
		cfg.birthDay = val;
		changed = true;
	}

	val = t9LoadBX(0x6C34);
	if (cfg.alarmHour != val) {
		cfg.alarmHour = val;
		changed = true;
	}
	val = t9LoadBX(0x6C35);
	if (cfg.alarmMinute != val) {
		cfg.alarmMinute = val;
		changed = true;
	}

	val = t9LoadBX(0x6F87) & 1;
	if (cfg.language != val) {
		cfg.language = val;
		gLang = val;
		changed = true;
	}
	if (gMachine == HW_NGPCOLOR) {
		val = t9LoadBX(0x6F94) & 7;
		if (cfg.palette != val) {
			cfg.palette = val;
			gPaletteBank = val;
			changed = true;
		}
	}
	settingsChanged |= changed;

	return changed;
}

int loadSettings() {
    char previous[FILEPATH_MAX_LENGTH], selected[512];
    if(!storageEnter(previous))return 1;
    bool ok=safeFileSelect(settingName,selected,sizeof(selected),configValid,NULL);
    if(ok) {
        FILE *f=fopen(selected,"rb");ConfigData value;
        ok=f && fread(&value,1,sizeof(value),f)==sizeof(value) &&
            fgetc(f)==EOF && !ferror(f) && configDataValid(&value);
        if(f && fclose(f))ok=false;
        if(ok){cfg=value;applyConfigData();settingsLoadBlocked=false;infoOutput("Settings loaded.");}
        else {
            settingsLoadBlocked=true;emuSettings&=~(AUTOSAVE_SETTINGS|AUTOSAVE_NVRAM);
            infoOutput("Settings read failed: autosave disabled.");
        }
    } else if(!(access(settingName,F_OK)!=0&&errno==ENOENT) ||
              !(access("settings.cfg.bak",F_OK)!=0&&errno==ENOENT) ||
              !(access("settings.cfg.tmp",F_OK)!=0&&errno==ENOENT)) {
        settingsLoadBlocked=true;
        emuSettings&=~(AUTOSAVE_SETTINGS|AUTOSAVE_NVRAM);
        infoOutput("Invalid settings: autosave disabled.");
    }
    chdir(previous);return ok?0:1;
}
int saveSettings() {
    if(settingsLoadBlocked)return 1;
    updateConfigData();char previous[FILEPATH_MAX_LENGTH];
    if(!storageEnter(previous))return 1;
    bool ok=safeFileValid(settingName,configEqual,NULL);
    if(!ok){ok=safeFileReplace(settingName,configWrite,configValid,configEqual,NULL);if(ok)settingsWrites++;}
    chdir(previous);
    if(ok){settingsChanged=false;return 0;}
    persistenceFailures++;infoOutput("Settings save failed; previous copy kept.");return 1;
}
void saveSettingsManual(void) {settingsLoadBlocked=false;saveSettings();}

bool saveFlashChecked(void) {
    if(!gameInserted||!loadedFilename[0])return true;
    if(flashLoadBlocked){infoOutput("Flash read failed: reload ROM before saving.");return false;}
    char previous[FILEPATH_MAX_LENGTH],name[FILENAME_MAX_LENGTH];
    if(!storageEnter(previous)){persistenceFailures++;return false;}
    setFileExtension(name,loadedFilename,".fla",sizeof(name));
    FlashStore c=flashContext();int status=flashStoreSave(name,&c);
    chdir(previous);
    if(status<0){persistenceFailures++;infoOutput("Flash save failed; ROM kept, previous copy retained.");return false;}
    if(status>0)infoOutput("Flash saved.");
    return true;
}
void saveNVRAM(void) {saveFlashChecked();}
void loadNVRAM(void) {
    if(!gameInserted||!loadedFilename[0])return;
    char previous[FILEPATH_MAX_LENGTH],name[FILENAME_MAX_LENGTH];
    if(!storageEnter(previous)){flashLoadBlocked=true;return;}
    setFileExtension(name,loadedFilename,".fla",sizeof(name));
    FlashStore c=flashContext();int status=flashStoreLoad(name,&c);
    if(status==0 && loadedROMPath[0]) {
        /* Older direct-argv launches used an absolute currentFilename and
         * therefore placed Flash beside the ROM. Import read-only only when
         * no primary/recovery file exists in the selected data directory. */
        char legacy[512];setFileExtension(legacy,loadedROMPath,".fla",sizeof(legacy));
        status=flashStoreLoad(legacy,&c);
    }
    chdir(previous);
    if(status<0)flashLoadBlocked=true;else if(status>0)flashLoadBlocked=false;
    if(status<0){persistenceFailures++;infoOutput("Invalid/unreadable flash; saving blocked.");}
    else if(status>0)infoOutput("Flash loaded.");
}

typedef struct {void *data;unsigned size;} StateBuffer;
static bool stateValid(FILE *f,void *ctx) {
    StateBuffer *b=ctx;return !fseek(f,0,SEEK_END)&&ftell(f)==(long)b->size;
}
static bool stateEqual(FILE *f,void *ctx) {
    StateBuffer *b=ctx;unsigned at=0;unsigned char tmp[512];
    while(at<b->size) {unsigned n=b->size-at;if(n>sizeof(tmp))n=sizeof(tmp);
        if(fread(tmp,1,n,f)!=n||memcmp(tmp,(char *)b->data+at,n))return false;
        at+=n;}
    return fgetc(f)==EOF&&!ferror(f);
}
static bool stateWrite(FILE *f,void *ctx) {StateBuffer *b=ctx;return fwrite(b->data,1,b->size,f)==b->size;}
void loadState() {
    if(!gameInserted||!loadedFilename[0])return;
    char previous[FILEPATH_MAX_LENGTH],name[FILENAME_MAX_LENGTH],selected[512];
    if(!storageEnter(previous))return;
    setFileExtension(name,loadedFilename,".sta",sizeof(name));StateBuffer b={NULL,getStateSize()};
    bool found=safeFileSelect(name,selected,sizeof(selected),stateValid,&b);
    if(!found && loadedROMPath[0]) {
        char bak[512],tmp[512],legacy[512];
        snprintf(bak,sizeof(bak),"%s.bak",name);snprintf(tmp,sizeof(tmp),"%s.tmp",name);
        if(access(name,F_OK)!=0&&errno==ENOENT&&access(bak,F_OK)!=0&&errno==ENOENT&&access(tmp,F_OK)!=0&&errno==ENOENT) {
            setFileExtension(legacy,loadedROMPath,".sta",sizeof(legacy));
            found=safeFileSelect(legacy,selected,sizeof(selected),stateValid,&b);
        }
    }
    if(found && (b.data=malloc(b.size))) {
        FILE *f=fopen(selected,"rb");bool ok=f&&fread(b.data,1,b.size,f)==b.size;
        if(f&&fclose(f))ok=false;
        if(ok){unpackState(b.data);infoOutput("Loaded state (Flash is separate).");}
        free(b.data);
    }
    chdir(previous);
}
void saveState() {
    if(!gameInserted||!loadedFilename[0])return;
    char previous[FILEPATH_MAX_LENGTH],name[FILENAME_MAX_LENGTH];
    if(!storageEnter(previous))return;
    setFileExtension(name,loadedFilename,".sta",sizeof(name));StateBuffer b={malloc(getStateSize()),getStateSize()};
    if(b.data){packState(b.data);if(!safeFileReplace(name,stateWrite,stateValid,stateEqual,&b))infoOutput("State save failed; previous copy kept.");free(b.data);}
    chdir(previous);
}
bool persistenceCanRun(void) {return !flashLoadBlocked;}
bool persistBeforeChange(void) {
    if((emuSettings&AUTOSAVE_NVRAM)&&!saveFlashChecked())return false;
    if((emuSettings&AUTOSAVE_SETTINGS)&&saveSettings())return false;
    return true;
}
void exitGameSafely(void) {if(persistBeforeChange())exitEmulator();}
void autosaveSettingsSet(void) {
    if(settingsLoadBlocked){infoOutput("Settings unreadable: check storage and restart.");return;}
    autoSettingsSet();saveSettings();
}

/// Hold down the power button for ~40 frames.
static void turnPowerOff(void) {
	int i;
	if (g_BIOSBASE_COLOR != NULL || g_BIOSBASE_BNW != NULL) {
		EMUinput = 0;
		for (i = 0; i < 100; i++) {
			run();
			EMUinput |= 4;
			if (isConsoleSleeping()) {
				break;
			}
		}
		// Run a few more frames to turn off LED.
		for (i = 0; i < 10; i++) {
			run();
		}
		if (!isConsoleSleeping()) {
			resetCartridgePreservingFlash(true);
		}
	}
}

/// Hold down the power button for ~40 frames.
static void turnPowerOn(void) {
	int i;
	if (g_BIOSBASE_COLOR != NULL || g_BIOSBASE_BNW != NULL) {
		EMUinput = 0;
		for (i = 0; i < 100; i++) {
			run();
			EMUinput |= 4;
			if (isConsoleRunning()) {
				break;
			}
		}
	}
}

/* Keep incomplete cartridge reads out of the running emulation. ZIP reads
 * retain the shared bounded decompressor; raw reads check every I/O result. */
static int readCartridge(void *dest,const char *name,unsigned limit) {
    char ext[8];getFileExtension(ext,name);
    if(!strcmp(ext,".zip"))return loadROM(dest,name,limit);
    FILE *f=fopen(name,"rb");if(!f)return 0;
    long length=0;bool ok=!fseek(f,0,SEEK_END);
    if(ok){length=ftell(f);ok=length>0&&(unsigned long)length<=limit&&!fseek(f,0,SEEK_SET);}
    if(ok)ok=fread(dest,1,length,f)==(unsigned long)length&&!ferror(f);
    if(fclose(f))ok=false;
    if(!ok){infoOutput("ROM read failed; game not started.");return 0;}
    strlcpy(currentFilename,name,sizeof(currentFilename));return length;
}

//---------------------------------------------------------------------------------
bool loadGame(const char *gameName) {
	if (gameName) {
        char extension[8];getFileExtension(extension,gameName);
        const char *basename=strrchr(gameName,'/');basename=basename?basename+1:gameName;
        if(strcmp(extension,".zip") && strlen(basename)>=sizeof(loadedFilename)) {
            infoOutput("ROM filename too long; current game kept.");return true;
        }
        if(!persistBeforeChange())return true;
		cls(0);
		if (isConsoleRunning()) {
			drawText("     Please wait, power off.", 11, 0);
			turnPowerOff();
            if((emuSettings&AUTOSAVE_NVRAM)&&!saveFlashChecked())return true;
		}
		drawText("     Please wait, loading.", 11, 0);
		u32 maxSize = allocatedRomMemSize;
		u8 *romPtr = allocatedRomMem;
		gameInserted=false;
        flashLoadBlocked=false;
		gRomSize = readCartridge(romPtr, gameName, maxSize);
		if (!gRomSize) {
			// Enable Expansion RAM in GBA port
			if (cartRamInit(DETECT_RAM) != DETECT_RAM) {
				infoOutput("Trying Exp-RAM.");
				romPtr = (u8 *)cartRamUnlock();
				maxSize = cartRamSize();
				gameInserted=false;
        flashLoadBlocked=false;
		gRomSize = readCartridge(romPtr, gameName, maxSize);
				enableSlot2Cache();
			}
		}
		else {
			cartRamLock();
		}

		if (gRomSize) {
			maxRomSize = maxSize;
			romSpacePtr = romPtr;
			tlcs9000MemInit(romSpacePtr);
			checkMachine();
			setEmuSpeed(0);
			loadCart(emuFlags);
			gameInserted = true;
            char ext[8];getFileExtension(ext,gameName);
            /* Raw argv paths can exceed currentFilename's basename buffer.
             * ZIP saves still use the inner cartridge name supplied by loadROM. */
            const char *identity=strcmp(ext,".zip")?gameName:currentFilename;
            const char *base=strrchr(identity,'/');
            strlcpy(loadedFilename,base?base+1:identity,sizeof(loadedFilename));
            loadedROMPath[0]=0;
            if(strcmp(ext,".zip")) {
                if(gameName[0]=='/'||strchr(gameName,':'))strlcpy(loadedROMPath,gameName,sizeof(loadedROMPath));
                else {char dir[FILEPATH_MAX_LENGTH];if(getcwd(dir,sizeof(dir)))snprintf(loadedROMPath,sizeof(loadedROMPath),"%s%s%s",dir,strcmp(dir,"/")?"/":"",gameName);}
            }
			if (emuSettings & AUTOLOAD_NVRAM) {
				loadNVRAM();
			}
            if(flashLoadBlocked){gameInserted=false;return true;}
			if (emuSettings & AUTOLOAD_STATE) {
				loadState();
			}
			drawText("     Please wait, power on.", 11, 0);
			turnPowerOn();
			closeMenu();
			return false;
		}
	}
    if(!gameInserted)flashLoadBlocked=true;
	return true;
}

void selectGame() {
	pauseEmulation = true;
	ui10();
	const char *gameName = browseForFileType(FILEEXTENSIONS".zip");
	if (loadGame(gameName)) {
		backOutOfMenu();
	}
}

void checkMachine() {
	char fileExt[8];
	u8 newMachine = gMachineSet;
	if (newMachine == HW_AUTO) {
		getFileExtension(fileExt, currentFilename);
		if (ngpHeader->mode != 0 || strstr(fileExt, ".ngc")) {
			newMachine = HW_NGPCOLOR;
		}
		else {
			newMachine = HW_NGPMONO;
		}
	}
	if (gMachine != newMachine) {
		gMachine = newMachine;
		if (gMachine == HW_NGPMONO) {
			gSOC = SOC_K1GE;
		}
		else {
			gSOC = SOC_K2GE;
		}
		resetCartridgePreservingFlash(true);
	}
}

//---------------------------------------------------------------------------------
void ejectCart() {
    if(!persistBeforeChange())return;
	gRomSize = 0x200000;
	memset(romSpacePtr, -1, gRomSize);
	gameInserted = false;
}

//---------------------------------------------------------------------------------
static int loadBIOS(void *dest,const char *path,const int size) {
    if(!path[0])return 0;
    unsigned char *buffer=malloc(size);if(!buffer)return 0;
    char name[FILENAME_MAX_LENGTH],ext[8];strlcpy(name,currentFilename,sizeof(name));
    getFileExtension(ext,path);bool ok=false;
    if(!strcmp(ext,".zip"))ok=loadROM(buffer,path,size)==size;
    else {
        FILE *f=fopen(path,"rb");
        if(f){ok=!fseek(f,0,SEEK_END)&&ftell(f)==size&&!fseek(f,0,SEEK_SET)&&fread(buffer,1,size,f)==(unsigned)size;
            if(fclose(f))ok=false;}
    }
    if(ok)memcpy(dest,buffer,size);
    free(buffer);strlcpy(currentFilename,name,sizeof(currentFilename));
    return ok;
}

int loadColorBIOS(void) {
	if (loadBIOS(biosSpaceColor, cfg.biosPathColor, sizeof(biosSpaceColor))) {
		g_BIOSBASE_COLOR = biosSpaceColor;
		return 1;
	}
	return 0;
}

int loadBnWBIOS(void) {
	if (loadBIOS(biosSpace, cfg.biosPath, sizeof(biosSpace))) {
		g_BIOSBASE_BNW = biosSpace;
		return 1;
	}
	return 0;
}

static bool selectBios(char *dest, const char *fileTypes) {
	const char *biosName = browseForFileType(fileTypes);

	if (biosName) {
		strlcpy(dest, currentDir, FILEPATH_MAX_LENGTH);
		if(strcmp(dest,"/"))strlcat(dest, "/", FILEPATH_MAX_LENGTH);
		strlcat(dest, biosName, FILEPATH_MAX_LENGTH);
		return true;
	}
	return false;
}

void selectColorBios() {
    pauseEmulation=true;char old[FILEPATH_MAX_LENGTH];strlcpy(old,cfg.biosPathColor,sizeof(old));
    if(selectBios(cfg.biosPathColor,".ngp.ngc.zip.bin")) {
        if(loadColorBIOS()){settingsChanged=true;resetCartridgePreservingFlash(true);}
        else {strlcpy(cfg.biosPathColor,old,sizeof(cfg.biosPathColor));infoOutput("BIOS read failed; previous BIOS kept.");}
    }
    cls(0);
}
void selectBnWBios() {
    char old[FILEPATH_MAX_LENGTH];strlcpy(old,cfg.biosPath,sizeof(old));
    if(selectBios(cfg.biosPath,".ngp.ngc.zip.bin")) {
        if(loadBnWBIOS()){settingsChanged=true;resetCartridgePreservingFlash(true);}
        else {strlcpy(cfg.biosPath,old,sizeof(cfg.biosPath));infoOutput("BIOS read failed; previous BIOS kept.");}
    }
    cls(0);
}
