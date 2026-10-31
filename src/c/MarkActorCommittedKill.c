#include "actor_type.h"
#include "globals.h"

/* Mark `actor` in the committed-kill bitmap (0x8003b7c0). Ignores pointers
   outside the live pool [g_pActorListBase, g_pActorPoolDynBase); otherwise sets
   the bit for its pool index. */
void MarkActorCommittedKill(Actor *actor) {
  int index;
  int word;
  int bit;

  if (actor >= g_pActorListBase && actor < g_pActorPoolDynBase) {
    index = actor - g_pActorListBase;
    word = index >> 5;
    bit = index & 0x1F;
    g_anCommittedKillBitmap[word] |= 1 << bit;
  }
}
