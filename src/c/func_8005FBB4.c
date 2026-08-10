#include "globals.h"

extern char D_800118CC[];
extern char D_80074B20[];

/* 0x8005fbb4: links a run of `count` 4-byte GPU packet tags into an OT chain
   (word = (tag_byte << 24) | next_addr), terminating the chain at the fixed
   address D_80074B20. Debug-traces via g_pfnGpuDebugPrintf when
   g_bGpuDebugLevel >= 2. Returns the last linked packet.

   The parameters are used directly (no `p = arg0;` local copies): the copies
   home a1 before a0 and reverse the prologue's save/copy pairs. `mask` is a
   loop-invariant local, not a literal — it fixes the order in which the two
   merge constants are materialized (a1 before a2). */
unsigned int *func_8005FBB4(unsigned int *p, int n) {
  int mask;

  if (((unsigned char)g_bGpuDebugLevel) >= 2) {
    ((void (*)(char *, unsigned int *, int))g_pfnGpuDebugPrintf)(D_800118CC, p,
                                                                 n);
  }

  n = n - 1;
  if (n != 0) {
    do {
      unsigned int next;
      unsigned int old;

      n -= 1;
      next = ((unsigned int)p) + 4;
      *(((unsigned char *)p) + 3) = 0;
      old = *p;
      mask = 0xFFFFFF;
      *p = (old & 0xFF000000) | (next & mask);
      p = (unsigned int *)next;
    } while (n != 0);
  }

  *p = ((unsigned int)D_80074B20) & 0xFFFFFF;
  return p;
}
