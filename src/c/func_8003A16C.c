#include "globals.h"

extern void SubtractVector();
extern unsigned int VectorLength();
extern int ArcTan2(int y, int x, int high_precision);
extern int abs(int n);
extern void ApplyEulerRotation();
extern void RotateVectorByMatrix();
extern void AddVector();
extern void DispatchActorContactAtSphere();
extern int FindGroundHeightBelow();
extern void CopyVector();
extern void EncodeCachedVecToActorDirCode(char *actor);
extern void func_800529E4();

/* Path-follow steering step (0x8003a16c, 0x2b4 bytes). Moves `actor` toward
 * waypoint path[1] of `path` (u8 count, u8 index, 16-byte waypoints at +8):
 * builds a byte euler {0, pitch-to-waypoint, yaw} where the yaw chases the
 * waypoint bearing, stepped at most +/-`clamp` per tick (movement stalls when
 * the remaining turn exceeds `limit`), advances by `speed` (capped at the
 * remaining distance) through the rotated basis, optionally applies a contact
 * probe of radius `contact`, then snaps to the ground within 0x400 below.
 * Within `radius` of the waypoint: clears the actor +0x49 latch, advances
 * (and wraps) the waypoint index, and returns index + 0x100; else returns 0.
 * `d` carries the turn delta and is reused for the ground height. */
int func_8003A16C(char *actor, unsigned char *path, int radius, int speed,
                  int contact, int clamp, int limit, int *yawp) {
  char eul[8];
  int vec[4];
  int d;
  int mtx[8];
  int ret;
  SubtractVector(vec, &path[path[1] * 0x10 + 8], actor + 0xC);
  eul[0] = 0;
  eul[1] = ArcTan2(VectorLength(vec, 0), vec[2], 0);
  if (((int)VectorLength(vec, 0)) < radius) {
    eul[2] = *yawp;
  } else {
    int yaw;
    d = ArcTan2(vec[0], vec[1], 0);
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
    int len = VectorLength(vec, 1);
    if (len < speed) {
      speed = len;
    }
  }
  ApplyEulerRotation(eul, mtx, 0);
  vec[0] = speed;
  vec[1] = 0;
  vec[2] = 0;
  RotateVectorByMatrix(mtx, vec, vec);
  AddVector(vec, actor + 0xC, vec);
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
  CopyVector(actor + 0xC, vec);
  EncodeCachedVecToActorDirCode(actor);
  func_800529E4(actor, 2);
  ret = 0;
  SubtractVector(vec, &path[path[1] * 0x10 + 8], actor + 0xC);
  if (((int)VectorLength(vec, 1)) < radius) {
    actor[0x49] = 0;
    path[1] += 1;
    if (path[1] == path[0]) {
      path[1] = 0;
    }
    ret = path[1] + 0x100;
  }
  return ret;
}
