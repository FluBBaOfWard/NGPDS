#ifdef __arm__

#include "TLCS900H/TLCS900H.i"
#include "ARMZ80/ARMZ80.i"
#include "K2GE/K2GE.i"
#include "Shared/EmuMenu.i"

	.global joyCfg
	.global EMUinput
	.global gSubBatteryLevel
	.global batteryLevel
	.global systemMemory

	.global ioReset
	.global convertInput
	.global refreshEMUjoypads
	.global transferTime
	.global Z80In
	.global Z80Out
	.global ioSaveState
	.global ioLoadState
	.global ioGetStateSize

	.global updateSlowIO
	.global z80LatchR
	.global z80LatchW
	.global system_comms_read
	.global system_comms_write
	.global ADStart

	.syntax unified
	.arm

	.section .text
	.align 2
;@----------------------------------------------------------------------------
ioReset:
;@----------------------------------------------------------------------------
	stmfd sp!,{lr}

	adr r0,SysMemDefault
	bl initSysMem
	bl transferTime

	ldmfd sp!,{pc}
;@----------------------------------------------------------------------------
initSysMem:					;@ In r0=values ptr.
;@----------------------------------------------------------------------------
	stmfd sp!,{t9Mem,r5,t9cycles,t9ptr,lr}
	ldr t9ptr,=tlcs900HState

	mov r5,r0
	mov t9Mem,#0xBF
initMemLoop:
	ldrb r0,[r5,t9Mem]
	bl t9StoreB_Low
	subs t9Mem,t9Mem,#1
	bpl initMemLoop

	ldmfd sp!,{t9Mem,r5,t9cycles,t9ptr,pc}
;@----------------------------------------------------------------------------
ioSaveState:				;@ In r0=destination. Out r0=size.
	.type   ioSaveState STT_FUNC
;@----------------------------------------------------------------------------
	mov r2,#0x100
	stmfd sp!,{r2,lr}

	ldr r1,=systemMemory
	bl memcpy

	ldmfd sp!,{r0,lr}
	bx lr
;@----------------------------------------------------------------------------
ioLoadState:				;@ In r0=source. Out r0=size.
	.type   ioLoadState STT_FUNC
;@----------------------------------------------------------------------------
	stmfd sp!,{lr}

	bl initSysMem

	ldmfd sp!,{lr}
;@----------------------------------------------------------------------------
ioGetStateSize:				;@ Out r0=state size.
	.type   ioGetStateSize STT_FUNC
;@----------------------------------------------------------------------------
	mov r0,#0x100
	bx lr
;@----------------------------------------------------------------------------
transferTime:
	.type transferTime STT_FUNC
