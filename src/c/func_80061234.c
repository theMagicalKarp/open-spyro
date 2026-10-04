#include "globals.h"

extern void ResetGpuTimeoutDeadline(void);
extern int CheckGpuTimeout(void);

#define GPU_STAT (*(volatile unsigned int *)g_pGpuStatReg)
#define GPU_DATA (*(volatile unsigned int *)g_pGpuDataReg)

/* 0x80061234 (0x23c) — libgpu `_dws`, the body of LoadImage: clamp the
   rectangle size to the screen mode, then upload `p` to VRAM -- the remainder
   words by PIO through GP0 (0xa0 copy command), the 16-word blocks by GPU DMA.
   Returns -1 on an empty rectangle or a GPU timeout. `raw` selects the 0xb0
   command and is always 0 here (it survives as a callee-saved register because
   the wait loop ends cse's block).

   The clamps are the get_cs-style nested ternaries; the remainder is read
   with `%` before the block count with `/`, which is the order that leaves
   the shared quotient in s0 and the count in s4 as in the original. */
int func_80061234(RECT *rect, unsigned int *p) {
  int size;
  int n;
  int raw;
  int blocks;

  ResetGpuTimeoutDeadline();
  raw = 0;
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
  GPU_DATA = raw ? 0xb0000000 : 0xa0000000;
  GPU_DATA = *(unsigned int *)&rect->x;
  GPU_DATA = *(unsigned int *)&rect->w;
  while (n-- != 0) {
    GPU_DATA = *p++;
  }
  if (blocks != 0) {
    GPU_STAT = 0x04000002;
    *(volatile unsigned int *)g_pDmaGpuMadrReg = (unsigned int)p;
    *(volatile unsigned int *)g_pDmaGpuBcrReg = (blocks << 16) | 0x10;
    *(volatile unsigned int *)g_pDmaGpuChcrReg = 0x01000201;
  }
  return 0;
}
