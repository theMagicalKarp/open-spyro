#include "globals.h"

extern void ZeroVector(int *vec);
extern void CopyVector(int *dst, int *src);
extern void AddVector(int *dst, int *a, int *b);
extern uint VectorLength(int *vec, int include_z);
extern int ArcTan2(int y, int x, int high_precision);
extern void RotateVectorByMatrix(int *mtx, int *dst, int *src);
extern void ApplyActiveGteRotation(int *dst, int *src);
extern int CastRayWorldAndActors(int *start, int *end);
extern void UnpackWorldCollisionTri(int chunk, int *verts);

/* The whole Spyro ground/collision state block hangs off one base register:
   [0] airborne frames, [2..4] ground normal, [5] slope, [0xa] cliff-edge hint,
   [0x2d] ground-run flag, [0x76] ground chunk id, [0x77..] chunk verts, and the
   body matrix / world position sit at -0x68 / -0x9c below it. */
extern int g_anSpyroAirborneFramesBlock[0x200];
#define BLK g_anSpyroAirborneFramesBlock
#define MTX (&BLK[-0x1a]) /* g_anSpyroBodyMtx */
#define POS (&BLK[-0x27]) /* g_anSpyroWorldPos */
#define NORMAL (&BLK[2])  /* g_anSpyroGroundNormal */

/* Per-frame ground probe. Casts a body-relative ray straight down from Spyro
   (and, on a shallow hit, a second wider one) through the world + actor
   collision set; on a hit it caches the contact normal, the slope angle from
   ArcTan2 and the hit chunk's triangle, and clears the airborne counter when
   the slope is walkable (< 0x21). A third, purely vertical probe re-runs the
   test when the first two missed. (0x8003e318, 784 bytes.) */
void ProbeSpyroGroundContact(void) {
  int down[4];
  int out[4];
  int t;
  g_anSpyroAirborneFramesBlock[0] += 1;
  ZeroVector(&g_anSpyroAirborneFramesBlock[2]);
  g_anSpyroGroundNormal[2] = 0x1000;
  if (((!out) && (!out)) && (!out)) {
  }
  g_nSpyroGroundSlope = 0;
  g_anSpyroAirborneFramesBlock[0xa] = 0;
  if ((g_dwSpyroRequestMask & 0x4000) == 0) {
    down[0] = 0;
    out[0] = 0;
    down[1] = 0;
    out[1] = 0;
    down[2] = -0x104;
    out[2] = -0x1c4;
    RotateVectorByMatrix(&g_anSpyroAirborneFramesBlock[-0x1a], down, down);
    AddVector(down, down, &g_anSpyroAirborneFramesBlock[-0x27]);
    ApplyActiveGteRotation(out, out);
    AddVector(out, out, &g_anSpyroAirborneFramesBlock[-0x27]);
    if (CastRayWorldAndActors(down, out) != 0) {
      CopyVector(&g_anSpyroAirborneFramesBlock[2], g_anCollisionProbeVec);
      t = (signed char)ArcTan2(g_anCollisionProbeVec[2],
                               VectorLength(g_anCollisionProbeVec, 0), 0);
      g_nSpyroGroundSlope = t;
      if (t < 0) {
        g_nSpyroGroundSlope = 0x400;
      }
      g_anSpyroAirborneFramesBlock[0x76] = g_nLastCollisionHitChunkId;
      UnpackWorldCollisionTri(g_nLastCollisionHitChunkId,
                              &g_anSpyroAirborneFramesBlock[0x77]);
      if (g_nSpyroGroundSlope < 0x21) {
        g_anSpyroAirborneFramesBlock[0x2d] = g_nSpyroGroundSlope >= 0x17;
        g_anSpyroAirborneFramesBlock[0] = 0;
        down[0] = 0x104;
        down[1] = 0;
        down[2] = -0x104;
        out[0] = 0x1c4;
        out[1] = 0;
        out[2] = -0x1c4;
        RotateVectorByMatrix(&g_anSpyroAirborneFramesBlock[-0x1a], down, down);
        AddVector(down, down, &g_anSpyroAirborneFramesBlock[-0x27]);
        ApplyActiveGteRotation(out, out);
        AddVector(out, out, &g_anSpyroAirborneFramesBlock[-0x27]);
        if (CastRayWorldAndActors(down, out) == 0) {
          g_anSpyroAirborneFramesBlock[0xa] = 1;
        }
      }
    } else {
      g_anSpyroAirborneFramesBlock[0x76] = -1;
    }
    if ((g_anSpyroAirborneFramesBlock[0] != 0) &&
        (g_nSpyroGroundProbeSuspendFlag == 0)) {
      down[0] = 0;
      down[1] = 0;
      down[2] = -0x104;
      out[0] = 0;
      out[1] = 0;
      out[2] = -0x1c4;
      AddVector(down, down, &g_anSpyroAirborneFramesBlock[-0x27]);
      AddVector(out, out, &g_anSpyroAirborneFramesBlock[-0x27]);
      if (CastRayWorldAndActors(down, out) != 0) {
        CopyVector(&g_anSpyroAirborneFramesBlock[2], g_anCollisionProbeVec);
        t = (signed char)ArcTan2(
            g_anSpyroGroundNormal[2],
            VectorLength(&g_anSpyroAirborneFramesBlock[2], 0), 0);
        g_nSpyroGroundSlope = t;
        if (t < 0) {
          g_nSpyroGroundSlope = 0x400;
        }
        /* The empty loop is load-bearing: its NOTE_INSN_LOOP_BEG ends cse's
           extended basic block, so the held block base below has no constant
           equivalence and the +0x14 / +0xB4 accesses keep their `off(s2)`
           register form instead of folding to absolute (cookbook §B-i). The
           first probe's slope test is spelled as the plain scalar
           g_nSpyroGroundSlope precisely because there the original DOES
           fold. */
        do {
        } while (0);
        if (g_anSpyroAirborneFramesBlock[5] < 0x21) {
          g_anSpyroAirborneFramesBlock[0] = 0;
          g_anSpyroAirborneFramesBlock[0x2d] =
              g_anSpyroAirborneFramesBlock[5] >= 0x17;
        }
      }
    }
  }
}
