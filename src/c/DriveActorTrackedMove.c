#include "funcs.h"
#include "globals.h"

/* Drive an actor's tracked move toward its target (0x80039910, 0x198).
   Picks the probe mode/distance from the actor's probeDist byte, runs the
   guided-move step (MoveActorAlongAngle) while *counter is live, then while
   *timer is live applies the vertical timer decay to the actor's Z, clamping
   the timer at -0x104 and floor-snapping via floorOffset once the timer runs
   out. Returns the move-step result, or 3 when the floor snap landed. */
int DriveActorTrackedMove(Actor *actor, int *counter, int angle, int *timer,
                          int step, int decay) {
  int ret;
  int mode;
  int dist;
  int b;
  int t;
  int r4;

  ret = 0;
  mode = 0x21;
  dist = 0x12C;
  if (timer == 0 || *timer == 0xFFFF) {
    mode = 0x25;
  }
  b = actor->probeDist;
  if (b == 0xFE) {
    mode = 1;
  } else if (b != 0) {
    mode &= ~0x20;
    dist = b << 2;
    if (b == 0xFF) {
      dist = 0;
    }
  }
  if (*counter != 0) {
    ret = MoveActorAlongAngle(actor, angle, *counter, dist, 0x1F4, mode);
    t = *counter - step;
    *counter = t;
    if (t < 0) {
      *counter = 0;
    }
  }
  if (ret == 2) {
    return 2;
  }
  if (timer != 0 && *timer != 0xFFFF) {
    r4 = ProbeActorGroundHeightRaised(actor, 0x258);
    actor->posZ += *timer;
    *timer -= decay;
    if (*timer < -0x104) {
      *timer = -0x104;
    }
    if (*timer <= 0) {
      t = r4 + actor->floorOffset;
      if (actor->posZ < t) {
        actor->posZ = t;
        ret = 3;
      }
    }
    EncodeCachedVecToActorDirCode(actor);
  }
  return ret;
}
