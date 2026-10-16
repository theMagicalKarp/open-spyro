#include "globals.h"

/* 0x800176c8, 40 bytes -- arithmetic-shift each component of a 3-int vector
   right by n, in place.

   Matching notes (ex-"handwritten" $at-as-data): the three components are
   pinned to $at/$v0/$v1 with local register variables (A374/A405). */
void RShiftVector3(int *v, int n) {
  register int a asm("$1");
  register int b asm("$2");
  register int c asm("$3");

  a = v[0];
  b = v[1];
  c = v[2];
  a >>= n;
  b >>= n;
  c >>= n;
  v[0] = a;
  v[1] = b;
  v[2] = c;
}
