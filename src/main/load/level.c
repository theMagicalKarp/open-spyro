#include "camera.h"
#include "cd_music.h"
#include "cd_stream.h"
#include "emit_record.h"
#include "frame_db.h"
#include "funcs.h"
#include "gamestate.h"
#include "globals.h"
#include "level_entry_points.h"
#include "level_id.h"
#include "main_shared.h"
#include "match_asm.h"
#include "model.h"
#include "portal.h"
#include "spu.h"
#include "spyro_state.h"
#include "vecmath.h"
#include "world_collision.h"

/* Publish the loaded level sample-bank header/entry pointers, and (when
   relocate != 0) patch each entry's leading file-relative sample offset to
   absolute by adding the SPU_SAMPLE_BASE (0x1010) SPU RAM base. Entries are
   0x14 bytes; the count sits at header+0x100, the entry array at header+0x104.
   Walks the entries back-to-front. (0x80012cf0, 104 bytes.) */
void RelocateLevelSampleBank(int *header, int relocate) {
  int i;
  int off;
  int *entry;
  g_pLevelSampleBankHeader = header;
  header += 0x40;
  i = *header;
  header += 1;
  g_pLevelSampleBankEntries = header;
  if (relocate != 0) {
    i -= 1;
    if (i >= 0) {
      off = i * 0x14;
      do {
        entry = (int *)(off + (int)g_pLevelSampleBankEntries);
        i -= 1;
        *entry = *entry + SPU_SAMPLE_BASE;
        off -= 0x14;
      } while (i >= 0);
    }
  }
}

/* The section counts that are re-read through held pointers go through
   incomplete-array views (g_an...Block, globals.h) so the pointer access keeps
   its register base instead of folding to the absolute form. */

/* 0x80012d58 — walk the loaded world bundle as a chain of size-prefixed
   sections (word 0 of each section links to the next), publish each
   section's table pointer + count globals and patch the relative offsets in
   each pointer table to absolute addresses (1240 b). reduced != 0 unpacks the
   reduced stream (vis-zone / trigger / collision-header and the
   portal, emit-texture and sample-bank sections are absent). Returns the cursor
   past the consumed data.

   One function-scope `base` holds every section start (s0, it crosses the
   sample-bank call), and the cursor stays the cse-canonical register at each
   `base = p` copy only because `p` is live to the final return; returning
   `base + *base` directly makes `base` canonical and folds every increment
   into displacement form. Each section's count is its own local so it stays
   block-local (v0/v1), and pad[14] reproduces the original 0x50 frame. */
