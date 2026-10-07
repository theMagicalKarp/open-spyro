#include "globals.h"

extern void RestartStalledCdRead(void);
extern int FUN_80063bd8(); /* CdSync */
extern void StartCdReadAsync(int lbaBase, void *dst, int size, int offset,
                             int marker);
extern void StopAllSoundExceptMask(int mask);
extern void CopyWords(void *dst, void *src, int byte_count);
extern void RelocateLevelSampleBank(void *bank, int mode);
extern void ApplyPerLevelGlobalsTable(void);
extern void LoadImage(RECT *rect, void *data);
extern int SetSpuTransferStartAddr(unsigned int addr);
extern unsigned int WriteSpuRam(void *buf, unsigned int size);
extern long GetSpuTransferStatus(int mode);
extern void *UnpackWorldDataChunks(void *src, int mode);
extern void *RelocateActorMeshHeader(void *header);
extern void RelocateMobActorTable(void *table);
extern void FillWord(void *dst, unsigned int value, int byte_count);
extern void BuildLoadedLevelRuntime(int full);
extern void ChangeSpyroState(int state);
extern void ComputeCameraToTargetAngle(int *pos);
extern void ComputeCameraOrbitOffset(int *out);
extern void AddVector(int *dst, int *a, int *b);
extern void CopyVector(int *dst, int *src);
extern void ZeroVector(int *vec);
extern void UpdateCameraEulerAngles(void);
extern void ActivateScriptedCameraView(int record);
extern int ArcTan2(int y, int x, int high_precision);
extern int abs(int x);

extern int g_anCdReadInFlightBlock[]; /* held-base view of g_nCdReadInFlight */
extern char
    *g_apWorldUnpackHeadBlock[]; /* [0] unpack head, [7] g_pWorkAreaTop */
extern int g_anWorldTimAudioChunkOffsetBlock[];
extern int g_anWorldRenderMeshBlock[]; /* [0] chunk count, [1] chunk array,
                                          [4] g_dwWorldFogColor */
extern int g_anActorMeshOffsetBlock[]; /* [-16] is g_nWorldDataExtChunkOffset */
extern void *g_apActorMeshTableBlock[];
extern int g_anSpawnAnchorBlock[];
extern int g_anCameraPosBlock[]; /* [0x14] spring yaw, [0x1A] camera yaw */
extern int g_anSpyroWorldPosBlock[];
extern int g_anSpyroWorldPosZBlock[]; /* 0x80078a60 */
extern int g_anSpyroStateFlagsBlock[];
extern int g_anCameraSpringYawBlock[];     /* [6] is g_nCameraYaw */
extern int g_anSpyroDeathCamRecordBlock[]; /* 0x8006ebe4 */
extern void *g_apDrawBufBBlock[];
extern int g_nSpyroWorldPosY; /* 0x80078a5c — g_anSpyroWorldPos[1] */
extern int g_nSpyroWorldPosZ; /* 0x80078a60 — g_anSpyroWorldPos[2] */
extern short g_anCosineLut[];
extern unsigned char g_abSpyroStateCameraModeTable[];

extern void *D_800113A0; /* level splash image buffer */
extern int D_80075670;
extern int D_80075674;
extern int D_8007577C;
extern int D_80075838;
extern int D_8007583C;
extern int D_80075850;
extern int D_80075854;
extern int D_80075870;
extern int D_80075874;
extern int D_80075910;
extern int D_8007595C; /* current world index */
extern int D_80076E24;
extern int D_800776CC; /* level-intro deadline */
extern int D_8007A71C; /* splash size column of the level archive table */

typedef struct {
  int w[5];
} Words5;
typedef struct {
  int w[6];
} Words6;
typedef struct {
  int *path;
  int pad[3];
  int reverse;
} MobActor;

