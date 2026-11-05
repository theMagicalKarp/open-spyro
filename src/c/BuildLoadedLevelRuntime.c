/* 0x8001364c — post-CD-stream level finalization driver, the "level-loader
   meat" (3708 b). Builds every piece of per-level runtime state out of the
   bundle the CD stream just dropped in g_pLevelArchiveBuffer. Callers:
   LoadAndStartLevelFromCd and TickLevelTransitionStream (case 0xd);
   fullSetup == 0 is the light path (no chunk-anim reset, no overlay hook).

   The bundle is a chain of self-sizing chunks: word 0 is the chunk's total
   byte size, word 1 the entry count, and the entries follow. Everything after
   the fixed header is a run of those chunks, each of which gets its list
   pointer + count published to a global and its stored offsets rebased onto
   the chunk itself. */

#include "camera.h"
#include "cd_music.h"
#include "funcs.h"
#include "gamestate.h"
#include "globals.h"
#include "level_id.h"
#include "main_shared.h"
#include "match_asm.h"
#include "spyro_state.h"
#include "vecmath.h"

struct CameraParamBlock {
  int w[6];
};

/* The kill-table else-arm reserves v1/a0 with empty asm defs/uses so global
   alloc hands `group` a1, as in the original. The block-scoped register
   pins (demo/poseWord, runBit, bitIdx/tbl/levelBit) put each value in the
   original's register (A374). */
