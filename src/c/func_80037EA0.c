#include "globals.h"

extern unsigned int GetRandomU32(void);

/* Uniform random integer in the inclusive range [lo, hi]: reduce a fresh 32-bit
   draw modulo the span (hi - lo + 1) and bias by lo. The draw is taken as
   signed, matching the signed remainder of the original. */
int func_80037EA0(int lo, int hi) {
  return (int)GetRandomU32() % (hi - lo + 1) + lo;
}
