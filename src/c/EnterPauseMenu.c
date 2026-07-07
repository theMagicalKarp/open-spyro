#include "globals.h"

/* Enter the pause menu gamestate (0x8002c420). param nonzero = paused from
   gameplay (stop SFX, reset the transition frames). Plays the menu-open sound,
   switches to gamestate 2 and zeros the pause-menu cursor/substate/idle timer.
   Flight levels skip all HUD work; otherwise re-init the HUD counters, and on
   the param==0 re-entry path force all five counters visible with roll phase
   0xc, then refresh the counters. */
extern void StopAllSoundExceptMask(unsigned int mask);
extern int PlaySoundEffect(unsigned int sample, int a, unsigned int b, void *c);
extern void InitHudCounters(int mode);
extern void UpdateHudCounters(void);

void EnterPauseMenu(int param_1) {
  if (param_1 != 0) {
    StopAllSoundExceptMask(0);
  }
  PlaySoundEffect(*(unsigned char *)((char *)g_pLevelSampleBankHeader + 0x2C),
                  0, 0x10, 0);
  g_nGamestate = 2;
  g_nPauseMenuCursor = 0;
  g_nPauseMenuSubstate = 0;
  g_nPauseMenuIdleFrames = 0;
  if (g_nFlightLevelActive != 0) {
    if (param_1 != 0) {
      g_nPauseMenuTransitionFrames = 0;
    }
    return;
  }
  InitHudCounters(0);
  if (param_1 != 0) {
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
    g_bHudLevelEggRollPhase = 0xC;
  }
  UpdateHudCounters();
}
