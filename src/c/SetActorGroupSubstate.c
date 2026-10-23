#include "actor.h"
#include "globals.h"

/* Set the +0x49 byte to `value` for every actor in the pickup group named by
   actor's group index (+0x43) whose mode (+0x48) is < 0x80
   (0x8003b728). If that group index is out of range, just stamp the passed
   actor's own +0x49 byte instead. */
void SetActorGroupSubstate(Actor *actor, int value) {
  unsigned short *ptr;
  int idx;
  int group;

  group = actor->group;
  if (group >= g_nLevelPickupTableCount) {
    actor->subMode = value;
  } else {
    ptr = ((unsigned short **)g_pLevelPickupTableEntries)[group];
    do {
      idx = *ptr & 0x7FFF;
      if ((unsigned int)((Actor *)g_pActorListBase)[idx].mode < 0x80) {
        ((Actor *)g_pActorListBase)[idx].subMode = value;
      }
    } while ((short)*ptr++ >= 0);
  }
}
