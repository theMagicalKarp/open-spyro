/* Native-only level warp for headless runs (`spyro --warp N:LEVEL`): the
 * teardown HandleLevelSelectInput (src/main/gamestate/cheats.c) runs when the
 * dev level select picks a level, from outside the pad handler. */

#include "camera.h"
#include "cd_stream.h"
#include "funcs.h"
#include "gamestate.h"
#include "globals.h"
#include "level_id.h"
#include "spyro_state.h"

/* Called by the host between frames. Returns 1 once the warp has started,
 * 0 while the game is not in a state to leave (the host retries). */
__attribute__((export_name("native_warp"))) int native_warp(int level) {
  if (g_nGamestate != GS_GAMEPLAY) {
    return 0;
  }
  g_nNextLevelId = level;
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
  return 1;
}
