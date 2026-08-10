#include "globals.h"

extern char D_8001189C[]; /* "ClearImage" GPU debug tag */
extern void ValidateGpuRect(char *name, RECT *rect);

/* libgpu ClearImage sibling that sets the semi-transparency bit (0x8005f98c,
   156 bytes): same clamp/validate + VRAM fill dispatch as ClearImage, but the
   packed fill colour carries 0x80000000 so the blit writes the mask bit. */
void func_8005F98C(RECT *rect, unsigned char r, unsigned char g,
                   unsigned char b) {
  ValidateGpuRect(D_8001189C, rect);
  {
    int hi = b << 16;
    int mid = (g << 8) | 0x80000000;

    (*(void (**)(int, RECT *, int, int))((char *)g_pGpuDispatchTable + 0x8))(
        *(int *)((char *)g_pGpuDispatchTable + 0xC), rect, 8, (hi | mid) | r);
  }
}
