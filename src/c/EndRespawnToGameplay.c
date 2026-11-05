#include "globals.h"
#include "main_shared.h"

/* Finishes the respawn->gameplay transition: returns to GS_GAMEPLAY, copies the
   world fog RGB into both frame draw-env background-fill colors, then rebuilds
   the light vector constants. (0x8002c8a4, 112 bytes.) */
void EndRespawnToGameplay(void) {
  u_char r = ((u_char *)&g_dwWorldFogColor)[0];
  u_char g = ((u_char *)&g_dwWorldFogColor)[1];
  u_char b = ((u_char *)&g_dwWorldFogColor)[2];

  g_nGamestate = GS_GAMEPLAY;
  g_FrameDrawEnv0.r0 = r;
  g_FrameDrawEnv0.g0 = g;
  g_FrameDrawEnv0.b0 = b;
  g_FrameDrawEnv1.r0 = r;
  g_FrameDrawEnv1.g0 = g;
  g_FrameDrawEnv1.b0 = b;
  InitLightVectorConstants();
}
