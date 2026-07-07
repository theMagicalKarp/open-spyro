#include "globals.h"

/* Return to the current world's hub (0x8002c664): reset the level-transition /
   CD-stream state, arm a scripted respawn back into the hub (camera mode
   0x80000012, gamestate 1, tally + paused flags), tag the preserve-mob slot as
   empty, and snap the target level id back to the world's hub id
   ((active level / 10) * 10). */
extern void InitLightVectorConstants(void);

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
