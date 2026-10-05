#include "globals.h"

extern void ChangeSpyroState(int state);

/* 0x8003fdc8 (0x78) — switch Spyro to `state` and restart his animation from
   that state's default clip: frame 1, sub-frame timer and anim mode cleared,
   the new state recorded as the previous one, and both the previous and
   current clip set from g_abSpyroStateAnimIndexMap.

   The clip table is re-read for each copy: a byte store may alias the byte
   table (sched.c true/anti_dependence exempt QImode from the in-struct
   escape), so each read is pinned between the byte stores around it. */
void RestartSpyroAnimWithState(int state) {
  int prev;
  int anim;

  ChangeSpyroState(state);
  prev = g_abSpyroStateAnimIndexMap[g_nSpyroState];
  g_bSpyroFramePrev = 0;
  g_bSpyroAnimPrev = prev;
  anim = g_abSpyroStateAnimIndexMap[g_nSpyroState];
  g_bSpyroAnimFrame = 1;
  g_bSpyroSubFrameTimer = 0;
  g_nSpyroAnimMode = 0;
  g_nSpyroPrevState = g_nSpyroState;
  g_bSpyroAnimCurrent = anim;
}
