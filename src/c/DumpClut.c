#include "globals.h"

extern char D_800116BC[];

/* libgpu DumpClut: print the CLUT's VRAM (x,y) origin via the GPU debug printf
   hook — x = (clut & 0x3f) << 4, y = (clut & 0xffff) >> 6 (0x8005ec64, 64
   bytes). */
void DumpClut(int clut) {
  (*(void (**)(char *, int, int))&g_pfnGpuDebugPrintf)(
      D_800116BC, (clut & 0x3F) << 4, (clut & 0xFFFF) >> 6);
}