;@----------------------------------------------------------------------------
	stmfd sp!,{lr}

	bl getTime					;@ r0 = ??ssMMHH, r1 = ??DDMMYY
	ldr r2,=systemMemory
	strb r1,[r2,#0x91]			;@ Year
	mov r1,r1,lsr#8
	strb r1,[r2,#0x92]			;@ Month
	mov r1,r1,lsr#8
	strb r1,[r2,#0x93]			;@ Day
	and r1,r0,#0x3F
	strb r1,[r2,#0x94]			;@ Hours
	mov r0,r0,lsr#8
	strb r0,[r2,#0x95]			;@ Minutes
	mov r0,r0,lsr#8
	strb r0,[r2,#0x96]			;@ Seconds

	ldmfd sp!,{lr}
	bx lr

;@----------------------------------------------------------------------------
SysMemDefault:
	;@ 0x00													;@ 0x08
	.byte 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x08, 0xFF, 0xFF
	;@ 0x10													;@ 0x18
	.byte 0x34, 0x3C, 0xFF, 0xFF, 0xFF, 0x3F, 0x00, 0x00,	0x3F, 0xFF, 0x2D, 0x01, 0xFF, 0xFF, 0x03, 0xB2
	;@ 0x20													;@ 0x28
	.byte 0x80, 0x00, 0x01, 0x90, 0x03, 0xCC, 0x90, 0x62,	0x05, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x4C, 0x4C
	;@ 0x30													;@ 0x38
	.byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	0x30, 0x00, 0x00, 0x00, 0x20, 0xFF, 0x80, 0x7F
	;@ 0x40													;@ 0x48
	.byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	;@ 0x50													;@ 0x58
	.byte 0x00, 0x20, 0x69, 0x15, 0x00, 0x00, 0x00, 0x00,	0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF
	;@ 0x60													;@ 0x68
	.byte 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	0x17, 0x17, 0x03, 0x03, 0x02, 0x00, 0x10, 0x4E
	;@ 0x70													;@ 0x78
	.byte 0x02, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	;@ 0x80													;@ 0x88
	.byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	;@ 0x90													;@ 0x98
	.byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	;@ 0xA0													;@ 0xA8
	.byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	;@ 0xB0													;@ 0xB8
	.byte 0x00, 0x00, 0x00, 0x04, 0x0A, 0x00, 0x00, 0x00,	0xAA, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	;@ 0xC0													;@ 0xC8
	.byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	;@ 0xD0													;@ 0xD8
	.byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	;@ 0xE0													;@ 0xE8
	.byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	;@ 0xF0													;@ 0xF8
	.byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
;@----------------------------------------------------------------------------
#ifdef GBA
	.section .ewram, "ax", %progbits	;@ For the GBA
	.align 2
#endif
;@----------------------------------------------------------------------------
convertInput:			;@ Convert from device keys to target r0=input/output
	.type convertInput STT_FUNC
;@----------------------------------------------------------------------------
	mvn r1,r0
	tst r1,#KEY_L|KEY_R				;@ Keys to open menu
	orreq r0,r0,#KEY_OPEN_MENU
	bx lr
;@----------------------------------------------------------------------------
refreshEMUjoypads:			;@ Call every frame
;@----------------------------------------------------------------------------

		ldr r4,=frameTotal
		ldr r4,[r4]
		movs r0,r4,lsr#2		;@ C=frame&2 (autofire alternates every other frame)
	ldr r4,EMUinput
	mov r3,r4
	and r0,r4,#0xf0
		ldr r2,joyCfg
		andcs r3,r3,r2
		tstcs r3,r3,lsr#10		;@ NDS L?
		andcs r3,r3,r2,lsr#16
	adr r1,rlud2durl
	ldrb r0,[r1,r0,lsr#4]


	ands r1,r3,#3				;@ A/B buttons
	cmpne r1,#3
	eorne r1,r1,#3
	tst r2,#0x400				;@ Swap A/B?
	andne r1,r3,#3
	orr r0,r0,r1,lsl#4

	tst r4,#0x08				;@ NDS Start
	tsteq r4,#0x400				;@ NDS X
	orrne r0,r0,#0x40			;@ NGP Option
	tst r4,#0x200				;@ NDS L
	orrne r0,r0,#0x80			;@ NGP D

	strb r0,systemMemory+0xB0	;@ HW joypad

	mov r0,#0xFF
	tst r4,#0x04				;@ NDS Select
	tsteq r4,#0x800				;@ NDS Y
	bicne r0,r0,#0x01			;@ NGP Power
	ldr r1,=gSubBatteryLevel
	ldr r1,[r1]
	tst r1,#0x2000000			;@ Highest bit of subbattery level
	biceq r0,r0,#0x02
	strb r0,systemMemory+0xB1	;@ HW powerbutton + subbattery

	bx lr

joyCfg: .long 0x00ff01ff		;@ byte0=auto mask, byte1=(saves R), byte2=R auto mask
								;@ bit 31=single/multi, 30,29=1P/2P, 27=(multi) link active, 24=reset signal received
playerCount:.long 0				;@ Number of players in multilink.
			.byte 0
			.byte 0
			.byte 0
			.byte 0
rlud2durl:	.byte 0x00,0x08,0x04,0x0C, 0x01,0x09,0x05,0x0D, 0x02,0x0A,0x06,0x0E, 0x03,0x0B,0x07,0x0F

EMUinput:						;@ This label here for main.c to use
	.long 0						;@ EMUjoypad (this is what Emu sees)

;@----------------------------------------------------------------------------
z80LatchW:					;@ Write communication latch (0x8000)
;@----------------------------------------------------------------------------
	cmp addy,#0x8000
	strbeq r0,systemMemory+0xBC	;@ Z80 communication byte
	bxeq lr
	b empty_W
;@----------------------------------------------------------------------------
z80LatchR:					;@ Read communication latch (0x8000)
;@----------------------------------------------------------------------------
	cmp addy,#0x8000
	bne empty_R
	stmfd sp!,{lr}
	mov r0,#0
	bl Z80SetNMIPinCurrentCpu
	ldmfd sp!,{lr}
	ldrb r0,systemMemory+0xBC	;@ Z80 communication byte
	bx lr

;@----------------------------------------------------------------------------
system_comms_read:			;@ r0 = (u8 *buffer)
	.type system_comms_read STT_FUNC
;@----------------------------------------------------------------------------
	mov r0,#0
	bx lr
;@----------------------------------------------------------------------------
system_comms_write:			;@ r0 = (u8 data)
	.type system_comms_write STT_FUNC
;@----------------------------------------------------------------------------
	mov r0,#0
	bx lr
;@----------------------------------------------------------------------------
updateSlowIO:				;@ Call once every frame, updates rtc and battery levels.
;@----------------------------------------------------------------------------
	ldrb r0,rtcTimer
	subs r0,r0,#1
	movmi r0,#59
	strb r0,rtcTimer
	bxpl lr

	stmfd sp!,{r12,lr}
	blx getBatteryLevel			;@ Get NDS battery level.
	ldmfd sp!,{r12,lr}
	ldrb r1,lastBattery
	strb r0,lastBattery
	ldr r2,batteryLevel
	eor r1,r1,r0
	tst r1,#0xF
	beq notLowBatt
	ands r0,r0,#0xC
	bne notLowBatt
	cmp r2,#0x8400
	movcs r2,#0x8400
notLowBatt:
	subs r2,r2,#1
	movmi r2,#1
	str r2,batteryLevel

	ldr r1,=gSubBatteryLevel
	ldr r0,[r1]
	subs r0,r0,#0x00000100
	movmi r0,#0x00001000
	str r0,[r1]

	adr r2,systemMemory
	ldrb r0,[r2,#0x90]			;@ RTC control
	tst r0,#1					;@ Enabled?
	bxeq lr

	ldrb r0,[r2,#0x96]			;@ Seconds
	add r0,r0,#0x01
	and r1,r0,#0x0F
	cmp r1,#0x0A
	addpl r0,r0,#0x06
	cmp r0,#0x60
	movpl r0,#0
	strb r0,[r2,#0x96]			;@ Seconds
	bmi checkForAlarm

	ldrb r0,[r2,#0x95]			;@ Minutes
	add r0,r0,#0x01
	and r1,r0,#0x0F
	cmp r1,#0x0A
	addpl r0,r0,#0x06
	cmp r0,#0x60
	movpl r0,#0
	strb r0,[r2,#0x95]			;@ Minutes
	bmi checkForAlarm

	ldrb r0,[r2,#0x94]			;@ Hours
	add r0,r0,#0x01
	and r1,r0,#0x0F
	cmp r1,#0x0A
	addpl r0,r0,#0x06
	cmp r0,#0x24
	movpl r0,#0
	strb r0,[r2,#0x94]			;@ Hours
	bmi checkForAlarm

	ldrb r0,[r2,#0x93]			;@ Days
	add r0,r0,#0x01
	and r1,r0,#0x0F
	cmp r1,#0x0A
	addpl r0,r0,#0x06
	cmp r0,#0x32
	movpl r0,#0
	strb r0,[r2,#0x93]			;@ Days
	bmi checkForAlarm

	ldrb r0,[r2,#0x92]			;@ Months
	add r0,r0,#0x01
	and r1,r0,#0x0F
	cmp r1,#0x0A
	addpl r0,r0,#0x06
	cmp r0,#0x13
	movpl r0,#1
	strb r0,[r2,#0x92]			;@ Months

checkForAlarm:
	ldrb r0,[r2,#0x96]			;@ Seconds
	cmp r0,#0x00
	ldrbeq r0,[r2,#0x95]		;@ RTC Minutes
	ldrbeq r1,[r2,#0x9A]		;@ ALARM Minutes
	cmpeq r0,r1
	ldrbeq r0,[r2,#0x94]		;@ RTC Hours
	ldrbeq r1,[r2,#0x99]		;@ ALARM Hours
	cmpeq r0,r1
	ldrbeq r0,[r2,#0x93]		;@ RTC Days
	ldrbeq r1,[r2,#0x98]		;@ ALARM Days
	cmpeq r0,r1
	moveq r0,#0x0A
	beq setInterrupt

	bx lr

;@----------------------------------------------------------------------------
ADStart:
;@----------------------------------------------------------------------------
	tst r0,#0x04
	bxeq lr
	ldr r0,batteryLevel
	orr r0,r0,#0x3F				;@ bit 0=ready, bit 1-5=1.
	strh r0,systemMemory+0x60
	mov r0,#0x1C
	b setInterrupt

;@----------------------------------------------------------------------------
Z80In:
;@----------------------------------------------------------------------------
	mov r11,r11					;@ No$GBA breakpoint
	mov r0,#0
	bx lr
;@----------------------------------------------------------------------------
Z80Out:
;@----------------------------------------------------------------------------
;@	mov r11,r11					;@ No$GBA breakpoint
	mov r0,#0
	b Z80SetIRQPinCurrentCpu
;@----------------------------------------------------------------------------
gSubBatteryLevel:
	.long 0x3000000				;@ subBatteryLevel
batteryLevel:
	.long 0xFFFF				;@ Max = 0xFFFF (0x3FF)
								;@ To start > 0x8400 (0x210)
								;@ Low < 0x8000 (0x200)
								;@ Bad < 0x7880 (0x1E2)
								;@ Shutdown <= 0x74C0 (0x1D3)
								;@ Alarm minimum = 0x5B80 (0x16E)
;@----------------------------------------------------------------------------
systemMemory:
	.space 0x100

lastBattery:
	.byte 0
rtcTimer:
	.byte 0
sc0Buf:
	.byte 0
commStatus:
	.byte 0

;@----------------------------------------------------------------------------
	.end
#endif // #ifdef __arm__
