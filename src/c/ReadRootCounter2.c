#include "globals.h"

/* 0x80017fd4, 16 bytes -- read root counter 2's current value
   (0x1F801120).

   Matching notes (ex-B2 "wall"): the original addresses the register as
   `lui $at,0x1f80; lhu $v0,0x1120($at)`, which gcc never emits for a
   literal address (the base goes into the destination register). Naming
   $at with a local register variable reaches it; the base is seeded through
   an empty asm so cse cannot fold the single-use constant back into the
   load. */
unsigned short ReadRootCounter2(void) {
  register unsigned short *io asm("$1");

  __asm__("" : "=r"(io) : "0"(0x1F800000));
  return io[0x1120 / 2];
}
