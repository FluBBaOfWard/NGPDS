#ifdef __arm__

#include "K2Audio/SN76496.i"

	.extern pauseEmulation

	.global k2Audio_0

	.global soundInit
	.global soundReset
	.global VblSound2
	.global setMuteSoundGUI
	.global setMuteT6W28
	.global soundUpdate
	.global T6W28_R_W
	.global T6W28_L_W
	.global T6W28_DAC_L_W
	.global T6W28_DAC_R_W

#define WAV_BUFFER_SIZE (0x800)
#define SHIFTVAL (21)

;@----------------------------------------------------------------------------

	.syntax unified
	.arm

	.section .text
	.align 2
;@----------------------------------------------------------------------------
soundInit:
	.type soundInit STT_FUNC
;@----------------------------------------------------------------------------
//	stmfd sp!,{lr}

//	ldmfd sp!,{lr}
//	bx lr

;@----------------------------------------------------------------------------
soundReset:
;@----------------------------------------------------------------------------
	stmfd sp!,{lr}
	ldr r0,=k2Audio_0
	bl sn76496Reset			;@ sound
	ldmfd sp!,{lr}
	bx lr

;@----------------------------------------------------------------------------
setMuteSoundGUI:
	.type   setMuteSoundGUI STT_FUNC
;@----------------------------------------------------------------------------
	ldr r1,=pauseEmulation		;@ Output silence when emulation paused.
	ldrb r0,[r1]
	strb r0,muteSoundGUI
	bx lr
;@----------------------------------------------------------------------------
setMuteT6W28:
;@----------------------------------------------------------------------------
	and r0,r0,#0xFF
	cmp r0,#0xAA
	cmpne r0,#0x55
	andeq r0,r0,#0xAA
	strbeq r0,muteSoundChip
	bx lr
;@----------------------------------------------------------------------------
VblSound2:					;@ r0=length, r1=pointer
;@----------------------------------------------------------------------------
;@	mov r11,r11
	ldr r2,muteSound
	tst r2,#0x00FF
	bne silenceMix
	tst r2,#0xFF00
	bne playSamples

	stmfd sp!,{r0,lr}
	ldr r2,=k2Audio_0
	bl sn76496Mixer
	ldmfd sp!,{r0,lr}
	bx lr

playSamples:
	stmfd sp!,{r0,lr}
	ldr r12,pcmWritePtr
	sub r12,r12,r0,lsr#2
	ldr r2,=WAVBUFFER
	mov r12,r12,lsl#SHIFTVAL
wavLoop:
	ldrb r3,[r2,r12,lsr#SHIFTVAL-1]
//	ldrb lr,[r2,r12,lsr#SHIFTVAL-1]
	eor r3,r3,#0x80
	mov r3,r3,lsl#8
	orr r3,r3,r3,lsl#16
	str r3,[r1],#4
	str r3,[r1],#4
	str r3,[r1],#4
	str r3,[r1],#4
	add r12,r12,#1<<SHIFTVAL
	subs r0,r0,#4
	bhi wavLoop

	ldmfd sp!,{r0,lr}
	bx lr

silenceMix:
	mov r12,r0
	ldr r2,=0x80008000
silenceLoop:
	subs r12,r12,#1
	strpl r2,[r1],#4
	bhi silenceLoop

	bx lr

;@----------------------------------------------------------------------------
soundUpdate:
;@----------------------------------------------------------------------------
	ldrb r0,muteSoundChip
	cmp r0,#0
	bxeq lr
	ldr r2,pcmWritePtr
	add r1,r2,#1
	str r1,pcmWritePtr
	ldr r1,=WAVBUFFER
	mov r2,r2,lsl#SHIFTVAL		;@ Only keep 11 bits
	add r1,r1,r2,lsr#SHIFTVAL-1
	ldrh r0,dacLeft
	strh r0,[r1]
	bx lr
;@----------------------------------------------------------------------------
T6W28_DAC_L_W:
;@----------------------------------------------------------------------------
	strb r0,dacLeft
	bx lr
;@----------------------------------------------------------------------------
T6W28_DAC_R_W:
;@----------------------------------------------------------------------------
	strb r0,dacRight
	bx lr
;@----------------------------------------------------------------------------
T6W28_R_W:				;@ Sound right write
;@----------------------------------------------------------------------------
	stmfd sp!,{r3,lr}
	ldr r1,=k2Audio_0
	bl sn76496W
	ldmfd sp!,{r3,lr}
	bx lr
;@----------------------------------------------------------------------------
T6W28_L_W:				;@ Sound left write
;@----------------------------------------------------------------------------
	stmfd sp!,{r3,lr}
	ldr r1,=k2Audio_0
	bl sn76496L_W
	ldmfd sp!,{r3,lr}
	bx lr

;@----------------------------------------------------------------------------
pcmWritePtr:	.long 0
pcmReadPtr:		.long 0
dacLeft:		.byte 0
dacRight:		.byte 0
	.space 2

muteSound:
muteSoundGUI:
	.byte 0
muteSoundChip:
	.byte 0
	.space 2

#ifdef GBA
	.section .sbss				;@ This is EWRAM on GBA with devkitARM
#else
	.section .bss
#endif
	.align 2
k2Audio_0:
	.space snSize
WAVBUFFER:
	.space WAV_BUFFER_SIZE*2
;@----------------------------------------------------------------------------
	.end
#endif // #ifdef __arm__
