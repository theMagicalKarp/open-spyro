#include "camera.h"
#include "cd_stream.h"
#include "funcs.h"
#include "globals.h"
#include "level_id.h"
#include "main_shared.h"
#include "pad_buttons.h"
#include "spyro_state.h"

/* Level-select (cheat menu) pad handler (0x8002d580, 0x290 bytes).
 * Circle/Cross/Square/Triangle/Right/Down/Left pick a slot 0-6; Up steps
 * g_nNextLevelId to the next level (wrapping past sub-level 5 into the
 * next world, and past the last level back to 10) and arms its world.
 * With a world already chosen, a slot press either reverts (out of range /
 * sub-level >= 6) or tears down into the level-load transition (gamestate
 * GS_LEVEL_LOAD, CD stream kick, level-transition camera); with no world
 * chosen the slot picks the world. */
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
      g_nNextLevelId = g_nActiveLevelId + 1;
      if (g_nNextLevelId % 10 >= 6) {
        g_nNextLevelId = (g_nNextLevelId / 10 + 1) * 10;
      }
      if ((unsigned int)(g_nNextLevelId - LEVEL_ARTISANS) >=
          LEVEL_GNASTYS_LOOT - LEVEL_ARTISANS + 1U) {
        g_nNextLevelId = LEVEL_ARTISANS;
      }
      g_nLevelSelectWorldIndex = g_nNextLevelId / 10;
      sel = g_nNextLevelId % 10;
      break;
    }
    if (sel >= 0) {
      if (g_nLevelSelectWorldIndex != 0) {
        g_nNextLevelId = g_nLevelSelectWorldIndex * 10 + sel;
        g_nLevelSelectSubIndex = sel;
        if ((unsigned int)(g_nNextLevelId - LEVEL_ARTISANS) >=
                LEVEL_GNASTYS_LOOT - LEVEL_ARTISANS + 1U ||
            sel >= 6) {
          g_nNextLevelId = g_nActiveLevelId;
        } else {
          StopAllSoundExceptMask(0);
          InitLightVectorConstants();
          g_nReturnPortalLevelId = 0;
          g_nEnteredPortalIdx = -1;
          ResetSpyroEntity(RESET_SPYRO_SOFT);
          RestartSpyroAnimWithState(SPYRO_ST_GLIDE);
          g_nGamestate = GS_LEVEL_LOAD;
          g_nCdStreamState = LSTREAM_FRESH;
          g_nPortalTransitionActive = 0;
          g_nLevelTransitionTallyActive = 0;
          g_nSkipGamestateDraw = 1;
          g_nCameraNextMode = (CAM_HELD | CAM_MODE_LEVEL_TRANSITION);
        }
        g_nLevelSelectModeActive = 0;
        return;
      }
      g_nLevelSelectWorldIndex = sel;
    }
  }
}
