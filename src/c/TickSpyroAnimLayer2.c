#include "funcs.h"
#include "globals.h"
#include "main_shared.h"

extern int g_nSpyroAnimLayer2Phase;  /* 0 = tracking the primary anim */
extern int g_nSpyroAnimLayer2Rate;   /* frame advance per tick */
extern int g_nSpyroAnimLayer2Active; /* layer-2 anim now playing */
extern unsigned char g_bSpyroAnimLayer2Substep;

/* 0x80049e8c — per-frame layer-2 anim driver: while the gate counter is
   synced, either seed the layer-2 anim state from Spyro's layer-1 state
   (gate == 0) or advance the layer-2 frame. */
void TickSpyroAnimLayer2(void) {
  unsigned char prev;
  unsigned char framePrev;
  unsigned char cur;
  unsigned char frame;
  unsigned char sub;

  if (g_nSpyroAnimLayer2Phase == 0) {
    if (g_nSpyroAnimLayer2Request == g_nSpyroAnimLayer2Active) {
      if (g_nSpyroAnimLayer2Request == 0) {
        /* volatile accesses pin the original's load-all-then-store-all
           schedule (the -g3 scheduler otherwise pairs each store with
           its load). */
        prev = *(volatile byte *)&g_bSpyroAnimPrev;
        framePrev = *(volatile byte *)&g_bSpyroFramePrev;
        cur = *(volatile byte *)&g_bSpyroAnimCurrent;
        frame = *(volatile byte *)&g_bSpyroAnimFrame;
        sub = *(volatile byte *)&g_bSpyroSubFrameTimer;
        *(volatile unsigned char *)&g_bSpyroAnimLayer2Prev = prev;
        *(volatile unsigned char *)&g_bSpyroAnimLayer2FramePrev = framePrev;
        *(volatile unsigned char *)&g_bSpyroAnimLayer2Current = cur;
        *(volatile unsigned char *)&g_bSpyroAnimLayer2Frame = frame;
        *(volatile unsigned char *)&g_bSpyroAnimLayer2Substep = sub;
      } else {
        AdvanceSpyroAnimLayer2Frame(g_nSpyroAnimLayer2Rate);
      }
    }
  }
}
