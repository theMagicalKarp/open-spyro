#include "globals.h"

extern unsigned char D_800116A4[];

/* Debug dump of a GPU tpage word: decode the texture-page bitfield (tp page
   x/y, abr, colour mode) and hand the pieces to the installed debug-printf
   hook. (0x8005ec04, 96 bytes.) */
void func_8005EC04(int tpage) {
  unsigned int tp = tpage & 0xFFFF;
  ((void (*)())g_pfnGpuDebugPrintf)(D_800116A4, (tp >> 7) & 3, (tp >> 5) & 3,
                                    (tp << 6) & 0x7C0,
                                    ((tp << 4) & 0x100) + ((tp >> 2) & 0x200));
}
