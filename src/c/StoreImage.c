#include "globals.h"

extern char D_800118B4[]; /* "StoreImage" GPU debug tag */
extern void ValidateGpuRect(char *name, RECT *rect);

/* libgpu StoreImage: clamp/validate the transfer RECT, then dispatch a VRAM
   download through the GPU command table — the store-image method at +0x8,
   called with the table's image-transfer context (+0x1c), the rect, mode 8, and
   the destination pixel buffer. (0x8005fa8c, 100 bytes.) */
void StoreImage(RECT *rect, unsigned int *addr) {
  ValidateGpuRect(D_800118B4, rect);
  (*(void (**)(int, RECT *, int, unsigned int *))((char *)g_pGpuDispatchTable +
                                                  0x8))(
      *(int *)((char *)g_pGpuDispatchTable + 0x1C), rect, 8, addr);
}
