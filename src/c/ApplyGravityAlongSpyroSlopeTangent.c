#include "funcs.h"
#include "globals.h"

/* Apply gravity projected along the current ground slope (0x8003e0b4, 0xF8).
   Take the ground normal, rescale it so its Z component equals one gravity
   step (ratio = step*nz/len), flip it downhill if it points up, then reset
   the slope-slide accumulator (normal block +0x2C) and subtract the tangent
   vector from it, recording the step in g_nSpyroSlopeGravityStep. No-op on a
   zero-length normal (free fall). */

void ApplyGravityAlongSpyroSlopeTangent(void) {
  int vec[3];
  int *p;
  int len;

  CopyVector(vec, g_anSpyroGroundNormal);
  len = VectorLength(vec, 1);
  if (len != 0) {
    ScaleVector3ByRatio(vec, len, (g_nSpyroGravityStep * vec[2]) / len);
    if (vec[2] > 0) {
      vec[2] = -vec[2];
      vec[0] = -vec[0];
      vec[1] = -vec[1];
    }
    p = g_anSpyroGroundNormal + 11;
    ZeroVector(p);
    g_nSpyroSlopeGravityStep = g_nSpyroGravityStep;
    SubtractVector(p, p, vec);
  }
}
