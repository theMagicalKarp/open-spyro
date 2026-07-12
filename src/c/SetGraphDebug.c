#include "globals.h"

/* PSY-Q SetGraphDebug: set the libgpu debug verbosity level, returning the
   previous one. When the new level is non-zero, trace the level/type/reverse
   state through the debug printf hook. (0x8005f53c, 100 bytes.) */

extern unsigned char
    D_800117F4[]; /* "SetGraphDebug:level:%d,type:%d reverse:%d\n" */
extern unsigned char
    g_abGpuDebugLevelBlock[]; /* alias view of g_bGpuDebugLevel */

unsigned char SetGraphDebug(unsigned char param_1) {
  unsigned char *level;
  unsigned char prev;

  level = g_abGpuDebugLevelBlock;
  prev = level[0];
  level[0] = param_1;
  if (param_1 != 0) {
    ((void (*)())g_pfnGpuDebugPrintf)(D_800117F4, param_1, g_bGpuDebugType,
                                      g_bGpuReverseFlag);
  }
  return prev;
}
