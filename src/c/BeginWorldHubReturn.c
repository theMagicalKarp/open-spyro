#include "camera.h"
#include "globals.h"
#include "main_shared.h"

void BeginWorldHubReturn(void) {
  int hub;
  InitLightVectorConstants();
  hub = (g_nActiveLevelId / 10) * 10;
  g_nCdStreamState = 0;
  g_nLevelTransitionPhase = 0;
  g_nCameraNextMode = (CAM_HELD | CAM_MODE_LEVEL_TRANSITION);
  g_nEnteredPortalIdx = -1;
  g_nReturnPortalLevelId = g_nActiveLevelId;
  g_nGamestate = GS_LEVEL_LOAD;
  g_nScriptedRespawnFlag = 1;
  g_nLevelTransitionTallyActive = 1;
  g_nGamePaused = 1;
  g_nCurrentLevelId = hub;
}
