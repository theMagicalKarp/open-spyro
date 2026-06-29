#include "globals.h"

extern void InitLightVectorConstants(void);

/* Finishes the respawn->gameplay transition: returns to gamestate 0, copies the
   world fog RGB into both frame draw-env background-fill colors, then rebuilds
   the light vector constants. (0x8002c8a4, 112 bytes.) */
void EndRespawnToGameplay(void) {
  u_char r = ((u_char *)&g_dwWorldFogColor)[0];
  u_char g = ((u_char *)&g_dwWorldFogColor)[1];
  u_char b = ((u_char *)&g_dwWorldFogColor)[2];

  g_nGamestate = 0;
  g_abFrameDrawEnv0.r0 = r;
  g_abFrameDrawEnv0.g0 = g;
  g_abFrameDrawEnv0.b0 = b;
  g_abFrameDrawEnv1.r0 = r;
  g_abFrameDrawEnv1.g0 = g;
  g_abFrameDrawEnv1.b0 = b;
  InitLightVectorConstants();
}
