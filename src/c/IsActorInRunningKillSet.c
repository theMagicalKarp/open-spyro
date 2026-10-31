#include "actor_type.h"
#include "globals.h"

/* Test whether an actor is one of the "running kill" set (0x80038494,
   0x88 bytes): its slot index in the actor pool selects a bit in
   g_anRunningKillBitmap, and the actor only counts when its dropKind is 0xFF
   (the random gem roll).

   The word index and the bit index must be separate locals, in that order:
   they are the only two independent insns in the block and they are emitted in
   declaration order. The bit test also has to be a VALUE temp — written
   directly as a condition, fold rewrites `(x & (1 << b)) != 0` into
   `(x >> b) & 1` and emits `srav` with the unmasked index. */
int IsActorInRunningKillSet(Actor *actor) {
  int idx = actor - g_pActorListBase;
  int hit = 0;

  {
    int word = idx >> 5;
    int bit = idx & 0x1F;
    int t = g_anRunningKillBitmap[word] & (1 << bit);
    if (t != 0) {
      hit = actor->dropKind == 0xFF;
    }
  }
  return hit;
}
