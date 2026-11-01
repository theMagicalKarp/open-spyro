#include "cd_stream.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "pad_buttons.h"
#include "spyro_state.h"

/* 0x8002edf0 (1492 bytes) — update for gamestate 5 (GS_GAME_OVER).

   Three arms on g_nRespawnOrGameOverMode. Mode 0 is the load frame: after 16
   frames it either hands straight back to gameplay (when the death happened
   outside a game-over, g_nGamestate == GS_RESPAWN) or streams the game-over
   scene in behind the render scratch region, rebases its chunk pointer table,
   parks the camera on the fixed game-over shot and resets Spyro's
   inventory/anim state. Mode 1 plays the presentation: frames 0xb4..0x174 sweep
   Spyro around the dragon on a cosine path, then he lands and the "press start"
   window opens. Mode 2 waits out the level-transition stream and returns to
   gameplay.

   The game-over scene's byte size is read TWICE off one held base for the
   CdReadSyncSectors call, and the second read must be a VOLATILE load routed
   through the loop counter `i`: cse merges the two reads in every non-volatile
   form, and a single-set destination would be boosted to max priority by
   sched.c's birthing_insn_p and sink below the second base's `la`. */

void RespawnOrGameOver_Update(void) {
  int unused[8];
  int mode;
  if (g_nRespawnOrGameOverFrame == 0) {
    StopAllSoundExceptMask(0);
  }
  g_nRespawnOrGameOverFrame += 1;
  RotateLightVectorXZ(3);
  mode = g_nRespawnOrGameOverMode;
  if (mode == 0) {
    if (g_nRespawnOrGameOverFrame >= 0x10) {
      if (g_nGamestate == GS_RESPAWN) {
        EndRespawnToGameplay();
        LoadAndStartLevelFromCd();
      } else {
        volatile int *size = g_anGameOverSceneBlock;
        int lba;
        unsigned char *scene;
        unsigned char *p;
        int count;
        unsigned char *fog;
        int n;
        int i;
        lba = g_nCdBaseLba;
        i = *size;
        CdReadSyncSectors(lba, g_apRenderScratchRegionBlock[0] - (*size), i,
                          g_nGameOverSceneSectorOffset, 0x258);
        p = g_apRenderScratchRegionBlock[0] - (*size);
        fog = (unsigned char *)(&g_anWorldRenderMeshBlock[4]);
        fog[0] = p[0];
        fog[1] = p[1];
        fog[2] = p[2];
        scene = p;
        g_abFrameDrawEnv0.r0 = fog[0];
        g_abFrameDrawEnv0.g0 = fog[1];
        g_abFrameDrawEnv1.r0 = fog[0];
        g_abFrameDrawEnv1.g0 = fog[1];
        g_abFrameDrawEnv0.b0 = fog[2];
        g_abFrameDrawEnv1.b0 = fog[2];
        p += 4;
        count = *((int *)p);
        p += 4;
        g_anWorldRenderMeshBlock[1] = (int)p;
        g_anWorldRenderMeshBlock[0] = count;
        for (i = 0; i < g_anWorldRenderMeshBlock[0]; i++) {
          *((int *)p) = ((int)scene) + (*((int *)p));
          p += 4;
        }

        g_anCameraPos[0] = 0x2800;
        g_anCameraPos[1] = 0x80;
        g_nCameraEulerRoll = 0;
        g_anCameraPos[2] = 0x800;
        g_nCameraEulerPitch = g_anGameOverCameraEuler[0];
        g_nCameraEulerYaw = g_anGameOverCameraEuler[1];
        ResetSpyroEntity(1);
        g_bSpyroAnimPrev = 0x10;
        g_bSpyroAnimCurrent = 0x10;
        g_bSpyroAnimFrame = 1;
        g_nExtraLives = 4;
        g_nSparxFoodCount = 0;
        g_nSparxHealth = SPARX_GOLD;
        g_nPortalTransitionActive = 0;
        g_nReturnPortalLevelId = 0;
        g_nGameOverLevelIndex = g_nLevelIndex;
        FillWord(&g_nSpawnAnchorValid, 0, 0x68);
        if (g_nActiveLevelId != (((lba = g_nActiveLevelId) / 10) * 10)) {
          g_nCurrentLevelId = (g_nActiveLevelId / 10) * 10;
          g_nCdStreamState = LSTREAM_SELECT_LEVEL;
          g_nEnteredPortalIdx = -1;
        } else {
          g_nCdStreamState = LSTREAM_READ_PATHS;
        }
        g_nRespawnOrGameOverMode = 1;
        g_nRespawnOrGameOverFrame = 0;
      }
    }
  } else if (mode == 1) {
    int t = g_nRespawnOrGameOverFrame - 0xb4;
    if (((unsigned int)t) < 0xc0) {
      int angle = ((t * 16) + 0xe00) & 0xfff;
      int swing = LookupCosine(angle);
      int radius = 0x87c - ((t * 1086) / 192);
      g_anSpyroWorldPos[0] = ((swing * radius) >> 12) + 0x2a00;
      g_anSpyroWorldPos[1] = ((LookupSine(angle) * radius) >> 12) + 0xc00;
      g_anSpyroWorldPos[2] = 0xb44 - (t * 6);
      g_abSpyroPersistentEuler[2] = (angle + 0x400) >> 4;
      g_abSpyroPersistentEuler[1] = 0;
      g_abSpyroPersistentEuler[0] = (t / 12) - 0x20;
      if (g_nRespawnOrGameOverFrame < 0x164) {
        AdvanceSpyroAnimFrame(3);
      } else {
        g_bSpyroAnimCurrent = 0xe;
        g_bSpyroAnimFrame = 0;
        g_bSpyroSubFrameTimer = g_nRespawnOrGameOverFrame - 0x64;
        g_abSpyroPersistentEuler[1] =
            -(((g_nRespawnOrGameOverFrame - 0x164) * 10) / 16);
        g_abSpyroPersistentEuler[0] = g_nRespawnOrGameOverFrame - 0x74;
      }
      TickSpyroAnimLayer1();
      TickSpyroAnimLayer2();
    }
    if (g_nRespawnOrGameOverFrame >= 0x174) {
      if (g_nRespawnOrGameOverFrame == 0x174) {
        g_bSpyroAnimPrev = 0xe;
        g_bSpyroAnimCurrent = 0xe;
        g_bSpyroFramePrev = 0;
        g_bSpyroAnimFrame = 1;
        g_bSpyroSubFrameTimer = 0;
      }
      g_anSpyroWorldPos[0] = 0x26f8;
      g_anSpyroWorldPos[1] = 0x900;
      g_anSpyroWorldPos[2] = 0x6c4;
      g_abSpyroPersistentEuler[1] = 0xf6;
      g_abSpyroPersistentEuler[0] = 0;
      g_abSpyroPersistentEuler[2] = 0xe0;
      AdvanceSpyroAnimFrame(3);
      TickSpyroAnimLayer1();
      TickSpyroAnimLayer2();
    }
    if (g_nCdStreamState < LSTREAM_READ_PATHS) {
      TickLevelTransitionStream(1);
    } else if ((g_nRespawnOrGameOverFrame >= 0x169) &&
               ((g_dwPadHeld & PAD_START) != 0)) {
      RestartSpyroAnimWithState(SPYRO_ST_IDLE);
      g_nRespawnOrGameOverMode = 2;
      g_nRespawnOrGameOverFrame = 0;
    }
  } else if (g_nRespawnOrGameOverFrame >= 0x11) {
    TickLevelTransitionStream(1);
    if (g_nCdStreamState < LSTREAM_PRESERVE) {
      EndRespawnToGameplay();
    }
  }
}
