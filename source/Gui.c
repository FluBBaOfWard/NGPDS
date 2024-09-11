#include <nds.h>

#include "Gui.h"
#include "Shared/EmuMenu.h"
#include "Shared/EmuSettings.h"
#include "Main.h"
#include "FileHandling.h"
#include "Cart.h"
#include "Gfx.h"
#include "io.h"
#include "cpu.h"
#include "bios.h"
#include "TLCS900H/Version.h"
#include "ARMZ80/Version.h"
#include "K2GE/Version.h"
#include "K2Audio/Version.h"

#define EMUVERSION "V0.5.7 2024-09-11"

#define ALLOW_SPEED_HACKS	(1<<17)

void hacksInit(void);

static void paletteChange(void);
static void bufferModeSet(void);
static void languageSet(void);
static void machineSet(void);
static void batteryChange(void);
static void subBatteryChange(void);
static void speedHackSet(void);
static void z80SpeedSet(void);

static void uiMachine(void);
static void uiDebug(void);
static void updateGameInfo(void);
static void checkBattery(void);


const MItem fnList0[] = {{"",uiDummy}};
const MItem fnList1[] = {
	{"Load Game",selectGame},
	{"Load State",loadState},
	{"Save State",saveState},
	{"Load Flash",loadNVRAM},
	{"Save Flash",saveNVRAM},
	{"Save Settings",saveSettings},
	{"Eject Game",ejectGame},
	{"Reset Console",resetConsole},
	{"Quit Emulator",ui9}};
const MItem fnList2[] = {
	{"Controller",ui4},
	{"Display",ui5},
	{"Machine",ui6},
	{"Settings",ui7},
	{"Debug",ui8}};
const MItem fnList4[] = {{"",autoBSet}, {"",autoASet}, {"",swapABSet}};
const MItem fnList5[] = {{"",gammaSet}, {"",paletteChange}, {"",bufferModeSet}};
const MItem fnList6[] = {{"",languageSet}, {"",machineSet}, {"",batteryChange}, {"",subBatteryChange}, {"",speedHackSet}, {"",z80SpeedSet}, {"",selectBnWBios}, {"",selectColorBios}};
const MItem fnList7[] = {{"",speedSet}, {"",autoStateSet}, {"",autoNVRAMSet}, {"",autoSettingsSet}, {"",autoPauseGameSet}, {"",powerSaveSet}, {"",screenSwapSet}, {"",sleepSet}};
const MItem fnList8[] = {{"",debugTextSet}, {"",fgrLayerSet}, {"",bgrLayerSet}, {"",sprLayerSet}, {"",stepFrame}};
const MItem fnList9[] = {{"Yes ",exitEmulator}, {"No ",backOutOfMenu}};

const Menu menu0 = MENU_M("", uiNullNormal, fnList0);
Menu menu1 = MENU_M("", uiAuto, fnList1);
const Menu menu2 = MENU_M("", uiAuto, fnList2);
const Menu menu3 = MENU_M("", uiAbout, fnList0);
const Menu menu4 = MENU_M("Controller Settings", uiController, fnList4);
const Menu menu5 = MENU_M("Display Settings", uiDisplay, fnList5);
const Menu menu6 = MENU_M("Machine Settings", uiMachine, fnList6);
const Menu menu7 = MENU_M("Settings", uiSettings, fnList7);
const Menu menu8 = MENU_M("Debug", uiDebug, fnList8);
const Menu menu9 = MENU_M("Quit Emulator?", uiAuto, fnList9);
const Menu menu10 = MENU_M("", uiDummy, fnList0);

const Menu *const menus[] = {&menu0, &menu1, &menu2, &menu3, &menu4, &menu5, &menu6, &menu7, &menu8, &menu9, &menu10 };

static int oldBattery;
u8 gGammaValue = 0;
u8 gZ80Speed = 0;
char gameInfoString[32];

const char *const autoTxt[]  = {"Off", "On", "With R"};
const char *const speedTxt[] = {"Normal", "200%", "Max", "50%"};
const char *const brighTxt[] = {"I", "II", "III", "IIII", "IIIII"};
const char *const sleepTxt[] = {"5min", "10min", "30min", "Off"};
const char *const ctrlTxt[]  = {"1P", "2P"};
const char *const dispTxt[]  = {"Unscaled", "Scaled"};
const char *const flickTxt[] = {"No Flicker", "Flicker"};

