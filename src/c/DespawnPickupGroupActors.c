#include "funcs.h"
#include "globals.h"

/* Walk pickup group `a0`'s actor-id list (0x8003b688) and despawn every actor
   whose mode is < ACTOR_MODE_DORMANT (not dormant). The call clobbers the id
   load, so the bit-15 last-entry test reloads *ptr as a signed halfword. No-op
   when the group index is out of range. */
void DespawnPickupGroupActors(int a0) {
  unsigned short *ptr;
  int idx;

  if (a0 < g_nLevelPickupTableCount) {
    ptr = ((unsigned short **)g_pLevelPickupTableEntries)[a0];
    do {
      idx = *ptr & 0x7FFF;
      if ((unsigned int)((
              unsigned char *)g_pActorListBase)[idx * ACTOR_SIZE +
                                                ACTOR_OFFSET(mode)] < 0x80) {
        DespawnActorRecord((unsigned char *)g_pActorListBase +
                           idx * ACTOR_SIZE);
      }
    } while ((short)*ptr++ >= 0);
  }
}
