#include "globals.h"
#include "main_shared.h"

extern unsigned char g_bSpyroAnimLayer2Substep[]; /* layer-2 substep accumulator
   — incomplete array so the base address is materialized once and held (single
   `a1` across the load + both stores), matching the original. */

/* 0x80049dfc — layer-2 sibling of AdvanceSpyroAnimLayer1Frame: accumulate
   sub-frame steps; every 0x10 substeps advance the layer-2 anim frame,
   looping back to the anim's start frame at its loop point. */
void AdvanceSpyroAnimLayer2Frame(int step) {
  unsigned char *sub_ptr = g_bSpyroAnimLayer2Substep;
  int sum;
  int off;
  unsigned char frame;
  unsigned char anim;
  unsigned char newFrame;

  sum = *sub_ptr + step;
  *sub_ptr = sum;
  if ((unsigned char)sum >= 0x10) {
    *sub_ptr = sum - 0x10;

    frame = g_bSpyroAnimLayer2Frame;
    anim = g_bSpyroAnimLayer2Current;
    newFrame = frame + 1;
    g_bSpyroAnimLayer2Prev = anim;
    off = anim * 4;
    g_bSpyroAnimLayer2FramePrev = frame;
    g_bSpyroAnimLayer2Frame = newFrame;

    if (newFrame == g_abSpyroAnimDescTable[off + 1]) {
      g_bSpyroAnimLayer2Frame = g_abSpyroAnimDescTable[off];
    }
  }
}