void *UnpackWorldDataChunks(int *p, int reduced) {
  int *base;
  int *textureCountRef;
  int i;
  int textureCount;
  int chunkCount;
  int visZoneCount;
  int skyZoneListCount;
  int triggerCount;
  int skyChunkCount;
  int portalCount;
  int pad[14];

  /* section: world textures (0x10-byte LOD UV records, then 0xA8-byte detail
   * records) */
  base = p;
  p++;
  textureCount = *p;
  p++;
  textureCountRef = g_anWorldTextureCountBlock;
  g_pWorldTextureLodRecords = p;
  *textureCountRef = textureCount;
  p += textureCount * 4;
  g_pWorldTextureDetailRecords = p;
  p = (int *)((char *)base + *base);

  /* section: world chunks */
  base = p;
  p++;
  chunkCount = *p;
  p++;
  g_pWorldChunkArray = p;
  g_nWorldChunkCount = chunkCount;
  i = 0;
  if (chunkCount > 0) {
    int *chunkCountRef = textureCountRef - 7; /* g_nWorldChunkCount */

    do {
      int rel = *p + 4;
      *p = (int)base + rel;
      i++;
      p++;
    } while (i < *chunkCountRef);
  }
  p = (int *)((char *)base + *base);

  if (reduced == 0) {
    /* section: vis zones (world chunk lists, then the sky chunk lists, per
     * zone) */
    base = p;
    if (*base >= 5) {
      p += 2;
      visZoneCount = *p;
      p++;
      i = 0;
      {
        int *visZoneCountRef = g_anVisZoneCountBlock;

        g_pVisZoneChunkLists = p;
        *visZoneCountRef = visZoneCount;
        if (visZoneCount > 0) {
          do {
            int rel = *p + 4;
            *p = (int)base + rel;
            i++;
            p++;
          } while (i < *visZoneCountRef);
        }
      }
      i = 0;
      p = (int *)((char *)base + (base[1] + 4));
      skyZoneListCount = *p;
      p++;
      {
        int *skyZoneListCountRef = g_anLevelSkyZoneListCountBlock;

        g_pLevelSkyZoneLists = p;
        *skyZoneListCountRef = skyZoneListCount;
        if (skyZoneListCount > 0) {
          do {
            int tableBase;
            int rel;

            i++;
            tableBase = (int)base + base[1];
            rel = *p + 4;
            *p = tableBase + rel;
            p++;
          } while (i < *skyZoneListCountRef);
        }
      }
    } else {
      g_anVisZoneCountBlock[0] = 0;
      g_pVisZoneChunkLists = 0;
      g_pLevelSkyZoneLists = 0;
    }
    p = (int *)((char *)base + *base);
    if (reduced == 0) {
      /* section: trigger regions */
      base = p;
      p++;
      triggerCount = *p;
      p++;
      i = 0;
      {
        int *triggerCountRef = g_anTriggerRegionCount;

        g_pTriggerRegionTable = p;
        *triggerCountRef = triggerCount;
        if (triggerCount > 0) {
          do {
            int rel = *p + 4;
            *p = (int)base + rel;
            i++;
            p++;
          } while (i < *triggerCountRef);
        }
      }
      p = (int *)((char *)base + *base);
      if (reduced == 0) {
        /* section: collision header (five relative offsets made absolute) */
        base = p;
        p++;
        g_pWorldCollisionHeader = p;
        {
          int *hdr = *(int *volatile *)&g_pWorldCollisionHeader;

          COLL_HDR(hdr, xIndex) = (int)p + COLL_HDR(hdr, xIndex);
        }
        {
          int *hdr = *(int *volatile *)&g_pWorldCollisionHeader;

          COLL_HDR(hdr, yzIndex) = (int)p + COLL_HDR(hdr, yzIndex);
        }
        {
          int *hdr = *(int *volatile *)&g_pWorldCollisionHeader;

          COLL_HDR(hdr, tris) = (int)p + COLL_HDR(hdr, tris);
        }
        {
          int *hdr = *(int *volatile *)&g_pWorldCollisionHeader;

          COLL_HDR(hdr, visZone) = (int)p + COLL_HDR(hdr, visZone);
        }
        {
          int *hdr = *(int *volatile *)&g_pWorldCollisionHeader;

          COLL_HDR(hdr, groundCode) = (int)p + COLL_HDR(hdr, groundCode);
        }
        p = (int *)((char *)base + *base);
      }
    }
  }

  /* section: the level sky (background colour, then its chunk pointers) */
  base = p;
  p++;
  {
    unsigned char *skyBg = g_abLevelSkyBgColor;

    skyBg[0] = ((unsigned char *)p)[0];
    skyBg[1] = ((unsigned char *)p)[1];
    skyBg[2] = ((unsigned char *)p)[2];
    p++;
    skyChunkCount = *p;
    p++;
    g_pLevelSkyChunkArray = p;
    g_nLevelSkyChunkCount = skyChunkCount;
    i = 0;
    if (skyChunkCount > 0) {
      int *skyChunkCountRef = (int *)(skyBg - 0x10);

      do {
        int rel = *p + 4;
        *p = (int)base + rel;
        i++;
        p++;
      } while (i < *skyChunkCountRef);
    }
  }
  p = (int *)((char *)base + *base);
  if (reduced != 0) {
    return p;
  }

  /* section: portals */
  portalCount = *p;
  p++;
  g_nPortalCount = portalCount;
  i = 0;
  if (portalCount > 0) {
    Portal **portalSlot = g_apPortals;

    do {
      int j;
      Portal *portal;

      *portalSlot = (Portal *)p;
      portal = (Portal *)p;
      p += portal->nPoints * 3 + 8; /* the header words + nPoints xyz points */
      portal->pSky = (SkyHeader *)p;
      p += 5;
      base = p;
      p++;
      (*portalSlot)->pSky->color = *p;
      p++;
      (*portalSlot)->pSky->chunkCount = *p;
      p++;
      (*portalSlot)->pSky->chunks = (void **)p;
      j = 0;
      if ((*portalSlot)->pSky->chunkCount > 0) {
        Portal **slot = portalSlot;

        do {
          int rel = *p + 4;
          *p = (int)base + rel;
          j++;
          p++;
        } while (j < (*slot)->pSky->chunkCount);
      }
      portalSlot++;
      i++;
      p = (int *)((char *)base + *base);
    } while (i < g_nPortalCount);
  }

  /* section: emit texture tables (effect id -> texture frame list) */
  base = p;
  p++;
  i = *p;
  p++;
  for (; i > 0; i--) {
    g_apEmitTextureTable[*(short *)p] = p;
    p = (int *)((char *)p + ((short *)p)[1] + 4);
  }
  p = (int *)((char *)base + *base);

  /* section: level sample bank */
  base = p;
  g_nLevelSampleBankCount = *p;
  p++;
  RelocateLevelSampleBank(p, 1);
  p = (int *)((char *)base + *base);
  return p;
}

/* 0x800133e0 — relocate a freshly loaded actor Model in place (620 b, leaf).
   Every pointer in it is stored on disc as an offset from the model's own
   base, so loading it means adding the base back in:

     - a model whose count word is negative is a stub: fix up its three
       header pointers and hand back the masked base;
     - otherwise the eight m_CollisionModels slots are relocated (skipping
       empty ones), m_Data is rebased, and every present m_Animations header
       is walked: its face / colour pointers move by m_Data, then its frame
       records are rebased. Spyro-format animations (m_IsSpyroAnimation) keep
       one word per frame and halve the rebased offset back into its low 21
       bits; actor animations keep two words per frame, add m_Data to the
       first and patch the second word's 16-bit link when it is set. */

/* Model words and one animation header's words, as int indexes. */
#define DATA hdr[MODEL_WORD(m_Data)]
#define ANIM_SLOT(i) hdr[MODEL_WORD(m_Animations) + (i)]
#define ANIM(i) ((int *)ANIM_SLOT(i))
#define AW(field) ANIM_HEADER_WORD(field)

