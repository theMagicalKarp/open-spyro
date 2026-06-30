#include "globals.h"

extern char D_80011864[]; /* GPU debug format string */

/* libgpu DrawSync: optionally trace the call (debug level >= 2) then forward to
   the GPU dispatch table's sync entry at +0x3c, returning its result.
   (0x8005f764, 108 bytes.) */
int DrawSync(int mode) {
  if (g_bGpuDebugLevel >= 2) {
    ((void (*)(char *, int))g_pfnGpuDebugPrintf)(D_80011864, mode);
  }
  return (*(int (**)(int))((char *)g_pGpuDispatchTable + 0x3c))(mode);
}
