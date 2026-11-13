#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "match_asm.h"
#include "spyro_block.h"
#include "spyro_state.h"
#include "vecmath.h"

/* Apply gravity projected along the current ground slope (0x8003e0b4, 0xF8).
   Take the ground normal, rescale it so its Z component equals one gravity
   step (ratio = step*nz/len), flip it downhill if it points up, then reset
   the slope-slide vector (SPY_SLOPE_SLIDE, normal block +0x2C) and subtract the
   tangent vector from it, recording the step in g_nSpyroSlopeGravityStep. No-op
   on a zero-length normal (free fall). */

void ApplyGravityAlongSpyroSlopeTangent(void) {
  int vec[3];
  int *p;
  int len;

  CopyVector(vec, g_anSpyroGroundNormal);
  len = VectorLength(vec, VEC_LEN_XYZ);
  if (len != 0) {
    ScaleVector3ByRatio(vec, len, (g_nSpyroGravityStep * vec[2]) / len);
    if (vec[2] > 0) {
      vec[2] = -vec[2];
      vec[0] = -vec[0];
      vec[1] = -vec[1];
    }
    p = g_anSpyroGroundNormal + 11;
    ZeroVector(p);
    g_nSpyroSlopeGravityStep = g_nSpyroGravityStep;
    SubtractVector(p, p, vec);
  }
}

/* Rebuild Spyro's gravity vector: copy the ground normal (reached off the
   gravity vector's own base), scale it to the absolute per-tick gravity
   step, then negate it into place (0x8003e1ac). */
void ApplyGravityAlongSpyroSlopeNormal(void) {
  int zero[4];
  int step;

  CopyVector(
      g_anSpyroSlopeGravityVec,
      SPY_VIA(g_anSpyroSlopeGravityVec, SPY_SLOPE_GRAVITY, SPY_GROUND_NORMAL));
  step = g_nSpyroGravityStep;
  if (step < 0) {
    step = -step;
  }
  SetVector3Magnitude(g_anSpyroSlopeGravityVec, step);
  ZeroVector(zero);
  SubtractVector(g_anSpyroSlopeGravityVec, zero, g_anSpyroSlopeGravityVec);
}

/* Airborne probe for a surface ahead of Spyro (0x8003e218, 0x100; JUMP /
   LIFT / FALL / GLIDE_DROP when no wall was touched this step). Cast a ray
   from Spyro's body anchor (SPYRO_RADIUS down in body space) to a point
   0x1C4 ahead / 0x1A4 down; on a hit whose surface normal rises at least
   0x17 (8-bit angle) above horizontal, i.e. a surface no steeper than a
   wall, latch the same wall-contact pair AdvanceSpyroPhysics sets on a wall
   slide: g_nSpyroWallSlideContributedFlag = 1 and the contact normal as the
   nudge (g_anSpyroWallSlideNudgeAccum). Spyro 1 has no ledge grab; the
   integrator just slides his velocity along that normal. */

