#include "globals.h"

/* Packs a GP0(0xE5) "set drawing offset" command word from (x, y). Old/new GPU
   builds (g_bGpuDebugType 1 or 2) use 12-bit fields; otherwise 11-bit.
   (0x80060d60, 68 bytes.) */
unsigned int BuildDrawOffsetCommand(int x, int y) {
  if (g_bGpuDebugType == 1 || g_bGpuDebugType == 2) {
    return 0xE5000000 | ((y & 0xFFF) << 12) | (x & 0xFFF);
  }
  return 0xE5000000 | ((y & 0x7FF) << 11) | (x & 0x7FF);
}
