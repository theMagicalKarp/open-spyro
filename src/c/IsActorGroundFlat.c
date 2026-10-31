#include "funcs.h"
#include "globals.h"

/* Floor-probe at the actor's collision point (+0xC) with a temporary +0x5DC Z
   bias, then test whether the probed surface is near-flat: the 8-bit slope
   angle of the cached probe vector (its Z vs planar length) must read below
   0x18. */
int IsActorGroundFlat(Actor *actor) {
  actor->posZ += 0x5DC;
  FindGroundHeightBelow(&actor->posX, 0x1000);
  actor->posZ -= 0x5DC;
  return (signed char)ArcTan2_8bit(
             g_anCollisionProbeVec[2],
             VectorLength(g_anCollisionProbeVec, VEC_LEN_XY)) < 0x18;
}
