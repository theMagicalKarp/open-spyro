#include "globals.h"

/* PSY-Q DrawOTag core: kick off a GPU linked-list (ordering-table) DMA starting
   at `addr` (0x80061780, 76 bytes). Arms GP1 DMA mode, points the GPU DMA
   channel at the list, then triggers the linked-list transfer. */
void SendGpuLinkedList(unsigned int addr) {
  *(volatile int *)g_pGpuStatReg = 0x4000002;
  *(volatile int *)g_pDmaGpuMadrReg = addr;
  *(volatile int *)g_pDmaGpuBcrReg = 0;
  *(volatile int *)g_pDmaGpuChcrReg = 0x1000401;
}