const char *const machTxt[]  = {"Auto", "NeoGeo Pocket", "NeoGeo Pocket Color"};
const char *const bordTxt[]  = {"Black", "Border Color", "None"};
const char *const palTxt[]   = {"Black & White", "Red", "Green", "Blue", "Classic"};
const char *const langTxt[]  = {"Japanese", "English"};
const char *const cpuSpeedTxt[]  = {"Full Speed", "Half Speed", "1/4 Speed", "1/8 Speed", "1/16 Speed"};

/// This is called at the start of the emulator
void setupGUI() {
	emuSettings = AUTOPAUSE_EMULATION | AUTOLOAD_NVRAM | ALLOW_SPEED_HACKS | AUTOSLEEP_OFF;
	keysSetRepeat(25, 4);	// Delay, repeat.
	menu1.itemCount = ARRSIZE(fnList1) - (enableExit?0:1);
	openMenu();
}

/// This is called when going from emu to ui.
void enterGUI() {
	if (updateSettingsFromNGP() && (emuSettings & AUTOSAVE_SETTINGS)) {
		saveSettings();
		settingsChanged = false;
	}
}

/// This is called going from ui to emu.
void exitGUI() {
}

void quickSelectGame(void) {
	openMenu();
	selectGame();
	closeMenu();
}

void uiNullNormal() {
	uiNullDefault();
	oldBattery = 0;
}

void uiAbout() {
	cls(1);
	updateGameInfo();
	drawTabs();
	drawMenuText("B:        NGP A button", 4, 0);
	drawMenuText("A:        NGP B button", 5, 0);
	drawMenuText("L:        NGP D button", 6, 0);
	drawMenuText("X/Start:  Option button", 7, 0);
	drawMenuText("Y/Select: Power button", 8, 0);
	drawMenuText("DPad:     Joystick", 9, 0);

	drawMenuText(gameInfoString, 11, 0);

	drawMenuText("NGPDS        " EMUVERSION, 19, 0);
	drawMenuText("ARMZ80       " ARMZ80VERSION, 20, 0);
	drawMenuText("ARMTLCS-900H " TLCS900VERSION, 21, 0);
	drawMenuText("ARMK2GE      " K2GEVERSION, 22, 0);
	drawMenuText("ARMK2Audio   " ARMK2AUDIOVERSION, 23, 0);
}

void uiController() {
	setupSubMenuText();
	drawSubItem("B Autofire:", autoTxt[autoB]);
	drawSubItem("A Autofire:", autoTxt[autoA]);
	drawSubItem("Swap A-B:  ", autoTxt[(joyCfg>>10)&1]);
}

void uiDisplay() {
	setupSubMenuText();
	drawSubItem("Gamma:", brighTxt[gGammaValue]);
	drawSubItem("B&W Palette:", palTxt[gPaletteBank]);
	drawSubItem("VRAM Double Buffer:", autoTxt[gBufferEnable]);
}

static void uiMachine() {
	setupSubMenuText();
	drawSubItem("Language:", langTxt[gLang]);
	drawSubItem("Machine:", machTxt[gMachineSet]);
	drawSubItem("Change Batteries", NULL);
	drawSubItem("Change Sub Battery", NULL);
	drawSubItem("Cpu Speed Hacks:", autoTxt[(emuSettings&ALLOW_SPEED_HACKS)>>17]);
	drawSubItem("Z80 Clock:", cpuSpeedTxt[gZ80Speed&7]);
	drawSubItem("Select BnW Bios", NULL);
	drawSubItem("Select Color Bios", NULL);
}

void uiSettings() {
	setupSubMenuText();
	drawSubItem("Speed:", speedTxt[(emuSettings>>6)&3]);
	drawSubItem("Autoload State:", autoTxt[(emuSettings>>2)&1]);
	drawSubItem("Autoload Flash RAM:", autoTxt[(emuSettings>>10)&1]);
	drawSubItem("Autosave Settings:", autoTxt[(emuSettings>>9)&1]);
	drawSubItem("Autopause Game:", autoTxt[emuSettings&1]);
	drawSubItem("Powersave 2nd Screen:", autoTxt[(emuSettings>>1)&1]);
	drawSubItem("Emulator on Bottom:", autoTxt[(emuSettings>>8)&1]);
	drawSubItem("Autosleep:", sleepTxt[(emuSettings>>4)&3]);
}

