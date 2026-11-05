#include "funcs.h"
#include "globals.h"
#include "portal.h"
#include "world_collision.h"
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
