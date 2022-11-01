#ifdef __arm__

#include "TLCS900H/TLCS900H.i"
#include "ARMZ80/ARMZ80.i"

	.global empty_IO_R
	.global empty_IO_W
	.global empty_R
	.global empty_W
	.global rom_W
	.global t9StoreBX
	.global t9StoreWX
	.global t9StoreLX
	.global t9LoadBX
	.global t9LoadWX
	.global t9LoadLX
	.global t9StoreB
	.global t9StoreB_mem
	.global t9StoreW
	.global t9StoreW_mem
	.global t9StoreL
	.global t9StoreL_mem
	.global t9LoadB
	.global t9LoadB_mem
	.global t9LoadW
	.global t9LoadW_mem
	.global t9LoadL
	.global t9LoadL_mem
	.global tlcs_rom_R
	.global tlcs_romH_R
	.global z80RamW
	.global z80SoundW
	.global z80IrqW
	.global z80RamR


	.syntax unified
	.arm

	.section .text
	.align 2
;@----------------------------------------------------------------------------
empty_IO_R:					;@ Read bad IO address (error)
;@----------------------------------------------------------------------------
	mov r11,r11					;@ No$GBA breakpoint
	mov r0,#0x10
	bx lr
;@----------------------------------------------------------------------------
empty_IO_W:					;@ Write bad IO address (error)
;@----------------------------------------------------------------------------
	mov r11,r11					;@ No$GBA breakpoint
	mov r0,#0x18
	bx lr
;@----------------------------------------------------------------------------
empty_R:					;@ Read bad address (error)
;@----------------------------------------------------------------------------
	mov r11,r11					;@ No$GBA breakpoint
	mov r0,#0
	bx lr
;@----------------------------------------------------------------------------
empty_W:					;@ Write bad address (error)
;@----------------------------------------------------------------------------
	mov r11,r11					;@ No$GBA breakpoint
	mov r0,#0xBA
	bx lr
;@----------------------------------------------------------------------------
rom_W:						;@ Write ROM address (error)
;@----------------------------------------------------------------------------
	mov r11,r11					;@ No$GBA breakpoint
	mov r0,#0xB0
	bx lr
;@----------------------------------------------------------------------------
t9StoreBX:					;@ r0=value, r1=address
	.type	t9StoreBX STT_FUNC
;@----------------------------------------------------------------------------
	stmfd sp!,{t9optbl,lr}
	ldr t9optbl,=tlcs900HState
	bl t9StoreB
	ldmfd sp!,{t9optbl,lr}
	bx lr
;@----------------------------------------------------------------------------
t9StoreWX:					;@ r0=value, r1=address
	.type	t9StoreWX STT_FUNC
;@----------------------------------------------------------------------------
	stmfd sp!,{t9optbl,lr}
	ldr t9optbl,=tlcs900HState
	bl t9StoreW
	ldmfd sp!,{t9optbl,lr}
	bx lr
;@----------------------------------------------------------------------------
t9StoreLX:					;@ r0=value, r1=address
	.type	t9StoreLX STT_FUNC
;@----------------------------------------------------------------------------
	stmfd sp!,{t9optbl,lr}
	ldr t9optbl,=tlcs900HState
	bl t9StoreL
	ldmfd sp!,{t9optbl,lr}
	bx lr
;@----------------------------------------------------------------------------
t9LoadBX:					;@ r0=address
	.type	t9LoadBX STT_FUNC
;@----------------------------------------------------------------------------
	stmfd sp!,{t9optbl,lr}
	ldr t9optbl,=tlcs900HState
	bl t9LoadB
	ldmfd sp!,{t9optbl,lr}
	bx lr
;@----------------------------------------------------------------------------
t9LoadWX:					;@ r0=address
	.type	t9LoadWX STT_FUNC
;@----------------------------------------------------------------------------
	stmfd sp!,{t9optbl,lr}
	ldr t9optbl,=tlcs900HState
	bl t9LoadW
	ldmfd sp!,{t9optbl,lr}
	bx lr
;@----------------------------------------------------------------------------
t9LoadLX:					;@ r0=address
	.type	t9LoadLX STT_FUNC
;@----------------------------------------------------------------------------
	stmfd sp!,{t9optbl,lr}
	ldr t9optbl,=tlcs900HState
	bl t9LoadL
	ldmfd sp!,{t9optbl,lr}
	bx lr
;@----------------------------------------------------------------------------

#ifdef NDS
	.section .itcm						;@ For the NDS ARM9
#elif GBA
	.section .iwram, "ax", %progbits	;@ For the GBA
