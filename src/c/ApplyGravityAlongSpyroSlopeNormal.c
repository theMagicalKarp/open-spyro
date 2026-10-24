#include "funcs.h"
#include "globals.h"

/* Rebuild Spyro's gravity vector: copy the slope normal, scale it to the
   (unsigned) per-tick gravity step, then negate it into place (0x8003e1ac). */
void ApplyGravityAlongSpyroSlopeNormal(void) {
  VECTOR zero;
  int step;

  CopyVector(g_anSpyroSlopeGravityVec, g_anSpyroSlopeGravityVec - 0xE);
  step = g_nSpyroGravityStep;
  if (step < 0) {
    step = -step;
  }
  SetVector3Magnitude(g_anSpyroSlopeGravityVec, step);
  ZeroVector(&zero);
  SubtractVector(g_anSpyroSlopeGravityVec, &zero, g_anSpyroSlopeGravityVec);
}
