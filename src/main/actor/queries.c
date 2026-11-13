#include "actor_states.h"
#include "actor_type.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "vecmath.h"

/* True when the actor's byte yaw (rotZ) points within `threshold` of the
   direction from the actor to Spyro.
   Unreferenced: no call, branch, pointer or table entry in main or any overlay
   (xref). */
int IsActorFacingSpyro(Actor *actor, int threshold) {
  return AbsAngleDelta8(ArcTan2(g_anSpyroWorldPos[0] - actor->posX,
                                g_anSpyroWorldPos[1] - actor->posY,
                                ATAN_ANGLE8),
                        actor->rotZ) < threshold;
}

/* Ray-march occlusion probe from Spyro toward target (0x80038250): like
   CheckLineOfSightSegment but sources the start point from g_anSpyroWorldPos
   and refines the segment length via the sqrt LUT before stepping. */
unsigned int CheckSpyroLineOfSightTo(int *target) {
  int seg[4];
  int cur[4];
  int next[4];
  int len;
  int steps;
  int i;

  SubtractVector(seg, g_anSpyroWorldPos, target);
  len = VectorLength(seg, VEC_LEN_XYZ);
  RefineSqrtEstimate(seg, len, 1);
  ScaleVector3ByRatio(seg, len, 0x400);
  CopyVector(cur, target);
  steps = len >> 10;
  for (i = 0; i < steps; i++) {
    AddVector(next, cur, seg);
    if (CastRayWorldAndActors(cur, next) != 0) {
      return 0;
    }
    CopyVector(cur, next);
  }
  return 1;
}

/* Probe the world floor below the actor's collision point (+0xC), temporarily
   biasing the cached Z at +0x14 up by 0x5DC across the probe call, then
   restoring it. Returns the probed ground height. The bias is reloaded after
   the call (into $v1) since the probe may touch the actor's memory while the
   probe result stays live in $v0 for the return. */
int ProbeActorGroundHeight(Actor *actor) {
  int ground;
  actor->posZ += 0x5DC;
  ground = FindGroundHeightBelow(&actor->posX, 0x1000);
  actor->posZ -= 0x5DC;
  return ground;
}

/* Floor-probe at the actor's collision point (+0xC) with a temporary +0x5DC Z
   bias, then test whether the probed surface is near-flat: the 8-bit slope
   angle of the hit normal (its Z vs planar length) must read below
   0x18. */
int IsActorGroundFlat(Actor *actor) {
  actor->posZ += 0x5DC;
  FindGroundHeightBelow(&actor->posX, 0x1000);
  actor->posZ -= 0x5DC;
  return (signed char)ArcTan2_8bit(
             g_anCollisionHitNormal[2],
             VectorLength(g_anCollisionHitNormal, VEC_LEN_XY)) < 0x18;
}

/* Like ProbeActorGroundHeight but with a caller-supplied Z bias: bump the
   cached Z at +0x14 by `delta` across the floor probe (whose search ceiling is
   also raised by `delta`), then restore it. Returns the probed ground height.
 */
int ProbeActorGroundHeightRaised(Actor *actor, int delta) {
  int ground;
  actor->posZ += delta;
  ground = FindGroundHeightBelow(&actor->posX, delta + 0x1000);
  actor->posZ -= delta;
  return ground;
}

/* Set the actor's posZ to the floor below it plus its signed floorOffset,
   then re-encode its packed ground code. */
void SnapActorToGround(Actor *actor) {
  actor->posZ = ProbeActorGroundHeight(actor) + actor->floorOffset;
  EncodeHitNormalToActorGroundCode(actor);
}

/* Test whether an actor is one of the checkpoint kill set (0x80038494,
   0x88 bytes): its slot index in the actor pool selects a bit in
   g_anCheckpointKillBitmap, and the actor only counts when its dropKind is 0xFF
   (the random gem roll).

   The word index and the bit index must be separate locals, in that order:
   they are the only two independent insns in the block and they are emitted in
   declaration order. The bit test also has to be a VALUE temp — written
   directly as a condition, fold rewrites `(x & (1 << b)) != 0` into
   `(x >> b) & 1` and emits `srav` with the unmasked index. */
int IsActorInCheckpointKillSet(Actor *actor) {
  int idx = actor - g_pActorListBase;
  int hit = 0;

  {
    int word = idx >> 5;
    int bit = idx & 0x1F;
    int t = g_anCheckpointKillBitmap[word] & (1 << bit);
    if (t != 0) {
      hit = actor->dropKind == DROP_GEM_ROLL;
    }
  }
  return hit;
}

