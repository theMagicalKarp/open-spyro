#include "funcs.h"
#include "globals.h"

/* Refresh the actor's cached Z at +0x14 from the floor probe plus the signed
   short bias at +0x38, then re-encode its packed direction code. */
void SnapActorToGround(Actor *actor) {
  actor->posZ = ProbeActorGroundHeight(actor) + actor->floorOffset;
  EncodeCachedVecToActorDirCode(actor);
}
