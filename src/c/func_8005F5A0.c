#include "globals.h"

extern char D_80011820[]; /* GPU debug tag */
extern void DmaCallback();
extern unsigned char g_abGpuQueueModeBlock[];

/* Set the GPU command-queue mode (0x8005f5a0): optionally trace, and when the
   mode actually changes, flush via the +0x34 command-table method, store the
   new mode, and re-arm the DMA callback. Returns the previous mode. */
unsigned char func_8005F5A0(int mode) {
  unsigned char *pmode = g_abGpuQueueModeBlock;
  unsigned char prev = *pmode;
  if (g_bGpuDebugLevel >= 2) {
    ((void (*)(char *, int))g_pfnGpuDebugPrintf)(D_80011820, mode);
  }
  if (mode != *pmode) {
    (*(void (**)(int))((char *)g_pGpuDispatchTable + 0x34))(1);
    *pmode = mode;
    DmaCallback(2, 0);
  }
  return prev;
}
