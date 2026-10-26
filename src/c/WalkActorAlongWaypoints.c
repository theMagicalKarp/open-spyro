#include "funcs.h"
#include "main_shared.h"
/* 0x8003A420 (size 0x300): actor waypoint-follow step. Steers an actor toward
 * its current waypoint (path->nodes[path->cur]), clamps the
 * per-frame turn to +/-maxTurn, moves by the (matrix-rotated) capped speed,
 * resolves ground contact, then advances to the next waypoint when close
 * enough.
 *
 * The ground-probe result is a `$3` register local: as a pseudo its call-result
 * copy gives it a hard v0 preference, find_reg takes v0, and jump.c then
 * cross-jumps the two `vec[2] =` stores (-3 insns).  `ret = 0` sits ABOVE the
 * last SubtractVector so ret's range overlaps arriveDist's and it takes a fresh
 * callee-saved (s4) instead of sharing arriveDist's s1.  `abs()` is the builtin
 * (A160). */

int WalkActorAlongWaypoints(Actor *actor, PathData *path, int arriveDist,
                            int cap, int contactRadius, int maxTurn,
                            int stallAngle, int *smoothVel) {
  unsigned char eul[3];
  int vec[3];
  int mtx[8];
  int len;
  int base;
  int delta;
  int absv;
  int ret;

  SubtractVector(vec, &path->nodes[path->cur].x, &actor->posX);
  eul[0] = 0;
  eul[1] = ArcTan2(VectorLength(vec, 0), vec[2], 0);
  if ((int)VectorLength(vec, 0) < arriveDist) {
    eul[2] = actor->rotZ;
  } else {
    delta = ArcTan2(vec[0], vec[1], 0);
    base = actor->rotZ;
    delta = (delta - base) & 0xFF;
    if (delta >= 0x81) {
      delta -= 0x100;
    }
    absv = abs(delta);
    if (stallAngle < absv) {
      cap = 0;
    }
    if (delta < -maxTurn) {
      delta = -maxTurn;
    }
    if (maxTurn < delta) {
      delta = maxTurn;
    }
    eul[2] = base + delta;
    actor->rotZ = base + delta;
  }
  len = VectorLength(vec, 1);
  if (len < cap) {
    cap = len;
  }
  ApplyEulerRotation((unsigned int *)eul, (unsigned int *)mtx, 0);
  vec[0] = cap;
  vec[1] = 0;
  vec[2] = 0;
  RotateVectorByMatrix(mtx, vec, vec);
  if (smoothVel != 0) {
    AddVector(smoothVel, smoothVel, vec);
    CopyVector(vec, smoothVel);
    RShiftVector3(vec, 2);
    SubtractVector(smoothVel, smoothVel, vec);
    AddVector(vec, &actor->posX, smoothVel);
  } else {
    AddVector(vec, &actor->posX, vec);
  }
  if (contactRadius != 0) {
    DispatchActorContactAtSphere(vec, contactRadius, 0, 0, (int)actor, 0);
  }
  vec[2] += 0x400;
  {
    register int ground asm("$3");
    ground = FindGroundHeightBelow(vec, 0x400);
    if (ground != 0) {
      vec[2] = ground;
    } else {
      vec[2] -= 0x400;
    }
  }
  CopyVector(&actor->posX, vec);
  EncodeCachedVecToActorDirCode((int)actor);
  RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
  ret = 0;
  SubtractVector(vec, &path->nodes[path->cur].x, &actor->posX);
  if ((int)VectorLength(vec, 1) < arriveDist) {
    actor->subMode = 0;
    path->cur = path->cur + 1;
    if ((path->cur & 0xFF) == path->count) {
      path->cur = 0;
    }
    ret = path->cur + 0x100;
  }
  return ret;
}
