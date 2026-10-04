#include "globals.h"

extern void ResetGpuTimeoutDeadline(void);
extern int CheckGpuTimeout(void);

#define GPU_STAT (*(volatile unsigned int *)g_pGpuStatReg)
#define GPU_DATA (*(volatile unsigned int *)g_pGpuDataReg)

/* 0x80061470 (0x284) — libgpu `_drs`, the body of StoreImage: the read-back
   twin of func_80061234 (0xc0 copy command, waits for GPU ready-to-send before
   draining the remainder words, DMA direction from the GPU).

   The clamps are the get_cs-style nested ternaries; the remainder is read
   with `%` before the block count with `/`, which is the order that leaves
   the shared quotient in s0 and the count in s4 as in the original. */
int func_80061470(RECT *rect, unsigned int *p) {
  int size;
  int n;
  int blocks;

  ResetGpuTimeoutDeadline();
  rect->w = rect->w < 0
                ? 0
                : (rect->w > g_nGpuScreenWidth ? g_nGpuScreenWidth : rect->w);
  rect->h = rect->h < 0
                ? 0
                : (rect->h > g_nGpuScreenHeight ? g_nGpuScreenHeight : rect->h);
  size = (rect->w * rect->h + 1) / 2;
  if (size <= 0) {
    return -1;
  }
  n = size % 16;
  blocks = size / 16;
  while (!(GPU_STAT & 0x04000000)) {
    if (CheckGpuTimeout()) {
      return -1;
    }
  }
  GPU_STAT = 0x04000000;
  GPU_DATA = 0x01000000;
  GPU_DATA = 0xc0000000;
  GPU_DATA = *(unsigned int *)&rect->x;
  GPU_DATA = *(unsigned int *)&rect->w;
  while (!(GPU_STAT & 0x08000000)) {
    if (CheckGpuTimeout()) {
      return -1;
    }
  }
  while (n-- != 0) {
    *p++ = GPU_DATA;
  }
  if (blocks != 0) {
    GPU_STAT = 0x04000003;
    *(volatile unsigned int *)g_pDmaGpuMadrReg = (unsigned int)p;
    *(volatile unsigned int *)g_pDmaGpuBcrReg = (blocks << 16) | 0x10;
    *(volatile unsigned int *)g_pDmaGpuChcrReg = 0x01000200;
  }
  return 0;
}
