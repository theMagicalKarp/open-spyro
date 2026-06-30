#include "globals.h"

extern unsigned char D_800116A4[];
extern unsigned char D_800116CC[];
extern unsigned char D_800116E4[];
extern unsigned char D_800116F4[];
extern unsigned char D_8001170C[];
extern unsigned char D_80011718[];

/* Debug dump of a DRAWENV: clip rect, draw offset, texture window, dtd/dfe
   flags, and the decoded tpage bitfield, via the debug-printf hook.
   (0x8005f0e4, 276 bytes.) */
void func_8005F0E4(DRAWENV *e) {
  unsigned int tp;
  ((void (*)())g_pfnGpuDebugPrintf)(D_800116CC, e->clip.x, e->clip.y, e->clip.w,
                                    e->clip.h);
  ((void (*)())g_pfnGpuDebugPrintf)(D_800116E4, e->ofs[0], e->ofs[1]);
  ((void (*)())g_pfnGpuDebugPrintf)(D_800116F4, e->tw.x, e->tw.y, e->tw.w,
                                    e->tw.h);
  ((void (*)())g_pfnGpuDebugPrintf)(D_8001170C, e->dtd);
  ((void (*)())g_pfnGpuDebugPrintf)(D_80011718, e->dfe);
  tp = e->tpage;
  ((void (*)())g_pfnGpuDebugPrintf)(D_800116A4, (tp >> 7) & 3, (tp >> 5) & 3,
                                    (tp << 6) & 0x7C0,
                                    ((tp << 4) & 0x100) + ((tp >> 2) & 0x200));
}
