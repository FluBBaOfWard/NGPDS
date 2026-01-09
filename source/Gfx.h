#ifndef GFX_HEADER
#define GFX_HEADER

#ifdef __cplusplus
extern "C" {
#endif

#include "K2GE/K2GE.h"

extern u8 gFlicker;
extern u8 gTwitch;
extern u8 gScaling;
extern u8 gGfxMask;
extern u8 gBufferEnable;

extern K2GE k2GE_0;
extern u16 EMUPALBUFF[0x200];
extern u32 GFX_DISPCNT;
extern u16 GFX_BG0CNT;
extern u16 GFX_BG1CNT;

void gfxInit(void);
void vblIrqHandler(void);
void monoPalInit(int palBank);
void paletteInit(u8 gammaVal);
void paletteTxAll(void);
void refreshGfx(void);

/**
 * Enables/disables buffered VRAM mode.
 * @param  enable: Enable buffered VRAM mode.
 */
void k2GE_0EnableBufferMode(bool enable);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // GFX_HEADER