void *RelocateActorMeshHeader(int *table) {
  int *hdr;
  int *w;
  int i;
  int j;
  int word;
  int lo;

  hdr = table;
  if (table[0] < 0) {
    table[1] = table[1] + (int)table;
    table[2] = table[2] + (int)table;
    table[3] = table[3] + (int)table;
    return (void *)((int)table & 0x7FFFFFFF);
  }

  for (i = 0; i < 8; i++) {
    if (table[MODEL_WORD(m_CollisionModels) + i] != 0) {
      table[MODEL_WORD(m_CollisionModels) + i] =
          ((int)table & 0x7FFFFFFF) + table[MODEL_WORD(m_CollisionModels) + i];
    }
  }

  DATA = (int)table + DATA;

  for (i = 0; i < hdr[0]; i++) {
    if (ANIM_SLOT(i) != -1) {
      ANIM_SLOT(i) = (int)table + ANIM_SLOT(i);
      ANIM(i)[AW(m_Faces)] = DATA + ANIM(i)[AW(m_Faces)];
      ANIM(i)[AW(m_Colors)] = DATA + ANIM(i)[AW(m_Colors)];
      if (ANIM(i)[AW(m_LpFaces)] != 0) {
        ANIM(i)[AW(m_LpFaces)] = DATA + ANIM(i)[AW(m_LpFaces)];
        ANIM(i)[AW(m_LpColors)] = DATA + ANIM(i)[AW(m_LpColors)];
      }
      if (ANIM_HEADER_RAW(ANIM_SLOT(i), unsigned char, m_IsSpyroAnimation) !=
          0) {
        ANIM(i)
        [AW(m_AnimationVertices)] = DATA + ANIM(i)[AW(m_AnimationVertices)];
        w = (int *)(ANIM_SLOT(i) + ANIM_HEADER_OFFSET(m_FrameWords));
        for (j = 0; j < *(short *)ANIM_SLOT(i); j++) {
          word = *w;
          lo = word & 0x1FFFFF;
          *w = word & 0xFFE00000;
          lo = ((lo + DATA) >> 1) & 0x1FFFFF;
          *w += lo;
          w++;
        }
      } else {
        w = (int *)(ANIM_SLOT(i) + ANIM_HEADER_OFFSET(m_FrameWords));
        for (j = 0; j < *(short *)ANIM_SLOT(i); j++, w++) {
          *w += DATA & 0x1FFFFF;
          w++;
          if ((*w & 0xFFFF) != 0) {
            *w += ((DATA - (int)w + 4) >> 2) & 0xFFFF;
          }
        }
      }
    }
  }
  return table;
}

