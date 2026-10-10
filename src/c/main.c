#include "globals.h"
#include "sdata.h"

extern void Initialize(void);
extern void GamestateUpdate(void);
extern void GamestateDraw(void);

SDATA(g_nInGameTick, 4);
SDATA(g_nFrameTicks, 4);
SDATA(g_nFrameStep, 4);
SDATA(g_nGamePaused, 4);

/* Program entry (0x80012204, 136 bytes): one-time Initialize, then the frame
   loop. g_nFrameStep is the number of VBlanks the last frame took, clamped to
   2..4; GamestateDraw is skipped while the game is paused. The low byte of
   g_nInGameTick flags "between update and draw". */
int main(void) {
  Initialize();
  for (;;) {
    *(char *)&g_nInGameTick = 0;
    GamestateUpdate();
    g_nFrameStep = g_nFrameTicks;
    *(char *)&g_nInGameTick = 1;
    if (g_nFrameStep < 2) {
      g_nFrameStep = 2;
    }
    if (g_nFrameStep >= 5) {
      g_nFrameStep = 4;
    }
    g_nFrameTicks = 0;
    if (!g_nGamePaused) {
      GamestateDraw();
    }
  }
}
