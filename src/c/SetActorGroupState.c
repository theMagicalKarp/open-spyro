#include "actor.h"
#include "globals.h"

/* Set the mode byte (+0x48) to `value` for every actor in the pickup group
   named by actor's group index (+0x43) whose current mode is < 0x80
   (0x8003b1e8). If that group index is out of range, apply the same guarded
   write to the passed actor itself. */
void SetActorGroupState(Actor *actor, int value) {
  unsigned short *ptr;
  int idx;
  int group;

  group = actor->group;
  if (group >= g_nLevelPickupTableCount) {
    if ((unsigned int)actor->mode < ACTOR_MODE_DORMANT) {
      actor->mode = value;
    }
  } else {
    ptr = ((unsigned short **)g_pLevelPickupTableEntries)[group];
    do {
      idx = *ptr & 0x7FFF;
      if ((unsigned int)((Actor *)g_pActorListBase)[idx].mode <
          ACTOR_MODE_DORMANT) {
        ((Actor *)g_pActorListBase)[idx].mode = value;
      }
    } while ((short)*ptr++ >= 0);
  }
}
