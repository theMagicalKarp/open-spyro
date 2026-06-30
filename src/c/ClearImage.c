#include "globals.h"

extern char D_8001189C[]; /* "ClearImage" GPU debug tag */
extern void ValidateGpuRect(char *name, RECT *rect);

/* libgpu ClearImage: clamp/validate the RECT, then dispatch a VRAM fill through
   the GPU command table — the fill method at +0x8, called with the table's
   image context (+0xc), the rect, mode 8, and the packed 24-bit fill colour
   (b<<16 | g<<8 | r). (0x8005f8f8, 148 bytes.) */
void ClearImage(RECT *rect, unsigned char r, unsigned char g, unsigned char b) {
  ValidateGpuRect(D_8001189C, rect);
  (*(void (**)(int, RECT *, int, int))((char *)g_pGpuDispatchTable + 0x8))(
      *(int *)((char *)g_pGpuDispatchTable + 0xC), rect, 8,
      (b << 16) | (g << 8) | r);
}
