#include "actor_type.h"
#include "globals.h"

/* Walk pickup group `group`'s actor-id list (0x8003b5f4); for every actor not
   dormant (mode < ACTOR_MODE_DORMANT), OR `bits` into its flags. The |= store
   may alias the id list, so the bit-15 last-entry test reloads *ptr as a signed
   halfword. No-op when the group index is out of range. */
void OrPickupGroupActorFlags(int group, int bits) {
  unsigned short *ptr;
  int idx;
  Actor *base;

  if (group < g_nLevelPickupTableCount) {
    ptr = ((unsigned short **)g_pLevelPickupTableEntries)[group];
    base = g_pActorListBase;
    do {
      idx = *ptr & 0x7FFF;
      if ((unsigned int)base[idx].mode < ACTOR_MODE_DORMANT) {
        base[idx].flags |= bits;
      }
    } while ((short)*ptr++ >= 0);
  }
}
