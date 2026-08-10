#include "globals.h"

extern char D_800117DC[]; /* GPU debug tag */
extern unsigned char g_abGpuReverseFlagBlock[];

/* Set the GPU display reverse flag (0x8005f428): optionally trace, publish the
   flag, then re-issue the display mode through the +0x28/+0x10 command-table
   methods with the reverse bit folded in, and (in debug type 2) push the
   matching debug word. Returns the previous flag. */
unsigned char func_8005F428(int reverse) {
  unsigned char *pflag = g_abGpuReverseFlagBlock;
  unsigned char prev = *pflag;
  int mode;

  if (g_bGpuDebugLevel >= 2) {
    ((void (*)(char *, int))g_pfnGpuDebugPrintf)(D_800117DC, reverse);
  }
  *pflag = reverse;
  mode = (*(int (**)(int))((char *)g_pGpuDispatchTable + 0x28))(8);
  if (*pflag != 0) {
    mode |= 0x8000080;
  } else {
    mode |= 0x8000000;
  }
  (*(void (**)(int))((char *)g_pGpuDispatchTable + 0x10))(mode);
  if (g_bGpuDebugType == 2) {
    (*(void (**)(int))((char *)g_pGpuDispatchTable + 0x10))(
        g_bGpuReverseFlag != 0 ? 0x20000501 : 0x20000504);
  }
  return prev;
}
