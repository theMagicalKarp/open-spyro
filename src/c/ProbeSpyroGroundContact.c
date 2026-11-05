#include "funcs.h"
#include "globals.h"
#include "match_asm.h"
#include "spyro_block.h"
#include "spyro_state.h"

/* The whole Spyro ground/collision state block hangs off one base register,
   g_nSpyroAirborneFrames: BLK(i) is the Spyro-block word i (spyro_block.h)
   reached from it. */
#define BLK(i) g_anSpyroAirborneFramesBlock[(i) - SPY_AIRBORNE]

/* Per-frame ground probe. Casts a body-relative ray straight down from Spyro
   (and, on a shallow hit, a second wider one) through the world + actor
   collision set; on a hit it caches the contact normal, the slope angle from
   ArcTan2 and the hit chunk's triangle, and clears the airborne counter when
   the slope counts as ground (< SLOPE_GROUND_MAX). A third, purely vertical
   probe re-runs the test when the first two missed. (0x8003e318, 784 bytes.) */
void ProbeSpyroGroundContact(void) {
  int down[4];
  int out[4];
  int t;
  BLK(SPY_AIRBORNE) += 1;
  ZeroVector(&BLK(SPY_GROUND_NORMAL));
  g_anSpyroGroundNormal[2] = 0x1000;
  if (((!out) && (!out)) && (!out)) {
  }
  g_nSpyroGroundSlope = 0;
  BLK(SPY_CLIFF) = 0;
  if ((g_dwSpyroRequestMask & SPYRO_RQ_NO_CLIP) == 0) {
    down[0] = 0;
    out[0] = 0;
    down[1] = 0;
    out[1] = 0;
    down[2] = -0x104;
    out[2] = -0x1c4;
    RotateVectorByMatrix(&BLK(SPY_BODY_MTX), down, down);
    AddVector(down, down, &BLK(SPY_POS));
    ApplyActiveGteRotation(out, out);
    AddVector(out, out, &BLK(SPY_POS));
    if (CastRayWorldAndActors(down, out) != 0) {
      CopyVector(&BLK(SPY_GROUND_NORMAL), g_anCollisionProbeVec);
      t = (signed char)ArcTan2(g_anCollisionProbeVec[2],
                               VectorLength(g_anCollisionProbeVec, VEC_LEN_XY),
                               ATAN_ANGLE8);
      g_nSpyroGroundSlope = t;
      if (t < 0) {
        g_nSpyroGroundSlope = SLOPE_CEILING;
      }
      BLK(SPY_GROUND_TRI) = g_nCollisionHitTriIndex;
      UnpackWorldCollisionTri(g_nCollisionHitTriIndex, &BLK(SPY_GROUND_VERTS));
      if (g_nSpyroGroundSlope < SLOPE_GROUND_MAX) {
        BLK(SPY_GLIDE_SPENT) = g_nSpyroGroundSlope >= SLOPE_STEEP;
        BLK(SPY_AIRBORNE) = 0;
        down[0] = 0x104;
        down[1] = 0;
        down[2] = -0x104;
        out[0] = 0x1c4;
        out[1] = 0;
        out[2] = -0x1c4;
        RotateVectorByMatrix(&BLK(SPY_BODY_MTX), down, down);
        AddVector(down, down, &BLK(SPY_POS));
        ApplyActiveGteRotation(out, out);
        AddVector(out, out, &BLK(SPY_POS));
        if (CastRayWorldAndActors(down, out) == 0) {
          BLK(SPY_CLIFF) = 1;
        }
      }
    } else {
      BLK(SPY_GROUND_TRI) = -1;
    }
    if ((BLK(SPY_AIRBORNE) != 0) && (g_nSpyroGroundProbeSuspendFlag == 0)) {
      down[0] = 0;
      down[1] = 0;
      down[2] = -0x104;
      out[0] = 0;
      out[1] = 0;
      out[2] = -0x1c4;
      AddVector(down, down, &BLK(SPY_POS));
      AddVector(out, out, &BLK(SPY_POS));
      if (CastRayWorldAndActors(down, out) != 0) {
        CopyVector(&BLK(SPY_GROUND_NORMAL), g_anCollisionProbeVec);
        t = (signed char)ArcTan2(
            g_anSpyroGroundNormal[2],
            VectorLength(&BLK(SPY_GROUND_NORMAL), VEC_LEN_XY), ATAN_ANGLE8);
        g_nSpyroGroundSlope = t;
        if (t < 0) {
          g_nSpyroGroundSlope = SLOPE_CEILING;
        }
        /* The empty loop is load-bearing: its NOTE_INSN_LOOP_BEG ends cse's
           extended basic block, so the held block base below has no constant
           equivalence and the +0x14 / +0xB4 accesses keep their `off(s2)`
           register form instead of folding to absolute (cookbook §B-i). The
           first probe's slope test is spelled as the plain scalar
           g_nSpyroGroundSlope precisely because there the original DOES
           fold. */
        EBB_BREAK();
        if (BLK(SPY_GROUND_SLOPE) < SLOPE_GROUND_MAX) {
          BLK(SPY_AIRBORNE) = 0;
          BLK(SPY_GLIDE_SPENT) = BLK(SPY_GROUND_SLOPE) >= SLOPE_STEEP;
        }
      }
    }
  }
}
