#include "globals.h"

extern void LoadImage(RECT *rect, void *data);
extern int DrawSync(int mode);
extern void InitLightVectorConstants(void);
extern void HandleMusicCommand(int track, int cmd);

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
  g_nGamestate = 0;
  InitLightVectorConstants();
  g_abHudCounterDisplayState[0] = 0;
  g_abHudCounterDisplayState[1] = 0;
  g_abHudCounterDisplayState[2] = 0;
  g_abHudCounterDisplayState[3] = 0;
  g_abHudCounterDisplayState[4] = 0;
  HandleMusicCommand(g_nCurrentMusicTrack, 8);
}
