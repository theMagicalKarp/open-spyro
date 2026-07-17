#include "globals.h"

extern void StopAllSoundExceptMask();
extern void InitLightVectorConstants(void);
extern void ResetSpyroEntity();
extern void RestartSpyroAnimWithState();

/* Level-select (cheat menu) pad handler (0x8002d580, 0x290 bytes).
 * Square/Triangle/Circle/Cross/L1/R1/L2 pick a slot 0-6; R2 (0x1000) steps
 * g_nCurrentLevelId to the next level (wrapping past sub-level 5 into the
 * next world, and past the last level back to 10). With a world already
 * chosen, a slot press either reverts (out of range / sub-level >= 6) or
 * tears down into the level-load transition (gamestate 1, CD stream kick,
 * scripted camera 0x80000012); with no world chosen the slot picks the
 * world. */
void HandleLevelSelectInput(void) {
  int pad;
  int sel;
  pad = g_dwPadPressed;
  if (pad != 0) {
    sel = -1;
    switch (pad) {
    case 0x20:
      sel = 0;
      break;
    case 0x40:
      sel = 1;
      break;
    case 0x80:
      sel = 2;
      break;
    case 0x10:
      sel = 3;
      break;
    case 0x2000:
      sel = 4;
      break;
    case 0x4000:
      sel = 5;
      break;
    case 0x8000:
      sel = 6;
      break;
    case 0x1000:
      g_nCurrentLevelId = g_nActiveLevelId + 1;
      if (g_nCurrentLevelId % 10 >= 6) {
        g_nCurrentLevelId = (g_nCurrentLevelId / 10 + 1) * 10;
      }
      if ((unsigned int)(g_nCurrentLevelId - 10) >= 0x37U) {
        g_nCurrentLevelId = 10;
      }
      g_nLevelSelectWorldIndex = g_nCurrentLevelId / 10;
      sel = g_nCurrentLevelId % 10;
      break;
    }
    if (sel >= 0) {
      if (g_nLevelSelectWorldIndex != 0) {
        g_nCurrentLevelId = g_nLevelSelectWorldIndex * 10 + sel;
        g_nLevelSelectSubIndex = sel;
        if ((unsigned int)(g_nCurrentLevelId - 10) >= 0x37U || sel >= 6) {
          g_nCurrentLevelId = g_nActiveLevelId;
        } else {
          StopAllSoundExceptMask(0);
          InitLightVectorConstants();
          g_nScriptedMobTag = 0;
          g_nPreserveMobIdx = -1;
          ResetSpyroEntity(0);
          RestartSpyroAnimWithState(0xF);
          g_nGamestate = 1;
          g_nCdStreamState = 1;
          g_nScriptedRespawnFlag = 0;
          g_nLevelTransitionTallyActive = 0;
          g_nGamePaused = 1;
          g_nCameraNextMode = 0x80000012;
        }
        g_nLevelSelectModeActive = 0;
        return;
      }
      g_nLevelSelectWorldIndex = sel;
    }
  }
}
