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
   ApproachAngle8 (deferring the turn when another live actor whose group is
   `turnGroup` is walking; 0xFF = no gating); otherwise marks the actor
   walking and delegates the step to TurnActorYawTowardAngle +
   MoveActorAlongHeading. */

/* Turn the actor's yaw toward `ang`, at most `step` per frame. */
#define TurnActorTowardAngle(actor, ang, step, lim)                            \
  do {                                                                         \
    actor->rotZ = ApproachAngle8(ang, actor->rotZ, step, lim);                 \
  } while (0)

int PivotActorAlongPath(Actor *actor, PathData *path, int thresh, int speed,
                        int contactRadius, int step, int lim, int turnGroup,
                        int flags) {
  int ret;
  int zclose;
  int ang;
  int idx;
  int wrap;
  int *node;
  Actor **list;
  Actor *peer;

  if (g_nActorFrameDelta == 3) {
    speed += speed >> 1;
  } else if (g_nActorFrameDelta == 4) {
    speed <<= 1;
  }

  ret = 0;
  if (flags & MOVE_CHECK_Z) {
    zclose = abs(actor->posZ - path->nodes[path->cur].z) < thresh;
  } else {
    zclose = 1;
  }

  if (ApproxDist2D(&actor->posX, &path->nodes[path->cur].x) < thresh &&
      zclose != 0) {
    actor->subMode = PIVOT_SUB_ARRIVED;
    if (path->dir == -1) {
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
    ret = PATH_ARRIVED(path->cur);
  }

  node = (int *)((char *)path + (path->cur << 4)); /* node x/y at [2]/[3] */
  ang = ArcTan2(node[2] - actor->posX, node[3] - actor->posY, ATAN_ANGLE8);

  if (AbsAngleDelta8(ang, actor->rotZ) <= lim) {
    actor->subMode = PIVOT_SUB_WALKING;
    TurnActorYawTowardAngle(actor, ang, step, lim, YAW_STEP_IN_BAND);
    MoveActorAlongHeading(actor, speed, contactRadius, 0, flags);
    goto done;
  }

  if (actor->subMode == PIVOT_SUB_ARRIVED && turnGroup != PIVOT_TURN_ANY) {
    /* A freshly arrived actor starts its turn only while no peer of its turn
       group is walking (PIVOT_SUB_WALKING). */
    list = g_apActiveActorListBlock;
    while ((peer = *(list++)) != 0) {
      if (peer->group == turnGroup &&
          (unsigned int)peer->mode < ACTOR_MODE_DORMANT &&
          peer->subMode == PIVOT_SUB_WALKING) {
        break;
      }
    }
    if (peer != 0) {
      goto done;
    }
    TurnActorTowardAngle(actor, ang, step, lim);
    actor->subMode = PIVOT_SUB_TURNING;
  } else {
    TurnActorTowardAngle(actor, ang, step, lim);
  }

done:
  return ret;
}
