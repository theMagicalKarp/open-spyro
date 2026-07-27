#include "globals.h"

/* Per-frame top-level draw dispatcher (0x8001ED5C, 0x3fc).

   Flips to the other of the two frame environments (they sit 0x84 apart, so
   the second is derived from the first) and republishes the per-frame buffer
   cursors held in its tail: the prim-buffer write cursor (+0x70) with its
   0x1C000-byte limit, the OT depth-bin array (+0x74) and the active depth slot
   (+0x78). The sprite-record buffer starts where the prim buffer ends. Then
   the camera view matrix, and the active gamestate's Draw handler.

   State 0 is the gameplay frame: push the world fog color into both frame
   DRAWENVs, build the render entity lists, overlay the loading-screen sprites
   (skipped in flight levels) and the demo-mode overlay, compose the scene,
   then the fullscreen tint while the generic countdown runs and the cinematic
   letterbox while gameplay is blocked. Ends with the standard 2-vblank frame
   submit. */
extern void BuildCameraViewMatrix(void);
extern void Gamestate01_09_Draw(void);
extern void Gamestate02_03_06_Draw(void);
extern void RespawnOrGameOver_Draw(void);
extern void Gamestate08_Draw(void);
extern void Gamestate0A_Draw(void);
extern void SaveLoadMenu_Draw(void);
extern void Gamestate0C_Draw(void);
extern void StoryIntro_Draw(void);
extern void func_8007CEE4(void); /* titlescreen overlay draw */
extern void GemCutscene_Draw(void);
extern void func_8007BFD0(void); /* credits overlay draw */
extern void ReturningHome_Draw(void);
extern void BuildRenderEntityLists(void);
extern void EnqueueLoadingScreenSprites(void);
extern void DrawDemoModeOverlay(void);
extern void ComposeFrameScene(void);
extern void SetupFrameOT(void);
extern void DrawActors(void);
extern void RasterizeEmitList(void);
extern void DrawFullscreenTint(int mode, int r, int g, int b);
extern void DrawCinematicLetterbox(void);
extern void DrawMotionTrailRibbons(void);
extern void DrawSync(int mode);
extern int VSync(int mode);
extern DISPENV *PutDispEnv(DISPENV *env);
extern void PutDrawEnv(void *env);
extern void *LinkOTPrimitives(int depth_max);
extern void DrawOTag(void *ot);

extern int g_anVsyncFrameEndBlock[]; /* held-base alias: [-1] = pace anchor */

void GamestateDraw(void) {
  char *env;
  void *otbase;
  int *otslot;
  char *cursor;
  char *limit;
  int r;
  int g;
  int b;
  int tint;

  env = (char *)&g_abFrameDrawEnv0;
  if ((char *)g_pActiveFrameDrawEnv == env) {
    env += 0x84;
  }
  otbase = *(void **)(env + 0x74);
  otslot = *(int **)(env + 0x78);
  cursor = *(char **)(env + 0x70);
  g_pActiveFrameDrawEnv = (DRAWENV *)env;
  g_nPrimBufferOverflowFlag = 0;
  g_pPrimBufferWriteCursor = cursor;
  limit = cursor + 0x1C000;
  g_pOtDepthBinArrayBase = otbase;
  g_pOtActiveDepthSlot = otslot;
  g_pPrimBufferLimit = limit;
  g_pSpriteRecordBufferTop = limit;
  g_pSpriteRecordWriteCursor = limit;
  BuildCameraViewMatrix();

  if (g_nGamestate != 0) {
    if (g_nGamestate == 1) {
      Gamestate01_09_Draw();
    } else if (g_nGamestate == 2) {
      Gamestate02_03_06_Draw();
    } else if (g_nGamestate == 3) {
      Gamestate02_03_06_Draw();
    } else if (g_nGamestate == 4) {
      RespawnOrGameOver_Draw();
    } else if (g_nGamestate == 5) {
      RespawnOrGameOver_Draw();
    } else if (g_nGamestate == 6) {
      Gamestate02_03_06_Draw();
    } else if (g_nGamestate == 7) {
      (*(void (*)())g_pfnGamestate7DrawHook)();
    } else if (g_nGamestate == 8) {
      Gamestate08_Draw();
    } else if (g_nGamestate == 9) {
      Gamestate01_09_Draw();
    } else if (g_nGamestate == 0xA) {
      Gamestate0A_Draw();
    } else if (g_nGamestate == 0xB) {
      SaveLoadMenu_Draw();
    } else if (g_nGamestate == 0xC) {
      Gamestate0C_Draw();
    } else if (g_nGamestate == 0xD) {
      if (g_nGamestate0dMode == 3) {
        StoryIntro_Draw();
      } else {
        func_8007CEE4();
      }
    } else if (g_nGamestate == 0xE) {
      GemCutscene_Draw();
    } else if (g_nGamestate == 0xF) {
      if (g_nGamestate0fMode < 0x63) {
        func_8007BFD0();
      } else {
        ReturningHome_Draw();
      }
    }
  } else {
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
    if (g_nFlightLevelActive == 0) {
      EnqueueLoadingScreenSprites();
    }
    if (g_nDeathState != 0) {
      DrawDemoModeOverlay();
    }
    ComposeFrameScene();
    SetupFrameOT();
    DrawActors();
    RasterizeEmitList();
    if (g_nGenericCountdown != 0) {
      tint = g_nGenericCountdown << 3;
      DrawFullscreenTint(2, tint, tint, tint);
    }
    if (g_nGameplayBlocked != 0 || g_nLetterboxBarHeight != 0) {
      DrawCinematicLetterbox();
    }
    DrawMotionTrailRibbons();

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
}
