#include "funcs.h"
#include "globals.h"
#include "level_sfx.h"
#include "main_shared.h"
#include "pad_buttons.h"
#include "sample_bank.h"

/* Inventory sub-menu per-frame update (0x8002eb2c, 0x2c4). Advances the
   HUD bob phase / frame-cycle counters and the pause-menu idle timer,
   spins the menu light vector, then (once the 5-frame transition is
   done) either runs level-select input or the normal sub-menu: cheat
   detection, up/down scrolling between visited levels (0x1C0 fixed-step
   scroll with cursor wrap), and pause/exit pad handling. */

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
        if (g_dwPadHeld & PAD_LEFT) {
          PlaySoundEffect(LEVEL_SFX(SFX_MENU_PAGE), 0, SFX_MODE_PANNED, 0);
          g_nSubMenuScrollDelta = 0x40;
        }
      }
    }
    if (g_nSubMenuCursor < 5) {
      if (g_abLevelVisitedFlag[(g_nSubMenuCursor * 6) + 6] != 0) {
        if (g_dwPadHeld & PAD_RIGHT) {
          PlaySoundEffect(LEVEL_SFX(SFX_MENU_PAGE), 0, SFX_MODE_PANNED, 0);
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
  if (g_dwPadPressed & (PAD_TRIANGLE | PAD_START)) {
    EnterPauseMenu(0);
  } else if (g_dwPadPressed & (PAD_CROSS | PAD_SELECT)) {
    if (g_nLevelSelectModeActive == 0) {
      ExitSubMenuToTitle();
    }
  }
}
