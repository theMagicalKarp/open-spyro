#include "globals.h"

/* Scan pickup group `a0`'s actor-id list (0x8003b0dc). Each entry is a u16:
   low 15 bits index the 0x58-byte actor pool, bit 15 marks the last entry.
   Returns 1 as soon as one of the group's actors has type byte (+0x48) < 0x80,
   else 0 (also when the group index is out of range). */
int func_8003B0DC(int a0) {
  unsigned short *ptr;
  unsigned short id;
  int idx;
  unsigned char *base;

  if (a0 < g_nLevelPickupTableCount) {
    ptr = ((unsigned short **)g_pLevelPickupTableEntries)[a0];
    base = (unsigned char *)g_pActorListBase;
    do {
      id = *ptr;
      idx = id & 0x7FFF;
      if ((unsigned int)base[idx * 0x58 + 0x48] < 0x80) {
        return 1;
      }
      ptr++;
    } while ((short)id >= 0);
  }
  return 0;
}
