#include "funcs.h"
#include "globals.h"
#include "hud_counter.h"
#include "main_shared.h"

/* Leave the pause menu back to gameplay (0x8002c534). Blit the saved 0x200x0xE1
   backdrop tile from render scratch back into the frame buffer, wait for the
   transfer, return to gamestate 0 and re-init lighting, then force all five HUD
   counters visible with roll phase 0xd. When leaving from gameplay (param != 0)
   resume the current music track. */

void ExitPauseMenuToGame(int resumeMusic) {
  RECT rect;

  rect.x = 0x200;
  rect.w = 0x100;
  rect.y = 0;
  rect.h = 0xE1;
  LoadImage(&rect, (unsigned int)g_pRenderScratchPrimTop + 0xFFFE3E00);
  DrawSync(0);
  g_nGamestate = GS_GAMEPLAY;
  InitLightVectorConstants();
  g_abHudCounterDisplayState[HUD_CTR_GEMS] = HUD_CTR_ROLL_OUT;
  g_abHudCounterDisplayState[HUD_CTR_DRAGONS] = HUD_CTR_ROLL_OUT;
  g_abHudCounterDisplayState[HUD_CTR_LIVES] = HUD_CTR_ROLL_OUT;
  g_abHudCounterDisplayState[HUD_CTR_EGGS] = HUD_CTR_ROLL_OUT;
  g_abHudCounterDisplayState[HUD_CTR_KEY] = HUD_CTR_ROLL_OUT;
  g_bHudLevelGemRollPhase = 0xD;
  g_bHudDragonRollPhase = 0xD;
  g_bHudLivesRollPhase = 0xD;
  g_bHudWorldEggRollPhase = 0xD;
  g_bHudKeyRollPhase = 0xD;
  if (resumeMusic != 0) {
    HandleMusicCommand(g_nCurrentMusicTrack, 8);
  }
}
