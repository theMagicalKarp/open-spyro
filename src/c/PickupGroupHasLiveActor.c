#include "actor_type.h"
#include "globals.h"

/* Scan pickup group `group`'s actor-id list (0x8003b0dc). Each entry is a u16:
   low 15 bits index the actor pool, bit 15 marks the last entry. Returns 1 as
   soon as one of the group's actors is not dormant (mode < ACTOR_MODE_DORMANT),
   else 0 (also when the group index is out of range). */
int PickupGroupHasLiveActor(int group) {
  unsigned short *ptr;
  unsigned short id;
  int idx;
  Actor *base;

  if (group < g_nLevelPickupTableCount) {
    ptr = g_pLevelPickupTableEntries[group];
    base = g_pActorListBase;
    do {
      id = *ptr;
      idx = id & 0x7FFF;
      if ((unsigned int)base[idx].mode < ACTOR_MODE_DORMANT) {
        return 1;
      }
      ptr++;
    } while ((short)id >= 0);
  }
  return 0;
}
