#include "globals.h"

/* Set the type byte (+0x48) to `a1` for every actor in the pickup group named
   by actor `a0`'s +0x43 group index whose current type byte is < 0x80
   (0x8003b1e8). If that group index is out of range, apply the same guarded
   write to the passed actor itself. */
void func_8003B1E8(int a0, int a1) {
  unsigned short *ptr;
  int idx;
  int group;

  group = *(unsigned char *)(a0 + 0x43);
  if (group >= g_nLevelPickupTableCount) {
    if ((unsigned int)*(unsigned char *)(a0 + 0x48) < 0x80) {
      *(unsigned char *)(a0 + 0x48) = a1;
    }
  } else {
    ptr = ((unsigned short **)g_pLevelPickupTableEntries)[group];
    do {
      idx = *ptr & 0x7FFF;
      if ((unsigned int)((unsigned char *)g_pActorListBase)[idx * 0x58 + 0x48] <
          0x80) {
        ((unsigned char *)g_pActorListBase)[idx * 0x58 + 0x48] = a1;
      }
    } while ((short)*ptr++ >= 0);
  }
}
