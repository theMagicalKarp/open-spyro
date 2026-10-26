#include "frame_db.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"

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
    env += sizeof(FrameDB);
  }
  otbase = FRAME_DB(env)->otDepthBinBase;
  otslot = FRAME_DB(env)->otMergedChainSlot;
  cursor = FRAME_DB(env)->primBufferBase;
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

  if (g_nGamestate != GS_GAMEPLAY) {
    if (g_nGamestate == GS_LEVEL_LOAD) {
      LevelTransition_Draw();
    } else if (g_nGamestate == GS_PAUSE_MENU) {
      Gamestate02_03_06_Draw();
    } else if (g_nGamestate == GS_INVENTORY) {
      Gamestate02_03_06_Draw();
    } else if (g_nGamestate == GS_RESPAWN) {
      RespawnOrGameOver_Draw();
    } else if (g_nGamestate == GS_GAME_OVER) {
      RespawnOrGameOver_Draw();
    } else if (g_nGamestate == GS_OVERLAY_SCREEN) {
      Gamestate02_03_06_Draw();
    } else if (g_nGamestate == GS_OVERLAY_HOOK) {
      (*(void (*)())g_pfnGamestate7DrawHook)();
    } else if (g_nGamestate == GS_DRAGON_RESCUE) {
      DragonRescue_Draw();
    } else if (g_nGamestate == GS_ARRIVAL_CAMERA) {
      LevelTransition_Draw();
    } else if (g_nGamestate == GS_MENU_TRANSITION) {
      MenuTransition_Draw();
    } else if (g_nGamestate == GS_SAVE_MENU) {
      SaveLoadMenu_Draw();
    } else if (g_nGamestate == GS_ACTOR_CUTSCENE) {
      ActorCutscene_Draw();
    } else if (g_nGamestate == GS_TITLE) {
      if (g_nTitleMode == 3) {
        StoryIntro_Draw();
      } else {
        Titlescreen_Draw();
      }
    } else if (g_nGamestate == GS_CUTSCENE) {
      Cutscene_Draw();
    } else if (g_nGamestate == GS_CREDITS) {
      if (g_nCreditsMode < 0x63) {
        Credits_Draw();
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
      EnqueueHudSprites();
    }
    if (g_nDemoMode != 0) {
      DrawDemoModeOverlay();
    }
    ComposeFrameScene();
    SetupFrameOT();
    DrawPortalsAndStaticActors();
    RasterizeEmitList();
    if (g_nScreenFadeLevel != 0) {
      tint = g_nScreenFadeLevel << 3;
      DrawFullscreenTint(2, tint, tint, tint);
    }
    if (g_nGameplayBlocked != 0 || g_nLetterboxBarHeight != 0) {
      DrawCinematicLetterbox();
    }
    DrawMotionTrailRibbons();

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
    DrawOTag(LinkOTPrimitives(0x800));
  }
}
