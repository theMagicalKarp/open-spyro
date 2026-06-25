#include "globals.h"

extern int FindGroundHeightBelow(unsigned int *pt, int z);

/* Probe the world floor below the actor's collision point (+0xC), temporarily
   biasing the cached Z at +0x14 up by 0x5DC across the probe call, then
   restoring it. Returns the probed ground height. The bias is reloaded after
   the call (into $v1) since the probe may touch the actor's memory while the
   probe result stays live in $v0 for the return. */
int func_80038340(int *e) {
  int ground;
  e[5] += 0x5DC;
  ground = FindGroundHeightBelow((unsigned int *)&e[3], 0x1000);
  e[5] -= 0x5DC;
  return ground;
}
