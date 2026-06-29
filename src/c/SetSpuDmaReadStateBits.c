#include "globals.h"

extern int *D_80073568;

/* Set the SPU DMA channel control word to "read" mode: clear the transfer-mode
   nibble (0xF0FFFFFF mask) and select 0x22______ (0x8005c6f4, 44 bytes). */
void SetSpuDmaReadStateBits(void) {
  *(volatile int *)D_80073568 =
      (*(volatile int *)D_80073568 & 0xF0FFFFFF) | 0x22000000;
}
