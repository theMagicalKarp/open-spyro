#include "funcs.h"
#include "globals.h"

/* Probe the world floor below the actor's collision point (+0xC), temporarily
   biasing the cached Z at +0x14 up by 0x5DC across the probe call, then
   restoring it. Returns the probed ground height. The bias is reloaded after
   the call (into $v1) since the probe may touch the actor's memory while the
   probe result stays live in $v0 for the return. */
int ProbeActorGroundHeight(Actor *actor) {
  int ground;
  actor->posZ += 0x5DC;
  ground = FindGroundHeightBelow(&actor->posX, 0x1000);
  actor->posZ -= 0x5DC;
  return ground;
}
