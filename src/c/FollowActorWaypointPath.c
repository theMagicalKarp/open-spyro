#include "actor_type.h"
#include "globals.h"
#include "main_shared.h"
#include "vecmath.h"

extern void SteerActorTowardPoint(unsigned char *pos, int *wp, int speed, int a,
                                  int b, int c);

/* 0x8003c994 (0x130) — advance an actor along its waypoint path record:
   measure the distance from `pos`+0xC to the current waypoint (path index
   byte +0x1, 0x10-byte waypoint records from +0x8); when closer than
   `threshold`, step the index (+0x6 halfword == -1 selects forward wrap
   [0..count), else backward wrap to count-1); then steer toward the (possibly
   new) waypoint with speed = dist>>4 clamped to `cap`. */
void FollowActorWaypointPath(unsigned char *pos, PathData *path, int threshold,
                             int cap, int radius, int yawLim, int pitchLim) {
  int v[3];
  int dist;

  SubtractVector(v, (int *)(pos + 0xC), &path->nodes[path->cur].x);
  dist = VectorLength(v, 1);
  if (dist < threshold) {
    if (path->reversed == -1) {
      path->cur += 1;
      if (path->cur == path->count) {
        path->cur = 0;
      }
    } else {
      path->cur -= 1;
      if (path->cur == 0xFF) {
        path->cur = path->count - 1;
      }
    }
  }

  dist = dist >> 4;
  if (cap < dist) {
    dist = cap;
  }
  SteerActorTowardPoint(pos, &path->nodes[path->cur].x, dist, yawLim, pitchLim,
                        radius);
}
