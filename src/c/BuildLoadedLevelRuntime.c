/* 0x8001364c — post-CD-stream level finalization driver, the "level-loader
   meat" (3708 b). Builds every piece of per-level runtime state out of the
   bundle the CD stream just dropped in g_pPathTableBuffer. Callers:
   LoadAndStartLevelFromCd and TickLevelTransitionStream (case 0xd);
   param_1 == 0 is the light path (no chunk-anim reset, no overlay hook).

   The bundle is a chain of self-sizing chunks: word 0 is the chunk's total
   byte size, word 1 the entry count, and the entries follow. Everything after
   the fixed header is a run of those chunks, each of which gets its list
   pointer + count published to a global and its stored offsets rebased onto
   the chunk itself. */

#include "globals.h"

extern void StopAllSoundExceptMask(unsigned int mask);
extern void InitLightVectorConstants(void);
extern void ResetPadFrameSubstepTable(unsigned int *pad);
extern void CopyVector(int *dst, int *src);
extern void ResetSpyroEntity();
extern void ChangeSpyroState();
extern void ResetCameraStateToTarget(void);
extern void FillWord(void *dst, unsigned int value, int byte_count);
extern void InsertActorIntoSpatialGrid(char *rec);
extern void HideActorRenderRecord(char *rec);
extern void FindGroundHeightBelow(int *pos, int range);
extern void EncodeCachedVecToActorDirCode(char *rec);
extern void DespawnActorRecord(char *rec);
extern void ResetWorldParticleTables(void);
extern void ResetWorldChunkAnimations(void);
extern void InitHudCounters(int full);
extern int HandleMusicCommand(int track, int arg);
extern void srand(unsigned int seed);

extern short g_anCosineLut[];
extern int g_nLevelSpawnPosZ;
extern int g_nSpyroWorldPosY;
extern int D_80075698;
extern int D_80075700;
extern int D_80078A60;
extern int D_8006F140[];
extern int g_anSpawnAnchorBlock[];
extern int g_anLevelSpawnPosZBlock[];
extern int g_nLevelSpawnPosX;
extern int g_nLevelSpawnPosY;
extern int g_anWorldAnimMeshKeyframeCountBlock[];
extern int g_anWorldAnimUvCycleCountBlock[];
extern int g_anWorldAnimColor2CountBlock[];
extern int g_anWorldAnimColor0CountBlock[];
extern int g_anWorldAnimVertexMorphCountBlock[];
extern int g_anWorldAnimColor3CountBlock[];
extern int g_anWorldAnimColor1CountBlock[];

struct CameraParamBlock {
  int w[6];
};

/* The kill-table else-arm reserves v1/a0 with empty asm defs/uses so global
   alloc hands `tb` a1, as in the original. */