/* Find the path point nearest to `actor`'s position. Returns the
   smallest 2D distance and, when `out_index` is non-NULL, stores the winning
   record's index. */
int FindNearestPathPointToActor(Actor *actor, PathData *path, int *out_index) {
  int best = 0xFFFFFF;
  int best_i;
  int i;
  for (i = 0; i < path->count; i++) {
    int d = ApproxDist2D(&actor->posX, &path->nodes[i].x);
    if (d < best) {
      best = d;
      best_i = i;
    }
  }
  if (out_index != 0) {
    *out_index = best_i;
  }
  return best;
}

/* Find the path point nearest to Spyro: list[0] is the point count and each
   0x10-byte record starts at offset 8. Returns the smallest 2D distance and,
   when `out_index` is non-NULL, stores the winning record's index. */
int FindNearestPathPointToSpyro(PathData *path, int *out_index) {
  int best = 0xFFFFFF;
  int best_i;
  int i;
  for (i = 0; i < path->count; i++) {
    int d = ApproxDist2D(g_anSpyroWorldPos, &path->nodes[i].x);
    if (d < best) {
      best = d;
      best_i = i;
    }
  }
  if (out_index != 0) {
    *out_index = best_i;
  }
  return best;
}

/* Index of the list point farthest from Spyro: list[0] is the point count and
   each 0x10-byte record starts at offset 8. Returns the winning record's
   index (the running maximum 2D distance starts at 0). */
int FindFarthestPathPointFromSpyro(PathData *path) {
  int best = 0;
  int best_i;
  int i;
  for (i = 0; i < path->count; i++) {
    int d = ApproxDist2D(g_anSpyroWorldPos, &path->nodes[i].x);
    if (best < d) {
      best = d;
      best_i = i;
    }
  }
  return best_i;
}

/* 0x80038c4c (0x108) — rotate the XY delta between `pos` and the box's
   centre into the box's frame using its yaw (sine-LUT index), and return
   whether the rotated point lies inside the box's half-width / half-depth. The
   rotated |x|/|y| pair is also written to a scratch pair (never read back). */
int IsPointInRotatedBox(int *pos, TriggerBox *box) {
  volatile int out[4];
  int delta[3];
  short *cosp;
  short *sinp;
  int angle;
  int dx;
  int dy;
  int rx;
  int ry;
  int ay;

  angle = box->yaw;
  SubtractVector(delta, pos, &box->x);
  cosp = &g_anCosineLut[angle];
  sinp = &g_anSineLut[angle];
  dx = delta[0];
  dy = delta[1];

  rx = ((dx * *cosp) >> 12) - ((dy * *sinp) >> 12);
  out[0] = rx;
  ry = ((dx * *sinp) >> 12) + ((dy * *cosp) >> 12);
  rx = abs(rx);
  out[0] = rx;
  ay = abs(ry);
  out[1] = ry;
  out[1] = ay;

  {
    /* Hard-register pins (A374 / F14j): each holds the register the
       original uses for it, which settles the competing temps. */
    register int r asm("$2") = 0;

    if (rx < box->halfWidth) {
      r = ay < box->halfDepth;
    }
    return r;
  }
}

/* Load-bearing forms: volatile out[4] keeps both dead stores per slot; abs()
   (not if/ternary) expands via single-insn abssi2 so the ry mult pipeline
   interleaves across the x-abs + store; g_anCosineLut alias (sym at
   lut+0x80) stops cse deriving the sine base via addiu -0x80. The result is
   a `$2` register variable set to 0 BEFORE the halfWidth load: with $2 live
   there, local-alloc cannot give halfWidth and halfDepth the same v0, so
   halfWidth takes v1 and ay drops to a1. As a pseudo the same `r = 0` form
   costs a move. */

/* |dot(plane.xyz, pos) >> 10 + plane.w|: the distance of `pos` from a plane
   stored as a 1.10 normal (x, y, z) plus offset (w) (0x80038d54). The unused
   `t` keeps the original's stack frame size. */
int PlaneDistanceAbs(int *pos, int *plane) {
  int r;
  int t[4];
  r = ((plane[0] * pos[0]) + (plane[1] * pos[1]) + (plane[2] * pos[2])) >> 10;
  r += plane[3];
  return r >= 0 ? r : -r;
}
