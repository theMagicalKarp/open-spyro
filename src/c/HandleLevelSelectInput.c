#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "pad_buttons.h"
#include "spyro_state.h"

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
    case PAD_CIRCLE:
      sel = 0;
      break;
    case PAD_CROSS:
      sel = 1;
      break;
    case PAD_SQUARE:
      sel = 2;
      break;
    case PAD_TRIANGLE:
      sel = 3;
      break;
    case PAD_RIGHT:
      sel = 4;
      break;
    case PAD_DOWN:
      sel = 5;
      break;
    case PAD_LEFT:
      sel = 6;
      break;
    case PAD_UP:
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
          RestartSpyroAnimWithState(SPYRO_ST_GLIDE);
          g_nGamestate = GS_LEVEL_LOAD;
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
