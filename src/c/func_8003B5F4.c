#include "globals.h"

/* Walk pickup group `a0`'s actor-id list (0x8003b5f4); for every actor whose
   type byte (+0x48) is < 0x80, OR the flag bits `a1` into its +0x18 word. The
   |= store may alias the id list, so the bit-15 last-entry test reloads *ptr as
   a signed halfword. No-op when the group index is out of range. */
void func_8003B5F4(int a0, int a1) {
  unsigned short *ptr;
  int idx;
  unsigned char *base;

  if (a0 < g_nLevelPickupTableCount) {
    ptr = ((unsigned short **)g_pLevelPickupTableEntries)[a0];
    base = (unsigned char *)g_pActorListBase;
    do {
      idx = *ptr & 0x7FFF;
      if ((unsigned int)base[idx * 0x58 + 0x48] < 0x80) {
        *(int *)(base + idx * 0x58 + 0x18) |= a1;
      }
    } while ((short)*ptr++ >= 0);
  }
}
