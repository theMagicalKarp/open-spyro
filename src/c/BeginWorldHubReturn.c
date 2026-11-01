#include "camera.h"
#include "cd_stream.h"
#include "globals.h"
#include "main_shared.h"

/* Leave a level for its world's home (0x8002c664): the hub id is the level
   id rounded down to a multiple of 10. Starts the GS_LEVEL_LOAD stream in its
   preserve phase with the level-transition camera, remembers the level as the
   return portal (g_nReturnPortalLevelId) and pauses the game while it loads. */
void BeginWorldHubReturn(void) {
  int hub;
  InitLightVectorConstants();
  hub = (g_nActiveLevelId / 10) * 10;
  g_nCdStreamState = LSTREAM_PRESERVE;
  g_nLevelTransitionPhase = 0;
  g_nCameraNextMode = (CAM_HELD | CAM_MODE_LEVEL_TRANSITION);
  g_nEnteredPortalIdx = -1;
  g_nReturnPortalLevelId = g_nActiveLevelId;
  g_nGamestate = GS_LEVEL_LOAD;
  g_nPortalTransitionActive = 1;
  g_nLevelTransitionTallyActive = 1;
  g_nGamePaused = 1;
  g_nCurrentLevelId = hub;
}
