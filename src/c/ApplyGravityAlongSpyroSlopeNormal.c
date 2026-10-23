#include "globals.h"

extern int g_anSpyroSlopeGravityVec[]; /* slope-gravity step vector, added to
                                          velocity */
extern void CopyVector();
extern void SetVector3Magnitude();
extern void ZeroVector();
extern void SubtractVector();

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