void BuildLoadedLevelRuntime(int fullSetup) {
  int pos[3];
  int pad[8];
  int dyaw;
  int *p;
  int *base;
  int *root;
  int *dst;
  int *src;
  char *rec;
  Actor *placed;
  int mask;
  int off;
  int i;
  int yaw;
  int gidx;
  int radial;
  int killWord;
  int bit;
  int group;
  int row;
  short *sinp;
  int groupBit;
  int sh;
  int *pickupSlot;
  int *relocSlot;
  int *triggerSlot;
  int k;
  int j;
  int w;
  int *pcount;
  int count;
  unsigned char type;
  StopAllSoundExceptMask(0);
  InitLightVectorConstants();
  p = (int *)g_pLevelArchiveBuffer;
  g_nVblankTickCount = 0;
  g_dwGamestateFrames = 0;
  g_nSpyroDrawSuppressed = 0;
  g_nMotionTrailRibbonCount = 0;
  root = p;
  if (g_nActiveLevelId == LEVEL_GNASTYS_LOOT) {
    g_nLevelKeyState = KEYSTATE_NONE;
  }
  g_nLevelLoadUnreadWord = 0;
  if ((g_nActiveLevelId == LEVEL_BEAST_MAKERS) ||
      (g_nActiveLevelId == LEVEL_MISTY_BOG)) {
    g_nPortalDistanceFogEnabled = 1;
  } else {
    g_nPortalDistanceFogEnabled = 0;
  }
  ResetPadFrameSubstepTable(&g_dwPadPressed);
  if (g_nFlightLevelActive == 0) {
    g_nSparxHealthStash = g_nSparxHealth;
  }
  if (g_anSpawnAnchorBlock[0] != 0) {
    CopyVector(g_anSpyroWorldPos,
               g_anSpawnAnchorBlock + 20); /* g_anLevelSpawnPos */
    yaw = g_nLevelSpawnYaw >> 4;
    g_abSpyroPersistentEuler[2] = yaw;
  } else if (g_nReturnPortalLevelId == 0) {
    g_anLevelSpawnPos[0] = p[0];
    g_anLevelSpawnPos[1] = p[1];
    g_nLevelSpawnPosZ = p[2];
    g_nLevelSpawnYaw = ((unsigned char *)p)[0xE];
    if ((g_nPortalTransitionActive == 0) &&
        (g_nGamestate != GS_ACTOR_CUTSCENE)) {
      if (((g_nGamestate == GS_GAME_OVER) &&
           (((unsigned int)(g_nActiveLevelId - 0x28)) < 0x14)) &&
          ((gidx = g_nGameOverLevelIndex - 0x12,
            g_anGameOverRespawnPosTable[gidx * 3] != 0))) {
        CopyVector(
            g_anSpyroWorldPos,
            &g_anGameOverRespawnPosVirtualBase[g_nGameOverLevelIndex * 3]);
        yaw = g_abGameOverRespawnYawVirtualBase[g_nGameOverLevelIndex];
      } else {
        CopyVector(g_anSpyroWorldPos, g_anLevelSpawnPos);
        yaw = g_nLevelSpawnYaw;
      }
      g_abSpyroPersistentEuler[2] = yaw;
    }
  }
  g_nSparxHealth = g_nSparxHealthStash;
  if (g_nGamestate != GS_ACTOR_CUTSCENE) {
    ResetSpyroEntity(RESET_SPYRO_SOFT);
    g_nSpyroInputBlankCountdown = 0xC;
  }
  if ((g_nActiveLevelId % 10) == 5) {
    g_nFlightLevelActive = 1;
    g_nSpyroFlightMode = 1;
    g_nSpyroFallReferenceZ = g_nLevelSpawnPosZ;
    g_nSpyroFloorReferenceZ = g_nLevelSpawnPosZ;
    ChangeSpyroState(SPYRO_ST_FLY);
    if (g_nPortalTransitionActive == 0) {
      g_nSpyroSubstate = 0;
      g_abSpyroPersistentEuler[0] = 0;
      g_abSpyroPersistentEuler[1] = 0;
      g_nSpyroBodyRoll = 0;
      g_nSpyroBodyPitch = 0;
      g_nSpyroPitchRateAccum = 0;
      g_nSpyroTurnRateAccum = 0;
      g_nSpyroMotionAux = 0;
      g_nSpyroWorldPosZ -= 0x400;
    }
  } else {
    g_nFlightLevelActive = 0;
    g_nSpyroFlightMode = 0;
    if (g_nPortalTransitionActive != 0) {
      g_anLevelSpawnPosZBlock[0] += g_anLevelSpawnNudgeZTable[g_nLevelIndex];
      sinp = &g_anSineLut[g_nLevelSpawnYaw];
      radial = g_anCosineLut[g_nLevelSpawnYaw] >> 2;
      g_nLevelSpawnPosX -= radial;
      radial = (*sinp) >> 2;
      g_nLevelSpawnPosY -= radial;
    }
  }
  p += 4;
  if (g_nActiveLevelId == LEVEL_GNASTYS_LOOT) {
    g_nSpyroFallReferenceZ = 0x3E80;
    g_nGnastyLootFallRefZCap = 0x4638;
  }
  g_nScreenFadeLevel = 0;
  if (g_nPortalTransitionActive == 0) {
    if (g_nGamestate == GS_ACTOR_CUTSCENE) {
      goto emitlist;
    }
    *((struct CameraParamBlock *)g_anCameraActiveTargetParams) =
        *((struct CameraParamBlock *)g_anCameraParamFollowBehind);
    g_pCameraAnchorPos = g_anSpyroWorldPos;
    g_nCameraCurrentMode = CAM_MODE_STAND;
    g_nCameraNextMode = CAM_MODE_STAND;
    g_anCameraActiveTargetParams[2] = 0x600;
    g_pCameraTargetParams = g_anCameraActiveTargetParams;
    g_nCameraParamsReferenceYaw = g_nSpyroBodyYaw;
    ResetCameraStateToTarget();
    g_nScreenFadeLevel = 0x20;
  }
  if (g_nGamestate != GS_ACTOR_CUTSCENE) {
    g_nGamestate = GS_GAMEPLAY;
    g_nSkipGamestateDraw = 1;
    g_nCinematicActive = 0;
    g_nLetterboxBarHeight = 0;
  }
emitlist:
  FillWord(g_anSpyroFlameAnchorPos, 0, 0x138);

  g_anSpyroFlameRibbonTexWords[0] = p[0];
  g_anSpyroFlameRibbonTexWords[1] = p[1];
  FillWord(g_abSpyroShadowRingHeights, 0, 0x28);
  p += 2;
  g_anActorShadowTexWords[0] = p[0];
  g_anActorShadowTexWords[1] = p[1];
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
  g_anSpyroFlameRibbonTexWords[2] = p[0];
  g_anSpyroFlameRibbonTexWords[3] = p[1];
  p += 2;
  g_anSecondaryActorTexWords[0] = p[0];
  g_anSecondaryActorTexWords[1] = p[1];
  p += 2;
  base = p;
  p++;
  w = *p;
  p++;
  g_pWorldAnimTextureFrameList = p;
  pcount = g_anWorldAnimTextureFrameCountBlock;
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
  g_pWorldAnimDetailVertsList = p;
  pcount = g_anWorldAnimDetailVertsCountBlock;
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
  g_pWorldAnimLodVertsList = p;
  pcount = g_anWorldAnimLodVertsCountBlock;
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
  g_pWorldAnimDetailColorsList = p;
  pcount = g_anWorldAnimDetailColorsCountBlock;
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
  g_pWorldAnimLodColorsList = p;
  pcount = g_anWorldAnimLodColorsCountBlock;
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
    g_nWorldAnimTextureFrameCount = 0;
    g_nWorldAnimUvCycleCount = 0;
    g_nWorldAnimVertexMorphCount = 0;
    g_nWorldAnimDetailColorsCount = 0;
    g_nWorldAnimLodColorsCount = 0;
  }
  base = p;
  p++;
  i = *p;
  p++;
  g_pActorListBase = (Actor *)p;
  p = (int *)(((char *)base) + (*base));
  g_pActorPoolDynBase = &g_pActorListBase[i];
  g_pActorPoolFreeHead = g_pActorPoolDynBase;
  g_pActorPoolDynBase->mode = ACTOR_MODE_END;
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
  g_pLevelPickupTableEntries = (unsigned short **)p;
  g_nLevelPickupTableCount = count;
  i = count - 1;
  if (i >= 0) {
    pickupSlot = (int *)((i * 4) + ((int)p));
    do {
      *pickupSlot += (int)base;
      i--;
      pickupSlot--;
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
      relocSlot = (int *)(((char *)root) + (*p));
      *relocSlot += (int)root;
      i++;
      p++;
    } while (i < (*base));
  }
  if (g_nDemoMode != DEMO_OFF) {
    {
      register int demo asm("$3");
      register int poseWord asm("$2");

      srand(1234);
      demo = g_nDemoIndex;
      poseWord = g_anDemoStartPose[demo * 4];
      g_anSpyroWorldPos[0] = poseWord;
      poseWord = g_anDemoStartPose[(demo * 4) + 1];
      g_nSpyroWorldPosY = poseWord;
      poseWord = g_anDemoStartPose[(demo * 4) + 2];
      g_nSpyroWorldPosZ = poseWord;
      poseWord = g_anDemoStartPose[(demo * 4) + 3];
      g_nDemoActive = 1;
      g_abSpyroPersistentEuler[2] = poseWord;
      g_nSpyroBodyYaw = (poseWord & 0xFF) * 0x10;
    }
    if (g_nDemoMode == DEMO_PLAYBACK) {
      g_pDemoPadCursor = p;
    } else {
      g_pDemoPadCursor = (void *)0x80600000;
      FillWord((void *)0x80600000, 0, 0x4000);
    }
  }
  mask = 0;
  i = 0;
  rec = (char *)g_pActorListBase;
  if (((Actor *)rec)->mode != ACTOR_MODE_END) {
    do {
      /* An int sum, not &((Actor *)rec)[i]: the index form swaps the addu. */
      placed = (Actor *)((i * ACTOR_SIZE) + ((int)rec));
      if (((int *)(&g_apActorMeshTable))[placed->type] < 0) {
        InsertActorIntoSpatialGrid(placed);
      } else {
        HideActorRenderRecord(placed);
      }
      CopyVector(pos, &g_pActorListBase[i].posX);
      pos[2] += 0x400;
      FindGroundHeightBelow(pos, 0x10000);
      EncodeHitNormalToActorGroundCode(&g_pActorListBase[i]);
      type = g_pActorListBase[i].killGroup;
      if (((unsigned int)(type & 0x7F)) < 0x20) {
        register int runBit asm("$5");
        killWord = i >> 5;
        runBit = i & 0x1F;
        runBit = 1 << runBit;
        if ((g_anCheckpointKillBitmap[killWord] & runBit) == 0) {
          mask |= 1 << type;
        } else {
          row = (g_nLevelIndex << 5) + ((int)g_anLevelKillBitmapTable);
          if (((*((int *)((killWord * 4) + row))) & runBit) == 0) {
            mask |= 1 << type;
          }
        }
      }
      rec = (char *)g_pActorListBase;
      i++;
    } while (((unsigned char)rec[(i * ACTOR_SIZE) + ACTOR_OFFSET(mode)]) !=
             0xFF);
    rec = *(char *volatile *)&g_pActorListBase;
  }
  i = 0;
  if (((Actor *)rec)->mode != ACTOR_MODE_END) {
    do {
      register int bitIdx asm("$5");
      register int tbl asm("$3");
      register int levelBit asm("$3");
      killWord = i >> 5;
      bitIdx = i & 0x1F;
      tbl = (int)g_anLevelKillBitmapTable;
      row = (g_nLevelIndex << 5) + tbl;
      killWord <<= 2;
      levelBit = 1 << bitIdx;
      if ((*((int *)(killWord + row))) & levelBit) {
        if ((*(int *)((char *)g_anCheckpointKillBitmap + killWord) &
             levelBit) == 0) {
          placed = (Actor *)((i * ACTOR_SIZE) + ((int)rec));
          if ((placed->killGroup & ACTOR_KILL_GROUP_MASK) ==
              ACTOR_KILL_GROUP_NEVER_RESPAWN) {
            DespawnActorRecord(placed);
          } else {
            placed->dropKind = DROP_GEM_ROLL;
          }
        } else {
          {
            register int k3 asm("$3");
            register int k4 asm("$4");

            REG_RESERVE(k3);
            REG_RESERVE(k4);
            group = g_pActorListBase[i].killGroup & ACTOR_KILL_GROUP_MASK;
            if (group < 0x20) {
              groupBit = 1 << group;
              count = (mask & groupBit) != 0;
            } else {
              count = group == ACTOR_KILL_GROUP_ALWAYS_RESPAWN;
            }
            __asm__ volatile("" : : "r"(k3), "r"(k4));
          }
          if (count != 0) {
            g_pActorListBase[i].dropKind = DROP_GEM_ROLL;
          } else {
            DespawnActorRecord(&g_pActorListBase[i]);
          }
        }
      }
      rec = (char *)g_pActorListBase;
      i++;
    } while (((unsigned char)rec[(i * ACTOR_SIZE) + ACTOR_OFFSET(mode)]) !=
             0xFF);
  }
  i = 0;
  do {
    g_anLiveKillBitmap[i] = g_anLiveKillBitmap[i - 8];
    i++;
  } while (i < 8);
  i = 9;
  triggerSlot = &g_anLevelTriggerStateSlots[9];
  do {
    *triggerSlot = 0;
    i--;
    triggerSlot--;
  } while (i >= 0);
  g_pEmitListBase = g_pEmitListBuffer;
  g_pEmitListWriteCursor = g_pEmitListBuffer;
  ((unsigned char *)g_pEmitListBuffer)[1] = 0xFF;
  ((int *)g_pEmitListBase)[0x800] = -1;
  ResetWorldParticleTables();
  if ((fullSetup != 0) && (g_nFlightLevelActive == 0)) {
    g_pSparxActor = g_pfnLevelSpawnActor(ACTOR_TYPE_SPARX, 0);
    InitHudCounters(1);
  }
  HandleMusicCommand(g_nCurrentMusicTrack, MUSIC_CMD_PLAY);
}
