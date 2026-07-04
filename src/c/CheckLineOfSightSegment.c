#include "globals.h"

extern void SubtractVector();
extern int VectorLength();
extern void ScaleVector3ByRatio();
extern void CopyVector();
extern void AddVector();
extern int CastRayWorldAndActors();

/* March a ray along the segment param_2 - param_1 in 0x400-unit steps, testing
   world/actor occlusion at each step; returns 1 if the whole segment is clear,
   0 on the first hit (0x80033e40). */
unsigned int CheckLineOfSightSegment(int *param_1, int *param_2,
                                     unsigned int param_3, int *param_4) {
  VECTOR seg;
  VECTOR cur;
  VECTOR next;
  int len;
  int steps;
  int i;

  SubtractVector(&seg, param_2, param_1);
  len = VectorLength(&seg, 1);
  steps = len >> 10;
  ScaleVector3ByRatio(&seg, len, 0x400);
  CopyVector(&cur, param_1);
  for (i = 0; i < steps; i++) {
    AddVector(&next, &cur, &seg);
    if (CastRayWorldAndActors(&cur, &next) != 0) {
      return 0;
    }
    CopyVector(&cur, &next);
  }
  return 1;
}
