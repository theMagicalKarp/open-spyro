#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "match_asm.h"

/* 0x8003bfc0 — advance an actor one sub-step along its waypoint ring.
   `path` is the waypoint block (count byte [0], cursor byte [1], vec3s at
   +8 + i*0x10) and `step` the sub-step counter within the current leg, which
   takes `legs` frames. Once the counter reaches the leg length the cursor
   advances (wrapping to 0, returning 2 instead of 1) and the raw leg vector
   is published in `prev`. Otherwise the remaining leg is divided into the
   frames still to run; for the first steps of a leg that share of the new
   leg is cross-faded with `prev` through the per-length weight table
   (BlendVectorPairByMode's tables). Flags bit 2 keeps the vertical component.
   The step vector is folded into the actor position and its yaw/pitch bytes
   (+0x46/+0x45) are re-aimed along it.
   Match notes: the empty do/while(0) keeps the idx load ahead of the *step
   load (the original's load-delay nop); `ret++` precedes the cursor test so
   reorg puts it in the branch delay slot; the divisor is a `$6` register
   local (the original's a2). */
int TweenActorAlongPath(Actor *actor, PathData *path, int *prev, int *step,
                        int legs, int flags) {
  int dir[4];
  int blend[4];
  short *tbl;
  int *pos;
  int idx;
  int t;
  int lim;
  int x;
  int n;
  register int span asm("$6");
  int ret;

  ret = 0;
  if (legs >= 9) {
    tbl = g_anTweenBlendWeights8;
    lim = 7;
  } else if (legs >= 7) {
    tbl = g_anTweenBlendWeights6;
    lim = 5;
  } else {
    lim = 1;
    if (legs >= 5) {
      tbl = g_anTweenBlendWeights4;
      lim = 3;
    } else {
      tbl = g_anTweenBlendWeights2;
    }
  }

  idx = path->cur;
  EBB_BREAK();
  t = *step;
  if (t >= legs) {
    SubtractVector(dir, &path->nodes[idx].x, &actor->posX);
    CopyVector(prev, dir);
    ret++;
    if (++path->cur >= path->count) {
      path->cur = 0;
      ret++;
    }
    *step = 0;
  } else if (lim < t) {
    SubtractVector(dir, &path->nodes[idx].x, &actor->posX);
    n = *step - 1;
    span = legs - n;
    dir[0] = dir[0] / span;
    dir[1] = dir[1] / span;
    dir[2] = dir[2] / span;
    *step = *step + 1;
  } else {
    SubtractVector(dir, &path->nodes[idx].x, &actor->posX);
    n = *step - 1;
    span = legs - n;
    dir[0] = dir[0] / span;
    dir[1] = dir[1] / span;
    dir[2] = dir[2] / span;
    ScaleVector3Sat(dir, dir, ((short *)((*step << 2) + (int)tbl))[0]);
    ScaleVector3Sat(blend, prev, ((short *)((*step << 2) + (int)tbl))[1]);
    AddVector(dir, dir, blend);
    RShiftVector3(dir, 0xa);
    *step = *step + 1;
  }

  if (!(flags & TWEEN_FLY)) {
    dir[2] = 0;
  }
  actor->rotZ = ArcTan2_8bit(dir[0], dir[1]);
  x = ArcTan2_8bit(dir[2], VectorLength(dir, VEC_LEN_XY));
  pos = &actor->posX;
  actor->rotY = -x + 0x40;
  AddVector(pos, pos, dir);
  RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
  return ret;
}
