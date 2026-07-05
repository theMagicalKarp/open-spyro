#include "globals.h"

extern void DespawnActorRecord();

/* Walk pickup group `a0`'s actor-id list (0x8003b688) and despawn every actor
   whose type byte (+0x48) is < 0x80. The call clobbers the id load, so the
   bit-15 last-entry test reloads *ptr as a signed halfword. No-op when the
   group index is out of range. */
void func_8003B688(int a0) {
  unsigned short *ptr;
  int idx;

  if (a0 < g_nLevelPickupTableCount) {
    ptr = ((unsigned short **)g_pLevelPickupTableEntries)[a0];
    do {
      idx = *ptr & 0x7FFF;
      if ((unsigned int)((unsigned char *)g_pActorListBase)[idx * 0x58 + 0x48] <
          0x80) {
        DespawnActorRecord((unsigned char *)g_pActorListBase + idx * 0x58);
      }
    } while ((short)*ptr++ >= 0);
  }
}