#endif
	.align 2
;@----------------------------------------------------------------------------
t9StoreB_32:
;@----------------------------------------------------------------------------
	bic r1,r1,#0xFF000000
	b t9StoreB
;@----------------------------------------------------------------------------
t9StoreB_mem:
;@----------------------------------------------------------------------------
	bic r1,t9Mem,#0xFF000000
;@----------------------------------------------------------------------------
t9StoreB:					;@ r0=value, r1=address
;@----------------------------------------------------------------------------
	movs r2,r1,lsr#8
	beq t9StoreB_Low
	movs r2,r1,lsr#14
	cmp r2,#1
	beq t9StoreB_ram
	cmp r2,#2
	beq t9StoreB_vram
	mov r2,r2,lsr#7
	cmp r2,#1
	beq FlashWriteLO
	cmp r2,#4
	beq FlashWriteHI
	b rom_W
;@----------------------------------------------------------------------------
t9StoreW_32:
;@----------------------------------------------------------------------------
	bic r1,r1,#0xFF000000
	b t9StoreW
;@----------------------------------------------------------------------------
t9StoreW_mem:
;@----------------------------------------------------------------------------
	bic r1,t9Mem,#0xFF000000
;@----------------------------------------------------------------------------
t9StoreW:					;@ r0=value, r1=address
;@----------------------------------------------------------------------------
	tst r1,#1
	bne t9StoreUnevenW
	mov r2,r1,lsr#14
	cmp r2,#1
	beq t9StoreW_ram
	cmp r2,#2
	beq t9StoreW_vram

;@----------------------------------------------------------------------------
t9StoreUnevenW:
;@----------------------------------------------------------------------------
	sub t9cycles,t9cycles,#1*T9CYCLE
;@----------------------------------------------------------------------------
t9StoreWB:
;@----------------------------------------------------------------------------
	stmfd sp!,{r0,r1,lr}
	bl t9StoreB
	ldmfd sp!,{r0,r1,lr}
	mov r0,r0,lsr#8
	add r1,r1,#1
	b t9StoreB
;@----------------------------------------------------------------------------
t9StoreL_32:
;@----------------------------------------------------------------------------
	bic r1,r1,#0xFF000000
	b t9StoreL
;@----------------------------------------------------------------------------
t9StoreL_mem:
;@----------------------------------------------------------------------------
	bic r1,t9Mem,#0xFF000000
;@----------------------------------------------------------------------------
t9StoreL:					;@ r0=value, r1=address
;@----------------------------------------------------------------------------
	stmfd sp!,{r0,r1,lr}
	bl t9StoreW
	ldmfd sp!,{r0,r1,lr}
	mov r0,r0,lsr#16
	add r1,r1,#2
	b t9StoreW
;@----------------------------------------------------------------------------
t9StoreW_ram:				;@ Write RAM (0x004000-0x007FFF)
;@----------------------------------------------------------------------------
	ldr r2,=ngpRAM
	mov r1,r1,lsl#18
	add r2,r2,r1,lsr#18
	strh r0,[r2]
	bx lr
