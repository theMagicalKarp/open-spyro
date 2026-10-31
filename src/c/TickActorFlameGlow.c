#include "actor_type.h"
#include "globals.h"

/* Tick the +0x4E countdown byte on an actor. When the 0x10000 flag (+0x18) is
   set it counts UP by 0x18, wrapping back to 0x60 once it reaches 0x61. When
   the flag is clear it counts DOWN: by 2 while >= 0x21, by 1 below that, and
   holds at 0. Named for the flag: 0x10000 is the flame-hit bit (a flame
   breaks the type-146 prop at once) and SelectActorLodBand pushes the shade
   band out for the same flag, so +0x4E reads as a scorch glow (likely). */
void TickActorFlameGlow(Actor *actor) {
  if (actor->flags & ACTOR_HIT_FLAME) {
    int v = (unsigned char)actor->tint2 + 0x18;
    actor->tint2 = v;
    if ((unsigned char)v >= 0x61) {
      actor->tint2 = 0x60;
    }
  } else {
    unsigned char b;
    if ((b = actor->tint2) >= 0x21) {
      actor->tint2 = b - 2;
    } else if (b != 0) {
      actor->tint2 = b - 1;
    }
  }
}
