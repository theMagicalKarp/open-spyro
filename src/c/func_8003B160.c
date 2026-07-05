#include "globals.h"

/* Scan pickup group `a0`'s actor-id list (0x8003b160). Each entry is a u16:
   low 15 bits index the 0x58-byte actor pool, bit 15 marks the last entry.
   Returns 0 as soon as one of the group's actors has a type byte (+0x48)
   different from `a1`; returns 1 if every actor matches (also 0 when the group
   index is out of range). */
int func_8003B160(int a0, int a1) {
  unsigned short *ptr;
  unsigned short id;
  int idx;
  unsigned char *base;

  if (a0 >= g_nLevelPickupTableCount) {
    return 0;
  }
  ptr = ((unsigned short **)g_pLevelPickupTableEntries)[a0];
  base = (unsigned char *)g_pActorListBase;
  do {
    id = *ptr;
    idx = id & 0x7FFF;
    if (base[idx * 0x58 + 0x48] != a1) {
      return 0;
    }
    ptr++;
  } while ((short)id >= 0);
  return 1;
}
