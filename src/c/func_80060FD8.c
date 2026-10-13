#include "globals.h"

extern unsigned int GetGpuInfo(unsigned int index);
extern void SendGpuLinkedList(void *packet);

/* The clear-image packet buffer (tag word + up to 12 GP0 words). */
extern unsigned int D_80075978[];

#define GPU_STAT (*(volatile unsigned int *)g_pGpuStatReg)

/* 0x80060fd8 (0x25c) — libgpu `_clr`, the body of ClearImage: clamp the
   rectangle to the screen mode, then fill it with `color`. A 64-pixel-aligned
   rectangle uses the GP0 VRAM fill (0x02); anything else is drawn as a flat
   rectangle (0x60) inside a temporary full-screen draw area, which is then
   restored from the current GPU state. */
int func_80060FD8(RECT *rect, unsigned int color) {
  rect->w =
      rect->w < 0
          ? 0
          : (rect->w > g_nGpuScreenWidth - 1 ? g_nGpuScreenWidth - 1 : rect->w);
  rect->h = rect->h < 0
                ? 0
                : (rect->h > g_nGpuScreenHeight - 1 ? g_nGpuScreenHeight - 1
                                                    : rect->h);
  if ((rect->x & 0x3F) || (rect->w & 0x3F)) {
    unsigned int *term = &D_80075978[9];

    D_80075978[0] = ((unsigned int)term & 0xFFFFFF) | 0x08000000;
    D_80075978[1] = 0xE3000000;
    D_80075978[2] = 0xE4FFFFFF;
    D_80075978[3] = 0xE5000000;
    D_80075978[4] = 0xE6000000;
    D_80075978[5] = 0xE1000000 | (GPU_STAT & 0x7FF) | ((color >> 31) << 10);
    D_80075978[6] = (color & 0xFFFFFF) | 0x60000000;
    D_80075978[7] = *(unsigned int *)&rect->x;
    D_80075978[8] = *(unsigned int *)&rect->w;
    *term = 0x3FFFFFF;
    D_80075978[10] = GetGpuInfo(3) | 0xE3000000;
    D_80075978[11] = GetGpuInfo(4) | 0xE4000000;
    D_80075978[12] = GetGpuInfo(5) | 0xE5000000;
  } else {
    D_80075978[0] = 0x5FFFFFF;
    D_80075978[1] = 0xE6000000;
    D_80075978[2] = 0xE1000000 | (GPU_STAT & 0x7FF) | ((color >> 31) << 10);
    D_80075978[3] = (color & 0xFFFFFF) | 0x02000000;
    D_80075978[4] = *(unsigned int *)&rect->x;
    D_80075978[5] = *(unsigned int *)&rect->w;
  }
  SendGpuLinkedList(D_80075978);
  return 0;
}
