#include "funcs.h"
#include "globals.h"

/* Set the actor's posZ to the floor below it plus its signed floorOffset,
   then re-encode its packed ground code. */
void SnapActorToGround(Actor *actor) {
  actor->posZ = ProbeActorGroundHeight(actor) + actor->floorOffset;
  EncodeCachedVecToActorDirCode(actor);
}
