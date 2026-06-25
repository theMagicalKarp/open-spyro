#include "globals.h"

extern int FindGroundHeightBelow(unsigned int *pt, int z);
extern int VectorLength(int *vec, int include_z);
extern signed char ArcTan2_8bit(int a, int b);

/* Floor-probe at the actor's collision point (+0xC) with a temporary +0x5DC Z
   bias, then test whether the probed surface is near-flat: the 8-bit slope
   angle of the cached probe vector (its Z vs planar length) must read below
   0x18. */
int func_8003838C(int *e) {
  e[5] += 0x5DC;
  FindGroundHeightBelow((unsigned int *)&e[3], 0x1000);
  e[5] -= 0x5DC;
  return ArcTan2_8bit(g_anCollisionProbeVec[2],
                      VectorLength(g_anCollisionProbeVec, 0)) < 0x18;
}
