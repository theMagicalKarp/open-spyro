#include "globals.h"

extern unsigned char D_80011724[];
extern unsigned char D_80011740[];
extern unsigned char D_8001175C[];
extern unsigned char D_80011768[];

/* Debug dump of a DISPENV: print the disp and screen rects, then the isinter /
   isrgb24 flags, via the installed debug-printf hook. (0x8005f1f8, 168 bytes.)
 */
void func_8005F1F8(DISPENV *e) {
  ((void (*)())g_pfnGpuDebugPrintf)(D_80011724, e->disp.x, e->disp.y, e->disp.w,
                                    e->disp.h);
  ((void (*)())g_pfnGpuDebugPrintf)(D_80011740, e->screen.x, e->screen.y,
                                    e->screen.w, e->screen.h);
  ((void (*)())g_pfnGpuDebugPrintf)(D_8001175C, e->isinter);
  ((void (*)())g_pfnGpuDebugPrintf)(D_80011768, e->isrgb24);
}
