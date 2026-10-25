#include "funcs.h"
#include "globals.h"

/* Ray-march occlusion probe from Spyro toward target (0x80038250): like
   CheckLineOfSightSegment but sources the start point from g_anSpyroWorldPos
   and refines the segment length via the sqrt LUT before stepping. */
unsigned int CheckSpyroLineOfSightTo(int *target) {
  VECTOR seg;
  VECTOR cur;
  VECTOR next;
  int len;
  int steps;
  int i;

  SubtractVector(&seg, g_anSpyroWorldPos, target);
  len = VectorLength(&seg, 1);
  RefineSqrtEstimate(&seg, len, 1);
  ScaleVector3ByRatio(&seg, len, 0x400);
  CopyVector(&cur, target);
  steps = len >> 10;
  for (i = 0; i < steps; i++) {
    AddVector(&next, &cur, &seg);
    if (CastRayWorldAndActors(&cur, &next) != 0) {
      return 0;
    }
    CopyVector(&cur, &next);
  }
  return 1;
}
