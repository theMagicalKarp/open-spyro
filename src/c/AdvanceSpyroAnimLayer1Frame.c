#include "globals.h"
#include "main_shared.h"

/* 0x800495d8 */
void AdvanceSpyroAnimLayer1Frame(int step) {
  unsigned char *sub_ptr = g_abSpyroAnimLayer1SubstepBlock;
  unsigned char frame;
  unsigned char anim;
  unsigned char newFrame;
  unsigned char len;

  *sub_ptr = *sub_ptr + step;
  if (*sub_ptr >= 0x10) {
    *sub_ptr = *sub_ptr - 0x10;

    frame = g_bSpyroAnimLayer1Frame;
    anim = g_bSpyroAnimLayer1Current;
    newFrame = frame + 1;
    g_bSpyroAnimLayer1Prev = anim;
    g_bSpyroAnimLayer1FramePrev = frame;
    g_bSpyroAnimLayer1Frame = newFrame;

    len = g_abSpyroAnimDescTable[anim * 4 + 2];
    if ((unsigned char)newFrame >= len) {
      g_bSpyroAnimLayer1Frame = len - 1;
    }
  }
}
