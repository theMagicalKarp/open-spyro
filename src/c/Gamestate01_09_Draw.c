#include "globals.h"

extern void FillWord(void *dst, unsigned int value, int byte_count);
extern void DrawLevelTransitionOverlay(void);
extern void RasterizePairedActor(void);
extern void SetTransMatrix(MATRIX *m);
extern int LookupSine(unsigned int angle);
extern int LookupCosine(unsigned int angle);
extern void MulMatrix(MATRIX *a, MATRIX *b);
extern void CopyWords(void *dst, void *src, int byte_count);
extern void EmitStaticActorMeshList(int lod, MATRIX *viewMtx, MATRIX *worldMtx);
extern void DrawSync(int mode);
extern int VSync(int mode);
extern DISPENV *PutDispEnv(DISPENV *env);
extern void PutDrawEnv(void *env);
extern void *LinkOTPrimitives(int depth_max);
extern void DrawOTag(void *ot);

/* Held-base alias for &g_nCameraEulerPitch (0x80076e1e). */
extern short g_anCameraEulerPitchBlock[];
extern int g_anVsyncFrameEndBlock[]; /* held-base alias: [-1] = pace anchor */
extern int D_80075910;               /* level-intro camera parallax countdown */
extern int D_80076DE4[]; /* camera view matrix (worldToCam at -0x14) */

/* Draw gamestates 1 and 9 (level intro / world arrival): copies the world
   fog color into both frame DRAWENVs, clears the OT bins, draws the
   transition gem tally when active, Spyro, then the static world mesh —
   with a temporary pitch-parallax view matrix while the intro countdown
   (D_80075910, -2/frame) runs. Ends with the standard 2-vblank frame
   submit (0x8001a050, 956 bytes). */
void Gamestate01_09_Draw(void) {
  int r;
  int g;
  int b;
  int iVar1;
  MATRIX m1;
  MATRIX m2;
  MATRIX *p2;

  r = ((unsigned char *)&g_dwWorldFogColor)[0];
  g = ((unsigned char *)&g_dwWorldFogColor)[1];
  b = ((unsigned char *)&g_dwWorldFogColor)[2];
  g_abFrameDrawEnv0.r0 = r;
  g_abFrameDrawEnv0.g0 = g;
  g_abFrameDrawEnv0.b0 = b;
  g_abFrameDrawEnv1.r0 = r;
  g_abFrameDrawEnv1.g0 = g;
  g_abFrameDrawEnv1.b0 = b;
  FillWord(&g_pOtDepthBinHead0, 0, 0x900);
  if (g_nLevelTransitionTallyActive != 0) {
    DrawLevelTransitionOverlay();
  }
  RasterizePairedActor();
  if (g_nWorldRenderMeshChunkCount != 0) {
    D_80075910 -= 2;
    if (D_80075910 < 0) {
      D_80075910 = 0;
    }
    if (D_80075910 != 0) {
      FillWord(&m1, 0, 0x20);
      SetTransMatrix(&m1);
      m1.m[0][0] = 0x1000;
      iVar1 = LookupCosine(g_anCameraEulerPitchBlock[0] - D_80075910);
      m1.m[1][1] = iVar1;
      iVar1 = LookupSine(g_anCameraEulerPitchBlock[0] - D_80075910);
      m1.m[2][1] = iVar1;
      iVar1 = LookupSine(g_anCameraEulerPitchBlock[0] - D_80075910);
      m1.m[1][2] = -iVar1;
      iVar1 = LookupCosine(g_anCameraEulerPitchBlock[0] - D_80075910);
      m1.m[2][2] = iVar1;

      p2 = &m2;
      FillWord(p2, 0, 0x20);
      iVar1 = LookupCosine(g_nCameraEulerYaw);
      m2.m[0][0] = iVar1;
      iVar1 = LookupSine(g_nCameraEulerYaw);
      m2.m[2][0] = -iVar1;
      m2.m[1][1] = 0x1000;
      iVar1 = LookupSine(g_nCameraEulerYaw);
      m2.m[0][2] = iVar1;
      iVar1 = LookupCosine(g_nCameraEulerYaw);
      m2.m[2][2] = iVar1;
      MulMatrix(&m1, p2);

      FillWord(p2, 0, 0x20);
      iVar1 = LookupCosine(g_nCameraEulerRoll);
      m2.m[0][0] = iVar1;
      iVar1 = LookupSine(g_nCameraEulerRoll);
      m2.m[1][0] = -iVar1;
      iVar1 = LookupSine(g_nCameraEulerRoll);
      m2.m[0][1] = iVar1;
      iVar1 = LookupCosine(g_nCameraEulerRoll);
      m2.m[1][1] = iVar1;
      m2.m[2][2] = 0x1000;
      MulMatrix(&m1, p2);

      CopyWords(p2, &m1, 0x14);
      iVar1 = m1.m[1][0] * 0x140;
      if (iVar1 < 0) {
        iVar1 += 0x1ff;
      }
      m2.m[1][0] = iVar1 >> 9;
      iVar1 = m1.m[1][1] * 0x140;
      if (iVar1 < 0) {
        iVar1 += 0x1ff;
      }
      m2.m[1][1] = iVar1 >> 9;
      iVar1 = m1.m[1][2] * 0x140;
      if (iVar1 < 0) {
        iVar1 += 0x1ff;
      }
      m2.m[1][2] = iVar1 >> 9;
      EmitStaticActorMeshList(-1, &m1, p2);
    } else {
      EmitStaticActorMeshList(-1, (MATRIX *)D_80076DE4,
                              (MATRIX *)((char *)D_80076DE4 - 0x14));
    }
  }
  DrawSync(0);
  if (g_nDeathRespawnPending != 0) {
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
  PutDispEnv((DISPENV *)((char *)g_pActiveFrameDrawEnv + 0x5c));
  PutDrawEnv(g_pActiveFrameDrawEnv);
  DrawOTag(LinkOTPrimitives(0x800));
}
