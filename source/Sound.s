#ifdef __arm__

#include "K2Audio/SN76496.i"

	.global soundInit
	.global soundReset
	.global VblSound2
	.global setMuteSoundGUI
	.global setMuteSoundChip
	.global T6W28_L_W
	.global T6W28_R_W
	.global k2Audio_0

	.extern pauseEmulation


;@----------------------------------------------------------------------------

	.syntax unified
	.arm

	.section .text
	.align 2
;@----------------------------------------------------------------------------
soundInit:
	.type soundInit STT_FUNC
;@----------------------------------------------------------------------------
	stmfd sp!,{lr}

	ldmfd sp!,{lr}
//	bx lr

;@----------------------------------------------------------------------------
soundReset:
;@----------------------------------------------------------------------------
	stmfd sp!,{lr}
	mov r0,#0
	ldr snptr,=k2Audio_0
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
setMuteSoundChip:
;@----------------------------------------------------------------------------
	strb r0,muteSoundChip
	bx lr
;@----------------------------------------------------------------------------
VblSound2:					;@ r0=length, r1=pointer
;@----------------------------------------------------------------------------
;@	mov r11,r11
	stmfd sp!,{r0,r1,lr}

	ldr r2,muteSound
	cmp r2,#0
	bne silenceMix

	ldr snptr,=k2Audio_0
	mov r0,r0,lsl#2
	bl sn76496Mixer

	ldmfd sp!,{r0,r1,lr}
	bx lr
	mov r12,r0
	ldr r3,pcmPtr0
wavLoop:
	ldrb r2,[r3],#1
	mov r2,r2,lsl#8
	strh r2,[r1],#2
	subs r12,r12,#1
	bhi wavLoop

//	ldmfd sp!,{lr}
	bx lr

silenceMix:
	ldmfd sp!,{r0,r1}
	mov r12,r0
	ldr r2,=0x80008000
silenceLoop:
	subs r12,r12,#1
	strpl r2,[r1],#4
	bhi silenceLoop

	ldmfd sp!,{lr}
	bx lr

;@----------------------------------------------------------------------------
T6W28_L_W:				;@ Sound left write
;@----------------------------------------------------------------------------
	stmfd sp!,{r3,snptr,lr}
	ldr snptr,=k2Audio_0
	bl sn76496L_W
	ldmfd sp!,{r3,snptr,lr}
	bx lr
;@----------------------------------------------------------------------------
T6W28_R_W:				;@ Sound right write
;@----------------------------------------------------------------------------
	stmfd sp!,{r3,snptr,lr}
	ldr snptr,=k2Audio_0
	bl sn76496W
	ldmfd sp!,{r3,snptr,lr}
	bx lr

;@----------------------------------------------------------------------------
pcmPtr0:	.long WAVBUFFER
pcmPtr1:	.long WAVBUFFER+528

muteSound:
muteSoundGUI:
	.byte 0
muteSoundChip:
	.byte 0
	.space 2

soundLatch:
	.byte 0
	.space 3

	.section .bss
	.align 2
k2Audio_0:
	.space snSize
WAVBUFFER:
	.space 0x1000
;@----------------------------------------------------------------------------
	.end
#endif // #ifdef __arm__
