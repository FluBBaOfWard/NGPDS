#ifdef __arm__

#include "TLCS900H/TLCS900H.i"
#include "ARMZ80/ARMZ80.i"
#include "K2GE/K2GE.i"

	.extern reset				;@ from bios.c

	.global gRomSize
	.global maxRomSize
	.global allocatedRomMemSize
	.global emuFlags
	.global gConfig
	.global gMachineSet
	.global gMachine
	.global gSOC
	.global gLang
	.global gPaletteBank

	.global ngpRAM
	.global biosSpace
	.global biosSpaceColor
	.global romSpacePtr
	.global allocatedRomMem
	.global ngpHeader
	.global g_BIOSBASE_COLOR
	.global g_BIOSBASE_BNW
	.global cartFlags
	.global romStart
	.global isBiosLoaded

	.global machineInit
	.global loadCart
	.global tlcs9000MemInit


	.syntax unified
	.arm

	.section .rodata
	.align 2

ROM_Space:
//	.incbin "ngproms/Bust-A-Move Pocket (U).ngc"
//	.incbin "ngproms/Cool Boarders Pocket (JE) (M2).ngc"
//	.incbin "ngproms/Dark Arms - Beast Buster 1999 (JUE) (M2).ngc"
//	.incbin "ngproms/Evolution - Eternal Dungeons (E).ngc"
//	.incbin "ngproms/Fantastic Night Dreams Cotton (E).ngc"
//	.incbin "ngproms/Fatal Fury F-Contact (JUE) (M2).ngc"
//	.incbin "ngproms/King of Fighters R-2 (JUE) (M2).ngc"
//	.incbin "ngproms/Last Blade, The - Beyond the Destiny (E).ngc"
//	.incbin "ngproms/Metal Slug - 1st Mission (JUE) (M2).ngc"
//	.incbin "ngproms/SNK Gals' Fighters (UE).ngc"
//	.incbin "ngproms/Sonic the Hedgehog - Pocket Adventure (JUE).ngc"
//biosSpace:
//	.incbin "ngproms/[BIOS] SNK Neo Geo Pocket (J).ngp"
//	.incbin "ngproms/[BIOS] SNK Neo Geo Pocket Color (JE).ngp"

	.align 2
;@----------------------------------------------------------------------------
machineInit: 				;@ Called from C
	.type   machineInit STT_FUNC
;@----------------------------------------------------------------------------
	stmfd sp!,{r4,lr}

	ldr r1,=romSpacePtr
