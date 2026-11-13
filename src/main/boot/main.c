#include "funcs.h"
#include "globals.h"
#include "sdata.h"

SDATA(g_nPadLatchOpen, 4);
SDATA(g_nFrameTicks, 4);
SDATA(g_nFrameStep, 4);
SDATA(g_nSkipGamestateDraw, 4);

/* Program entry (0x80012204, 136 bytes): one-time Initialize, then the frame
   loop. g_nFrameStep is the number of VBlanks the last frame took, clamped to
   2..4; GamestateDraw is skipped on a frame whose update switched gamestate
   (g_nSkipGamestateDraw). The low byte of
   g_nPadLatchOpen is 0 while the update runs, so the VBlank pad callback
   does not latch new pad state under it. */
int main(void) {
  Initialize();
  for (;;) {
    *(char *)&g_nPadLatchOpen = 0;
    GamestateUpdate();
    g_nFrameStep = g_nFrameTicks;
    *(char *)&g_nPadLatchOpen = 1;
    if (g_nFrameStep < 2) {
      g_nFrameStep = 2;
    }
    if (g_nFrameStep >= 5) {
      g_nFrameStep = 4;
    }
    g_nFrameTicks = 0;
    if (!g_nSkipGamestateDraw) {
      GamestateDraw();
    }
  }
}
