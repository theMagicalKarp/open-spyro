#include "frame_db.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"

/* Draw gamestates 1 and 9 (GS_LEVEL_LOAD's portal flight / GS_ARRIVAL_CAMERA):
   copies the sky colour (g_dwWorldFogColor) into both frame DRAWENVs, clears
   the OT bins, draws the transition gem tally when active, Spyro, then the
   sky — through a view matrix tilted by g_nTransitionSkyPitchTilt (the
   entered portal's sky tilt, -2/frame) until it reaches 0. Ends with the
   standard 2-vblank frame submit (0x8001a050, 956 bytes). */
void LevelTransition_Draw(void) {
  int r;
  int g;
  int b;
  int v;
  MATRIX m1;
  MATRIX m2;
  MATRIX *p2;

  r = ((unsigned char *)&g_dwWorldFogColor)[0];
  g = ((unsigned char *)&g_dwWorldFogColor)[1];
  b = ((unsigned char *)&g_dwWorldFogColor)[2];
  g_FrameDrawEnv0.r0 = r;
  g_FrameDrawEnv0.g0 = g;
  g_FrameDrawEnv0.b0 = b;
  g_FrameDrawEnv1.r0 = r;
  g_FrameDrawEnv1.g0 = g;
  g_FrameDrawEnv1.b0 = b;
  FillWord(&g_apMainDrawEntityList, 0, FRAME_ARENA_LIST_WORDS);
  if (g_nLevelTransitionTallyActive != 0) {
    DrawLevelTransitionOverlay();
  }
  RasterizeSpyroModel();
  if (g_nSkyChunkCount != 0) {
    g_nTransitionSkyPitchTilt -= 2;
    if (g_nTransitionSkyPitchTilt < 0) {
      g_nTransitionSkyPitchTilt = 0;
    }
    if (g_nTransitionSkyPitchTilt != 0) {
      FillWord(&m1, 0, 0x20);
      SetTransMatrix(&m1);
      m1.m[0][0] = 0x1000;
      v = LookupCosine(g_anCameraEulerPitchBlock[0] -
                       g_nTransitionSkyPitchTilt);
      m1.m[1][1] = v;
      v = LookupSine(g_anCameraEulerPitchBlock[0] - g_nTransitionSkyPitchTilt);
      m1.m[2][1] = v;
      v = LookupSine(g_anCameraEulerPitchBlock[0] - g_nTransitionSkyPitchTilt);
      m1.m[1][2] = -v;
      v = LookupCosine(g_anCameraEulerPitchBlock[0] -
                       g_nTransitionSkyPitchTilt);
      m1.m[2][2] = v;

      p2 = &m2;
      FillWord(p2, 0, 0x20);
      v = LookupCosine(g_nCameraEulerYaw);
      m2.m[0][0] = v;
      v = LookupSine(g_nCameraEulerYaw);
      m2.m[2][0] = -v;
      m2.m[1][1] = 0x1000;
      v = LookupSine(g_nCameraEulerYaw);
      m2.m[0][2] = v;
      v = LookupCosine(g_nCameraEulerYaw);
      m2.m[2][2] = v;
      MulMatrix(&m1, p2);

      FillWord(p2, 0, 0x20);
      v = LookupCosine(g_nCameraEulerRoll);
      m2.m[0][0] = v;
      v = LookupSine(g_nCameraEulerRoll);
      m2.m[1][0] = -v;
      v = LookupSine(g_nCameraEulerRoll);
      m2.m[0][1] = v;
      v = LookupCosine(g_nCameraEulerRoll);
      m2.m[1][1] = v;
      m2.m[2][2] = 0x1000;
      MulMatrix(&m1, p2);

      CopyWords(p2, &m1, 0x14);
      v = m1.m[1][0] * 0x140;
      if (v < 0) {
        v += 0x1ff;
      }
      m2.m[1][0] = v >> 9;
      v = m1.m[1][1] * 0x140;
      if (v < 0) {
        v += 0x1ff;
      }
      m2.m[1][1] = v >> 9;
      v = m1.m[1][2] * 0x140;
      if (v < 0) {
        v += 0x1ff;
      }
      m2.m[1][2] = v >> 9;
      EmitSkyMesh(-1, &m1, p2);
    } else {
      EmitSkyMesh(-1, (MATRIX *)g_anCameraViewMtx,
                  (MATRIX *)CAM_W2C_MTX_VIA_VIEW);
    }
  }
  DrawSync(0);
  if (g_nDemoActive != 0) {
    VSync(0);
  }
  g_nVsyncFrameEndCount = VSync(-1);
  if (g_nVsyncFrameEndCount - g_nVsyncFramePaceAnchor < 2) {
    int *end = g_anVsyncFrameEndBlock;
    do {
      VSync(0);
      g_nVsyncFrameEndCount = VSync(-1);
    } while (g_nVsyncFrameEndCount - end[-1] < 2);
  }
  g_nVsyncFramePaceAnchor = VSync(-1);
  PutDispEnv(&FRAME_DB(g_pActiveFrameDrawEnv)->disp);
  PutDrawEnv(g_pActiveFrameDrawEnv);
  DrawOTag(LinkOTPrimitives(OT_BIN_COUNT));
}
