#include "actor_type.h"
#include "globals.h"

/* Step an actor's shade band and rebuild its tint word from it (0x8003a9ec).
   `band` 0 with no hit flags means "leave it alone"; otherwise the band is
   pushed out by 0x10 (up to 0x50) while the flame-hit bit is set and eased
   back in by 2 above 0x20, by 1 below. The quarter band n then becomes the
   tagged tint 0xA << 20 | (0x18 + n) << 12 | (0x18 - n) << 6 | (0x18 - n)
   (see Actor.tint0): the flame level rises as the other two fall. Returns
   the adjusted band. */
int SelectActorLodBand(Actor *actor, int band) {
  int n;
  int tint;

  if (band != 0 || actor->flags != 0) {
    if ((actor->flags & ACTOR_HIT_FLAME) && band < 0x50) {
      band += 0x10;
    } else if (band >= 0x21) {
      band -= 2;
    } else if (band > 0) {
      band -= 1;
    }
    n = band >> 2;
    tint = 0xA00000 + ((n + 0x18) << 12) + ((0x18 - n) << 6) + 0x18;
    *(int *)&actor->tint0 = tint - n;
  }
  return band;
}
