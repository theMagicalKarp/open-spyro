#include "globals.h"

/* Waypoint path-follow driver (0x80039e94, 0x2d8). Steers the actor
   toward its current path node (path[1] = node index, 16-byte node
   records at +8/+0xC/+0x10). On arrival (2D distance and, with flags
   bit 8, Z distance within `thresh`) advances the node index — forward
   with wrap when the path's +6 halfword is -1, backward with wrap to
   path[0]-1 otherwise — and reports it as 0x100+index in the return.
   When the heading delta to the node exceeds `lim`, turns in place via
   ApproachAngle8 (deferring the turn when another listed actor of the
   same `kind` is already mid-turn); otherwise marks the actor moving
   and delegates the step to func_80038EE0 + func_80039398. */

extern int ApproxDist2D(int *a, int *b);
extern int ArcTan2(int x, int y, int mode);
extern int AbsAngleDelta8(int a, int b);
extern int ApproachAngle8(int target, int cur, int step, int lim);
extern int func_80038EE0(int *actor, int ang, int step, int lim, int mode);
extern int func_80039398(int *actor, int speed, int group, int radius,
                         int flags);
extern int abs(int);

extern int D_800756C4;
extern void *g_apSaveMenuAnimActorListBlock[];

#define ACTOR_YAW(a) (*(unsigned char *)((char *)(a) + 0x46))
#define ACTOR_TURNSTATE(a) (*(unsigned char *)((char *)(a) + 0x49))

/* Turn the actor's yaw toward `ang`, at most `step` per frame. */
#define TurnActorTowardAngle(actor, ang, step, lim)                            \
  do {                                                                         \
    ACTOR_YAW(actor) = ApproachAngle8(ang, ACTOR_YAW(actor), step, lim);       \
  } while (0)

int func_80039E94(int *actor, unsigned char *path, int thresh, int speed,
                  int group, int step, int lim, int kind, int flags) {
  int ret;
  int zclose;
  int ang;
  int idx;
  int wrap;
  int *node;
  void **list;
  unsigned char *peer;

  if (D_800756C4 == 3) {
    speed += speed >> 1;
  } else if (D_800756C4 == 4) {
    speed <<= 1;
  }

  ret = 0;
  if (flags & 0x100) {
    zclose = abs(actor[5] - ((int *)(path + (path[1] << 4)))[4]) < thresh;
  } else {
    zclose = 1;
  }

  if (ApproxDist2D(actor + 3, (int *)(path + ((path[1] << 4) + 8))) < thresh &&
      zclose != 0) {
    ACTOR_TURNSTATE(actor) = 0;
    if (*(short *)(path + 6) == -1) {
      wrap = path[0];
      idx = path[1] + 1;
      path[1] = idx;
      if ((idx & 0xFF) == wrap) {
        path[1] = 0;
      }
    } else {
      wrap = 0xFF;
      idx = (path[1] = path[1] - 1);
      if ((idx & 0xFF) == wrap) {
        path[1] = path[0] - 1;
      }
    }
    ret = path[1] + 0x100;
  }

  node = (int *)(path + (path[1] << 4));
  ang = ArcTan2(node[2] - actor[3], node[3] - actor[4], 0);

  if (AbsAngleDelta8(ang, ACTOR_YAW(actor)) <= lim) {
    ACTOR_TURNSTATE(actor) = 1;
    func_80038EE0(actor, ang, step, lim, 1);
    func_80039398(actor, speed, group, 0, flags);
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
