#include "actor.h"
#include "globals.h"

/* Set ->mode to `value` for every actor in the pickup group
   named by actor's ->group whose current mode is live (< ACTOR_MODE_DORMANT)
   and equal to `match` (0x8003b538). Out-of-range group index applies the same
   guarded write to the passed actor itself. */
void ReplaceActorGroupState(Actor *actor, int value, int match) {
  unsigned short *ptr;
  int idx;
  int group;
  unsigned int mode;

  group = actor->group;
  if (group >= g_nLevelPickupTableCount) {
    mode = actor->mode;
    if (mode < ACTOR_MODE_DORMANT && mode == match) {
      actor->mode = value;
    }
  } else {
    ptr = g_pLevelPickupTableEntries[group];
    do {
      idx = *ptr & 0x7FFF;
      mode = g_pActorListBase[idx].mode;
      if (mode < ACTOR_MODE_DORMANT && mode == match) {
        g_pActorListBase[idx].mode = value;
      }
    } while ((short)*ptr++ >= 0);
  }
}
