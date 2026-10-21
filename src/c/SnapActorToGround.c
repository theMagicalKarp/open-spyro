#include "globals.h"

extern int ProbeActorGroundHeight(int *e);
extern void EncodeCachedVecToActorDirCode(int *e);

/* Refresh the actor's cached Z at +0x14 from the floor probe plus the signed
   short bias at +0x38, then re-encode its packed direction code. */
void SnapActorToGround(int *e) {
  e[5] = ProbeActorGroundHeight(e) + ((short *)e)[0x1C];
  EncodeCachedVecToActorDirCode(e);
}
