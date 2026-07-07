#include "globals.h"

extern void DelaySpuRegisterWrite(void);
extern void SetSpuDmaReadStateBits(void);

extern char *D_80073554;
extern volatile int *D_80073558;
extern volatile int *D_8007355C;
extern volatile int *D_80073560;

/* Start an SPU->CPU DMA read transfer (0x8005c114, subcmd 0 of the SPU DMA
   controller): stamp the encoded transfer address into the SPU addr register
   (+0x1a6), set the SPUCNT DMA-read mode bits (+0x1aa |= 0x30), flag the
   pending-read direction, and kick the DMA channel via the MADR/BCR/CHCR
   shadows. */
void func_8005C114(int madr, short encodedAddr, int chunks) {
  *(volatile short *)(D_80073554 + 0x1A6) = encodedAddr;
  DelaySpuRegisterWrite();
  *(volatile unsigned short *)(D_80073554 + 0x1AA) |= 0x30;
  DelaySpuRegisterWrite();
  SetSpuDmaReadStateBits();
  *D_80073558 = madr;
  *D_8007355C = (chunks << 16) | 0x10;
  g_nSpuPendingDmaDirection = 1;
  *D_80073560 = 0x01000200;
}