#define MESH g_anWorldRenderMeshBlock
#define CAM g_anCameraPosBlock
#define SPYB ((unsigned char *)g_anSpyroWorldPosBlock)
#define DRAWBUF_B (g_apDrawBufBBlock[0])
#define LEVEL_SPLASH_OFFSET(i) ((&g_nLevelSplashOffset)[(i) * 4])
#define LEVEL_SPLASH_SIZE(i) ((&D_8007A71C)[(i) * 4])
#define LEVEL_ARCHIVE_OFFSET(i) ((&g_nLevelArchiveOffset)[(i) * 4])
#define MOB_ENTRY(i) ((int *)(&g_apScriptedMobEntries)[i])

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
    if (FUN_80063bd8(1, 0) != 2) {
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
    if (g_nPreserveMobIdx >= 0) {
      char *rec;

      rec = *(char **)(&g_apScriptedMobEntries)[g_nPreserveMobIdx];
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
    D_80076E24 = -1;
    g_nCdStreamState = 2;
  } break;
  case 1:
    StopAllSoundExceptMask(0);
    g_nWorldRenderMeshChunkCount = 0;
    D_80076E24 = -1;
    g_nCdStreamState++;
  case 2: {
    int world;
    int intro;

    g_nActiveLevelId = g_nCurrentLevelId;
    ApplyPerLevelGlobalsTable();
    g_nPrevLevelIntroIndex = g_nLevelIntroIndex;
    world = g_nActiveLevelId / 10 - 1;
    D_8007595C = world;
    intro = world * 6 + g_nActiveLevelId % 10;
    g_nLevelIntroIndex = intro;
    if (full) {
      int lba = *(volatile int *)&g_nCdBaseLba;
      void *dst = *(void *volatile *)&D_800113A0;

      int size = LEVEL_SPLASH_SIZE(intro);
      int off = LEVEL_SPLASH_OFFSET(intro);

      do {
      } while (0);
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
    g_nPreserveMobIdx = 0;
    g_nCdStreamState++;
    break;
  case 5:
    rect.x = 0x200;
    rect.w = 0x200;
    rect.h = 0x100;
    rect.y = g_nPreserveMobIdx << 8;
    LoadImage(&rect, (char *)DRAWBUF_B + (g_nPreserveMobIdx << 18));
    if (++g_nPreserveMobIdx == 2) {
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
    if (MESH[0] != 0 && g_nGamestate != 5 && g_nGamestate != 0xC) {
      if (g_nScriptedMobTag != 0) {
        for (i = 0; i < g_nScriptedMobCount; i++) {
          int *e = MOB_ENTRY(i);

          if (e[7] == g_nScriptedMobTag) {
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
    slot = (short *)(list + 0x40) + 1;
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
    if (D_80075910 != 0) {
      break;
    }
    if (g_nScriptedMobTag != 0) {
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
      D_800776CC = 0xA8C0;
    } else {
      D_800776CC = 0x7080;
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
        *(Words6 *)&g_nCameraSpringYaw = *(Words6 *)&g_nCameraYaw;
        g_abSpyroPersistentEuler[2] = g_nLevelSpawnYaw;
        g_nSpyroBodyYaw = (g_nLevelSpawnYaw & 0xFF) << 4;
        g_nCameraSpringYaw -= SPYB[0xE] << 4;
        ComputeCameraOrbitOffset(g_anCameraPos);
        AddVector(g_anCameraPos, g_anCameraPos, &g_anSpyroStateFlagsBlock[-31]);
        UpdateCameraEulerAngles();
        CopyVector(g_anSpyroDeathCamRecordBlock, g_anCameraPos);
        ActivateScriptedCameraView(
            (int)(g_anSpyroDeathCamRecordBlock - (0x18 / 4)));
        g_nGamestate = 9;
      } else if (g_nScriptedMobTag != 0) {
        for (i = 0; i < g_nScriptedMobCount; i++) {
          int *e = MOB_ENTRY(i);
          unsigned char *pe = SPYB + 0xE;
          int *pyaw = (int *)(SPYB + 0x11C);
          int *cam = CAM;
          int *spring = CAM + 0x14;

          if (e[7] == g_nScriptedMobTag) {
            unsigned char yaw;
            MobActor *actor;

            yaw = *pe;
            actor = *(MobActor **)((char *)g_pActorListBase + e[6] * 0x58);
            do {
            } while (0);
            ChangeSpyroState(0xF);
            *(int *)(pe + 0x6E) = 9;
            ComputeCameraToTargetAngle(cam);
            g_anCameraSpringYawBlock[6] = g_nCameraYaw + *(int *)(pe + 0x10E);
            if (actor->reverse != 0) {
              CopyVector((int *)(pe - 0xE), actor->path + 2);
              CopyVector((int *)(pe + 0x1EA), actor->path + 6);
              *pe = ArcTan2(actor->path[6] - actor->path[2],
                            actor->path[7] - actor->path[3], 0);
            } else {
              CopyVector((int *)(pe - 0xE), actor->path + 6);
              CopyVector((int *)(pe + 0x1EA), actor->path + 2);
              *pe = ArcTan2(actor->path[2] - actor->path[6],
                            actor->path[3] - actor->path[7], 0);
            }
            *(Words6 *)spring = *(Words6 *)(spring + 6);
            *pyaw = *((unsigned char *)pyaw - 0x10E) << 4;
            *spring -= *((unsigned char *)pyaw - 0x10E) << 4;
            ComputeCameraOrbitOffset(cam);
            AddVector(cam, cam, (int *)((char *)pyaw - 0x11C));
            UpdateCameraEulerAngles();
            *((unsigned char *)pyaw - 0xF5) = 0x7F;
            g_nScriptedMobViewPitchTilt = 0;
            g_nScriptedMobViewYawSpin = (yaw - *((unsigned char *)pyaw - 0x10E))
                                        << 4;
            break;
          }
        }
      } else {
        int *yawp;
        int *pos;

        ChangeSpyroState(0xF);
        ComputeCameraToTargetAngle(g_anCameraPos);
        yawp = (int *)(SPYB + 0x11C);
        pos = (int *)SPYB;
        g_nCameraYaw += *yawp;
        *(Words6 *)&g_nCameraSpringYaw = *(Words6 *)&g_nCameraYaw;
        g_abSpyroPersistentEuler[2] = g_nLevelSpawnYaw;
        *yawp = (g_nLevelSpawnYaw & 0xFF) << 4;
        g_nCameraSpringYaw -= SPYB[0xE] << 4;
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
          g_nGamestate = 0;
          g_bSpyroOtBinBias = 4;
        } else {
          AddVector(g_anCameraPos, g_anCameraPos, pos);
          UpdateCameraEulerAngles();
          CopyVector(g_anSpyroDeathCamRecordBlock, g_anCameraPos);
          ActivateScriptedCameraView(
              (int)(g_anSpyroDeathCamRecordBlock - (0x18 / 4)));
          g_nSpyroStateFlags = 0xA;
          g_nGamestate = 9;
        }
      }
    } else if (g_nFlightLevelActive != 0) {
      g_nSpyroStateFlags = 0;
    } else if (g_nGamestate != 0xC) {
      ChangeSpyroState(0);
    }
    *(Words5 *)&g_nWorldRenderMeshChunkCount =
        *(Words5 *)&g_nWorldCollisionTriCount;
    g_nScriptedRespawnFlag = 0;
    g_nScriptedMobTag = 0;
    D_80075870 = 0;
    D_80075874 = 0;
    D_80075838 = 0;
    D_8007583C = 0;
    D_80075670 = 0;
    D_80075674 = 0;
    D_8007577C = 0;
    D_80075854 = 0;
    D_80075850 = 0;
    g_nCreditsReturnLevelId = -1;
    g_nCdStreamState = -1;
  } break;
  }
}
