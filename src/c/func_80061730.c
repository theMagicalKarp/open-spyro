#include "globals.h"

/* Puts the GPU DMA controller in CPU-write mode (GP1 0x04 = DMA off) then feeds
   `count` command words from `list` straight to the GP0 data port.
   (0x80061730, 80 bytes.) */
int func_80061730(unsigned int *list, int count) {
  *(volatile int *)g_pGpuStatReg = 0x4000000;
  while (count--) {
    *(volatile int *)g_pGpuDataReg = *list++;
  }
  return 0;
}
