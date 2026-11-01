#include "actor_type.h"
#include "globals.h"
#include "main_shared.h"
#include "vecmath.h"

/* Rotate `actor`'s byte yaw (rotZ) toward the direction of Spyro, stepping
   by at most `step` (boosted 1.5x in camera mode 3, 2x in mode 4) but never
   past the actual delta. Returns 0 while still outside the `threshold`
   dead-band (and keeps turning); returns 1 once within it, applying one more
   step when `flag` is set. */
int TrackEntityYawTowardSpyro(Actor *actor, int step, int threshold, int flag) {
  int target = ArcTan2(g_anSpyroWorldPos[0] - actor->posX,
                       g_anSpyroWorldPos[1] - actor->posY, ATAN_ANGLE8);
  int delta = AbsAngleDelta8(target, actor->rotZ);

  if (g_nActorFrameDelta == 3) {
    step += step >> 1;
  } else if (g_nActorFrameDelta == 4) {
    step <<= 1;
  }
  if (delta < step) {
    step = delta;
  }
  if (threshold < (int)AbsAngleDelta8(target, actor->rotZ)) {
    actor->rotZ = ApproachAngle8(target, actor->rotZ, step, (step >> 1) + 1);
    return 0;
  }
  if (flag) {
    actor->rotZ = ApproachAngle8(target, actor->rotZ, step, (step >> 1) + 1);
  }
  return 1;
}
