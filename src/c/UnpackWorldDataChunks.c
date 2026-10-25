#include "funcs.h"
#include "globals.h"
/* The section counts that are re-read through held pointers go through
   incomplete-array views (g_an...Block, globals.h) so the pointer access keeps
   its register base instead of folding to the absolute form. */

/* The world collision header (g_pWorldCollisionHeader). Its table fields
 * hold bundle-relative offsets that the unpack below makes absolute; the
 * world probes then read them (FindWorldFloorBelowPoint). COLL_HDR keeps
 * the original's byte-offset access: an `int *` index (`h[2]`) gives a
 * different address RTL and the function stops matching. */
typedef struct WorldCollisionHeader {
  int pad00[2];
  int xIndex;  /* 0x08 X-axis cell index (ushorts) */
  int yzIndex; /* 0x0C combined Y/Z cell index (ushorts) */
  int tris;    /* 0x10 triangle records, stride 0xC */
  int surface; /* 0x14 per-triangle surface-type bytes */
  int table18; /* 0x18 */
} WorldCollisionHeader;
#define COLL_HDR(h, field)                                                     \
  (*(int *)((char *)(h) + (int)&((WorldCollisionHeader *)0)->field))

/* 0x80012d58 — walk the loaded world bundle as a chain of size-prefixed
   sections (word 0 of each section links to the next), publish each
   section's table pointer + count globals and patch the relative offsets in
   each pointer table to absolute addresses (1240 b). flag != 0 unpacks the
   reduced stream (mesh index / trigger / collision-header and the mob
   sections are absent). Returns the cursor past the consumed data.

   One function-scope `base` holds every section start (s0, it crosses the
   sample-bank call), and the cursor stays the cse-canonical register at each
   `base = p` copy only because `p` is live to the final return; returning
   `base + *base` directly makes `base` canonical and folds every increment
   into displacement form. Each section's count is its own local so it stays
   block-local (v0/v1), and pad[14] reproduces the original 0x50 frame. */
