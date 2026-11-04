#include "funcs.h"
#include "globals.h"
#include "world_collision.h"
/* The section counts that are re-read through held pointers go through
   incomplete-array views (g_an...Block, globals.h) so the pointer access keeps
   its register base instead of folding to the absolute form. */

/* 0x80012d58 — walk the loaded world bundle as a chain of size-prefixed
   sections (word 0 of each section links to the next), publish each
   section's table pointer + count globals and patch the relative offsets in
   each pointer table to absolute addresses (1240 b). reduced != 0 unpacks the
   reduced stream (mesh index / trigger / collision-header and the
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
  int *meshCountRef;
  int i;
  int meshCount;
  int chunkCount;
  int visZoneCount;
  int meshIndexCount;
  int triggerCount;
  int triCount;
  int portalCount;
  int pad[14];

  /* section: mesh headers */
  base = p;
  p++;
  meshCount = *p;
  p++;
  meshCountRef = g_anWorldMeshCountBlock;
  g_pWorldMeshHeaderArray = p;
  *meshCountRef = meshCount;
  p += meshCount * 4;
  g_pWorldMeshVertexData = p;
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
    int *chunkCountRef = meshCountRef - 7;

    do {
      int rel = *p + 4;
      *p = (int)base + rel;
      i++;
      p++;
    } while (i < *chunkCountRef);
  }
  p = (int *)((char *)base + *base);

  if (reduced == 0) {
    /* section: mesh index tables */
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
      meshIndexCount = *p;
      p++;
      {
        int *meshIndexCountRef = g_anWorldMeshIndexCountBlock;

        g_pWorldMeshIndexTable = p;
        *meshIndexCountRef = meshIndexCount;
        if (meshIndexCount > 0) {
          do {
            int tableBase;
            int rel;

            i++;
            tableBase = (int)base + base[1];
            rel = *p + 4;
            *p = tableBase + rel;
            p++;
          } while (i < *meshIndexCountRef);
        }
      }
    } else {
      g_anVisZoneCountBlock[0] = 0;
      g_pVisZoneChunkLists = 0;
      g_pWorldMeshIndexTable = 0;
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

  /* section: collision tris */
  base = p;
  p++;
  {
    unsigned char *collisionHdrBytes = g_abCollisionHeaderBytes;

    collisionHdrBytes[0] = ((unsigned char *)p)[0];
    collisionHdrBytes[1] = ((unsigned char *)p)[1];
    collisionHdrBytes[2] = ((unsigned char *)p)[2];
    p++;
    triCount = *p;
    p++;
    g_pWorldCollisionTriArray = p;
    g_nWorldCollisionTriCount = triCount;
    i = 0;
    if (triCount > 0) {
      int *triCountRef = (int *)(collisionHdrBytes - 0x10);

      do {
        int rel = *p + 4;
        *p = (int)base + rel;
        i++;
        p++;
      } while (i < *triCountRef);
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
    int **portalSlot = (int **)g_apPortals;

    do {
      int j;
      int *portal;

      *portalSlot = p;
      portal = p;
      p += p[1] * 3 + 8;
      *(int **)portal = p;
      p += 5;
      base = p;
      p++;
      ((int *)**portalSlot)[4] = *p;
      p++;
      ((int *)**portalSlot)[0] = *p;
      p++;
      ((int *)**portalSlot)[1] = (int)p;
      j = 0;
      if (*(int *)**portalSlot > 0) {
        int **slot = portalSlot;

        do {
          int rel = *p + 4;
          *p = (int)base + rel;
          j++;
          p++;
        } while (j < *(int *)**slot);
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
