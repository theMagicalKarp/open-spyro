#include "globals.h"

extern int D_80078B34[];
extern void CopyVector();
extern void SetVector3Magnitude();
extern void ZeroVector();
extern void SubtractVector();

/* Rebuild Spyro's gravity vector: copy the slope normal, scale it to the
   (unsigned) per-tick gravity step, then negate it into place (0x8003e1ac). */
void ApplyGravityAlongSpyroSlopeNormal(void) {
  VECTOR zero;
  int step;

  CopyVector(D_80078B34, D_80078B34 - 0xE);
  step = g_nSpyroGravityStep;
  if (step < 0) {
    step = -step;
  }
  SetVector3Magnitude(D_80078B34, step);
  ZeroVector(&zero);
  SubtractVector(D_80078B34, &zero, D_80078B34);
}
