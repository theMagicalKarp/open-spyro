#include "actor_type.h"
#include "globals.h"

/* Mark `actor` in the committed-kill bitmap (0x8003b7c0). Ignores pointers
   outside the live pool [g_pActorListBase, g_pActorPoolDynBase); otherwise sets
   the bit for its pool index. */
void MarkActorCommittedKill(Actor *actor) {
  int index;
  int word;
  int bit;

  if ((unsigned int)actor >= (unsigned int)g_pActorListBase &&
      (unsigned int)actor < (unsigned int)g_pActorPoolDynBase) {
    index = actor - (Actor *)g_pActorListBase;
    word = index >> 5;
    bit = index & 0x1F;
    g_anCommittedKillBitmap[word] |= 1 << bit;
  }
}
