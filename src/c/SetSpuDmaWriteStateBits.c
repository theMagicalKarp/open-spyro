#include "globals.h"

extern int *D_80073568;

/* Set the SPU DMA channel control word to "write" mode: clear the transfer-mode
   nibble (0xF0FFFFFF mask) and select 0x2_______ (0x8005c6c8, 44 bytes). */
void SetSpuDmaWriteStateBits(void) {
  *(volatile int *)D_80073568 =
      (*(volatile int *)D_80073568 & 0xF0FFFFFF) | 0x20000000;
}
