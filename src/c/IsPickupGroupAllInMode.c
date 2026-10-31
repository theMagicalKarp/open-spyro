#include "actor_type.h"
#include "globals.h"

/* Scan pickup group `group`'s actor-id list (0x8003b160). Each entry is a u16:
   low 15 bits index the actor pool, bit 15 marks the last entry. Returns 0 as
   soon as one of the group's actors is not in `mode`; returns 1 if every actor
   is (also 0 when the group index is out of range). Nothing calls it. */
int IsPickupGroupAllInMode(int group, int mode) {
  unsigned short *ptr;
  unsigned short id;
  int idx;
  Actor *base;

  if (group >= g_nLevelPickupTableCount) {
    return 0;
  }
  ptr = ((unsigned short **)g_pLevelPickupTableEntries)[group];
  base = (Actor *)g_pActorListBase;
  do {
    id = *ptr;
    idx = id & 0x7FFF;
    if (base[idx].mode != mode) {
      return 0;
    }
    ptr++;
  } while ((short)id >= 0);
  return 1;
}
