#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "sample_bank.h"

/* Enter the inventory / sub-menu gamestate (0x8002c714). param nonzero = opened
   from gameplay (stop SFX first, reset the pause transition frames). Plays the
   menu-open sound (sample id at g_pLevelSampleBankHeader+0x2c), zeros the
   sub-menu scroll state, switches to gamestate 3, seats the cursor on the
   current world (level intro index / 6), then clears the cheat-code pad-history
   ring. */

void EnterInventoryMenu(int fromGameplay) {
  if (fromGameplay != 0) {
    StopAllSoundExceptMask(0);
  }
  PlaySoundEffect(LEVEL_SFX(0x2C), 0, 0x10, 0);
  g_nSubMenuScrollOffset = 0;
  g_nSubMenuScrollDelta = 0;
  g_nGamestate = GS_INVENTORY;
  g_nSubMenuCursor = g_nLevelIndex / 6;
  if (fromGameplay != 0) {
    g_nPauseMenuTransitionFrames = 0;
  }
  ClearPadHistoryRing();
}
