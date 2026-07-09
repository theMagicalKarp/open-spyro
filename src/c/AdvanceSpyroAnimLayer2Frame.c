#include "globals.h"

extern unsigned char D_80078A74;
extern unsigned char D_80078A75;
extern unsigned char D_80078A7A;
extern unsigned char D_80078A7B;
extern unsigned char D_80078A7E[]; /* layer-2 substep accumulator — incomplete
   array so the base address is materialized once and held (single `a1`
   across the load + both stores), matching the original. */

/* 0x80049dfc — layer-2 sibling of AdvanceSpyroAnimLayer1Frame: accumulate
   sub-frame steps; every 0x10 substeps advance the layer-2 anim frame,
   looping back to the anim's start frame at its loop point. */
void AdvanceSpyroAnimLayer2Frame(int param_1) {
  unsigned char *sub_ptr = D_80078A7E;
  int sum;
  int off;
  unsigned char frame;
  unsigned char anim;
  unsigned char newFrame;

  sum = *sub_ptr + param_1;
  *sub_ptr = sum;
  if ((unsigned char)sum >= 0x10) {
    *sub_ptr = sum - 0x10;

    frame = D_80078A7B;
    anim = D_80078A75;
    newFrame = frame + 1;
    D_80078A74 = anim;
    off = anim * 4;
    D_80078A7A = frame;
    D_80078A7B = newFrame;

    if (newFrame ==
        *(unsigned char *)((char *)g_abSpyroAnimDescTable + off + 1)) {
      D_80078A7B = *(unsigned char *)((char *)g_abSpyroAnimDescTable + off);
    }
  }
}
