
#ifndef __BIOS__
#define __BIOS__

#include "NGPHeader.h"

extern uint8 *ngpc_bios;

void iBIOSHLE(int);

/** Fill the bios rom area with a HLE BIOS. Call once at program start */
bool installHleBios(uint8 *ngpBios);

void BIOSHLE_Reset(void);

void resetBios(NgpHeader *ngpHeader);

void fixBiosSettings(void);

#endif
