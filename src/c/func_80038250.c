#include "globals.h"

extern void SubtractVector();
extern int VectorLength();
extern void RefineSqrtEstimate();
extern void ScaleVector3ByRatio();
extern void CopyVector();
extern void AddVector();
extern int CastRayWorldAndActors();

/* Ray-march occlusion probe from Spyro toward param_1 (0x80038250): like
   CheckLineOfSightSegment but sources the start point from g_anSpyroWorldPos
   and refines the segment length via the sqrt LUT before stepping. */
unsigned int func_80038250(int *param_1) {
  VECTOR seg;
  VECTOR cur;
  VECTOR next;
  int len;
  int steps;
  int i;

  SubtractVector(&seg, g_anSpyroWorldPos, param_1);
  len = VectorLength(&seg, 1);
  RefineSqrtEstimate(&seg, len, 1);
  ScaleVector3ByRatio(&seg, len, 0x400);
  CopyVector(&cur, param_1);
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