void *UnpackWorldDataChunks(int *p, int flag) {
  int *base;
  int *pm;
  int i;
  int n1;
  int n2;
  int n3;
  int n4;
  int n5;
  int n6;
  int n7;
  int pad[14];

  /* section: mesh headers */
  base = p;
  p++;
  n1 = *p;
  p++;
  pm = g_anWorldMeshCountBlock;
  g_pWorldMeshHeaderArray = p;
  *pm = n1;
  p += n1 * 4;
  g_pWorldMeshVertexData = p;
  p = (int *)((char *)base + *base);

  /* section: world chunks */
  base = p;
  p++;
  n2 = *p;
  p++;
  g_pWorldChunkArray = p;
  g_nWorldChunkCount = n2;
  i = 0;
  if (n2 > 0) {
    int *pc = pm - 7;

    do {
      int t = *p + 4;
      *p = (int)base + t;
      i++;
      p++;
    } while (i < *pc);
  }
  p = (int *)((char *)base + *base);

  if (flag == 0) {
    /* section: mesh index tables */
    base = p;
    if (*base >= 5) {
      p += 2;
      n3 = *p;
      p++;
      i = 0;
      {
        int *pn = g_anStaticMeshLodCutoffBlock;

        g_pStaticMeshLodTable = p;
        *pn = n3;
        if (n3 > 0) {
          do {
            int t = *p + 4;
            *p = (int)base + t;
            i++;
            p++;
          } while (i < *pn);
        }
      }
      i = 0;
      p = (int *)((char *)base + (base[1] + 4));
      n4 = *p;
      p++;
      {
        int *pi = g_anWorldMeshIndexCountBlock;

        g_pWorldMeshIndexTable = p;
        *pi = n4;
        if (n4 > 0) {
          do {
            int s;
            int t;

            i++;
            s = (int)base + base[1];
            t = *p + 4;
            *p = s + t;
            p++;
          } while (i < *pi);
        }
      }
    } else {
      g_anStaticMeshLodCutoffBlock[0] = 0;
      g_pStaticMeshLodTable = 0;
      g_pWorldMeshIndexTable = 0;
    }
    p = (int *)((char *)base + *base);
    if (flag == 0) {
      /* section: trigger regions */
      base = p;
      p++;
      n5 = *p;
      p++;
      i = 0;
      {
        int *pt = g_anTriggerRegionCount;

        g_pTriggerRegionTable = p;
        *pt = n5;
        if (n5 > 0) {
          do {
            int t = *p + 4;
            *p = (int)base + t;
            i++;
            p++;
          } while (i < *pt);
        }
      }
      p = (int *)((char *)base + *base);
      if (flag == 0) {
        /* section: collision header (five relative offsets made absolute) */
        base = p;
        p++;
        g_pWorldCollisionHeader = p;
        {
          int *h = *(int *volatile *)&g_pWorldCollisionHeader;

          COLL_HDR(h, xIndex) = (int)p + COLL_HDR(h, xIndex);
        }
        {
          int *h = *(int *volatile *)&g_pWorldCollisionHeader;

          COLL_HDR(h, yzIndex) = (int)p + COLL_HDR(h, yzIndex);
        }
        {
          int *h = *(int *volatile *)&g_pWorldCollisionHeader;

          COLL_HDR(h, tris) = (int)p + COLL_HDR(h, tris);
        }
        {
          int *h = *(int *volatile *)&g_pWorldCollisionHeader;

          COLL_HDR(h, surface) = (int)p + COLL_HDR(h, surface);
        }
        {
          int *h = *(int *volatile *)&g_pWorldCollisionHeader;

          COLL_HDR(h, table18) = (int)p + COLL_HDR(h, table18);
        }
        p = (int *)((char *)base + *base);
      }
    }
  }

  /* section: collision tris */
  base = p;
  p++;
  {
    unsigned char *pb = g_abCollisionHeaderBytes;

    pb[0] = ((unsigned char *)p)[0];
    pb[1] = ((unsigned char *)p)[1];
    pb[2] = ((unsigned char *)p)[2];
    p++;
    n6 = *p;
    p++;
    g_pWorldCollisionTriArray = p;
    g_nWorldCollisionTriCount = n6;
    i = 0;
    if (n6 > 0) {
      int *ptc = (int *)(pb - 0x10);

      do {
        int t = *p + 4;
        *p = (int)base + t;
        i++;
        p++;
      } while (i < *ptc);
    }
  }
  p = (int *)((char *)base + *base);
  if (flag != 0) {
    return p;
  }

  /* section: portals */
  n7 = *p;
  p++;
  g_nPortalCount = n7;
  i = 0;
  if (n7 > 0) {
    int **pe = (int **)g_apPortals;

    do {
      int j;
      int *e;

      *pe = p;
      e = p;
      p += p[1] * 3 + 8;
      *(int **)e = p;
      p += 5;
      base = p;
      p++;
      ((int *)**pe)[4] = *p;
      p++;
      ((int *)**pe)[0] = *p;
      p++;
      ((int *)**pe)[1] = (int)p;
      j = 0;
      if (*(int *)**pe > 0) {
        int **q = pe;

        do {
          int t = *p + 4;
          *p = (int)base + t;
          j++;
          p++;
        } while (j < *(int *)**q);
      }
      pe++;
      i++;
      p = (int *)((char *)base + *base);
    } while (i < g_nPortalCount);
  }

  /* section: mob id -> entry lookup */
  base = p;
  p++;
  i = *p;
  p++;
  for (; i > 0; i--) {
    g_apMobEntryTable[*(short *)p] = p;
    p = (int *)((char *)p + ((short *)p)[1] + 4);
  }
  p = (int *)((char *)base + *base);

  /* section: level sample bank */
  base = p;
  g_nLevelSampleBankCount = *p;
  p++;
  RelocateLevelSampleBank((char *)p, 1);
  p = (int *)((char *)base + *base);
  return p;
}
