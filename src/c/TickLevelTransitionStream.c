#include "camera.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "match_asm.h"
#include "spyro_block.h"
#include "spyro_state.h"
#include "vecmath.h"

typedef struct {
  int w[5];
} Words5;
typedef struct {
  int *path;
  int pad[3];
  int reverse;
} PortalActor;

#define MESH g_anWorldRenderMeshBlock
#define CAM g_anCameraPosBlock
#define SPYB ((unsigned char *)g_anSpyroWorldPosBlock)

/* Offsets from the held Spyro pointers below: pe is the euler-Z byte, pyaw
   the body-yaw word; both reach the rest of the block relative to themselves.
 */
#define PE_TO(word) ((word) * 4 - SPYB_EULER_Z)
#define PE_WORD(word) (*(int *)(pe + PE_TO(word)))
#define PYAW_BACK_TO(byte) (SPY_BODY_YAW * 4 - (byte))
#define DRAWBUF_B (g_apDrawBufBBlock[0])
#define LEVEL_SPLASH_OFFSET(i) ((&g_nLevelSplashOffset)[(i) * 4])
#define LEVEL_SPLASH_SIZE(i) ((&g_nLevelSplashByteSize)[(i) * 4])
#define LEVEL_ARCHIVE_OFFSET(i) ((&g_nLevelArchiveOffset)[(i) * 4])
#define PORTAL_ENTRY(i) ((int *)g_apPortals[i])

/* 0x80015370 (0x1074): the level-transition loader. Each tick (once the
   drive is idle past the first two stages) it runs one stage: preserve the
   mesh chunks that must survive the swap, pick the new level's intro index,
   read the splash, the archive header, the TIM/audio chunk (uploading the
   TIMs to VRAM and the sample bank to SPU RAM), the world bundle and its ext
   chunk, relocate the moby meshes and the path table, wait for Spyro to face
   the scripted heading, then build the level runtime and place Spyro and the
   camera at the spawn point. */
