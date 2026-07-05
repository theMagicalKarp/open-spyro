#include "globals.h"

struct Actor58 {
  char _pad[0x58];
};

/* Mark actor `a0` in the committed-kill bitmap (0x8003b7c0). Ignores pointers
   outside the live pool [g_pActorListBase, g_pActorPoolDynBase); otherwise sets
   the bit for its 0x58-byte pool index. */
void func_8003B7C0(int a0) {
  int index;
  int word;
  int bit;

  if ((unsigned int)a0 >= (unsigned int)g_pActorListBase &&
      (unsigned int)a0 < (unsigned int)g_pActorPoolDynBase) {
    index = (struct Actor58 *)a0 - (struct Actor58 *)g_pActorListBase;
    word = index >> 5;
    bit = index & 0x1F;
    g_anCommittedKillBitmap[word] |= 1 << bit;
  }
}