int BuildLoadedLevelRuntime(int fullSetup) {
  int pos[3];
  int pad[8];
  int dyaw;
  int *p;
  int *base;
  int *root;
  int *dst;
  int *src;
  char *rec;
  char *r;
  int mask;
  int off;
  int i;
  int yaw;
  int gidx;
  int radial;
  int wi;
  int bit;
  int tb;
  int row;
  short *new_var;
  int tbit;
  int sh;
  int *pk;
  int *gd;
  int *tg;
  int k;
  int j;
  int w;
  int *pcount;
  int count;
  unsigned char type;
  StopAllSoundExceptMask(0);
  InitLightVectorConstants();
  p = (int *)g_pPathTableBuffer;
  g_nVblankTickCount = 0;
  g_dwGamestateFrames = 0;
  g_nSpyroDrawSuppressed = 0;
  g_nMotionTrailRibbonCount = 0;
  root = p;
  if (g_nActiveLevelId == 0x40) {
    g_nLevelEggCount = 0;
  }
  D_80075698 = 0;
  if ((g_nActiveLevelId == 0x28) || (g_nActiveLevelId == 0x2A)) {
    g_nLevelStormFlag = 1;
  } else {
    g_nLevelStormFlag = 0;
  }
  ResetPadFrameSubstepTable(&g_dwPadPressed);
  if (g_nFlightLevelActive == 0) {
    g_nLevelReadyFlagStash = g_nLevelReadyFlag;
  }
  if (g_anSpawnAnchorBlock[0] != 0) {
    CopyVector(g_anSpyroWorldPos, g_anSpawnAnchorBlock + 20);
    yaw = g_nLevelSpawnYaw >> 4;
    g_abSpyroPersistentEuler[2] = yaw;
  } else if (g_nScriptedMobTag == 0) {
    g_anLevelSpawnPos[0] = p[0];
    g_anLevelSpawnPos[1] = p[1];
    g_nLevelSpawnPosZ = p[2];
    g_nLevelSpawnYaw = ((unsigned char *)p)[0xE];
    if ((g_nScriptedRespawnFlag == 0) && (g_nGamestate != 0xC)) {
      if (((g_nGamestate == 5) &&
           (((unsigned int)(g_nActiveLevelId - 0x28)) < 0x14)) &&
          ((gidx = g_nGameOverLevelIntroIndex - 0x12,
            g_anGameOverRespawnPosTable[gidx * 3] != 0))) {
        CopyVector(g_anSpyroWorldPos,
                   &D_8006F140[g_nGameOverLevelIntroIndex * 3]);
        yaw = g_abGameOverRespawnYawVirtualBase[g_nGameOverLevelIntroIndex];
      } else {
        CopyVector(g_anSpyroWorldPos, g_anLevelSpawnPos);
        yaw = g_nLevelSpawnYaw;
      }
      g_abSpyroPersistentEuler[2] = yaw;
    }
  }
  g_nLevelReadyFlag = g_nLevelReadyFlagStash;
  if (g_nGamestate != 0xC) {
    ResetSpyroEntity(0);
    g_nSpyroPadSnapshotCountdown = 0xC;
  }
  if ((g_nActiveLevelId % 10) == 5) {
    g_nFlightLevelActive = 1;
    g_nSpyroFallTrackingFlag = 1;
    g_nSpyroFallReferenceZ = g_nLevelSpawnPosZ;
    g_nSpyroFloorReferenceZ = g_nLevelSpawnPosZ;
    ChangeSpyroState(0x20);
    if (g_nScriptedRespawnFlag == 0) {
      g_nSpyroStateFlags = 0;
      g_abSpyroPersistentEuler[0] = 0;
      g_abSpyroPersistentEuler[1] = 0;
      g_nSpyroBodyRoll = 0;
      g_nSpyroBodyPitch = 0;
      g_nSpyroPitchRateAccum = 0;
      g_nSpyroTurnRateAccum = 0;
      D_80075700 = 0;
      D_80078A60 -= 0x400;
    }
  } else {
    g_nFlightLevelActive = 0;
    g_nSpyroFallTrackingFlag = 0;
    if (g_nScriptedRespawnFlag != 0) {
      g_anLevelSpawnPosZBlock[0] +=
          g_anLevelSpawnNudgeZTable[g_nLevelIntroIndex];
      new_var = &g_anSineLut[g_nLevelSpawnYaw];
      radial = g_anCosineLut[g_nLevelSpawnYaw] >> 2;
      g_nLevelSpawnPosX -= radial;
      radial = (*new_var) >> 2;
      g_nLevelSpawnPosY -= radial;
    }
  }
  p += 4;
  if (g_nActiveLevelId == 0x40) {
    g_nSpyroFallReferenceZ = 0x3E80;
    g_nGnastyLootFallRefZCap = 0x4638;
  }
  g_nGenericCountdown = 0;
  if (g_nScriptedRespawnFlag == 0) {
    if (g_nGamestate == 0xC) {
      goto emitlist;
    }
    *((struct CameraParamBlock *)g_anCameraActiveTargetParams) =
        *((struct CameraParamBlock *)g_anCameraParamMode2Front);
    g_pCameraAnchorPos = g_anSpyroWorldPos;
    g_nCameraCurrentMode = 0;
    g_nCameraNextMode = 0;
    g_anCameraActiveTargetParams[2] = 0x600;
    g_pCameraTargetParams = g_anCameraActiveTargetParams;
    g_nCameraTargetYawBiasFromMode = g_nSpyroBodyYaw;
    ResetCameraStateToTarget();
    g_nGenericCountdown = 0x20;
  }
  if (g_nGamestate != 0xC) {
    g_nGamestate = 0;
    g_nGamePaused = 1;
    g_nGameplayBlocked = 0;
    g_nLetterboxBarHeight = 0;
  }
emitlist:
  FillWord(g_anSpyroHornStrikeAnchorPos, 0, 0x138);

  g_anSpyroHornStrikeRibbonTexWords[0] = p[0];
  g_anSpyroHornStrikeRibbonTexWords[1] = p[1];
  FillWord(g_abSpyroShadowRingHeights, 0, 0x28);
  p += 2;
  g_anEntitySpikeTexWords[0] = p[0];
  g_anEntitySpikeTexWords[1] = p[1];
  p += 2;
  g_anLevelSpriteTexWordPairs[0] = p[2];
  g_anLevelSpriteTexWordPairs[1] = p[3];
  i = 0;
sprites:
  j = i + 1;

  g_anLevelSpriteTexWordPairs[j * 2] = p[(i * 2) + 4];
  g_anLevelSpriteTexWordPairs[(j * 2) + 1] = p[(i * 2) + 5];
  i = j;
  if (i < 9) {
    goto sprites;
  }
  p += 0x16;
  i = 0;
  g_anSpyroHornStrikeRibbonTexWords[2] = p[0];
  g_anSpyroHornStrikeRibbonTexWords[3] = p[1];
  p += 2;
  g_anSecondaryActorTexWords[0] = p[0];
  g_anSecondaryActorTexWords[1] = p[1];
  p += 2;
  base = p;
  p++;
  w = *p;
  p++;
  g_pWorldAnimMeshKeyframeList = p;
  pcount = g_anWorldAnimMeshKeyframeCountBlock;
  *pcount = w;
  if ((*pcount) > 0) {
    do {
      w = (*p) + 4;
      *p = ((int)base) + w;
      i++;
      p++;
    } while (i < (*pcount));
    i = 0;
  }
  p = (int *)(((char *)base) + (*base));
  base = p;
  p++;
  w = *p;
  p++;
  g_pWorldAnimUvCycleList = p;
  pcount = g_anWorldAnimUvCycleCountBlock;
  *pcount = w;
  if ((*pcount) > 0) {
    do {
      w = (*p) + 4;
      *p = ((int)base) + w;
      i++;
      p++;
    } while (i < (*pcount));
  }
  i = 0;
  p = (int *)(((char *)base) + (*base));
  base = p;
  p++;
  w = *p;
  p++;
  g_pWorldAnimColor2List = p;
  pcount = g_anWorldAnimColor2CountBlock;
  *pcount = w;
  if ((*pcount) > 0) {
    do {
      w = (*p) + 4;
      *p = ((int)base) + w;
      i++;
      p++;
    } while (i < (*pcount));
    i = 0;
  }
  p = (int *)(((char *)base) + (*base));
  base = p;
  p++;
  w = *p;
  p++;
  g_pWorldAnimColor0List = p;
  pcount = g_anWorldAnimColor0CountBlock;
  *pcount = w;
  if ((*pcount) > 0) {
    do {
      w = (*p) + 4;
      *p = ((int)base) + w;
      i++;
      p++;
    } while (i < (*pcount));
  }
  i = 0;
  p = (int *)(((char *)base) + (*base));
  base = p;
  p++;
  w = *p;
  p++;
  g_pWorldAnimVertexMorphList = p;
  pcount = g_anWorldAnimVertexMorphCountBlock;
  *pcount = w;
  if ((*pcount) > 0) {
    do {
      w = (*p) + 4;
      *p = ((int)base) + w;
      i++;
      p++;
    } while (i < (*pcount));
    i = 0;
  }
  p = (int *)(((char *)base) + (*base));
  base = p;
  p++;
  w = *p;
  p++;
  g_pWorldAnimColor3List = p;
  pcount = g_anWorldAnimColor3CountBlock;
  *pcount = w;
  if ((*pcount) > 0) {
    do {
      w = (*p) + 4;
      *p = ((int)base) + w;
      i++;
      p++;
    } while (i < (*pcount));
  }
  i = 0;
  p = (int *)(((char *)base) + (*base));
  base = p;
  p++;
  w = *p;
  p++;
  g_pWorldAnimColor1List = p;
  pcount = g_anWorldAnimColor1CountBlock;
  *pcount = w;
  if ((*pcount) > 0) {
    do {
      w = (*p) + 4;
      *p = ((int)base) + w;
      i++;
      p++;
    } while (i < (*pcount));
  }
  p = (int *)(((char *)base) + (*base));
  if (fullSetup != 0) {
    ResetWorldChunkAnimations();
  } else {
    g_nWorldAnimMeshKeyframeCount = 0;
    g_nWorldAnimUvCycleCount = 0;
    g_nWorldAnimVertexMorphCount = 0;
    g_nWorldAnimColor3Count = 0;
    g_nWorldAnimColor1Count = 0;
  }
  base = p;
  p++;
  i = *p;
  p++;
  g_pActorListBase = p;
  p = (int *)(((char *)base) + (*base));
  g_pActorPoolDynBase = ((char *)g_pActorListBase) + (i * 0x58);
  g_pActorPoolFreeHead = g_pActorPoolDynBase;
  ((char *)g_pActorPoolDynBase)[0x48] = 0xFF;
  g_nActorPoolLiveCount = 0;
  g_pActorAuxPoolTop = p;
  g_pActorAuxPoolFreeHead = p;
  ((char *)p)[-1] = 0xFF;
  g_nActorPoolCapacity = ((unsigned int)(((char *)g_pActorAuxPoolTop) -
                                         ((char *)g_pActorPoolDynBase))) /
                         0x70;
  p = (int *)(((char *)p) + (*p));
  base = p;
  p++;
  count = *p;
  p++;
  g_pLevelPickupTableEntries = p;
  g_nLevelPickupTableCount = count;
  i = count - 1;
  if (i >= 0) {
    pk = (int *)((i * 4) + ((int)p));
    do {
      *pk += (int)base;
      i--;
      pk--;
    } while (i >= 0);
  }
  p = (int *)(((char *)base) + (*base));
  base = p;
  p++;
  g_pActorSpatialGridBase = p;
  p = (int *)(((char *)base) + (*base));
  base = p;
  i = 0;
  p++;
  if ((*base) > 0) {
    do {
      gd = (int *)(((char *)root) + (*p));
      *gd += (int)root;
      i++;
      p++;
    } while (i < (*base));
  }
  if (g_nDeathState != 0) {
    {
      register int kk asm("$3");
      register int dd asm("$2");

      srand(1234);
      kk = g_nDeathTryIndex;
      dd = g_anDeathReplaySpawnTable[kk * 4];
      g_anSpyroWorldPos[0] = dd;
      dd = g_anDeathReplaySpawnTable[(kk * 4) + 1];
      g_nSpyroWorldPosY = dd;
      dd = g_anDeathReplaySpawnTable[(kk * 4) + 2];
      D_80078A60 = dd;
      dd = g_anDeathReplaySpawnTable[(kk * 4) + 3];
      g_nDeathRespawnPending = 1;
      g_abSpyroPersistentEuler[2] = dd;
      g_nSpyroBodyYaw = (dd & 0xFF) * 0x10;
    }
    if (g_nDeathState == 1) {
      g_pDeathReplayCursor = p;
    } else {
      g_pDeathReplayCursor = (void *)0x80600000;
      FillWord((void *)0x80600000, 0, 0x4000);
    }
  }
  mask = 0;
  i = 0;
  rec = (char *)g_pActorListBase;
  if (((unsigned char)rec[0x48]) != 0xFF) {
    do {
      r = (char *)((i * 0x58) + ((int)rec));
      if (((int *)(&g_apActorMeshTable))[*((short *)(r + 0x36))] < 0) {
        InsertActorIntoSpatialGrid(r);
      } else {
        HideActorRenderRecord(r);
      }
      CopyVector(pos, (int *)((((char *)g_pActorListBase) + (i * 0x58)) + 0xC));
      pos[2] += 0x400;
      FindGroundHeightBelow(pos, 0x10000);
      EncodeCachedVecToActorDirCode(((char *)g_pActorListBase) + (i * 0x58));
      type =
          *((unsigned char *)(((i * 0x58) + ((int)g_pActorListBase)) + 0x3A));
      if (((unsigned int)(type & 0x7F)) < 0x20) {
        register int b1 asm("$5");
        wi = i >> 5;
        b1 = i & 0x1F;
        b1 = 1 << b1;
        if ((g_anRunningKillBitmap[wi] & b1) == 0) {
          mask |= 1 << type;
        } else {
          row = (g_nLevelIntroIndex << 5) + ((int)g_anLevelKillBitmapTable);
          if (((*((int *)((wi * 4) + row))) & b1) == 0) {
            mask |= 1 << type;
          }
        }
      }
      rec = (char *)g_pActorListBase;
      i++;
    } while (((unsigned char)rec[(i * 0x58) + 0x48]) != 0xFF);
    rec = *(char *volatile *)&g_pActorListBase;
  }
  i = 0;
  if (((unsigned char)rec[0x48]) != 0xFF) {
    do {
      register int sh2 asm("$5");
      register int tbl asm("$3");
      register int b3 asm("$3");
      wi = i >> 5;
      sh2 = i & 0x1F;
      tbl = (int)g_anLevelKillBitmapTable;
      row = (g_nLevelIntroIndex << 5) + tbl;
      wi <<= 2;
      b3 = 1 << sh2;
      if ((*((int *)(wi + row))) & b3) {
        if ((*(int *)((char *)g_anRunningKillBitmap + wi) & b3) == 0) {
          r = (char *)((i * 0x58) + ((int)rec));
          if (((*((unsigned char *)(r + 0x3A))) & 0x7F) == 0x7D) {
            DespawnActorRecord(r);
          } else {
            r[0x53] = 0xFF;
          }
        } else {
          {
            register int k3 asm("$3");
            register int k4 asm("$4");

            __asm__("" : "=r"(k3));
            __asm__("" : "=r"(k4));
            tb = (*((unsigned char *)(((i * 0x58) + ((int)g_pActorListBase)) +
                                      0x3A))) &
                 0x7F;
            if (tb < 0x20) {
              tbit = 1 << tb;
              count = (mask & tbit) != 0;
            } else {
              count = tb == 0x7E;
            }
            __asm__ volatile("" : : "r"(k3), "r"(k4));
          }
          if (count != 0) {
            (((char *)g_pActorListBase) + (i * 0x58))[0x53] = 0xFF;
          } else {
            DespawnActorRecord(((char *)g_pActorListBase) + (i * 0x58));
          }
        }
      }
      rec = (char *)g_pActorListBase;
      i++;
    } while (((unsigned char)rec[(i * 0x58) + 0x48]) != 0xFF);
  }
  i = 0;
  do {
    g_anCommittedKillBitmap[i] = g_anCommittedKillBitmap[i - 8];
    i++;
  } while (i < 8);
  i = 9;
  tg = &g_anLevelTriggerStateSlots[9];
  do {
    *tg = 0;
    i--;
    tg--;
  } while (i >= 0);
  g_pEmitListBase = g_pEmitListBuffer;
  g_pEmitListWriteCursor = g_pEmitListBuffer;
  ((unsigned char *)g_pEmitListBuffer)[1] = 0xFF;
  ((int *)g_pEmitListBase)[0x800] = -1;
  ResetWorldParticleTables();
  if ((fullSetup != 0) && (g_nFlightLevelActive == 0)) {
    g_pLevelOverlayInitRecord =
        (*((void *(**)())(&g_pfnLevelOverlayInitHook)))(0x78, 0);
    InitHudCounters(1);
  }
  return HandleMusicCommand(g_nCurrentMusicTrack, 1);
}
