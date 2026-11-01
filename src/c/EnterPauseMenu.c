#include "funcs.h"
#include "globals.h"
#include "level_sfx.h"
#include "main_shared.h"
#include "pause_menu.h"
#include "sample_bank.h"

/* Enter the pause menu gamestate (0x8002c420). param nonzero = paused from
   gameplay (stop SFX, reset the transition frames). Plays the menu-open sound,
   switches to gamestate 2 and zeros the pause-menu cursor/substate/idle timer.
   Flight levels skip all HUD work; otherwise re-init the HUD counters, and on
   the param==0 re-entry path force all five counters visible with roll phase
   0xc, then refresh the counters. */

void EnterPauseMenu(int fromGameplay) {
  if (fromGameplay != 0) {
    StopAllSoundExceptMask(0);
  }
  PlaySoundEffect(LEVEL_SFX(SFX_MENU_OPEN), 0, SFX_MODE_PANNED, 0);
  g_nGamestate = GS_PAUSE_MENU;
  g_nPauseMenuCursor = PAUSE_ROW_CONTINUE;
  g_nPauseMenuSubstate = PAUSE_PAGE_MAIN;
  g_nPauseMenuIdleFrames = 0;
  if (g_nFlightLevelActive != 0) {
    if (fromGameplay != 0) {
      g_nPauseMenuTransitionFrames = 0;
    }
    return;
  }
  InitHudCounters(0);
  if (fromGameplay != 0) {
    g_nPauseMenuTransitionFrames = 0;
  } else {
    g_abHudCounterDisplayState[0] = 1;
    g_abHudCounterDisplayState[1] = 1;
    g_abHudCounterDisplayState[2] = 1;
    g_abHudCounterDisplayState[3] = 1;
    g_abHudCounterDisplayState[4] = 1;
    g_bHudLevelGemRollPhase = 0xC;
    g_bHudDragonRollPhase = 0xC;
    g_bHudLivesRollPhase = 0xC;
    g_bHudWorldEggRollPhase = 0xC;
    g_bHudKeyRollPhase = 0xC;
  }
  UpdateHudCounters();
}
