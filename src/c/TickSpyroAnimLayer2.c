#include "globals.h"

extern void AdvanceSpyroAnimLayer2Frame(int amount);

extern int D_80078AC4;
extern int D_80078AC8;
extern int D_80078ACC;
extern int D_80078C40;
extern unsigned char D_80078A74;
extern unsigned char D_80078A75;
extern unsigned char D_80078A7A;
extern unsigned char D_80078A7B;
extern unsigned char D_80078A7E;

/* 0x80049e8c — per-frame layer-2 anim driver: while the gate counter is
   synced, either seed the layer-2 anim state from Spyro's layer-1 state
   (gate == 0) or advance the layer-2 frame. */
void TickSpyroAnimLayer2(void) {
  unsigned char prev;
  unsigned char framePrev;
  unsigned char cur;
  unsigned char frame;
  unsigned char sub;

  if (D_80078AC4 == 0) {
    if (D_80078C40 == D_80078ACC) {
      if (D_80078C40 == 0) {
        /* volatile accesses pin the original's load-all-then-store-all
           schedule (the -g3 scheduler otherwise pairs each store with
           its load). */
        prev = *(volatile byte *)&g_bSpyroAnimPrev;
        framePrev = *(volatile byte *)&g_bSpyroFramePrev;
        cur = *(volatile byte *)&g_bSpyroAnimCurrent;
        frame = *(volatile byte *)&g_bSpyroAnimFrame;
        sub = *(volatile byte *)&g_bSpyroSubFrameTimer;
        *(volatile unsigned char *)&D_80078A74 = prev;
        *(volatile unsigned char *)&D_80078A7A = framePrev;
        *(volatile unsigned char *)&D_80078A75 = cur;
        *(volatile unsigned char *)&D_80078A7B = frame;
        *(volatile unsigned char *)&D_80078A7E = sub;
      } else {
        AdvanceSpyroAnimLayer2Frame(D_80078AC8);
      }
    }
  }
}