void ProbeSpyroLedgeForward(void) {
  int from[4];
  int to[3];
  int *anchor;
  int *tp;
  int *cp;
  unsigned int len;
  from[2] = -SPYRO_RADIUS;
  to[0] = 0x1C4;
  from[1] = 0;
  from[0] = 0;
  to[1] = 0;
  to[2] = -0x1A4;
  RotateVectorByMatrix(g_anSpyroBodyMtx, from, from);
  anchor = SPY_VIA(g_anSpyroBodyMtx, SPY_BODY_MTX, SPY_POS);
  AddVector(from, from, anchor);
  /* The do / while (0) ends cse's extended basic block (A200): the
     match needs the block boundary, not the loop. */
  do {
    tp = to;
    ApplyActiveGteRotation(tp, tp);
  } while (0);
  AddVector(tp, tp, anchor);
  if (CastRayWorldAndActors(from, tp) != 0) {
    cp = g_anCollisionHitNormal;
    len = VectorLength(cp, VEC_LEN_XY);
    if (((signed char)ArcTan2(g_anCollisionHitNormal[2], len, ATAN_ANGLE8)) >=
        0x17) {
      g_anSpyroBodyMtx[32] = 1;
      CopyVector(SPY_VIA(g_anSpyroBodyMtx, SPY_BODY_MTX, SPY_NUDGE), cp);
    }
  }
}

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
      CopyVector(&BLK(SPY_GROUND_NORMAL), g_anCollisionHitNormal);
      t = (signed char)ArcTan2(g_anCollisionHitNormal[2],
                               VectorLength(g_anCollisionHitNormal, VEC_LEN_XY),
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
        CopyVector(&BLK(SPY_GROUND_NORMAL), g_anCollisionHitNormal);
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

#undef BLK

/* Average the frame-to-frame motion of the collision tri Spyro stands on
   (0x8003e628, 0x130). Unpacks the current ground tri, subtracts the
   previous frame's vertices, averages the three deltas into the pending
   platform impulse, and suppresses the camera spring when the platform
   moved far enough. */

void AverageSpyroPlatformMotion(void) {
  int tri[10];
  int avg[3];
  int *chunk = g_anSpyroGroundChunkBlock;
  int *v;
  int *w;
  int *sum;
  int i;

  v = avg;
  if (chunk[0] >= 0) {
    ZeroVector(v);
    UnpackWorldCollisionTri(chunk[0], tri);
    i = 0;
    sum = v;
    chunk += 1;
    w = tri;
    do {
      SubtractVector(w, w, chunk);
      AddVector(sum, sum, w);
      chunk += 3;
      i += 1;
      w += 3;
    } while (i < 3);
    avg[0] = avg[0] / 3;
    avg[1] = avg[1] / 3;
    avg[2] = avg[2] / 3;
    CopyVector(g_anSpyroPendingImpulse, sum);
    if (VectorLength(sum, VEC_LEN_XYZ) >= 0x21) {
      g_nCameraSpringSuppressFlag = 1;
    }
  }
}

/* shared-base view of g_anSpyroBodyMtx (0x80078a8c): the matrix arg, the
   world position at -0x34 (g_anSpyroWorldPos) and the airborne counter at
   +0x68 (g_nSpyroAirborneFrames) are all reached off one held register. */

/* 0x8003e758 (436 bytes) — snap Spyro onto the floor directly below: cast a
   ray body-down (from +0x80 above to -0x164 below, rotated through the body
   matrix), and on a hit nudge the world position by the resolved delta
   (axis components under 8 units are dropped) and clear the airborne
   counter; on a miss count another airborne frame. */

void StepSpyroOntoLocalFloor(void) {
  int down[3];
  int fore[3];
  int aft[3];
  int idx;
  int *mtx;
  int *pos;
  down[0] = 0;
  down[1] = 0;
  down[2] = -SPYRO_RADIUS;
  CopyVector(aft, down);
  aft[2] -= 0x80;
  CopyVector(fore, down);
  fore[2] += 0x80;
  mtx = g_anSpyroBodyMtxBlock;
  RotateVectorByMatrix(mtx, aft, aft);
  pos = &mtx[-13];
  AddVector(aft, pos, aft);
  ApplyActiveGteRotation(fore, fore);
  AddVector(fore, pos, fore);
  ApplyActiveGteRotation(down, down);
  AddVector(down, pos, down);
  if ((CastRayWorldAndActors(fore, aft) != 0) &&
      (g_anCollisionHitNormal[2] > 0)) {
    idx = 26;
    SubtractVector(down, g_anCollisionResolvedPos, down);
    if (((!down) && (!down)) && (!down)) {
    }
    if (abs(down[0]) < 8) {
      down[0] = 0;
    }
    if (abs(down[1]) < 8) {
      down[1] = 0;
    }
    if (abs(down[2]) < 8) {
      down[2] = 0;
    }
    AddVector(pos, pos, down);
    mtx[idx] = 0;
  } else {
    int *air = g_anSpyroAirborneFramesBlock;
    air[0] += 1;
  }
}

/* The tail's `lh; sra 4` pair is A233 (shift counts sh0/sh1 assigned
   outside the if-block). The loop is a goto loop: the original strength-
   reduces nothing (cos through a hand-advanced pointer, sine indexed by `a`)
   and recomputes `la g_anSineLut` inside the body for the tail's sine read.
   `a` is reused for the nudge angle (the original keeps both in s0).
   Allocation: sinTab is a `$18` register local (as a pseudo, its `la` is a
   3-ref cse temp that ranks last) and `pto` a `$20` one, set before `cosp`. */

/* 0x8003e90c (0x15c) — probe four rays around Spyro (at angles 0x20 +
   0x40*k, from 0x124 to 0x1a4 below his position); the bitmask of blocked
   probes selects a nudge angle from g_anSpyroWallNudgeAngleByProbeMask, and
   a non-negative angle pushes the motion vector (world pos +0xF4/+0xF8)
   1/16 along it. */
void NudgeSpyroFromWallProbes(void) {
  int from[3];
  int pad;
  int to[3];
  int mask;
  int i;
  int a;
  short *cosp;
  register short *sinTab asm("$18");
  int *pos;
  register int *pto asm("$20");
  int sh0;
  int sh1;

  mask = 0;
  a = 0x20;
  i = 0;
  pos = g_anSpyroWorldPos;
  pto = to;
  cosp = &g_anCosineLut[0x20];
  sh0 = 4;
  sh1 = 4;
probe:
  from[0] = to[0] = *cosp >> 4;
  from[1] = to[1] = g_anSineLut[a] >> 4;
  from[2] = -0x124;
  to[2] = -0x1a4;
  AddVector(from, from, pos);
  AddVector(pto, pto, pos);
  sinTab = g_anSineLut;
  if (CastRayWorldAndActors(from, pto) != 0) {
    mask |= 1 << i;
  }
  cosp += 0x40;
  i++;
  a += 0x40;
  if (i < 4) {
    goto probe;
  }
  a = g_anSpyroWallNudgeAngleByProbeMask[mask];
  if (a >= 0) {
    pos[0x3d] += g_anCosineLut[a] >> sh0;
    pos[0x3e] += sinTab[a] >> sh1;
  }
}

/* Views this function reads through (declared in globals.h):
   g_anSpyroBodyMtx — the matrix pointer, the -0x34 (== -13 words)
     g_anSpyroWorldPos AddVector argument and the +0x13C (== +0x4F words)
     trigger read all come off one held base;
   g_anSpyroShadowRingIndexBlock — shared-base view of g_nSpyroShadowRingIndex
     (0x8007aa30): the index RMW plus the -0x18 no-ground-flag and -0x20 height
     byte rings;
   g_anSpyroShadowHidden[1] — sized-array alias of g_nSpyroShadowHiddenFlag
     (0x8007aa34), see A134: only an array lvalue keeps the death-plane table
     load below this store. */

/* arr[g_nSpyroShadowRingIndex], spelled index + base: the array form swaps
   the addu operands. */
#define RING_SLOT(arr)                                                         \
  (*(unsigned char *)(g_nSpyroShadowRingIndex + (int)(arr)))

/* 0x80049fac (596 bytes) — per-tick Spyro shadow-ring sampler. Computes the
   shadow centre ((0,0,-0x164) through the body matrix + world pos) into
   g_anSpyroShadowPos with Z = ground height, refreshes the shadow OT-bin
   biases and the hidden flag, then advances the 8-slot ring index and probes
   ONE ring vertex: ground within +/-0x80 of the probe Z stores the scaled
   height (diff * 0xC2 / 0x200), else raises the no-ground flag. probe == 0
   clears both slots without sampling. Consumed by DrawSpyroDropShadow. */
void SampleSpyroShadowRingHeights(int probe) {
  int v[3];
  int *ring;
  int gz;
  int idx;
  int ground;
  int d;

  v[0] = 0;
  v[1] = 0;
  v[2] = -SPYRO_RADIUS;
  RotateVectorByMatrix(g_anSpyroBodyMtx, v, v);
  AddVector(v, v, SPY_VIA(g_anSpyroBodyMtx, SPY_BODY_MTX, SPY_POS));
  CopyVector(g_anSpyroShadowPos, v);
  gz = g_nSpyroGroundHeightZ;
  g_anSpyroShadowPos[2] = gz;
  if (g_bSpyroOtBinBias < SPYRO_OT_BIAS_PINNED) {
    g_bSpyroOtBinBias = SPYRO_OT_BIAS_DEFAULT;
  }
  g_nSpyroShadowOtBinBias = SHADOW_OT_BIAS;
  if ((g_anSpyroWorldPos[2] - gz) >= SHADOW_HIGH_ABOVE) {
    g_nSpyroShadowOtBinBias = SHADOW_OT_BIAS_HIGH;
  }
  /* The empty loop is load-bearing: its NOTE_INSN_LOOP_BEG ends cse's extended
     basic block, so the held matrix base below has no constant equivalence and
     the +0x13C read keeps the `lw v0,0x13C(s0)` register form instead of
     folding to an absolute address (see the cookbook's B-i fold). */
  EBB_BREAK();
  g_anSpyroShadowHidden[0] = 0;
  if ((g_anLevelDeathPlaneZTable[g_nLevelIndex] >= gz) ||
      (g_anSpyroBodyMtx[0x4F] != 0)) {
    g_anSpyroShadowHidden[0] = 1;
  }
  ring = g_anSpyroShadowRingIndexBlock;
  idx = (ring[0] + 1) & (SHADOW_RING_VERTS - 1);
  ring[0] = idx;
  if (probe != 0) {
    RotateVectorByMatrix(g_anSpyroBodyMtx,
                         &g_anSpyroShadowProbeOffsetTable[idx * 3], v);
    AddVector(v, v, SPY_VIA(g_anSpyroBodyMtx, SPY_BODY_MTX, SPY_POS));
    v[2] += SHADOW_PROBE_LIFT;
    ground = FindGroundHeightBelow(v, SHADOW_PROBE_DEPTH);
    v[2] -= SHADOW_PROBE_LIFT;
    if (((unsigned int)((v[2] - ground) + SHADOW_PROBE_WINDOW)) >=
        2 * SHADOW_PROBE_WINDOW) {
      char *flags = (char *)ring - 0x18;
      RING_SLOT(flags) = 1;
    } else {
      char *flags = (char *)ring - 0x18;
      char *heights = (char *)ring - 0x20;
      RING_SLOT(flags) = 0;
      d = (g_anSpyroShadowPos[2] - ground) * SHADOW_HEIGHT_SCALE;
      RING_SLOT(heights) = d / 0x200;
      return;
    }
  } else {
    char *flags = (char *)ring - 0x18;
    RING_SLOT(flags) = 0;
  }
  {
    char *heights = (char *)ring - 0x20;
    RING_SLOT(heights) = 0;
  }
}

#undef RING_SLOT
