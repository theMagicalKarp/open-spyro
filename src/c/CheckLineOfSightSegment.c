#include "funcs.h"
#include "globals.h"

/* March a ray along the segment to - from in 0x400-unit steps, testing
   world/actor occlusion at each step; returns 1 if the whole segment is clear,
   0 on the first hit (0x80033e40). */
int CheckLineOfSightSegment(int *from, int *to) {
  VECTOR seg;
  VECTOR cur;
  VECTOR next;
  int len;
  int steps;
  int i;

  SubtractVector(&seg, to, from);
  len = VectorLength(&seg, VEC_LEN_XYZ);
  steps = len >> 10;
  ScaleVector3ByRatio(&seg, len, 0x400);
  CopyVector(&cur, from);
  for (i = 0; i < steps; i++) {
    AddVector(&next, &cur, &seg);
    if (CastRayWorldAndActors(&cur, &next) != 0) {
      return 0;
    }
    CopyVector(&cur, &next);
  }
  return 1;
}
