#include "funcs.h"
#include "globals.h"

extern unsigned char g_abSpyroAnimLayer1SubstepBlock;

/* 0x80049660 — per-frame layer-1 (flame-breath overlay) anim driver: phase 0
   tracks the flame-breath state (snapshot the primary anim bytes when idle,
   advance the layer frame otherwise, or start a blend/hard-switch phase from
   the transition table on a state change); phases 1/2 run the blend timer and
   drop back to phase 0 when it expires. */
void TickSpyroAnimLayer1(void) {
  int state;
  int mode;
  int timer;
  unsigned char prev;
  unsigned char framePrev;
  unsigned char cur;
  unsigned char frame;
  unsigned char sub;
  unsigned char oldAnim;
  unsigned char oldFrame;
  unsigned char newAnim;
  unsigned char blendAnim;
  unsigned char blendFrame;
  unsigned char blendCur;
  unsigned char blendCurFrame;

  switch (g_anSpyroFlamePhaseBlock[0]) {
  case 0:
    state = g_nSpyroFlameState;
    if (state == g_anSpyroFlamePhaseBlock[2]) {
      if (state == 0) {
        /* volatile accesses pin the original's load-all-then-store-all
           schedule (the -g3 scheduler otherwise pairs each store with
           its load). */
        prev = *(volatile byte *)&g_bSpyroAnimPrev;
        framePrev = *(volatile byte *)&g_bSpyroFramePrev;
        cur = *(volatile byte *)&g_bSpyroAnimCurrent;
        frame = *(volatile byte *)&g_bSpyroAnimFrame;
        sub = *(volatile byte *)&g_bSpyroSubFrameTimer;
        *(volatile unsigned char *)&g_bSpyroAnimLayer1Prev = prev;
        *(volatile unsigned char *)&g_bSpyroAnimLayer1FramePrev = framePrev;
        *(volatile unsigned char *)&g_bSpyroAnimLayer1Current = cur;
        *(volatile unsigned char *)&g_bSpyroAnimLayer1Frame = frame;
        *(volatile unsigned char *)&g_abSpyroAnimLayer1SubstepBlock = sub;
      } else {
        AdvanceSpyroAnimLayer1Frame(g_nSpyroFlameAnimRate);
      }
    } else {
      mode = g_abSpyroFlameTransitionModes[g_nSpyroFlameLayer1State][state];
      switch (mode) {
      case 2:
        oldAnim = *(volatile byte *)&g_bSpyroAnimLayer1Current;
        oldFrame = *(volatile byte *)&g_bSpyroAnimLayer1Frame;
        g_anSpyroFlamePhaseBlock[0] = mode;
        *(volatile unsigned char *)&g_bSpyroAnimLayer1Prev = oldAnim;
        *(volatile unsigned char *)&g_bSpyroAnimLayer1FramePrev = oldFrame;
        newAnim = g_abSpyroFlameAnimTable[state];
        *(volatile unsigned char *)&g_bSpyroAnimLayer1Frame = 0;
        *(volatile unsigned char *)&g_abSpyroAnimLayer1SubstepBlock = 4;
        g_anSpyroFlamePhaseBlock[2] = state;
        *(volatile unsigned char *)&g_bSpyroAnimLayer1Current = newAnim;
        break;
      case 1:
        *(volatile int *)&g_anSpyroFlamePhaseBlock[0] = mode;
        blendAnim = *(volatile byte *)&g_bSpyroAnimLayer1Current;
        blendFrame = *(volatile byte *)&g_bSpyroAnimLayer1Frame;
        blendCur = *(volatile byte *)&g_bSpyroAnimCurrent;
        blendCurFrame = *(volatile byte *)&g_bSpyroAnimFrame;
        *(volatile unsigned char *)&g_abSpyroAnimLayer1SubstepBlock = 2;
        g_anSpyroFlamePhaseBlock[2] = state;
        *(volatile unsigned char *)&g_bSpyroAnimLayer1Prev = blendAnim;
        *(volatile unsigned char *)&g_bSpyroAnimLayer1FramePrev = blendFrame;
        *(volatile unsigned char *)&g_bSpyroAnimLayer1Current = blendCur;
        *(volatile unsigned char *)&g_bSpyroAnimLayer1Frame = blendCurFrame;
        break;
      }
    }
    break;
  case 2:
    timer = g_abSpyroAnimLayer1SubstepBlock + 4;
    g_abSpyroAnimLayer1SubstepBlock = timer;
    if ((unsigned char)timer >= 0x10) {
      AdvanceSpyroAnimLayer1Frame(0);
      g_anSpyroFlamePhaseBlock[0] = 0;
    }
    break;
  case 1:
    timer = g_abSpyroAnimLayer1SubstepBlock + 2;
    g_abSpyroAnimLayer1SubstepBlock = timer;
    if ((unsigned char)timer >= 0x10) {
      AdvanceSpyroAnimLayer1Frame(0);
      g_anSpyroFlamePhaseBlock[0] = 0;
    }
    break;
  }
}
