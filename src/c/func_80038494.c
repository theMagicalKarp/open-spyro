#include "globals.h"

typedef struct ActorSlot {
  unsigned char b[0x58];
} ActorSlot;

/* Test whether an actor is one of the "running kill" set and fully faded in
   (0x80038494, 0x88 bytes): its slot index in the actor pool selects a bit in
   g_anRunningKillBitmap, and the actor only counts when its +0x53 byte has
   reached 0xFF.

   The word index and the bit index must be separate locals, in that order:
   they are the only two independent insns in the block and they are emitted in
   declaration order. The bit test also has to be a VALUE temp — written
   directly as a condition, fold rewrites `(x & (1 << b)) != 0` into
   `(x >> b) & 1` and emits `srav` with the unmasked index. */
int func_80038494(unsigned char *actor) {
  int idx = (ActorSlot *)actor - (ActorSlot *)g_pActorListBase;
  int hit = 0;

  {
    int word = idx >> 5;
    int bit = idx & 0x1F;
    int t = g_anRunningKillBitmap[word] & (1 << bit);
    if (t != 0) {
      hit = actor[0x53] == 0xFF;
    }
  }
  return hit;
}
