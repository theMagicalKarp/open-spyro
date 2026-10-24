#include "funcs.h"
#include "globals.h"

/* Like ProbeActorGroundHeight but with a caller-supplied Z bias: bump the
   cached Z at +0x14 by `delta` across the floor probe (whose search ceiling is
   also raised by `delta`), then restore it. Returns the probed ground height.
 */
int ProbeActorGroundHeightRaised(Actor *actor, int delta) {
  int ground;
  actor->posZ += delta;
  ground = FindGroundHeightBelow(&actor->posX, delta + 0x1000);
  actor->posZ -= delta;
  return ground;
}
