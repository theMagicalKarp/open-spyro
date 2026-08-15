#include "globals.h"

/* Install (or clear) the DMA completion callback for one channel
   (0x8005e804): publish it in g_apfnDmaCallbacks[ch] and set/clear the
   channel's DICR enable bit, keeping the write-1-to-clear flag word intact.
   Returns the callback it replaced. */
/* MATCHED 2026-08-14.

   The last residue was the OR grouping in the enable arm: gcc 2.7.2's fold
   reassociates `(1<<n | C) | (x & M)` back into `((x & M) | C) | (1<<n)`, so
   neither written operand order survives as one expression. Giving the
   `(1<<n) | C` term its own block-scoped local (`bit`) pins the grouping —
   fold cannot reach across the assignment — and writing the store as
   `dicr | bit` (masked value on the LEFT) puts the dying masked temp in the
   final OR's destination, which is the original's `or v0,v0,v1` / `or a0,a0,v0`
   pair. `bit | dicr` is 2/42 (the same two ORs, operands swapped).

   Also load-bearing, from the earlier passes:
     * length — the RMW must be inlined through the global pointer in EACH arm
       with no shared temp; a shared `bit` temp across both arms lets cse hoist
       the shift above the branch and cross-jump the arms, which comes out 2
       insns SHORT (it deletes the original's two param copies).
*/
void *func_8005E804(int ch, void *cb) {
  int dicr;
  void *prev = g_apfnDmaCallbacks[ch];
  if (cb != prev) {
    if (cb != 0) {
      g_apfnDmaCallbacks[ch] = cb;
      dicr = (*((volatile unsigned int *)g_pDmaDicrReg)) & 0xFFFFFF;
      {
        int bit = (1 << (ch + 0x10)) | 0x800000;
        *((volatile unsigned int *)g_pDmaDicrReg) = dicr | bit;
      }
    } else {
      g_apfnDmaCallbacks[ch] = 0;
      *((volatile unsigned int *)g_pDmaDicrReg) =
          (((*((volatile unsigned int *)g_pDmaDicrReg)) & 0xFFFFFF) |
           0x800000) &
          (~(1 << (ch + 0x10)));
    }
  }
  return prev;
}
