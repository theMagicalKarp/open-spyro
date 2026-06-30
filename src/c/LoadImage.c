#include "globals.h"

extern char D_800118A8[]; /* "LoadImage" GPU debug tag */
extern void ValidateGpuRect(char *name, RECT *rect);

/* libgpu LoadImage: clamp/validate the transfer RECT, then dispatch a VRAM
   upload through the GPU command table — the load-image method at +0x8, called
   with the table's image-transfer context (+0x20), the rect, mode 8, and the
   source pixel buffer. (0x8005fa28, 100 bytes.) */
void LoadImage(RECT *rect, unsigned int *addr) {
  ValidateGpuRect(D_800118A8, rect);
  (*(void (**)(int, RECT *, int, unsigned int *))((char *)g_pGpuDispatchTable +
                                                  0x8))(
      *(int *)((char *)g_pGpuDispatchTable + 0x20), rect, 8, addr);
}
