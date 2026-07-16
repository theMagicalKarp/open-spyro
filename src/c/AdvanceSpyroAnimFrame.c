#include "globals.h"

/* array alias for the sub-frame accumulator at 0x80078a7c (see
   config/symbol_addrs.txt) -- forces the held base register, matching the
   original's single `a1` reuse across the load + both stores. */
extern unsigned char g_abSpyroSubFrameTimerBlock[];

/* 0x8003cb24: Adds delta to the sub-frame timer g_bSpyroSubFrameTimer. On
   overflow past 0xF, advances g_bSpyroAnimFrame by 1 within the current anim,
   wrapping back to the loop-start frame g_abSpyroAnimDescTable[anim*4] when it
   reaches the loop-end [anim*4+1]. Pure incremental step; does not change
   state (cf. StepSpyroAnimAndCommitTransition at 0x8003cbb8). */
void AdvanceSpyroAnimFrame(int delta) {
  unsigned char *timer = g_abSpyroSubFrameTimerBlock;
  unsigned char frame;
  unsigned char anim;
  unsigned char newFrame;
  unsigned char loopEnd;
  int idx;

  *timer = *timer + delta;
  if (*timer >= 0x10) {
    *timer = *timer - 0x10;

    frame = g_bSpyroAnimFrame;
    anim = g_bSpyroAnimCurrent;
    newFrame = frame + 1;
    g_bSpyroAnimPrev = anim;
    idx = anim * 4;
    g_bSpyroFramePrev = frame;
    g_bSpyroAnimFrame = newFrame;

    loopEnd = g_abSpyroAnimDescTable[idx + 1];
    if ((unsigned char)newFrame >= loopEnd) {
      g_bSpyroAnimFrame = g_abSpyroAnimDescTable[idx];
    }
  }
}
