#include "globals.h"

/* 0x8002edf0 (1492 bytes) — update for gamestate 5 (GS_GAME_OVER).

   Three arms on g_nGameplayDrawMode. Mode 0 is the load frame: after 16 frames
   it either hands straight back to gameplay (when the death happened outside a
   game-over, g_nGamestate == GS_IN_LEVEL) or streams the game-over scene in
   behind the render scratch region, rebases its chunk pointer table, parks the
   camera on the fixed game-over shot and resets Spyro's inventory/anim state.
   Mode 1 plays the presentation: frames 0xb4..0x174 sweep Spyro around the
   dragon on a cosine path, then he lands and the "press start" window opens.
   Mode 2 waits out the level-transition stream and returns to gameplay.

   The game-over scene's byte size is read TWICE off one held base for the
   CdReadSyncSectors call, and the second read must be a VOLATILE load routed
   through the loop counter `i`: cse merges the two reads in every non-volatile
   form, and a single-set destination would be boosted to max priority by
   sched.c's birthing_insn_p and sink below the second base's `la`. */
extern void StopAllSoundExceptMask(int owner_key);
extern void RotateLightVectorXZ(int steps);
extern void EndRespawnToGameplay(void);
extern void LoadAndStartLevelFromCd(void);
extern void CdReadSyncSectors(int lba, void *dst, int byte_count,
                              int sector_offset, int marker);
extern void ResetSpyroEntity(int mode);
extern void FillWord(void *dst, unsigned int value, int byte_count);
extern int LookupCosine(unsigned int angle_12_4);
extern int LookupSine(unsigned int angle_12_4);
extern void AdvanceSpyroAnimFrame(char layer_mask);
extern void TickSpyroAnimLayer1(void);
extern void TickSpyroAnimLayer2(void);
extern void TickLevelTransitionStream(int mode);
extern void RestartSpyroAnimWithState(void *state);

extern int g_anWorldRenderMeshBlock[]; /* 0x80078a40: [0] chunk count, [1] chunk
                                          array, fog colour bytes at [4] */
extern volatile int g_anGameOverSceneBlock[]; /* g_nGameOverSceneByteSize */
extern unsigned char
    *g_apRenderScratchRegionBlock[]; /* g_pRenderScratchRegionBase */

void RespawnOrGameOver_Update(void) {
  int unused[8];
  int mode;
  if (g_nGameplayDrawFrame == 0) {
    StopAllSoundExceptMask(0);
  }
  g_nGameplayDrawFrame += 1;
  RotateLightVectorXZ(3);
  mode = g_nGameplayDrawMode;
  if (mode == 0) {
    if (g_nGameplayDrawFrame >= 0x10) {
      if (g_nGamestate == 4) {
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
        g_nLevelReadyFlag = 3;
        g_nScriptedRespawnFlag = 0;
        g_nScriptedMobTag = 0;
        g_nGameOverLevelIntroIndex = g_nLevelIntroIndex;
        FillWord(&g_nSpawnAnchorValid, 0, 0x68);
        if (g_nActiveLevelId != (((lba = g_nActiveLevelId) / 10) * 10)) {
          g_nCurrentLevelId = (g_nActiveLevelId / 10) * 10;
          g_nCdStreamState = 2;
          g_nPreserveMobIdx = -1;
        } else {
          g_nCdStreamState = 0xb;
        }
        g_nGameplayDrawMode = 1;
        g_nGameplayDrawFrame = 0;
      }
    }
  } else if (mode == 1) {
    int t = g_nGameplayDrawFrame - 0xb4;
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
      if (g_nGameplayDrawFrame < 0x164) {
        AdvanceSpyroAnimFrame(3);
      } else {
        g_bSpyroAnimCurrent = 0xe;
        g_bSpyroAnimFrame = 0;
        g_bSpyroSubFrameTimer = g_nGameplayDrawFrame - 0x64;
        g_abSpyroPersistentEuler[1] =
            -(((g_nGameplayDrawFrame - 0x164) * 10) / 16);
        g_abSpyroPersistentEuler[0] = g_nGameplayDrawFrame - 0x74;
      }
      TickSpyroAnimLayer1();
      TickSpyroAnimLayer2();
    }
    if (g_nGameplayDrawFrame >= 0x174) {
      if (g_nGameplayDrawFrame == 0x174) {
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
    if (g_nCdStreamState < 0xb) {
      TickLevelTransitionStream(1);
    } else if ((g_nGameplayDrawFrame >= 0x169) &&
               ((g_dwPadHeld & 0x800) != 0)) {
      RestartSpyroAnimWithState(0);
      g_nGameplayDrawMode = 2;
      g_nGameplayDrawFrame = 0;
    }
  } else if (g_nGameplayDrawFrame >= 0x11) {
    TickLevelTransitionStream(1);
    if (g_nCdStreamState < 0) {
      EndRespawnToGameplay();
    }
  }
}
