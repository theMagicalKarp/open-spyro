#include "globals.h"

/* Leave the pause menu back to gameplay (0x8002c534). Blit the saved 0x200x0xE1
   backdrop tile from render scratch back into the frame buffer, wait for the
   transfer, return to gamestate 0 and re-init lighting, then force all five HUD
   counters visible with roll phase 0xd. When leaving from gameplay (param != 0)
   resume the current music track. */
extern void LoadImage(RECT *rect, unsigned int src);
extern int DrawSync(int mode);
extern void InitLightVectorConstants(void);
extern int HandleMusicCommand(int track, int cmd);

void ExitPauseMenuToGame(int param_1) {
  RECT rect;

  rect.x = 0x200;
  rect.w = 0x100;
  rect.y = 0;
  rect.h = 0xE1;
  LoadImage(&rect, (unsigned int)g_pRenderScratchPrimTop + 0xFFFE3E00);
  DrawSync(0);
  g_nGamestate = 0;
  InitLightVectorConstants();
  g_abHudCounterDisplayState[0] = 3;
  g_abHudCounterDisplayState[1] = 3;
  g_abHudCounterDisplayState[2] = 3;
  g_abHudCounterDisplayState[3] = 3;
  g_abHudCounterDisplayState[4] = 3;
  g_bHudLevelGemRollPhase = 0xD;
  g_bHudDragonRollPhase = 0xD;
  g_bHudLivesRollPhase = 0xD;
  g_bHudWorldEggRollPhase = 0xD;
  g_bHudLevelEggRollPhase = 0xD;
  if (param_1 != 0) {
    HandleMusicCommand(g_nCurrentMusicTrack, 8);
  }
}
