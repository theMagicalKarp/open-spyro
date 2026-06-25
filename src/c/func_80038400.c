#include "globals.h"

extern int FindGroundHeightBelow(unsigned int *pt, int z);

/* Like func_80038340 but with a caller-supplied Z bias: bump the cached Z at
   +0x14 by `delta` across the floor probe (whose search ceiling is also raised
   by `delta`), then restore it. Returns the probed ground height. */
int func_80038400(int *e, int delta) {
  int ground;
  e[5] += delta;
  ground = FindGroundHeightBelow((unsigned int *)&e[3], delta + 0x1000);
  e[5] -= delta;
  return ground;
}