void uiDebug() {
	setupSubMenuText();
	drawSubItem("Debug Output:", autoTxt[gDebugSet&1]);
	drawSubItem("Disable Foreground:", autoTxt[gGfxMask&1]);
	drawSubItem("Disable Background:", autoTxt[(gGfxMask>>1)&1]);
	drawSubItem("Disable Sprites:", autoTxt[(gGfxMask>>4)&1]);
	drawSubItem("Step Frame", NULL);
}

void checkBattery() {
	if (oldBattery != batteryLevel) {
		oldBattery = batteryLevel;
		if (batteryLevel < 0x8400) {
			drawText("   Batteries low in the NGP!", 15, 0);
		}
		else {
			drawText("", 15, 0);
		}
	}
}

void nullUINormal(int key) {
	checkBattery();
	if (key & KEY_TOUCH) {
		openMenu();
	}
}

void nullUIDebug(int key) {
	checkBattery();
	if (key & KEY_TOUCH) {
		openMenu();
	}
}

void ejectGame() {
	ejectCart();
}

void resetConsole() {
	checkMachine();
	machineInit();
	loadCart(0);
}

void updateGameInfo() {
	char catalog[8];
	strlMerge(gameInfoString, "Game Name: ", ngpHeader->name, sizeof(gameInfoString));
	strlcat(gameInfoString, " #", sizeof(gameInfoString));
	short2HexStr(catalog, ngpHeader->catalog);
	strlcat(gameInfoString, catalog, sizeof(gameInfoString));
}
//---------------------------------------------------------------------------------
void debugIO(u16 port, u8 val, const char *message) {
	char debugString[32];

	debugString[0] = 0;
	strlcat(debugString, message, sizeof(debugString));
	short2HexStr(&debugString[strlen(debugString)], port);
	strlcat(debugString, " val:", sizeof(debugString));
	char2HexStr(&debugString[strlen(debugString)], val);
	debugOutput(debugString);
}
//---------------------------------------------------------------------------------
void debugIOUnimplR(u16 port) {
	debugIO(port, 0, "Unimpl R port:");
}
void debugIOUnimplW(u8 val, u16 port) {
	debugIO(port, val, "Unimpl W port:");
}
void debugDivideError() {
	debugOutput("Divide Error.");
}
void debugUndefinedInstruction() {
	debugOutput("Undefined Instruction.");
}
void debugCrashInstruction() {
	debugOutput("CPU Crash!");
}

//---------------------------------------------------------------------------------
/// Swap A & B buttons
void swapABSet() {
	joyCfg ^= 0x400;
}

/// Change gamma (brightness)
void gammaSet() {
	gGammaValue++;
	if (gGammaValue > 4) gGammaValue = 0;
	paletteInit(gGammaValue);
	paletteTxAll();					// Make new palette visible
	setupMenuPalette();
	settingsChanged = true;
}

/// Turn on/off rendering of foreground
void fgrLayerSet(){
	gGfxMask ^= 0x01;
}
/// Turn on/off rendering of background
void bgrLayerSet(){
	gGfxMask ^= 0x02;
}
/// Turn on/off rendering of sprites
void sprLayerSet(){
	gGfxMask ^= 0x10;
}

void paletteChange() {
	gPaletteBank++;
	if (gPaletteBank > 4) {
		gPaletteBank = 0;
	}
	monoPalInit();
	paletteTxAll();
	fixBiosSettings();
	settingsChanged = true;
}
void bufferModeSet() {
	gBufferEnable ^= 0x01;
	k2GE_0EnableBufferMode(gBufferEnable);
}
/*
void borderSet() {
	bcolor++;
	if (bcolor > 2) {
		bcolor = 0;
	}
	makeborder();
}
*/
void languageSet() {
	gLang ^= 0x01;
	fixBiosSettings();
}

void machineSet() {
	gMachineSet++;
	if (gMachineSet >= HW_SELECT_END) {
		gMachineSet = 0;
	}
}

void batteryChange() {
	batteryLevel = 0xFFFF;				// 0xFFFF for 2 days battery?
}

void subBatteryChange() {
	gSubBatteryLevel = 0x3FFFFFF;		// 0x3FFFFFF for 2 years battery?
}

void speedHackSet() {
	emuSettings ^= ALLOW_SPEED_HACKS;
	hacksInit();
}

void z80SpeedSet() {
	gZ80Speed++;
	if (gZ80Speed >= 5) {
		gZ80Speed = 0;
	}
	tweakZ80Speed(gZ80Speed);
}
