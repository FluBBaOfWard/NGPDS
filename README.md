# NGPDS V0.5.7

<img align="right" width="220" src="./logo.png" />

This is a SNK Neogeo Pocket (Color) emulator for the Nintendo DS(i)/3DS.

## How to use

1. Create a folder named "ngpds" in either the root of your flash card or in
 the data folder. This is where settings and save files end up.
2. Now put game/bios files into a folder where you have (NGP) roms, max
 768 games per folder, filenames must not be longer than 127 chars. You can use
 zip-files (as long as they use the deflate compression). CAUTION! Games that
 require SLOT-2 RAM can not be used with zip-files!
3. Depending on your flashcart you might have to DLDI patch the emulator.

Note! You need a bios to be able to save in game.
The save file should be compatible with most other NeoGeo Pocket emulators.

When the emulator starts, you can either press L+R or tap on the screen to open
up the menu.
Now you can use the cross or touchscreen to navigate the menus, A or double tap
to select an option, B or the top of the screen to go back a step.

To select between the tabs use R & L or the touchscreen.

Since the DS/DS Lite only has 4MB of RAM you will need a SLOT-2/GBA cart with
 RAM on these devices to play games larger than 2MB.

## Menu

### File

* Load Game: Select a game to load.
* Load State: Load a previously saved state of the currently running game.
* Save State: Save a state of the currently running game.
* Load Flash: Load flash ram for the currently running game.
* Save Flash: Save flash ram for the currently running game.
* Save Settings: Save the current settings.
* Eject Game: Remove the game, can be used to enter bios settings.
* Reset Console: Reset the console.
* Quit Emulator: (If supported.)

### Options

* Controller:
  * B Autofire: Select if you want autofire on button B.
  * A Autofire: Select if you want autofire on button A.
  * Swap A-B: Swap which NDS button is mapped to which NGP button.
* Display:
  * Gamma: Lets you change the gamma ("brightness").
  * B&W Palette: Here you can select the palette for B & W games.
* Machine:
  * Language: Select between Japanese and English.
  * Machine: Select the emulated machine.
  * Change Batteries: Change to new main batteries (AA/LR6).
  * Change Sub Battery: Change to a new sub battery (CR2032).
  * Cpu Speed Hacks: Allow speed hacks.
  * Z80 Clock: You can underclock to get better speed on the DS.
  * Bios Settings: Load a real NGP Bios, recommended.
* Settings:
  * Speed: Switch between speed modes.
    * Normal: Game runs at it's normal speed.
    * 200%: Game runs at double speed.
    * Max: Games can run up to 4 times normal speed (might change).
    * 50%: Game runs at half speed.
  * Autoload State: Toggle Savestate autoloading. Automagically load the savestate associated with the current game.
  * Autoload Flash RAM: Toggle flash/save ram autoloading. Automagically load the flash ram associated with the current game.
  * Autosave Settings: This will save settings when leaving menu if any changes are made.
  * Autopause Game: Toggle if the game should pause when opening the menu.
  * Powersave 2nd Screen: If graphics/light should be turned off for the GUI screen when menu is not active.
  * Emulator on Bottom: Select if top or bottom screen should be used for emulator, when menu is active emulator screen is allways on top.
  * Autosleep: Doesn't work.
* Debug:
  * Debug Output: Show FPS and logged text.
  * Disable Foreground: Turn on/off foreground rendering.
  * Disable Background: Turn on/off background rendering.
  * Disable Sprites: Turn on/off sprite rendering.
  * Step Frame: Emulate one frame.

### About

Some dumb info about the game and emulator...

## Controls

* NDS A & B buttons are mapped to NeoGeo Pocket B & A.
* NDS Start & X is mapped to NeoGeo Pocket Option.
* NDS Select & Y is mapped to NeoGeo Pocket Power.
* NDS d-pad is mapped to NeoGeo Pocket d-pad.
* NDS L button is mapped to NeoGeo Pocket D (debug).

## Games

* Memories of - Pure: Glitches everywhere.
* Neo Poke Pro Yakyuu: Sprite multiplexing doesn't work.
* Sonic The Hedgehog: First boss sometimes disappear, try to use a save state soon before the boss and reload and try again.
* Super Real Mahjong - Premium Collection: Graphic bugs on intro. Sprite multiplexing.

## Credits

```text
Huge thanks to Loopy for the incredible PocketNES, without it this emu would
probably never have been made.
Thanks to:
Flavor & Koyote for NGP info.
Dwedit for help and inspiration with a lot of things.
```

Fredrik Ahlström

X/Twitter @TheRealFluBBa

https://www.github.com/FluBBaOfWard
