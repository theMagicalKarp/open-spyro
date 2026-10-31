#include "actor.h"
#include "globals.h"

/* Set the mode byte (+0x48) to `value` for every actor in the pickup group
   named by actor's group index (+0x43) whose current mode is < 0x80 and equal
   to `match` (0x8003b538). Out-of-range group index applies the same guarded
   write to the passed actor itself. */
void ReplaceActorGroupState(Actor *actor, int value, int match) {
  unsigned short *ptr;
  int idx;
  int group;
  unsigned int type;

  group = actor->group;
  if (group >= g_nLevelPickupTableCount) {
    type = actor->mode;
    if (type < 0x80 && type == match) {
      actor->mode = value;
    }
  } else {
    ptr = g_pLevelPickupTableEntries[group];
    do {
      idx = *ptr & 0x7FFF;
      type = g_pActorListBase[idx].mode;
      if (type < 0x80 && type == match) {
        g_pActorListBase[idx].mode = value;
      }
    } while ((short)*ptr++ >= 0);
  }
}
