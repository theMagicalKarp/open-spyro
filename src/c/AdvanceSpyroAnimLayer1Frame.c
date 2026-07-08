#include "globals.h"

extern unsigned char D_80078A72;
extern unsigned char D_80078A73;
extern unsigned char D_80078A78;
extern unsigned char D_80078A79;
extern unsigned char g_abSpyroAnimLayer1SubstepBlock[]; /* array alias for
   the substep accumulator at 0x80078a7d, see config/symbol_addrs.txt --
   forces the held base register, matching the original's single `a1` reuse
   across the load + both stores. */

/* 0x800495d8 */
void AdvanceSpyroAnimLayer1Frame(int param_1) {
  unsigned char *sub_ptr = g_abSpyroAnimLayer1SubstepBlock;
  unsigned char frame;
  unsigned char anim;
  unsigned char newFrame;
  unsigned char len;

  *sub_ptr = *sub_ptr + param_1;
  if (*sub_ptr >= 0x10) {
    *sub_ptr = *sub_ptr - 0x10;

    frame = D_80078A79;
    anim = D_80078A73;
    newFrame = frame + 1;
    D_80078A72 = anim;
    D_80078A78 = frame;
    D_80078A79 = newFrame;

    len = g_abSpyroAnimDescTable[anim * 4 + 2];
    if ((unsigned char)newFrame >= len) {
      D_80078A79 = len - 1;
    }
  }
}
