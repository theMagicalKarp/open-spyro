#include "globals.h"

extern int ArcTan2(int y, int x, int high_precision);
extern int AbsAngleDelta8(int a, int b);
extern int ApproachAngle8(int target, int yaw, int step, int rate);
extern int D_800756C4;

/* Rotate entity `e`'s byte yaw (+0x46) toward the direction of Spyro, stepping
   by at most `step` (boosted 1.5x in camera mode 3, 2x in mode 4) but never
   past the actual delta. Returns 0 while still outside the `threshold`
   dead-band (and keeps turning); returns 1 once within it, applying one more
   step when `flag` is set. */
int TrackEntityYawTowardSpyro(int *e, int step, int threshold, int flag) {
  int target =
      ArcTan2(g_anSpyroWorldPos[0] - e[3], g_anSpyroWorldPos[1] - e[4], 0);
  int delta = AbsAngleDelta8(target, ((unsigned char *)e)[0x46]);

  if (D_800756C4 == 3) {
    step += step >> 1;
  } else if (D_800756C4 == 4) {
    step <<= 1;
  }
  if (delta < step) {
    step = delta;
  }
  if (threshold < (int)AbsAngleDelta8(target, ((unsigned char *)e)[0x46])) {
    ((char *)e)[0x46] = ApproachAngle8(target, ((unsigned char *)e)[0x46], step,
                                       (step >> 1) + 1);
    return 0;
  }
  if (flag) {
    ((char *)e)[0x46] = ApproachAngle8(target, ((unsigned char *)e)[0x46], step,
                                       (step >> 1) + 1);
  }
  return 1;
}