void TickLevelTransitionStream(int full) {
  RECT rect;
  int pad[8];
  int size;
  int i;

  if (g_nCdStreamState > 1) {
    RestartStalledCdRead();
    if (*(volatile int *)g_anCdReadInFlightBlock != 0) {
      return;
    }
    if (CallCdSync(1, 0) != 2) {
      return;
    }
    if (!(g_dwMusicStreamStatus & 0x40)) {
      return;
    }
  }

  switch (g_nCdStreamState) {
  case 0: {
    int *p;
    char *base;
    int n;
    int k;
    char *src;

    StopAllSoundExceptMask(0);
    if (g_nEnteredPortalIdx >= 0) {
      char *rec;

      rec = *(char **)g_apPortals[g_nEnteredPortalIdx];
      src = rec + 0x14;
      size = *(int *)src + 0x400;
      p = (int *)((char *)g_pRenderScratchRegionBase - size);
      CopyWords(p, rec, size);
      p += 5;
      base = (char *)p;
      p++;
      MESH[4] = *p++;
      n = *p++;
      MESH[1] = (int)p;
      MESH[0] = n;
      for (k = 0; k < MESH[0]; k++) {
        *p = (base + *p) - src;
        p++;
      }
    } else {
      src = (char *)MESH[1] - 0xC;
      size = *(int *)src + 0x400;
      p = (int *)((char *)g_pRenderScratchRegionBase - size);
      CopyWords(p, src, size);
      base = (char *)p;
      p += 2;
      n = *p++;
      MESH[1] = (int)p;
      MESH[0] = n;
      for (k = 0; k < MESH[0]; k++) {
        *p = (base + *p) - src;
        p++;
      }
    }
    p = (int *)((char *)g_pRenderScratchRegionBase - size -
                g_nLevelSampleBankCount);
    CopyWords(p, g_pLevelSampleBankHeader, g_nLevelSampleBankCount);
    RelocateLevelSampleBank(p, 0);
    g_nCameraFloorSurfaceType = -1;
    g_nCdStreamState = 2;
  } break;
  case 1:
    StopAllSoundExceptMask(0);
    g_nWorldRenderMeshChunkCount = 0;
    g_nCameraFloorSurfaceType = -1;
    g_nCdStreamState++;
  case 2: {
    int world;
    int intro;

    g_nActiveLevelId = g_nCurrentLevelId;
    ApplyPerLevelGlobalsTable();
    g_nPrevLevelIntroIndex = g_nLevelIntroIndex;
    world = g_nActiveLevelId / 10 - 1;
    g_nActiveWorldIndex = world;
    intro = world * 6 + g_nActiveLevelId % 10;
    g_nLevelIntroIndex = intro;
    if (full) {
      int lba = *(volatile int *)&g_nCdBaseLba;
      void *dst = *(void *volatile *)&g_pSharedLoadBuffer;

      int size = LEVEL_SPLASH_SIZE(intro);
      int off = LEVEL_SPLASH_OFFSET(intro);

      EBB_BREAK();
      StartCdReadAsync(lba, dst, size, off, 0x258);
    }
    g_nCdStreamState++;
  } break;
  case 3: {
    int lba = *(volatile int *)&g_nCdBaseLba;
    int intro = *(volatile int *)&g_nLevelIntroIndex;
    void *dst = *(void *volatile *)&g_pDrawBufB;
    int off = LEVEL_ARCHIVE_OFFSET(intro);

    StartCdReadAsync(lba, dst, 0x800, off, 0x258);
  }
    g_nCdStreamState++;
    break;
  case 4:
    CopyWords(g_anWorldTimAudioChunkOffsetBlock, DRAWBUF_B, 0x1D0);
    {
      int lba = *(volatile int *)&g_nCdBaseLba;
      int intro = *(volatile int *)&g_nLevelIntroIndex;
      void *dst = *(void *volatile *)g_apDrawBufBBlock;
      int off = *(volatile int *)g_anWorldTimAudioChunkOffsetBlock;

      StartCdReadAsync(lba, dst, 0x80000, off + LEVEL_ARCHIVE_OFFSET(intro),
                       0x258);
    }
    g_nEnteredPortalIdx = 0;
    g_nCdStreamState++;
    break;
  case 5:
    rect.x = 0x200;
    rect.w = 0x200;
    rect.h = 0x100;
    rect.y = g_nEnteredPortalIdx << 8;
    LoadImage(&rect, (char *)DRAWBUF_B + (g_nEnteredPortalIdx << 18));
    if (++g_nEnteredPortalIdx == 2) {
      int lba = *(volatile int *)&g_nCdBaseLba;
      void *dst = DRAWBUF_B;
      int size = *(volatile int *)&g_nWorldTimAudioChunkSize;
      int intro = *(volatile int *)&g_nLevelIntroIndex;
      int off = *(volatile int *)&g_nWorldTimAudioChunkOffset;

      g_nCdStreamState++;

      StartCdReadAsync(lba, dst, size - 0x80000,
                       off + LEVEL_ARCHIVE_OFFSET(intro) + 0x80000, 0x258);
    }
    break;
  case 6:
    SetSpuTransferStartAddr(0x1010);
    WriteSpuRam(g_pDrawBufB, 0x7EFF0);
    g_nCdStreamState++;
    break;
  case 7:
    if (GetSpuTransferStatus(0) != 0) {
      g_nCdStreamState++;
    }
    break;
  case 8: {
    int lba = *(volatile int *)&g_nCdBaseLba;
    void *dst = *(void *volatile *)&g_pDrawBufB;
    int intro = *(volatile int *)&g_nLevelIntroIndex;
    int size = *(volatile int *)&g_nWorldBundleChunkSize;
    int off = *(volatile int *)&g_nWorldBundleChunkOffset;

    StartCdReadAsync(lba, dst, size, off + LEVEL_ARCHIVE_OFFSET(intro), 0x258);
  }
    g_nCdStreamState++;
    break;
  case 9:
    g_pWorldUnpackHead = UnpackWorldDataChunks(g_pDrawBufB, 0);
    if (MESH[0] != 0 && g_nGamestate != GS_GAME_OVER &&
        g_nGamestate != GS_ACTOR_CUTSCENE) {
      if (g_nReturnPortalLevelId != 0) {
        for (i = 0; i < g_nPortalCount; i++) {
          int *e = PORTAL_ENTRY(i);

          if (e[7] == g_nReturnPortalLevelId) {
            *(Words5 *)MESH = *(Words5 *)e[0];
            break;
          }
        }
      } else {
        *(Words5 *)MESH = *(Words5 *)&g_nWorldCollisionTriCount;
      }
    }
    {
      int lba = *(volatile int *)&g_nCdBaseLba;
      void *dst = *(void *volatile *)&g_pWorldUnpackHead;
      int intro = *(volatile int *)&g_nLevelIntroIndex;
      int size = *(volatile int *)&g_nWorldDataExtChunkSize;
      int off = *(volatile int *)&g_nWorldDataExtChunkOffset;

      StartCdReadAsync(lba, dst, size, off + LEVEL_ARCHIVE_OFFSET(intro),
                       0x258);
    }
    g_nCdStreamState++;
    break;
  case 10: {
    int *list;
    short *slot;
    void **mesh;

    if (full) {
      RelocateMobActorTable(
          (char *)g_pWorldUnpackHead +
          (g_anActorMeshOffsetList - g_nWorldDataExtChunkOffset));
    }
    for (i = 0; i < 0x200; i++) {
      if (((int)g_apActorMeshTableBlock[i] & 0xFFFFFF) <
          ((int)g_apWorldUnpackHeadBlock[7] & 0xFFFFFF)) {
        g_apActorMeshTableBlock[i] = 0;
      }
    }
    i = 1;
    mesh = g_apActorMeshTableBlock;
    list = g_anActorMeshOffsetBlock;
    slot = ACTOR_MESH_INDEX_LIST(list) + 1;
    for (; i < 0x40; i++) {
      if (list[i] <= 0) {
        break;
      }
      mesh[*slot++] = RelocateActorMeshHeader((char *)g_pWorldUnpackHead +
                                              (list[i] - list[-16]));
    }
    g_pPathTableBuffer = (char *)g_pWorldUnpackHead + list[-15];
    g_nLevelArchiveByteSize = list[-13];
    g_nLevelArchiveBaseLba = list[-14];
    g_nCdStreamState++;
  } break;
  case 11: {
    int lba = *(volatile int *)&g_nCdBaseLba;
    void *dst = *(void *volatile *)&g_pPathTableBuffer;
    int intro = *(volatile int *)&g_nLevelIntroIndex;
    int size = *(volatile int *)&g_nLevelArchiveByteSize;
    int off = *(volatile int *)&g_nLevelArchiveBaseLba;

    StartCdReadAsync(lba, dst, size, off + LEVEL_ARCHIVE_OFFSET(intro), 0x258);
  }
    g_nCdStreamState++;
    break;
  case 12: {
    int d;

    if (g_nScriptedRespawnFlag == 0) {
      g_nCdStreamState++;
      break;
    }
    if (g_nLevelIntroParallaxCountdown != 0) {
      break;
    }
    if (g_nReturnPortalLevelId != 0) {
      d = (g_nSpyroBodyYaw + g_nCameraYaw) & 0xFFF;
      if (d > 0x800) {
        d -= 0x1000;
      }
      if (abs(d) < 0x80) {
        g_nCdStreamState++;
      }
    } else {
      d = (g_abSpyroPersistentEuler[2] - g_abSpyroRequestEuler[2]) & 0xFF;
      if (d > 0x80) {
        d -= 0x100;
      }
      if (abs(d) < 0x10) {
        g_nCdStreamState++;
      }
    }
  } break;
  case 13: {
    if (g_abLevelVisitedFlag[g_nLevelIntroIndex] == 0) {
      g_nCdMusicStreamTimeout = 0xA8C0;
    } else {
      g_nCdMusicStreamTimeout = 0x7080;
    }
    g_nCurrentMusicTrack = (&g_nLevelMusicTrack)[g_nLevelIntroIndex];
    g_abLevelVisitedFlag[g_nLevelIntroIndex] = 1;
    FillWord(g_anSpawnAnchorBlock, 0, 0x68);
    g_nLevelEggCount = 0;
    g_nGemTallyIconCount = 0;
    g_nLevelGemsAtEntry = g_anLevelGemsCollected[g_nLevelIntroIndex];
    BuildLoadedLevelRuntime(full);
    if (g_nScriptedRespawnFlag != 0) {
      if (g_nFlightLevelActive != 0) {
        g_anSpyroStateFlagsBlock[0] = 10;
        ComputeCameraToTargetAngle(g_anCameraPos);
        g_nCameraYaw += g_nSpyroBodyYaw;
        CopyVector(&g_anSpyroStateFlagsBlock[-31], &g_anSpawnAnchorBlock[0x14]);
        *(CameraAngles *)&g_nCameraSpringYaw = *(CameraAngles *)&g_nCameraYaw;
        g_abSpyroPersistentEuler[2] = g_nLevelSpawnYaw;
        g_nSpyroBodyYaw = (g_nLevelSpawnYaw & 0xFF) << 4;
        g_nCameraSpringYaw -= SPYB[SPYB_EULER_Z] << 4;
        ComputeCameraOrbitOffset(g_anCameraPos);
        AddVector(g_anCameraPos, g_anCameraPos, &g_anSpyroStateFlagsBlock[-31]);
        UpdateCameraEulerAngles();
        CopyVector(g_anSpyroDeathCamRecordBlock, g_anCameraPos);
        ActivateScriptedCameraView(
            (int)(g_anSpyroDeathCamRecordBlock - (0x18 / 4)));
        g_nGamestate = GS_ARRIVAL_CAMERA;
      } else if (g_nReturnPortalLevelId != 0) {
        for (i = 0; i < g_nPortalCount; i++) {
          int *e = PORTAL_ENTRY(i);
          unsigned char *pe = SPYB + SPYB_EULER_Z;
          int *pyaw = (int *)(SPYB + SPY_BODY_YAW * 4);
          int *cam = CAM;
          int *spring = CAM + 0x14;

          if (e[7] == g_nReturnPortalLevelId) {
            unsigned char yaw;
            PortalActor *actor;

            yaw = *pe;
            actor =
                *(PortalActor **)((char *)g_pActorListBase + e[6] * ACTOR_SIZE);
            EBB_BREAK();
            ChangeSpyroState(SPYRO_ST_GLIDE);
            PE_WORD(SPY_STATE_FLAGS) = 9;
            ComputeCameraToTargetAngle(cam);
            g_anCameraSpringYawBlock[6] = g_nCameraYaw + PE_WORD(SPY_BODY_YAW);
            if (actor->reverse != 0) {
              CopyVector((int *)(pe - SPYB_EULER_Z), actor->path + 2);
              CopyVector((int *)(pe + PE_TO(SPY_REQUEST_TARGET)),
                         actor->path + 6);
              *pe = ArcTan2(actor->path[6] - actor->path[2],
                            actor->path[7] - actor->path[3], 0);
            } else {
              CopyVector((int *)(pe - SPYB_EULER_Z), actor->path + 6);
              CopyVector((int *)(pe + PE_TO(SPY_REQUEST_TARGET)),
                         actor->path + 2);
              *pe = ArcTan2(actor->path[2] - actor->path[6],
                            actor->path[3] - actor->path[7], 0);
            }
            *(CameraAngles *)spring = ((CameraAngles *)spring)[1];
            *pyaw = *((unsigned char *)pyaw - PYAW_BACK_TO(SPYB_EULER_Z)) << 4;
            *spring -= *((unsigned char *)pyaw - PYAW_BACK_TO(SPYB_EULER_Z))
                       << 4;
            ComputeCameraOrbitOffset(cam);
            AddVector(cam, cam, (int *)((char *)pyaw - PYAW_BACK_TO(0)));
            UpdateCameraEulerAngles();
            *((unsigned char *)pyaw - PYAW_BACK_TO(SPYB_OT_BIN_BIAS)) = 0x7F;
            g_nPortalViewPitchTilt = 0;
            g_nPortalViewYawSpin =
                (yaw - *((unsigned char *)pyaw - PYAW_BACK_TO(SPYB_EULER_Z)))
                << 4;
            break;
          }
        }
      } else {
        int *yawp;
        int *pos;

        ChangeSpyroState(SPYRO_ST_GLIDE);
        ComputeCameraToTargetAngle(g_anCameraPos);
        yawp = (int *)(SPYB + SPY_BODY_YAW * 4);
        pos = (int *)SPYB;
        g_nCameraYaw += *yawp;
        *(CameraAngles *)&g_nCameraSpringYaw = *(CameraAngles *)&g_nCameraYaw;
        g_abSpyroPersistentEuler[2] = g_nLevelSpawnYaw;
        *yawp = (g_nLevelSpawnYaw & 0xFF) << 4;
        g_nCameraSpringYaw -= SPYB[SPYB_EULER_Z] << 4;
        CopyVector(pos, &g_anSpawnAnchorBlock[0x14]);
        ComputeCameraOrbitOffset(g_anCameraPos);
        ZeroVector(yawp - 4);
        if (g_pCameraTargetParams == g_anCameraMode12Params) {
          g_anSpyroWorldPosZBlock[0] = g_nSpyroWorldPosZ - 0x1600;
          pos[0] -= (g_anCosineLut[g_abSpyroPersistentEuler[2]] * 5) >> 1;
          g_nSpyroWorldPosY -=
              (g_anSineLut[g_abSpyroPersistentEuler[2]] * 5) >> 1;
          AddVector(g_anCameraPos, g_anCameraPos, pos);
          UpdateCameraEulerAngles();
          g_nCameraNextMode = g_abSpyroStateCameraModeTable[g_nSpyroState];
          g_nSpyroStateFlags = 0xB;
          g_nGamestate = GS_GAMEPLAY;
          g_bSpyroOtBinBias = 4;
        } else {
          AddVector(g_anCameraPos, g_anCameraPos, pos);
          UpdateCameraEulerAngles();
          CopyVector(g_anSpyroDeathCamRecordBlock, g_anCameraPos);
          ActivateScriptedCameraView(
              (int)(g_anSpyroDeathCamRecordBlock - (0x18 / 4)));
          g_nSpyroStateFlags = 0xA;
          g_nGamestate = GS_ARRIVAL_CAMERA;
        }
      }
    } else if (g_nFlightLevelActive != 0) {
      g_nSpyroStateFlags = 0;
    } else if (g_nGamestate != GS_ACTOR_CUTSCENE) {
      ChangeSpyroState(SPYRO_ST_IDLE);
    }
    *(Words5 *)&g_nWorldRenderMeshChunkCount =
        *(Words5 *)&g_nWorldCollisionTriCount;
    g_nScriptedRespawnFlag = 0;
    g_nReturnPortalLevelId = 0;
    g_anLevelRunnerArrived = 0;
    g_nLevelRunnerArrivedGoal1 = 0;
    g_anLevelEscapeRunCount = 0;
    g_nLevelEscapeRunCountLane1 = 0;
    g_nLevelUnreadScratchA = 0;
    g_nLevelUnreadScratchB = 0;
    g_bLevelUseShortReleaseAnim = 0;
    g_bLevelBossDefeated = 0;
    g_bLevelPhasedBossKilled = 0;
    g_nCreditsReturnLevelId = -1;
    g_nCdStreamState = -1;
  } break;
  }
}
