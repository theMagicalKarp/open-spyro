#include "vecmath.h"

#include "globals.h"
/* Turn Spyro's byte yaw toward his motion vector (0x8003dfec): nothing inside
   +-`band`, a snap when within +-`step`, else one `step`; g_nSpyroBodyYaw is
   rebuilt from the byte. */
void RotateSpyroBodyTowardKnockbackDir(int step, int band) {
  int ret;
  int yaw;
  int cur;
  int diff;
  ret = ArcTan2(g_anSpyroMotionVec[0], g_anSpyroMotionVec[1], ATAN_ANGLE8);
  cur = g_abSpyroPersistentEuler[2];
  diff = ret - cur;
  diff = diff & 0xFF;
  yaw = ret;
  ret = band < diff;
  if (ret && (diff < (0x100 - band))) {
    if ((diff <= step) || (diff >= (0x100 - step))) {
      g_abSpyroPersistentEuler[2] = yaw;
    } else {
      g_abSpyroPersistentEuler[2] =
          (((unsigned int)diff) < 0x80) ? (cur + step) : (cur - step);
    }
    g_nSpyroBodyYaw = g_abSpyroPersistentEuler[2] << 4;
  }
}
