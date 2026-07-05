#include "globals.h"

/* Set the +0x49 byte to `a1` for every actor in the pickup group named by
   actor `a0`'s +0x43 group index whose type byte (+0x48) is < 0x80
   (0x8003b728). If that group index is out of range, just stamp the passed
   actor's own +0x49 byte instead. */
void func_8003B728(int a0, int a1) {
  unsigned short *ptr;
  int idx;
  int group;

  group = *(unsigned char *)(a0 + 0x43);
  if (group >= g_nLevelPickupTableCount) {
    *(unsigned char *)(a0 + 0x49) = a1;
  } else {
    ptr = ((unsigned short **)g_pLevelPickupTableEntries)[group];
    do {
      idx = *ptr & 0x7FFF;
      if ((unsigned int)((unsigned char *)g_pActorListBase)[idx * 0x58 + 0x48] <
          0x80) {
        ((unsigned char *)g_pActorListBase)[idx * 0x58 + 0x49] = a1;
      }
    } while ((short)*ptr++ >= 0);
  }
}
