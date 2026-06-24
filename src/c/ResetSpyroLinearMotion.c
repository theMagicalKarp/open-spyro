#include "globals.h"

extern void ZeroVector(int *vec);

/* Hard-zero of Spyro's linear motion. Clears the knockback/motion accumulator
   g_anSpyroMotionVec, the frame-delta length g_nSpyroFrameMotionLength, the
   live velocity g_anSpyroVelocity, and the speed scalar g_nSpyroMotionSpeed.
   Called by jump/glide/dive transitions just before they overwrite velocity
   with their own launch. (g_anSpyroVelocity sits 0x18 below g_anSpyroMotionVec,
   so gcc reuses the motion-vec base register with a -0x18 offset for the second
   ZeroVector call.) */

void ResetSpyroLinearMotion(void) {
  int *motion = g_anSpyroMotionVec;

  ZeroVector(motion);
  g_nSpyroFrameMotionLength = 0;
  ZeroVector(motion - 6); /* g_anSpyroVelocity sits 0x18 (6 ints) below */
  g_nSpyroMotionSpeed = 0;
}
