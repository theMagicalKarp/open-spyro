#include "globals.h"

/* Set the type byte (+0x48) to `a1` for every actor in the pickup group named
   by actor `a0`'s +0x43 group index whose current type byte is < 0x80 and not
   equal to `a2` (0x8003b47c). Out-of-range group index applies the same guarded
   write to the passed actor itself. */
void func_8003B47C(int a0, int a1, int a2) {
  unsigned short *ptr;
  int idx;
  int group;
  unsigned int type;

  group = *(unsigned char *)(a0 + 0x43);
  if (group >= g_nLevelPickupTableCount) {
    type = *(unsigned char *)(a0 + 0x48);
    if (type < 0x80 && type != a2) {
      *(unsigned char *)(a0 + 0x48) = a1;
    }
  } else {
    ptr = ((unsigned short **)g_pLevelPickupTableEntries)[group];
    do {
      idx = *ptr & 0x7FFF;
      type = ((unsigned char *)g_pActorListBase)[idx * 0x58 + 0x48];
      if (type < 0x80 && type != a2) {
        ((unsigned char *)g_pActorListBase)[idx * 0x58 + 0x48] = a1;
      }
    } while ((short)*ptr++ >= 0);
  }
}
