#include "globals.h"

/* 0x80060bc8 (0xcc) — libgpu draw-area top-left builder: clamp (x, y) to the
   current screen mode and pack a GP0(0xE3) command. Old/new GPU builds
   (g_bGpuDebugType 1 or 2) use 12-bit fields, otherwise 10-bit.

   K&R `short` parameters: the clamp compares the sign-extended values but
   the unclamped path keeps the raw argument register, and the promoted
   parameters leave the leaf an unused 16-byte frame, both as in the
   original. The nested ternary gives the original's branch layout. */
unsigned int BuildDrawAreaTopLeft(x, y)
short x, y;
{
  x = x < 0 ? 0 : (x > g_nGpuScreenWidth - 1 ? g_nGpuScreenWidth - 1 : x);
  y = y < 0 ? 0 : (y > g_nGpuScreenHeight - 1 ? g_nGpuScreenHeight - 1 : y);
  if (g_bGpuDebugType == 1 || g_bGpuDebugType == 2) {
    return 0xE3000000 | ((y & 0xFFF) << 12) | (x & 0xFFF);
  }
  return 0xE3000000 | ((y & 0x3FF) << 10) | (x & 0x3FF);
}
