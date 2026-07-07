#include "globals.h"

/* Enter the inventory / sub-menu gamestate (0x8002c714). param nonzero = opened
   from gameplay (stop SFX first, reset the pause transition frames). Plays the
   menu-open sound (sample id at g_pLevelSampleBankHeader+0x2c), zeros the
   sub-menu scroll state, switches to gamestate 3, seats the cursor on the
   current world (level intro index / 6), then clears the cheat-code pad-history
   ring. */
extern void StopAllSoundExceptMask(unsigned int mask);
extern int PlaySoundEffect(unsigned int sample, int a, unsigned int b, void *c);
extern void ClearPadHistoryRing(void);

void EnterInventoryMenu(int param_1) {
  if (param_1 != 0) {
    StopAllSoundExceptMask(0);
  }
  PlaySoundEffect(*(unsigned char *)((char *)g_pLevelSampleBankHeader + 0x2C),
                  0, 0x10, 0);
  g_nSubMenuScrollOffset = 0;
  g_nSubMenuScrollDelta = 0;
  g_nGamestate = 3;
  g_nSubMenuCursor = g_nLevelIntroIndex / 6;
  if (param_1 != 0) {
    g_nPauseMenuTransitionFrames = 0;
  }
  ClearPadHistoryRing();
}
