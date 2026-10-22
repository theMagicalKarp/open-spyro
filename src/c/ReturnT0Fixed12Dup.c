#include "globals.h"

/* 0x80017a9c, 8 bytes -- tail of a GTE helper: return the value the
   caller left in $t0, scaled down by 12 fractional bits.

   Matching notes (ex-wall "GTE-neighbor epilogue fragment"): $t0 is read as
   a local register variable seeded through an empty asm, so gcc emits only
   the shift into $v0 and the return. */
unsigned int ReturnT0Fixed12Dup(void) {
  register unsigned int t0 asm("$8");

  __asm__("" : "=r"(t0));
  return t0 >> 12;
}
