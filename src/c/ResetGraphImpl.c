#include "globals.h"

/* libgpu ResetGraph implementation (dispatched from the public ResetGraph
   thunk). mode = param&7: 0/2 = full reset (DMA-2 CHCR=0x401, enable GP DMA in
   DPCR, GP1(0) display reset, zero the GP1 status shadow and the 64-entry DMA
   op ring); 1/3/5 = cancel-only (CHCR + DPCR then GP1 soft-reset 0x02/0x01).
   Bracketed by SetInterruptMask save/restore; mode==0 also reinits the display
   env via InitDisplayEnv. (0x80061dec, 348 bytes.) */

extern int SetInterruptMask(int mask);
extern void memset(unsigned char *dst, int val, int n);
extern unsigned int InitDisplayEnv(unsigned int param);
extern void *D_80074B54; /* DMA primary-control (DPCR) register pointer */

unsigned int ResetGraphImpl(unsigned int param_1) {
  int mode;

  g_nResetGraphSavedIMask = SetInterruptMask(0);
  g_nGpuQueueReadIndex = 0;
  g_nGpuQueueWriteIndex = *(volatile int *)&g_nGpuQueueReadIndex;
  mode = param_1 & 7;
  if (mode == 1) {
    goto cancel_only;
  }
  if (mode < 2) {
    if (mode != 0) {
      goto done_reset;
    }
  } else {
    if (mode == 3) {
      goto cancel_only;
    }
    if (mode != 5) {
      goto done_reset;
    }
  }
  /* full reset (mode 0 or 5) */
  *(volatile unsigned int *)g_pDmaGpuChcrReg = 0x401;
  *(volatile unsigned int *)D_80074B54 |= 0x800;
  *(volatile unsigned int *)g_pGpuStatReg = 0;
  memset(g_abGpuStatusShadow, 0, 0x100);
  memset(g_abGpuOpQueueRing, 0, 0x1800);
  goto done_reset;

cancel_only:
  *(volatile unsigned int *)g_pDmaGpuChcrReg = 0x401;
  *(volatile unsigned int *)D_80074B54 |= 0x800;
  *(volatile unsigned int *)g_pGpuStatReg = 0x2000000;
  *(volatile unsigned int *)g_pGpuStatReg = 0x1000000;

done_reset:
  SetInterruptMask(g_nResetGraphSavedIMask);
  if ((param_1 & 7) != 0) {
    return 0;
  }
  return InitDisplayEnv(param_1);
}
