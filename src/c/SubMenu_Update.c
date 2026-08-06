#include "globals.h"

/* Inventory sub-menu per-frame update (0x8002eb2c, 0x2c4). Advances the
   HUD bob phase / frame-cycle counters and the pause-menu idle timer,
   spins the menu light vector, then (once the 5-frame transition is
   done) either runs level-select input or the normal sub-menu: cheat
   detection, up/down scrolling between visited levels (0x1C0 fixed-step
   scroll with cursor wrap), and pause/exit pad handling. */
extern void RotateLightVectorXZ(int mode);
extern void HandleLevelSelectInput(void);
extern void CheckCheatCodeMatch(void);
extern void PlaySoundEffect(int sample, int a, int b, int c);
extern void EnterPauseMenu(int mode);
extern void ExitSubMenuToTitle(void);

extern unsigned int g_adwHudBobSinePhaseBlock[]; /* g_dwHudBobSinePhase */

void SubMenu_Update(void) {
  g_nHudFrameCycleMod9 = (g_nHudFrameCycleMod9 + 1) % 9;
  g_adwHudBobSinePhaseBlock[0] =
      (g_adwHudBobSinePhaseBlock[0] - g_nFrameStep) & 0xFF;
  g_nPauseMenuIdleFrames += g_nFrameStep;
  RotateLightVectorXZ(3);
  if (g_nPauseMenuTransitionFrames < 5) {
    return;
  }
  if (g_nLevelSelectModeActive != 0) {
    HandleLevelSelectInput();
    return;
  }
  CheckCheatCodeMatch();
  if (g_nSubMenuScrollOffset == 0) {
    if (g_nSubMenuCursor > 0) {
      if (g_abLevelVisitedFlagVirtualBase[g_nSubMenuCursor * 6] != 0) {
        if (g_dwPadHeld & 0x8000) {
          PlaySoundEffect(
              *((unsigned char *)(((char *)g_pLevelSampleBankHeader) + 0x38)),
              0, 0x10, 0);
          g_nSubMenuScrollDelta = 0x40;
        }
      }
    }
    if (g_nSubMenuCursor < 5) {
      if (g_abLevelVisitedFlag[(g_nSubMenuCursor * 6) + 6] != 0) {
        if (g_dwPadHeld & 0x2000) {
          PlaySoundEffect(
              *((unsigned char *)(((char *)g_pLevelSampleBankHeader) + 0x38)),
              0, 0x10, 0);
          g_nSubMenuScrollDelta = -0x40;
        }
      }
    }
  }
  if (g_nSubMenuScrollDelta != 0) {
    g_nSubMenuScrollOffset += g_nSubMenuScrollDelta;
    if (g_nSubMenuScrollOffset == 0) {
      g_nSubMenuScrollDelta = 0;
    } else if (g_nSubMenuScrollOffset == (-0x1C0)) {
      g_nSubMenuScrollDelta = -0x40;
      g_nSubMenuScrollOffset = 0x1C0;
      g_nSubMenuCursor += 1;
    } else if (g_nSubMenuScrollOffset == 0x1C0) {
      g_nSubMenuScrollDelta = 0x40;
      g_nSubMenuScrollOffset = -0x1C0;
      g_nSubMenuCursor -= 1;
    }
  }
  if (g_dwPadPressed & 0x810) {
    EnterPauseMenu(0);
  } else if (g_dwPadPressed & 0x140) {
    if (g_nLevelSelectModeActive == 0) {
      ExitSubMenuToTitle();
    }
  }
}
