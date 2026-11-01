#include "funcs.h"
#include "globals.h"
#include "main_shared.h"

/* Path-follow steering step (0x8003a16c, 0x2b4 bytes). Moves `actor` toward
 * waypoint path->cur of `path` (u8 count, u8 index, 16-byte waypoints at +8):
 * builds a byte euler {0, pitch-to-waypoint, yaw} where the yaw chases the
 * waypoint bearing, stepped at most +/-`clamp` per tick (movement stalls when
 * the remaining turn exceeds `limit`), advances by `speed` (capped at the
 * remaining distance) through the rotated basis, optionally applies a contact
 * probe of radius `contact`, then snaps to the ground within 0x400 below.
 * Within `radius` of the waypoint: clears the actor +0x49 latch, advances
 * (and wraps) the waypoint index, and returns index + 0x100; else returns 0.
 * `d` carries the turn delta and is reused for the ground height. */
int SteerActorAlongWaypoints(Actor *actor, PathData *path, int radius,
                             int speed, int contact, int clamp, int limit,
                             int *yawp) {
  char eul[8];
  int vec[4];
  int d;
  int mtx[8];
  int ret;
  SubtractVector(vec, &path->nodes[path->cur].x, &actor->posX);
  eul[0] = 0;
  eul[1] = ArcTan2(VectorLength(vec, VEC_LEN_XY), vec[2], ATAN_ANGLE8);
  if (((int)VectorLength(vec, VEC_LEN_XY)) < radius) {
    eul[2] = *yawp;
  } else {
    int yaw;
    d = ArcTan2(vec[0], vec[1], ATAN_ANGLE8);
    yaw = *yawp;
    d = (d - yaw) & 0xFF;
    if (d >= 0x81) {
      d -= 0x100;
    }
    if (limit < abs(d)) {
      speed = 0;
    }
    if (d < (-clamp)) {
      d = -clamp;
    }
    if (clamp < d) {
      d = clamp;
    }
    eul[2] = d + yaw;
    *yawp = (d + yaw) & 0xFF;
  }
  {
    int len = VectorLength(vec, VEC_LEN_XYZ);
    if (len < speed) {
      speed = len;
    }
  }
  ApplyEulerRotation(eul, mtx, 0);
  vec[0] = speed;
  vec[1] = 0;
  vec[2] = 0;
  RotateVectorByMatrix(mtx, vec, vec);
  AddVector(vec, &actor->posX, vec);
  if (contact != 0) {
    DispatchActorContactAtSphere(vec, contact, 0, 0, actor, 0);
  }
  vec[2] += 0x400;
  d = FindGroundHeightBelow(vec, 0x400);
  if (d != 0) {
    vec[2] = d;
  } else {
    vec[2] -= 0x400;
  }
  CopyVector(&actor->posX, vec);
  EncodeCachedVecToActorDirCode(actor);
  RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
  ret = 0;
  SubtractVector(vec, &path->nodes[path->cur].x, &actor->posX);
  if (((int)VectorLength(vec, VEC_LEN_XYZ)) < radius) {
    actor->subMode = 0;
    path->cur += 1;
    if (path->cur == path->count) {
      path->cur = 0;
    }
    ret = path->cur + 0x100;
  }
  return ret;
}
