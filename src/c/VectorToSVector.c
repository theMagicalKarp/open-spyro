#include "globals.h"

/* 0x80017c68, 28 bytes -- narrow a 3-int vector to 3 shorts.

   Matching notes (ex-"handwritten" $at-as-data): the three components are
   pinned to $at/$v0/$v1 with local register variables (A374/A405). */
void VectorToSVector(short *dst, int *src) {
  register int a asm("$1");
  register int b asm("$2");
  register int c asm("$3");

  a = src[0];
  b = src[1];
  c = src[2];
  dst[0] = a;
  dst[1] = b;
  dst[2] = c;
}
