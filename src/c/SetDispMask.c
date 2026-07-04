#include "globals.h"

extern char D_80011850[];                      /* GPU debug format string */
extern unsigned char g_abGpuDebugStateBlock[]; /* g_bGpuDebugLevel base */
extern void *memset();

/* libgpu SetDispMask: optionally trace the call (debug level >= 2), on
   mask 0 invalidate the PutDispEnv cache (base+0x6a, 0x14 bytes of 0xff),
   then forward GP1 display enable/disable (0x03000000/1) through the GPU
   dispatch table entry at +0x10. (0x8005f6c8, 156 bytes.) */
void SetDispMask(int mask) {
  unsigned int cmd;
  char *tbl;

  if (g_abGpuDebugStateBlock[0] >= 2) {
    ((void (*)(char *, int))g_pfnGpuDebugPrintf)(D_80011850, mask);
  }
  if (mask == 0) {
    memset(&g_abGpuDebugStateBlock[0x6A], -1, 0x14);
  }
  tbl = (char *)g_pGpuDispatchTable;
  cmd = 0x3000001;
  if (mask) {
    cmd = 0x3000000;
  }
  (*(void (**)(unsigned int))(tbl + 0x10))(cmd);
}