;@----------------------------------------------------------------------------
t9StoreB_ram:				;@ Write RAM (0x004000-0x007FFF)
;@----------------------------------------------------------------------------
	ldr r2,=ngpRAM
	mov r1,r1,lsl#18
	strb r0,[r2,r1,lsr#18]
	bx lr
;@----------------------------------------------------------------------------
t9StoreW_vram:				;@ Write VRAM (0x009000-0x00BFFF)
;@----------------------------------------------------------------------------
	tst r1,#0x007000
	beq t9StoreWB
	ldr r2,=k2geRAM-0x1000
	mov r1,r1,lsl#17
	add r2,r2,r1,lsr#17
	strh r0,[r2]
	ldr r2,=DIRTYTILES-0x100
	strb r1,[r2,r1,lsr#21]		;@ Each dirty byte is 0x10 VRAM bytes
	bx lr
;@----------------------------------------------------------------------------
t9StoreB_vram:				;@ Write VRAM (0x009000-0x00BFFF)
;@----------------------------------------------------------------------------
	tst r1,#0x007000
	beq k2GE_0W
	ldr r2,=k2geRAM-0x1000
	mov r1,r1,lsl#17
	strb r0,[r2,r1,lsr#17]
	ldr r2,=DIRTYTILES-0x100
	strb r1,[r2,r1,lsr#21]		;@ Each dirty byte is 0x10 VRAM bytes
	bx lr
;@----------------------------------------------------------------------------
t9LoadB_32:
;@----------------------------------------------------------------------------
	bic r0,r0,#0xFF000000
	b t9LoadB
;@----------------------------------------------------------------------------
t9LoadB_mem:
;@----------------------------------------------------------------------------
	bic r0,t9Mem,#0xFF000000
;@----------------------------------------------------------------------------
t9LoadB:					;@ r0=address
;@----------------------------------------------------------------------------
	movs r2,r0,lsr#8
	beq t9LoadB_Low
	movs r2,r0,lsr#14
	cmp r2,#1
	beq tlcs_ram_R
	bmi ram_low_R
	cmp r2,#2
	beq tlcs_vram_R
	mov r2,r2,lsr#7
	cmp r2,#1
	ldreq pc,[t9optbl,#readRomPtrLo]
	cmp r2,#4
	ldreq pc,[t9optbl,#readRomPtrHi]
	cmp r2,#7
	beq tlcs_bios_R
ram_low_R:
	mov r11,r11
	mov r0,#0
	mov r1,#0
	bx lr
;@----------------------------------------------------------------------------
t9LoadW_32:
;@----------------------------------------------------------------------------
	bic r0,r0,#0xFF000000
	b t9LoadW
;@----------------------------------------------------------------------------
t9LoadW_mem:
;@----------------------------------------------------------------------------
	bic r0,t9Mem,#0xFF000000
;@----------------------------------------------------------------------------
t9LoadW:					;@ r0=address
;@----------------------------------------------------------------------------
	tst r0,#1
	bne t9LoadUnevenW
t9LoadEvenW:				;@ r0=address
	movs r2,r0,lsr#8
	beq t9LoadWAsB
	movs r2,r0,lsr#14
	cmp r2,#1
	beq tlcs_ram_W_R
	bmi ram_low_R
	cmp r2,#2
	beq tlcs_vram_W_R
	mov r2,r2,lsr#7
//	cmp r2,#1
//	beq tlcs_rom_W_R
//	cmp r2,#4
//	beq tlcs_romH_W_R
	cmp r2,#7
	beq tlcs_bios_W_R
	b t9LoadWAsB
;@----------------------------------------------------------------------------
t9LoadUnevenW:				;@ r0=address
;@----------------------------------------------------------------------------
	sub t9cycles,t9cycles,#1*T9CYCLE
;@----------------------------------------------------------------------------
t9LoadWAsB:					;@ r0=address
;@----------------------------------------------------------------------------
	stmfd sp!,{r4,lr}
	mov r3,r0
	bl t9LoadB
	mov r4,r0
	add r0,r3,#1
	bl t9LoadB
	orr r0,r4,r0,lsl#8
	ldmfd sp!,{r4,lr}
	bx lr
;@----------------------------------------------------------------------------
t9LoadL_32:
;@----------------------------------------------------------------------------
	bic r0,r0,#0xFF000000
	b t9LoadL
;@----------------------------------------------------------------------------
t9LoadL_mem:
;@----------------------------------------------------------------------------
	bic r0,t9Mem,#0xFF000000
;@----------------------------------------------------------------------------
t9LoadL:					;@ r0=address
;@----------------------------------------------------------------------------
	stmfd sp!,{r4,r5,lr}
	tst r0,#1
	bne t9LoadUnevenL
	mov r5,r0
	bl t9LoadEvenW
	mov r4,r0
	add r0,r5,#2
	bl t9LoadEvenW
	orr r0,r4,r0,lsl#16
	ldmfd sp!,{r4,r5,lr}
	bx lr
;@----------------------------------------------------------------------------
t9LoadUnevenL:				;@ r0=address
;@----------------------------------------------------------------------------
	sub t9cycles,t9cycles,#1*T9CYCLE
	mov r5,r0
	bl t9LoadB
	mov r4,r0
	add r0,r5,#1
	bl t9LoadEvenW
	orr r4,r4,r0,lsl#8
	add r0,r5,#3
	bl t9LoadB
	orr r0,r4,r0,lsl#24
	ldmfd sp!,{r4,r5,lr}
	bx lr
;@----------------------------------------------------------------------------
tlcs_ram_R:					;@ Read ram (0x004000-0x007FFF)
;@----------------------------------------------------------------------------
	ldr r1,=ngpRAM
	mov r0,r0,lsl#18
	ldrb r0,[r1,r0,lsr#18]!
	bx lr
;@----------------------------------------------------------------------------
tlcs_vram_R:				;@ Read vram (0x009000-0x00BFFF)
;@----------------------------------------------------------------------------
	tst r0,#0x007000
	beq k2GE_0R
	ldr r1,=k2geRAM-0x1000
	mov r0,r0,lsl#18
	ldrb r0,[r1,r0,lsr#18]!
	bx lr
;@----------------------------------------------------------------------------
tlcs_rom_R:					;@ Read rom (0x200000-0x3FFFFF)
;@----------------------------------------------------------------------------
	ldr r1,[t9optbl,#romBaseLo]
	mov r0,r0,lsl#11
	ldrb r0,[r1,r0,lsr#11]!
	bx lr
;@----------------------------------------------------------------------------
tlcs_romH_R:				;@ Read rom (0x800000-0x9FFFFF)
;@----------------------------------------------------------------------------
	ldr r1,[t9optbl,#romBaseHi]
	mov r0,r0,lsl#11
	ldrb r0,[r1,r0,lsr#11]!
	bx lr
;@----------------------------------------------------------------------------
tlcs_bios_R:				;@ Read bios (0xFF0000-0xFFFFFF)
;@----------------------------------------------------------------------------
	ldr r1,[t9optbl,#biosBase]
	mov r0,r0,lsl#16
	ldrb r0,[r1,r0,lsr#16]!
	bx lr

;@----------------------------------------------------------------------------
tlcs_ram_W_R:				;@ Read ram (0x004000-0x007FFF)
;@----------------------------------------------------------------------------
	ldr r1,=ngpRAM
	mov r0,r0,lsl#18
	add r1,r1,r0,lsr#18
	ldrh r0,[r1]
	bx lr
;@----------------------------------------------------------------------------
tlcs_vram_W_R:				;@ Read vram (0x009000-0x00BFFF)
;@----------------------------------------------------------------------------
	tst r0,#0x007000
	beq t9LoadWAsB
	ldr r1,=k2geRAM-0x1000
	mov r0,r0,lsl#18
	add r1,r1,r0,lsr#18
	ldrh r0,[r1]
	bx lr
;@----------------------------------------------------------------------------
tlcs_rom_W_R:				;@ Read rom (0x200000-0x3FFFFF)
;@----------------------------------------------------------------------------
	ldr r1,[t9optbl,#romBaseLo]
	mov r0,r0,lsl#11
	add r1,r1,r0,lsr#11
	ldrh r0,[r1]
	bx lr
;@----------------------------------------------------------------------------
tlcs_romH_W_R:				;@ Read rom (0x800000-0x9FFFFF)
;@----------------------------------------------------------------------------
	ldr r1,[t9optbl,#romBaseHi]
	mov r0,r0,lsl#11
	add r1,r1,r0,lsr#11
	ldrh r0,[r1]
	bx lr
;@----------------------------------------------------------------------------
tlcs_bios_W_R:				;@ Read bios (0xFF0000-0xFFFFFF)
;@----------------------------------------------------------------------------
	ldr r1,[t9optbl,#biosBase]
	mov r0,r0,lsl#16
	add r1,r1,r0,lsr#16
	ldrh r0,[r1]
	bx lr

;@----------------------------------------------------------------------------
z80RamW:					;@ Write mem (0x0000-0x0FFF)
;@----------------------------------------------------------------------------
	cmp addy,#0x1000
	ldrmi r1,=ngpRAM+0x3000
	strbmi r0,[r1,addy]
	bxmi lr
	b empty_W
;@----------------------------------------------------------------------------
z80RamR:					;@ Read Ram (0x0000-0x0FFF)
;@----------------------------------------------------------------------------
	cmp addy,#0x1000
	ldrmi r1,=ngpRAM+0x3000
	ldrbmi r0,[r1,addy]
	bxmi lr
	b empty_R
;@----------------------------------------------------------------------------
z80SoundW:					;@ Write mem (0x4000-0x4001)
;@----------------------------------------------------------------------------
	mov r1,#0x4000
	cmp addy,r1					;@ 0x4000, sound Right
	beq T6W28_R_W
	add r1,r1,#1				;@ 0x4001, sound Left
	cmp addy,r1
	beq T6W28_L_W
	b empty_W
;@----------------------------------------------------------------------------
z80IrqW:					;@ Write mem (0xC000)
;@----------------------------------------------------------------------------
	cmp addy,#0xC000
	bne empty_W
	stmfd sp!,{r3,lr}
		mov r0,#0x0C			;@ This must load up tlcs900 regs before executing.
		bl setInterruptExternal
	ldmfd sp!,{r3,lr}
	bx lr

;@----------------------------------------------------------------------------
	.end
#endif // #ifdef __arm__