//	ldr r0,=ROM_Space
//	str r0,[r1]
	ldr r0,[r1]

	bl tlcs9000MemInit

	ldr r4,=gMachine
	ldrb r4,[r4]
	cmp r4,#HW_NGPMONO
	ldreq r0,=biosSpace
	ldrne r0,=biosSpaceColor
	ldr t9ptr,=tlcs900HState
	sub r0,r0,#0xFF0000
	str r0,[t9ptr,#biosBase]

	bl gfxInit
//	bl ioInit
	bl soundInit
	bl z80MemInit
//	bl cpuInit

	bl gfxReset
	bl ioReset
	bl soundReset
	bl cpuReset

	cmp r4,#HW_NGPMONO
	ldreq r0,=g_BIOSBASE_BNW
	ldrne r0,=g_BIOSBASE_COLOR
	ldr r0,[r0]
	cmp r0,#0
	beq skipBiosSettings

	bl run						;@ Settings are cleared when new batteries are inserted.
	bl transferTime				;@ So set up time
	ldr r1,=fixBiosSettings		;@ And Bios settings after the first run.
	blx r1
skipBiosSettings:
	ldmfd sp!,{r4,lr}
	bx lr

	.section .ewram,"ax"
	.align 2
;@----------------------------------------------------------------------------
loadCart: 					;@ Called from C:  r0=emuflags
	.type   loadCart STT_FUNC
;@----------------------------------------------------------------------------
	stmfd sp!,{r4-r11,lr}
	str r0,emuFlags

	ldr r0,gRomSize
	ldr r1,romSpacePtr
	bl ngpFlashReset
	bl hacksInit

	ldr r4,=gMachine
	ldrb r4,[r4]
	cmp r4,#HW_NGPMONO
	ldreq r0,g_BIOSBASE_BNW
	ldrne r0,g_BIOSBASE_COLOR
	cmp r0,#0
	bne skipHWSetup

	bl gfxReset
	bl ioReset
	bl soundReset
	bl cpuReset
	ldr r0,ngpHeader			;@ First argument
	ldr r1,=resetHleBios
	blx r1
skipHWSetup:
	ldmfd sp!,{r4-r11,lr}
	bx lr

;@----------------------------------------------------------------------------
tlcs9000MemInit: 			;@ Called from C:  r0=rombase address
	.type   tlcs9000MemInit STT_FUNC
;@----------------------------------------------------------------------------
	ldr r1,=tlcs900HState
	sub r2,r0,#0x200000			;@ First bank is @ 0x200000
	str r2,[r1,#romBaseLo]
	add r0,r0,#0x200000-0x800000	;@ Second Bank @ 0x800000
	str r0,[r1,#romBaseHi]
	bx lr
;@----------------------------------------------------------------------------
z80MemInit:
;@----------------------------------------------------------------------------
	stmfd sp!,{z80ptr}
	ldr z80ptr,=Z80OpTable
	add r0,z80ptr,#z80ReadTbl
	ldr r1,=empty_R
	ldr r2,=empty_W
	mov r3,#8
z80MemLoop0:
	str r2,[r0,#32]				;@ z80WriteTbl
	str r1,[r0],#4
	subs r3,r3,#1
	bne z80MemLoop0

	add r0,z80ptr,#z80ReadTbl
	ldr r1,=z80RamR
	str r1,[r0]					;@ 0x0000-0x1FFF
	ldr r1,=z80LatchR
	str r1,[r0,#16]				;@ 0x8000-0x9FFF

	add r0,z80ptr,#z80WriteTbl
	ldr r1,=z80RamW
	str r1,[r0]					;@ 0x0000-0x1FFF
	ldr r1,=z80SoundW
	str r1,[r0,#8]				;@ 0x4000-0x5FFF
	ldr r1,=z80LatchW
	str r1,[r0,#16]				;@ 0x8000-0x9FFF
	ldr r1,=z80IrqW
	str r1,[r0,#24]				;@ 0xC000-0xDFFF

	ldr r0,=ngpRAM+0x3000		;@ Shared Z80/TLCS-900H RAM.
	add r1,z80ptr,#z80MemTbl
	mov r2,#8
z80MemLoop1:
	str r0,[r1],#4				;@ z80MemTbl
	subs r2,r2,#1
	bne z80MemLoop1

	ldmfd sp!,{z80ptr}
	bx lr


;@----------------------------------------------------------------------------

romInfo:						;@
emuFlags:
	.byte 0						;@ emuflags      (label this so GUI.c can take a peek) see EmuSettings.h for bitfields
//scaling:
	.byte 0						;@ (display type)
	.byte 0,0					;@ (sprite follow val)
cartFlags:
	.byte 0 					;@ cartflags
gConfig:
	.byte 0						;@ Config, bit 7=BIOS on/off
gMachineSet:
	.byte HW_AUTO
gMachine:
	.byte HW_NGPCOLOR
gSOC:
	.byte SOC_K2GE
gLang:
	.byte 1						;@ language
gPaletteBank:
	.byte 0						;@ palettebank
isBiosLoaded:
	.byte 0
//	.space 1					;@ alignment.

allocatedRomMem:
	.long 0
allocatedRomMemSize:
	.long 0
ngpHeader:
romSpacePtr:
	.long 0
g_BIOSBASE_COLOR:
	.long 0
g_BIOSBASE_BNW:
	.long 0
gRomSize:
romSize:
	.long 0
maxRomSize:
	.long 0

	.section .bss
ngpRAM:
	.space 0x4000
biosSpace:
	.space 0x10000
biosSpaceColor:
	.space 0x10000
;@----------------------------------------------------------------------------
	.end
#endif // #ifdef __arm__
