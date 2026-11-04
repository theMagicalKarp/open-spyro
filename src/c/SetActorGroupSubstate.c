#include "actor.h"
#include "globals.h"

/* Set ->subMode to `value` for every actor in the pickup group named by
   actor's ->group whose ->mode is < 0x80
   (0x8003b728). If that group index is out of range, just stamp the passed
   actor's own subMode instead. */
void SetActorGroupSubstate(Actor *actor, int value) {
  unsigned short *ptr;
  int idx;
  int group;

  group = actor->group;
  if (group >= g_nLevelPickupTableCount) {
    actor->subMode = value;
  } else {
    ptr = g_pLevelPickupTableEntries[group];
    do {
      idx = *ptr & 0x7FFF;
      if ((unsigned int)g_pActorListBase[idx].mode < ACTOR_MODE_DORMANT) {
        g_pActorListBase[idx].subMode = value;
      }
    } while ((short)*ptr++ >= 0);
  }
}
