#include "funcs.h"
#include "globals.h"
#include "hud_counter.h"
#include "main_shared.h"

/* Tear down a sub-menu and return to the title: blit the saved framebuffer
   region back from VRAM scratch, reset gamestate to 0, re-prime lighting, clear
   the HUD counter display flags, and restart the title music (0x8002c7bc). */
void ExitSubMenuToTitle(void) {
  RECT rect;

  rect.x = 0x200;
  rect.y = 0;
  rect.w = 0x100;
  rect.h = 0xE1;
  LoadImage(&rect, (char *)g_pRenderScratchPrimTop + 0xFFFE3E00);
  DrawSync(0);
  g_nGamestate = GS_GAMEPLAY;
  InitLightVectorConstants();
  g_abHudCounterDisplayState[HUD_CTR_GEMS] = HUD_CTR_HIDDEN;
  g_abHudCounterDisplayState[HUD_CTR_DRAGONS] = HUD_CTR_HIDDEN;
  g_abHudCounterDisplayState[HUD_CTR_LIVES] = HUD_CTR_HIDDEN;
  g_abHudCounterDisplayState[HUD_CTR_EGGS] = HUD_CTR_HIDDEN;
  g_abHudCounterDisplayState[HUD_CTR_KEY] = HUD_CTR_HIDDEN;
  HandleMusicCommand(g_nCurrentMusicTrack, 8);
}
