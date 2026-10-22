#include "globals.h"
#include "main_shared.h"

void BeginWorldHubReturn(void) {
  int hub;
  InitLightVectorConstants();
  hub = (g_nActiveLevelId / 10) * 10;
  g_nCdStreamState = 0;
  g_nLevelTransitionPhase = 0;
  g_nCameraNextMode = 0x80000012;
  g_nPreserveMobIdx = -1;
  g_nScriptedMobTag = g_nActiveLevelId;
  g_nGamestate = 1;
  g_nScriptedRespawnFlag = 1;
  g_nLevelTransitionTallyActive = 1;
  g_nGamePaused = 1;
  g_nCurrentLevelId = hub;
}