#undef DATA
#undef ANIM_SLOT
#undef ANIM
#undef AW

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
    yaw = ANGLE12_TO_8(g_nLevelSpawnYaw);
    g_abSpyroPersistentEuler[2] = yaw;
  } else if (g_nReturnPortalLevelId == 0) {
    g_anLevelSpawnPos[0] = p[0];
    g_anLevelSpawnPos[1] = p[1];
    g_nLevelSpawnPosZ = p[2];
    g_nLevelSpawnYaw = ((unsigned char *)p)[0xE];
    if ((g_nPortalTransitionActive == 0) &&
        (g_nGamestate != GS_ACTOR_CUTSCENE)) {
      if (((g_nGamestate == GS_GAME_OVER) &&
           (((unsigned int)(g_nActiveLevelId - LEVEL_BEAST_MAKERS)) <
            LEVEL_GNORC_GNEXUS - LEVEL_BEAST_MAKERS)) &&
          ((gidx = g_nGameOverLevelIndex - LEVEL_SLOT(LEVEL_BEAST_MAKERS),
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
  if (LEVEL_IN_WORLD(g_nActiveLevelId) == LEVEL_FLIGHT_SLOT) {
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
    *((CameraAngles *)g_anCameraActiveTargetParams) =
        *((CameraAngles *)g_anCameraParamFollowBehind);
    g_pCameraAnchorPos = g_anSpyroWorldPos;
    g_nCameraCurrentMode = CAM_MODE_STAND;
    g_nCameraNextMode = CAM_MODE_STAND;
    g_anCameraActiveTargetParams[CAMP_DISTANCE] = 0x600;
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
      g_nSpyroBodyYaw = (poseWord & ANGLE8_MASK) * 0x10;
    }
    if (g_nDemoMode == DEMO_PLAYBACK) {
      g_pDemoPadCursor = p;
    } else {
      g_pDemoPadCursor = DEMO_RECORD_BUFFER;
      FillWord(DEMO_RECORD_BUFFER, 0, DEMO_RECORD_BYTES);
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
      pos[2] += GROUND_PROBE_LIFT;
      FindGroundHeightBelow(pos, GROUND_PROBE_FAR);
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
  ((unsigned char *)g_pEmitListBuffer)[1] = EMIT_CLASS_END;
  ((int *)g_pEmitListBase)[EMIT_LIST_BYTES / 4] = -1;
  ResetWorldParticleTables();
  if ((fullSetup != 0) && (g_nFlightLevelActive == 0)) {
    g_pSparxActor = g_pfnLevelSpawnActor(ACTOR_TYPE_SPARX, 0);
    InitHudCounters(1);
  }
  HandleMusicCommand(g_nCurrentMusicTrack, MUSIC_CMD_PLAY);
}

/* 0x800144c8: loads + starts a level from CD. Silences sound, resolves the
   level's archive LBA (base + per-level offset table, stride 0x10), syncs
   it in, builds the runtime level, restarts Spyro's anim state (flying on
   flight levels, idle elsewhere), then gives Sparx full health. */
void LoadAndStartLevelFromCd(void) {
  int pad[8];
  int level;
  int base;
  int archiveBase;
  void *dst;
  int size;
  int offset;

  (void)pad;

  StopAllSoundExceptMask(0);
  level = *(volatile int *)&g_nLevelIndex;
  base = *(volatile int *)&g_nCdBaseLba;
  archiveBase = *(volatile int *)&g_nLevelArchiveBaseLba;
  dst = *(void *volatile *)&g_pLevelArchiveBuffer;
  size = *(volatile int *)&g_nLevelArchiveByteSize;
  offset = *(int *)((char *)&g_nLevelArchiveOffset + level * 0x10);
  CdReadSyncSectors(base, dst, size, archiveBase + offset,
                    CD_STALL_TIMEOUT_TICKS);
  BuildLoadedLevelRuntime(1);

  if (g_nFlightLevelActive) {
    RestartSpyroAnimWithState(SPYRO_ST_FLY);
  } else {
    RestartSpyroAnimWithState(SPYRO_ST_IDLE);
  }

  g_nSparxHealth = SPARX_GOLD;
}

/* 0x8005a470: point the per-level hook slots at the overlay just loaded for
   g_nActiveLevelId: every level sets the overlay end and its
   SpawnActor / UpdateActors / EmitParticles / AdvanceEmitList entries; homes
   add the actor-cutscene and save-menu hooks, flight levels the flight-run and
   results hooks. Ids 0..9 and 99 are the overlays not mapped from WAD.WAD
   (level_entry_points.h). */
void ApplyPerLevelGlobalsTable(void) {
  switch (g_nActiveLevelId) {
  case 0:
    g_pLoadStagingBuffer = LevelId0_OverlayEnd;
    g_pfnLevelSpawnActor = LevelId0_SpawnActor;
    g_pfnLevelUpdateActors = LevelId0_UpdateActors;
    g_pfnLevelEmitParticles = LevelId0_EmitParticles;
    g_pfnLevelAdvanceEmitList = LevelId0_AdvanceEmitList;
    g_pfnLevelBeginActorCutscene = LevelId0_BeginActorCutscene;
    g_pfnLevelOnSaveMenuExit = LevelId0_OnSaveMenuExit;
    g_pfnActorCutsceneSubstateUpdate = LevelId0_BalloonistCutscene_Update;
    g_pfnActorCutsceneSubstateDraw = LevelId0_BalloonistCutscene_Draw;
    break;
  case 1:
    g_pLoadStagingBuffer = LevelId1_OverlayEnd;
    g_pfnLevelSpawnActor = LevelId1_SpawnActor;
    g_pfnLevelUpdateActors = LevelId1_UpdateActors;
    g_pfnLevelEmitParticles = LevelId1_EmitParticles;
    g_pfnLevelAdvanceEmitList = LevelId1_AdvanceEmitList;
    break;
  case 2:
    g_pLoadStagingBuffer = LevelId2_OverlayEnd;
    g_pfnLevelSpawnActor = LevelId2_SpawnActor;
    g_pfnLevelUpdateActors = LevelId2_UpdateActors;
    g_pfnLevelEmitParticles = LevelId2_EmitParticles;
    g_pfnLevelAdvanceEmitList = LevelId2_AdvanceEmitList;
    break;
  case 3:
    g_pLoadStagingBuffer = LevelId3_OverlayEnd;
    g_pfnLevelSpawnActor = LevelId3_SpawnActor;
    g_pfnLevelUpdateActors = LevelId3_UpdateActors;
    g_pfnLevelEmitParticles = LevelId3_EmitParticles;
    g_pfnLevelAdvanceEmitList = LevelId3_AdvanceEmitList;
    break;
  case 4:
    g_pLoadStagingBuffer = LevelId4_OverlayEnd;
    g_pfnLevelSpawnActor = LevelId4_SpawnActor;
    g_pfnLevelUpdateActors = LevelId4_UpdateActors;
    g_pfnLevelEmitParticles = LevelId4_EmitParticles;
    g_pfnLevelAdvanceEmitList = LevelId4_AdvanceEmitList;
    break;
  case 7:
    g_pLoadStagingBuffer = LevelId7_OverlayEnd;
    g_pfnLevelSpawnActor = LevelId7_SpawnActor;
    g_pfnLevelUpdateActors = LevelId7_UpdateActors;
    g_pfnLevelEmitParticles = LevelId7_EmitParticles;
    g_pfnLevelAdvanceEmitList = LevelId7_AdvanceEmitList;
    break;
  case 9:
    g_pLoadStagingBuffer = LevelId9_OverlayEnd;
    g_pfnLevelSpawnActor = LevelId9_SpawnActor;
    g_pfnLevelUpdateActors = LevelId9_UpdateActors;
    g_pfnLevelEmitParticles = LevelId9_EmitParticles;
    g_pfnLevelAdvanceEmitList = LevelId9_AdvanceEmitList;
    break;
  case LEVEL_ARTISANS:
    g_pLoadStagingBuffer = Level0_OverlayEnd;
    g_pfnLevelSpawnActor = Level0_SpawnActor;
    g_pfnLevelUpdateActors = Level0_UpdateActors;
    g_pfnLevelEmitParticles = Level0_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level0_AdvanceEmitList;
    g_pfnLevelBeginActorCutscene = Level0_BeginActorCutscene;
    g_pfnLevelOnSaveMenuExit = Level0_OnSaveMenuExit;
    g_pfnActorCutsceneSubstateUpdate = Level0_BalloonistCutscene_Update;
    g_pfnActorCutsceneSubstateDraw = Level0_BalloonistCutscene_Draw;
    break;
  case LEVEL_STONE_HILL:
    g_pLoadStagingBuffer = Level1_OverlayEnd;
    g_pfnLevelSpawnActor = Level1_SpawnActor;
    g_pfnLevelUpdateActors = Level1_UpdateActors;
    g_pfnLevelEmitParticles = Level1_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level1_AdvanceEmitList;
    g_pfnLevelBeginActorCutscene = Level1_BeginActorCutscene;
    g_pfnLevelOnSaveMenuExit = Level1_OnSaveMenuExit;
    g_pfnActorCutsceneSubstateUpdate = Level1_BalloonistCutscene_Update;
    g_pfnActorCutsceneSubstateDraw = Level1_BalloonistCutscene_Draw;
    break;
  case LEVEL_DARK_HOLLOW:
    g_pLoadStagingBuffer = Level2_OverlayEnd;
    g_pfnLevelSpawnActor = Level2_SpawnActor;
    g_pfnLevelUpdateActors = Level2_UpdateActors;
    g_pfnLevelEmitParticles = Level2_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level2_AdvanceEmitList;
    break;
  case LEVEL_TOWN_SQUARE:
    g_pLoadStagingBuffer = Level3_OverlayEnd;
    g_pfnLevelSpawnActor = Level3_SpawnActor;
    g_pfnLevelUpdateActors = Level3_UpdateActors;
    g_pfnLevelEmitParticles = Level3_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level3_AdvanceEmitList;
    g_pfnLevelBeginActorCutscene = Level3_BeginActorCutscene;
    g_pfnLevelOnSaveMenuExit = Level3_OnSaveMenuExit;
    g_pfnActorCutsceneSubstateUpdate = Level3_BalloonistCutscene_Update;
    g_pfnActorCutsceneSubstateDraw = Level3_BalloonistCutscene_Draw;
    break;
  case LEVEL_TOASTY:
    g_pLoadStagingBuffer = Level4_OverlayEnd;
    g_pfnLevelSpawnActor = Level4_SpawnActor;
    g_pfnLevelUpdateActors = Level4_UpdateActors;
    g_pfnLevelEmitParticles = Level4_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level4_AdvanceEmitList;
    break;
  case LEVEL_SUNNY_FLIGHT:
    g_pLoadStagingBuffer = Level5_OverlayEnd;
    g_pfnLevelSpawnActor = Level5_SpawnActor;
    g_pfnLevelUpdateActors = Level5_UpdateActors;
    g_pfnLevelEmitParticles = Level5_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level5_AdvanceEmitList;
    g_pfnLevelEndFlightRun = Level5_FlightEndHook;
    g_pfnLevelHandleFlightResults = Level5_HandleFlightResults;
    g_pfnFlightResultsUpdate = Level5_FlightResultsMenu_Update;
    g_pfnFlightResultsDraw = Level5_FlightResults_Draw;
    g_pfnFlightTimeFormatter = Level5_DrawFlightRaceTime;
    break;
  case LEVEL_PEACE_KEEPERS:
    g_pLoadStagingBuffer = Level6_OverlayEnd;
    g_pfnLevelSpawnActor = Level6_SpawnActor;
    g_pfnLevelUpdateActors = Level6_UpdateActors;
    g_pfnLevelEmitParticles = Level6_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level6_AdvanceEmitList;
    g_pfnLevelBeginActorCutscene = Level6_BeginActorCutscene;
    g_pfnLevelOnSaveMenuExit = Level6_OnSaveMenuExit;
    g_pfnActorCutsceneSubstateUpdate = Level6_BalloonistCutscene_Update;
    g_pfnActorCutsceneSubstateDraw = Level6_BalloonistCutscene_Draw;
    break;
  case LEVEL_DRY_CANYON:
    g_pLoadStagingBuffer = Level7_OverlayEnd;
    g_pfnLevelSpawnActor = Level7_SpawnActor;
    g_pfnLevelUpdateActors = Level7_UpdateActors;
    g_pfnLevelEmitParticles = Level7_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level7_AdvanceEmitList;
    g_pfnLevelBeginActorCutscene = Level7_BeginActorCutscene;
    g_pfnLevelOnSaveMenuExit = Level7_OnSaveMenuExit;
    g_pfnActorCutsceneSubstateUpdate = Level7_BalloonistCutscene_Update;
    g_pfnActorCutsceneSubstateDraw = Level7_BalloonistCutscene_Draw;
    break;
  case LEVEL_CLIFF_TOWN:
    g_pLoadStagingBuffer = Level8_OverlayEnd;
    g_pfnLevelSpawnActor = Level8_SpawnActor;
    g_pfnLevelUpdateActors = Level8_UpdateActors;
    g_pfnLevelEmitParticles = Level8_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level8_AdvanceEmitList;
    g_pfnLevelBeginActorCutscene = Level8_BeginActorCutscene;
    g_pfnLevelOnSaveMenuExit = Level8_OnSaveMenuExit;
    g_pfnActorCutsceneSubstateUpdate = Level8_BalloonistCutscene_Update;
    g_pfnActorCutsceneSubstateDraw = Level8_BalloonistCutscene_Draw;
    break;
  case LEVEL_ICE_CAVERN:
    g_pLoadStagingBuffer = Level9_OverlayEnd;
    g_pfnLevelSpawnActor = Level9_SpawnActor;
    g_pfnLevelUpdateActors = Level9_UpdateActors;
    g_pfnLevelEmitParticles = Level9_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level9_AdvanceEmitList;
    break;
  case LEVEL_DOCTOR_SHEMP:
    g_pLoadStagingBuffer = Level10_OverlayEnd;
    g_pfnLevelSpawnActor = Level10_SpawnActor;
    g_pfnLevelUpdateActors = Level10_UpdateActors;
    g_pfnLevelEmitParticles = Level10_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level10_AdvanceEmitList;
    break;
  case LEVEL_NIGHT_FLIGHT:
    g_pLoadStagingBuffer = Level11_OverlayEnd;
    g_pfnLevelSpawnActor = Level11_SpawnActor;
    g_pfnLevelUpdateActors = Level11_UpdateActors;
    g_pfnLevelEmitParticles = Level11_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level11_AdvanceEmitList;
    g_pfnLevelEndFlightRun = Level11_FlightEndHook;
    g_pfnLevelHandleFlightResults = Level11_HandleFlightResults;
    g_pfnFlightResultsUpdate = Level11_FlightResultsMenu_Update;
    g_pfnFlightResultsDraw = Level11_FlightResults_Draw;
    g_pfnFlightTimeFormatter = Level11_DrawFlightRaceTime;
    break;
  case LEVEL_MAGIC_CRAFTERS:
    g_pLoadStagingBuffer = Level12_OverlayEnd;
    g_pfnLevelSpawnActor = Level12_SpawnActor;
    g_pfnLevelUpdateActors = Level12_UpdateActors;
    g_pfnLevelEmitParticles = Level12_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level12_AdvanceEmitList;
    g_pfnLevelBeginActorCutscene = Level12_BeginActorCutscene;
    g_pfnLevelOnSaveMenuExit = Level12_OnSaveMenuExit;
    g_pfnActorCutsceneSubstateUpdate = Level12_BalloonistCutscene_Update;
    g_pfnActorCutsceneSubstateDraw = Level12_BalloonistCutscene_Draw;
    break;
  case LEVEL_ALPINE_RIDGE:
    g_pLoadStagingBuffer = Level13_OverlayEnd;
    g_pfnLevelSpawnActor = Level13_SpawnActor;
    g_pfnLevelUpdateActors = Level13_UpdateActors;
    g_pfnLevelEmitParticles = Level13_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level13_AdvanceEmitList;
    break;
  case LEVEL_HIGH_CAVES:
    g_pLoadStagingBuffer = Level14_OverlayEnd;
    g_pfnLevelSpawnActor = Level14_SpawnActor;
    g_pfnLevelUpdateActors = Level14_UpdateActors;
    g_pfnLevelEmitParticles = Level14_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level14_AdvanceEmitList;
    break;
  case LEVEL_WIZARD_PEAK:
    g_pLoadStagingBuffer = Level15_OverlayEnd;
    g_pfnLevelSpawnActor = Level15_SpawnActor;
    g_pfnLevelUpdateActors = Level15_UpdateActors;
    g_pfnLevelEmitParticles = Level15_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level15_AdvanceEmitList;
    break;
  case LEVEL_BLOWHARD:
    g_pLoadStagingBuffer = Level16_OverlayEnd;
    g_pfnLevelSpawnActor = Level16_SpawnActor;
    g_pfnLevelUpdateActors = Level16_UpdateActors;
    g_pfnLevelEmitParticles = Level16_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level16_AdvanceEmitList;
    break;
  case LEVEL_CRYSTAL_FLIGHT:
    g_pLoadStagingBuffer = Level17_OverlayEnd;
    g_pfnLevelSpawnActor = Level17_SpawnActor;
    g_pfnLevelUpdateActors = Level17_UpdateActors;
    g_pfnLevelEmitParticles = Level17_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level17_AdvanceEmitList;
    g_pfnLevelEndFlightRun = Level17_FlightEndHook;
    g_pfnLevelHandleFlightResults = Level17_HandleFlightResults;
    g_pfnFlightResultsUpdate = Level17_FlightResultsMenu_Update;
    g_pfnFlightResultsDraw = Level17_FlightResults_Draw;
    g_pfnFlightTimeFormatter = Level17_DrawFlightRaceTime;
    break;
  case LEVEL_BEAST_MAKERS:
    g_pLoadStagingBuffer = Level18_OverlayEnd;
    g_pfnLevelSpawnActor = Level18_SpawnActor;
    g_pfnLevelUpdateActors = Level18_UpdateActors;
    g_pfnLevelEmitParticles = Level18_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level18_AdvanceEmitList;
    g_pfnLevelBeginActorCutscene = Level18_BeginActorCutscene;
    g_pfnLevelOnSaveMenuExit = Level18_OnSaveMenuExit;
    g_pfnActorCutsceneSubstateUpdate = Level18_BalloonistCutscene_Update;
    g_pfnActorCutsceneSubstateDraw = Level18_BalloonistCutscene_Draw;
    break;
  case LEVEL_TERRACE_VILLAGE:
    g_pLoadStagingBuffer = Level19_OverlayEnd;
    g_pfnLevelSpawnActor = Level19_SpawnActor;
    g_pfnLevelUpdateActors = Level19_UpdateActors;
    g_pfnLevelEmitParticles = Level19_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level19_AdvanceEmitList;
    break;
  case LEVEL_MISTY_BOG:
    g_pLoadStagingBuffer = Level20_OverlayEnd;
    g_pfnLevelSpawnActor = Level20_SpawnActor;
    g_pfnLevelUpdateActors = Level20_UpdateActors;
    g_pfnLevelEmitParticles = Level20_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level20_AdvanceEmitList;
    break;
  case LEVEL_TREE_TOPS:
    g_pLoadStagingBuffer = Level21_OverlayEnd;
    g_pfnLevelSpawnActor = Level21_SpawnActor;
    g_pfnLevelUpdateActors = Level21_UpdateActors;
    g_pfnLevelEmitParticles = Level21_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level21_AdvanceEmitList;
    break;
  case LEVEL_METALHEAD:
    g_pLoadStagingBuffer = Level22_OverlayEnd;
    g_pfnLevelSpawnActor = Level22_SpawnActor;
    g_pfnLevelUpdateActors = Level22_UpdateActors;
    g_pfnLevelEmitParticles = Level22_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level22_AdvanceEmitList;
    break;
  case LEVEL_WILD_FLIGHT:
    g_pLoadStagingBuffer = Level23_OverlayEnd;
    g_pfnLevelSpawnActor = Level23_SpawnActor;
    g_pfnLevelUpdateActors = Level23_UpdateActors;
    g_pfnLevelEmitParticles = Level23_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level23_AdvanceEmitList;
    g_pfnLevelEndFlightRun = Level23_FlightEndHook;
    g_pfnLevelHandleFlightResults = Level23_HandleFlightResults;
    g_pfnFlightResultsUpdate = Level23_FlightResultsMenu_Update;
    g_pfnFlightResultsDraw = Level23_FlightResults_Draw;
    g_pfnFlightTimeFormatter = Level23_DrawFlightRaceTime;
    break;
  case LEVEL_DREAM_WEAVERS:
    g_pLoadStagingBuffer = Level24_OverlayEnd;
    g_pfnLevelSpawnActor = Level24_SpawnActor;
    g_pfnLevelUpdateActors = Level24_UpdateActors;
    g_pfnLevelEmitParticles = Level24_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level24_AdvanceEmitList;
    g_pfnLevelBeginActorCutscene = Level24_BeginActorCutscene;
    g_pfnLevelOnSaveMenuExit = Level24_OnSaveMenuExit;
    g_pfnActorCutsceneSubstateUpdate = Level24_BalloonistCutscene_Update;
    g_pfnActorCutsceneSubstateDraw = Level24_BalloonistCutscene_Draw;
    break;
  case LEVEL_DARK_PASSAGE:
    g_pLoadStagingBuffer = Level25_OverlayEnd;
    g_pfnLevelSpawnActor = Level25_SpawnActor;
    g_pfnLevelUpdateActors = Level25_UpdateActors;
    g_pfnLevelEmitParticles = Level25_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level25_AdvanceEmitList;
    break;
  case LEVEL_LOFTY_CASTLE:
    g_pLoadStagingBuffer = Level26_OverlayEnd;
    g_pfnLevelSpawnActor = Level26_SpawnActor;
    g_pfnLevelUpdateActors = Level26_UpdateActors;
    g_pfnLevelEmitParticles = Level26_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level26_AdvanceEmitList;
    break;
  case LEVEL_HAUNTED_TOWERS:
    g_pLoadStagingBuffer = Level27_OverlayEnd;
    g_pfnLevelSpawnActor = Level27_SpawnActor;
    g_pfnLevelUpdateActors = Level27_UpdateActors;
    g_pfnLevelEmitParticles = Level27_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level27_AdvanceEmitList;
    break;
  case LEVEL_JACQUES:
    g_pLoadStagingBuffer = Level28_OverlayEnd;
    g_pfnLevelSpawnActor = Level28_SpawnActor;
    g_pfnLevelUpdateActors = Level28_UpdateActors;
    g_pfnLevelEmitParticles = Level28_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level28_AdvanceEmitList;
    break;
  case LEVEL_ICY_FLIGHT:
    g_pLoadStagingBuffer = Level29_OverlayEnd;
    g_pfnLevelSpawnActor = Level29_SpawnActor;
    g_pfnLevelUpdateActors = Level29_UpdateActors;
    g_pfnLevelEmitParticles = Level29_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level29_AdvanceEmitList;
    g_pfnLevelEndFlightRun = Level29_FlightEndHook;
    g_pfnLevelHandleFlightResults = Level29_HandleFlightResults;
    g_pfnFlightResultsUpdate = Level29_FlightResultsMenu_Update;
    g_pfnFlightResultsDraw = Level29_FlightResults_Draw;
    g_pfnFlightTimeFormatter = Level29_DrawFlightRaceTime;
    break;
  case LEVEL_GNORC_GNEXUS:
    g_pLoadStagingBuffer = Level30_OverlayEnd;
    g_pfnLevelSpawnActor = Level30_SpawnActor;
    g_pfnLevelUpdateActors = Level30_UpdateActors;
    g_pfnLevelEmitParticles = Level30_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level30_AdvanceEmitList;
    g_pfnLevelBeginActorCutscene = Level30_BeginActorCutscene;
    g_pfnLevelOnSaveMenuExit = Level30_OnSaveMenuExit;
    g_pfnActorCutsceneSubstateUpdate = Level30_BalloonistCutscene_Update;
    g_pfnActorCutsceneSubstateDraw = Level30_BalloonistCutscene_Draw;
    break;
  case LEVEL_GNORC_COVE:
    g_pLoadStagingBuffer = Level31_OverlayEnd;
    g_pfnLevelSpawnActor = Level31_SpawnActor;
    g_pfnLevelUpdateActors = Level31_UpdateActors;
    g_pfnLevelEmitParticles = Level31_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level31_AdvanceEmitList;
    break;
  case LEVEL_TWILIGHT_HARBOR:
    g_pLoadStagingBuffer = Level32_OverlayEnd;
    g_pfnLevelSpawnActor = Level32_SpawnActor;
    g_pfnLevelUpdateActors = Level32_UpdateActors;
    g_pfnLevelEmitParticles = Level32_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level32_AdvanceEmitList;
    break;
  case LEVEL_GNASTY_GNORC:
    g_pLoadStagingBuffer = Level33_OverlayEnd;
    g_pfnLevelSpawnActor = Level33_SpawnActor;
    g_pfnLevelUpdateActors = Level33_UpdateActors;
    g_pfnLevelEmitParticles = Level33_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level33_AdvanceEmitList;
    break;
  case LEVEL_GNASTYS_LOOT:
    g_pLoadStagingBuffer = Level34_OverlayEnd;
    g_pfnLevelSpawnActor = Level34_SpawnActor;
    g_pfnLevelUpdateActors = Level34_UpdateActors;
    g_pfnLevelEmitParticles = Level34_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level34_AdvanceEmitList;
    break;
  case 99:
    g_pLoadStagingBuffer = LevelId99_OverlayEnd;
    g_pfnLevelSpawnActor = LevelId99_SpawnActor;
    g_pfnLevelUpdateActors = LevelId99_UpdateActors;
    g_pfnLevelEmitParticles = LevelId99_EmitParticles;
    g_pfnLevelAdvanceEmitList = LevelId99_AdvanceEmitList;
    break;
  }
  g_pLevelDataBuffer = g_pLoadStagingBuffer;
}

#define VREAD(sym) (*(void *volatile *)&(sym))

/* 0x8005b6f8 (0xe0) — carve the render scratch regions down from
   g_pWorkAreaTop (emit list, OT, primitive top, two primitive buffers of
   0x13000 or 0x1c000 bytes depending on `split`), publish them to both
   frame slots and clear the merged-chain head and the OT. The empty volatile
   asm keeps the first FillWord's `a1 = 0` below the RegionBase store. */
void InitActorMeshScratchRegions(int split) {
  register char *top asm("$2");
  int size;

  top = g_pWorkAreaTop;
  g_pEmitListBuffer = top - EMIT_LIST_BYTES;
  EBB_BREAK();
  g_pRenderScratchOtBase = top - (EMIT_LIST_BYTES + OT_BIN_BYTES);
  top -= EMIT_LIST_BYTES + OT_BIN_BYTES + OT_CHAIN_SLOT_BYTES;
  g_pRenderScratchPrimTop = top;
  if (split) {
    size = -PRIM_BUFFER_BYTES_SPLIT;
    top += size;
  } else {
    size = -PRIM_BUFFER_BYTES;
    top += size;
  }
  g_pRenderScratchPrimBase1 = top;
  top += size;
  g_pRenderScratchRegionBase = top;
  SCHED_FENCE();
  g_pFramePrimBufferBase0 = VREAD(g_pRenderScratchRegionBase);
  g_pFramePrimBufferBase1 = VREAD(g_pRenderScratchPrimBase1);
  g_pFrameOtMergedChainSlot1 = g_pFrameOtMergedChainSlot0 =
      VREAD(g_pRenderScratchPrimTop);
  g_pFrameOtDepthBinBase1 = g_pFrameOtDepthBinBase0 =
      VREAD(g_pRenderScratchOtBase);
  FillWord(g_pFrameOtMergedChainSlot0, 0, OT_CHAIN_SLOT_BYTES);
  FillWord(g_pRenderScratchOtBase, 0, OT_BIN_BYTES);
}

#undef VREAD

/* 0x8005b7d8, 264 bytes — load the actor-mesh directory from CD into the top of
   the boot-load region. CdReadSyncSectors pulls g_nActorMeshDirByteSize bytes
   from sector g_nActorMeshDirSectorOffset into a buffer just under 0x80200000 -
   g_nCrt0StackSize; the first word gives the payload size, which is copied down
   to a fresh g_pWorkAreaTop. Up to 64 directory entries are then walked: each
   non-empty entry relocates a mesh header and stores the resolved pointer at
   g_apActorMeshTable[slot]. */

void LoadActorMeshTables(void) {
  int *dir;
  int top;
  int nbytes;
  int size;
  int i;
  int lba;
  int *hdr;
  int chunk;
  void **table;

  i = 0;
  table = g_apActorMeshTableBlock;
  top = RAM_END;
  hdr = g_nActorMeshDirByteSize;
  lba = *(volatile int *)&g_nCdBaseLba;
  nbytes = *(volatile int *)hdr;
  size = *hdr;
  dir = (int *)(top - size - size - g_nCrt0StackSize);
  CdReadSyncSectors(lba, dir, nbytes, g_nActorMeshDirSectorOffset,
                    CD_STALL_TIMEOUT_TICKS);

  chunk = dir[0];
  g_pWorkAreaTop = (void *)(top - g_nCrt0StackSize - chunk);
  nbytes = *(volatile int *)dir;
  CopyWords(g_pWorkAreaTop, dir + 0x200, nbytes);

  for (; i < MESH_STREAM_MAX; i++) {
    if (dir[i * 2 + 1] == 0) {
      break;
    }
    table[dir[i * 2 + 2]] = RelocateActorMeshHeader(
        (int *)((char *)g_pWorkAreaTop + dir[i * 2 + 1] - 0x800));
  }

  InitActorMeshScratchRegions(0);
}
