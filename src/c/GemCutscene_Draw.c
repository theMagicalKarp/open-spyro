#include "globals.h"

extern void BuildRenderEntityLists(void);
extern void BuildActorDrawList(void);
extern void FillWord(void *dst, unsigned int value, int byte_count);
extern void EmitActorDrawList(void);
extern void RenderWorldChunks(int lod);
extern void EmitStaticActorMeshList(int lod, int *viewMtx, int *worldMtx);
extern void DrawFullscreenTint(int slot, int r, int g, int b);
extern void DrawSync(int mode);
extern int VSync(int mode);
extern DISPENV *PutDispEnv(DISPENV *env);
extern void PutDrawEnv(void *env);
extern void *LinkOTPrimitives(int depth_max);
extern void DrawOTag(void *ot);

extern int D_800785D0;   /* per-gamestate far-plane cull limit */
extern int D_80076DE4[]; /* camera view matrix (worldToCam at -0x14) */
extern int g_anVsyncFrameEndBlock[]; /* held-base alias: [-1] = pace anchor */

/* Draw gamestate 0xE (GS_GEM_CUTSCENE): the collected gem shown in 3D with a
   g_nGenericCountdown fade-to-black. Copies the world fog color into both frame
   DRAWENVs, renders the actor + world passes, tints, then the standard 2-vblank
   frame submit (0x8001e9c8). */
void GemCutscene_Draw(void) {
  int r;
  int g;
  int b;
  int *ot;
  void *otHead;

  r = ((unsigned char *)&g_dwWorldFogColor)[0];
  g = ((unsigned char *)&g_dwWorldFogColor)[1];
  b = ((unsigned char *)&g_dwWorldFogColor)[2];
  g_abFrameDrawEnv0.r0 = r;
  g_abFrameDrawEnv0.g0 = g;
  g_abFrameDrawEnv0.b0 = b;
  g_abFrameDrawEnv1.r0 = r;
  g_abFrameDrawEnv1.g0 = g;
  g_abFrameDrawEnv1.b0 = b;
  BuildRenderEntityLists();
  BuildActorDrawList();
  otHead = &g_pOtDepthBinHead0;
  FillWord(otHead, 0, 0x900);
  EmitActorDrawList();
  FillWord(otHead, 0, 0x1c00);
  D_800785D0 = 0x14000;
  RenderWorldChunks(-1);
  EmitStaticActorMeshList(-1, D_80076DE4, (int *)((char *)D_80076DE4 - 0x14));
  if (g_nGenericCountdown != 0) {
    int t = g_nGenericCountdown << 4;
    DrawFullscreenTint(2, t, t, t);
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
  ot = (int *)LinkOTPrimitives(0x800);
  DrawOTag(ot);
}
