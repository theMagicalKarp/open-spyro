#include "globals.h"

/* 0x800176a0, 40 bytes -- shift each component of a 3-int vector left by n, in
   place.

   Matching notes (ex-"handwritten" $at-as-data): the three components are
   pinned to $at/$v0/$v1 with local register variables (A374/A405). */
void LShiftVector3(int *vec, int shift) {
  register int a asm("$1");
  register int b asm("$2");
  register int c asm("$3");

  a = vec[0];
  b = vec[1];
  c = vec[2];
  a <<= shift;
  b <<= shift;
  c <<= shift;
  vec[0] = a;
  vec[1] = b;
  vec[2] = c;
}
