#include "funcs.h"
#include "globals.h"

/* One frame of ballistic actor motion (0x80039910, 0x198): a knock-back,
   hop or leap. Picks the probe mode/distance from the actor's probeDist
   byte and, while *speed is nonzero, steps the actor along `heading`
   (MoveActorAlongAngle), then bleeds *speed by `friction` (to 0). Unless vz
   is null or *vz is 0xFFFF (no vertical motion), adds *vz to the actor's Z
   and pulls it down by `gravity`, clamped at -0x104; once *vz is <= 0 the
   actor snaps up to the ground plus floorOffset. Returns the move-step
   result, or MOVED_LANDED when the floor snap landed. */
int DriveActorTrackedMove(Actor *actor, int *speed, int heading, int *vz,
                          int friction, int gravity) {
  int ret;
  int mode;
  int dist;
  int b;
  int t;
  int ground;

  ret = MOVED_OK;
  mode = MOVE_SLIDE | MOVE_LIFT;
  dist = 0x12C;
  if (vz == 0 || *vz == 0xFFFF) {
    mode = MOVE_SLIDE | MOVE_FOLLOW_FLOOR | MOVE_LIFT;
  }
  b = actor->probeDist;
  if (b == 0xFE) {
    mode = MOVE_LIFT;
  } else if (b != 0) {
    mode &= ~MOVE_SLIDE;
    dist = b << 2;
    if (b == 0xFF) {
      dist = 0;
    }
  }
  if (*speed != 0) {
    ret = MoveActorAlongAngle(actor, heading, *speed, dist, 0x1F4, mode);
    t = *speed - friction;
    *speed = t;
    if (t < 0) {
      *speed = 0;
    }
  }
  if (ret == MOVED_BLOCKED) {
    return MOVED_BLOCKED;
  }
  if (vz != 0 && *vz != 0xFFFF) {
    ground = ProbeActorGroundHeightRaised(actor, 0x258);
    actor->posZ += *vz;
    *vz -= gravity;
    if (*vz < -0x104) {
      *vz = -0x104;
    }
    if (*vz <= 0) {
      t = ground + actor->floorOffset;
      if (actor->posZ < t) {
        actor->posZ = t;
        ret = MOVED_LANDED;
      }
    }
    EncodeCachedVecToActorDirCode(actor);
  }
  return ret;
}
