#include "funcs.h"
#include "globals.h"
#include "spyro_block.h"

/* Rebuild Spyro's gravity vector: copy the ground normal (reached off the
   gravity vector's own base), scale it to the absolute per-tick gravity
   step, then negate it into place (0x8003e1ac). */
void ApplyGravityAlongSpyroSlopeNormal(void) {
  int zero[4];
  int step;

  CopyVector(g_anSpyroSlopeGravityVec,
             g_anSpyroSlopeGravityVec - (SPY_PLATFORM - SPY_GROUND_NORMAL));
  step = g_nSpyroGravityStep;
  if (step < 0) {
    step = -step;
  }
  SetVector3Magnitude(g_anSpyroSlopeGravityVec, step);
  ZeroVector(zero);
  SubtractVector(g_anSpyroSlopeGravityVec, zero, g_anSpyroSlopeGravityVec);
}
