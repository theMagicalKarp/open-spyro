#include "globals.h"

extern void AdvanceSpyroAnimLayer1Frame(int amount);

extern int g_anSpyroHornStrikeSwingPhaseBlock[];
extern int D_80078AC0;
extern unsigned char D_80075264[][2];
extern unsigned char D_80075268[];
extern unsigned char D_80078A72;
extern unsigned char D_80078A73;
extern unsigned char D_80078A78;
extern unsigned char D_80078A79;
extern unsigned char g_abSpyroAnimLayer1SubstepBlock;

/* 0x80049660 — per-frame layer-1 (horn-strike overlay) anim driver: phase 0
   tracks the horn-strike state (snapshot the primary anim bytes when idle,
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

  switch (g_anSpyroHornStrikeSwingPhaseBlock[0]) {
  case 0:
    state = g_nSpyroHornStrikeState;
    if (state == g_anSpyroHornStrikeSwingPhaseBlock[2]) {
      if (state == 0) {
        /* volatile accesses pin the original's load-all-then-store-all
           schedule (the -g3 scheduler otherwise pairs each store with
           its load). */
        prev = *(volatile byte *)&g_bSpyroAnimPrev;
        framePrev = *(volatile byte *)&g_bSpyroFramePrev;
        cur = *(volatile byte *)&g_bSpyroAnimCurrent;
        frame = *(volatile byte *)&g_bSpyroAnimFrame;
        sub = *(volatile byte *)&g_bSpyroSubFrameTimer;
        *(volatile unsigned char *)&D_80078A72 = prev;
        *(volatile unsigned char *)&D_80078A78 = framePrev;
        *(volatile unsigned char *)&D_80078A73 = cur;
        *(volatile unsigned char *)&D_80078A79 = frame;
        *(volatile unsigned char *)&g_abSpyroAnimLayer1SubstepBlock = sub;
      } else {
        AdvanceSpyroAnimLayer1Frame(g_nSpyroHornStrikeAnimRate);
      }
    } else {
      mode = D_80075264[D_80078AC0][state];
      switch (mode) {
      case 2:
        oldAnim = *(volatile byte *)&D_80078A73;
        oldFrame = *(volatile byte *)&D_80078A79;
        g_anSpyroHornStrikeSwingPhaseBlock[0] = mode;
        *(volatile unsigned char *)&D_80078A72 = oldAnim;
        *(volatile unsigned char *)&D_80078A78 = oldFrame;
        newAnim = D_80075268[state];
        *(volatile unsigned char *)&D_80078A79 = 0;
        *(volatile unsigned char *)&g_abSpyroAnimLayer1SubstepBlock = 4;
        g_anSpyroHornStrikeSwingPhaseBlock[2] = state;
        *(volatile unsigned char *)&D_80078A73 = newAnim;
        break;
      case 1:
        *(volatile int *)&g_anSpyroHornStrikeSwingPhaseBlock[0] = mode;
        blendAnim = *(volatile byte *)&D_80078A73;
        blendFrame = *(volatile byte *)&D_80078A79;
        blendCur = *(volatile byte *)&g_bSpyroAnimCurrent;
        blendCurFrame = *(volatile byte *)&g_bSpyroAnimFrame;
        *(volatile unsigned char *)&g_abSpyroAnimLayer1SubstepBlock = 2;
        g_anSpyroHornStrikeSwingPhaseBlock[2] = state;
        *(volatile unsigned char *)&D_80078A72 = blendAnim;
        *(volatile unsigned char *)&D_80078A78 = blendFrame;
        *(volatile unsigned char *)&D_80078A73 = blendCur;
        *(volatile unsigned char *)&D_80078A79 = blendCurFrame;
        break;
      }
    }
    break;
  case 2:
    timer = g_abSpyroAnimLayer1SubstepBlock + 4;
    g_abSpyroAnimLayer1SubstepBlock = timer;
    if ((unsigned char)timer >= 0x10) {
      AdvanceSpyroAnimLayer1Frame(0);
      g_anSpyroHornStrikeSwingPhaseBlock[0] = 0;
    }
    break;
  case 1:
    timer = g_abSpyroAnimLayer1SubstepBlock + 2;
    g_abSpyroAnimLayer1SubstepBlock = timer;
    if ((unsigned char)timer >= 0x10) {
      AdvanceSpyroAnimLayer1Frame(0);
      g_anSpyroHornStrikeSwingPhaseBlock[0] = 0;
    }
    break;
  }
}
