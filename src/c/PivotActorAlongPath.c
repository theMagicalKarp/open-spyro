#include "actor_type.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "vecmath.h"

/* Waypoint path-follow driver (0x80039e94, 0x2d8). Steers the actor
   toward its current path node (path->cur = node index, 16-byte node
   records at +8/+0xC/+0x10). On arrival (2D distance and, with flags
   bit 8, Z distance within `thresh`) advances the node index — forward
   with wrap when the path's +6 halfword is -1, backward with wrap to
   path->count-1 otherwise — and reports it as 0x100+index in the return.
   When the heading delta to the node exceeds `lim`, turns in place via
   ApproachAngle8 (deferring the turn when another listed actor of the
   same `kind` is already mid-turn); otherwise marks the actor moving
   and delegates the step to TurnActorYawTowardAngle + MoveActorAlongHeading. */

#define ACTOR_YAW(a) ACTOR_RAW(a, unsigned char, rotZ)
#define ACTOR_TURNSTATE(a) ACTOR_RAW(a, unsigned char, subMode)

/* Turn the actor's yaw toward `ang`, at most `step` per frame. */
#define TurnActorTowardAngle(actor, ang, step, lim)                            \
  do {                                                                         \
    ACTOR_YAW(actor) = ApproachAngle8(ang, ACTOR_YAW(actor), step, lim);       \
  } while (0)

int PivotActorAlongPath(int *actor, PathData *path, int thresh, int speed,
                        int group, int step, int lim, int kind, int flags) {
  int ret;
  int zclose;
  int ang;
  int idx;
  int wrap;
  int *node;
  void **list;
  unsigned char *peer;

  if (g_nActorFrameDelta == 3) {
    speed += speed >> 1;
  } else if (g_nActorFrameDelta == 4) {
    speed <<= 1;
  }

  ret = 0;
  if (flags & 0x100) {
    zclose = abs(actor[5] - path->nodes[path->cur].z) < thresh;
  } else {
    zclose = 1;
  }

  if (ApproxDist2D(actor + 3, &path->nodes[path->cur].x) < thresh &&
      zclose != 0) {
    ACTOR_TURNSTATE(actor) = 0;
    if (path->reversed == -1) {
      wrap = path->count;
      idx = path->cur + 1;
      path->cur = idx;
      if ((idx & 0xFF) == wrap) {
        path->cur = 0;
      }
    } else {
      wrap = 0xFF;
      idx = (path->cur = path->cur - 1);
      if ((idx & 0xFF) == wrap) {
        path->cur = path->count - 1;
      }
    }
    ret = path->cur + 0x100;
  }

  node = (int *)((char *)path + (path->cur << 4)); /* node x/y at [2]/[3] */
  ang = ArcTan2(node[2] - actor[3], node[3] - actor[4], 0);

  if (AbsAngleDelta8(ang, ACTOR_YAW(actor)) <= lim) {
    ACTOR_TURNSTATE(actor) = 1;
    TurnActorYawTowardAngle(actor, ang, step, lim, 1);
    MoveActorAlongHeading(actor, speed, group, 0, flags);
    goto done;
  }

  if (ACTOR_TURNSTATE(actor) == 0 && kind != 0xFF) {
    /* Only one actor of this kind turns at a time — if a peer is already
       mid-turn (state 1), defer to it. */
    list = g_apSaveMenuAnimActorListBlock;
    while ((peer = (unsigned char *)(*(list++))) != 0) {
      if (peer[0x43] == kind && (unsigned int)peer[0x48] < 0x80 &&
          peer[0x49] == 1) {
        break;
      }
    }
    if (peer != 0) {
      goto done;
    }
    TurnActorTowardAngle(actor, ang, step, lim);
    ACTOR_TURNSTATE(actor) = 2;
  } else {
    TurnActorTowardAngle(actor, ang, step, lim);
  }

done:
  return ret;
}
