#include "globals.h"

/* 0x80016fd0, 120 bytes -- transpose the packed 3x3 short matrix at src into
   dst: three halfword-field swaps (word0<->word1 high halves, word1<->word3
   low halves, word2<->word3 high halves); the 5th word passes through.

   Matching notes (ex-B6 "wall"): the original uses $at as one of the two
   swap temps, which gcc never allocates on its own -- but a local register
   variable can name it, so every value is pinned to the original's register
   (A374). Each swap is six separate compound statements (and/and/xor/xor/
   or/or); the empty volatile asm keeps the 0xFFFF0000 `lui` below the five
   loads, where the backward scheduler would otherwise hoist it. gas warns about
   the explicit $at uses; the bytes are unaffected. */

void TransposePackedMatrix(unsigned int *dst, unsigned int *src) {
  register unsigned int w0 asm("$6");
  register unsigned int w1 asm("$7");
  register unsigned int w2 asm("$8");
  register unsigned int w3 asm("$9");
  register unsigned int w4 asm("$10");
  register unsigned int a asm("$1");
  register unsigned int b asm("$2");
  register unsigned int m asm("$3");

  w0 = src[0];
  w1 = src[1];
  w2 = src[2];
  w3 = src[3];
  w4 = src[4];
  __asm__ volatile("");
  m = 0xFFFF0000;

  a = w0 & m;
  b = w1 & m;
  w0 ^= a;
  w1 ^= b;
  w0 |= b;
  w1 |= a;

  a = w1 & 0xFFFF;
  b = w3 & 0xFFFF;
  w1 ^= a;
  w3 ^= b;
  w1 |= b;
  w3 |= a;

  a = w2 & m;
  b = w3 & m;
  w2 ^= a;
  w3 ^= b;
  w2 |= b;
  w3 |= a;

  dst[0] = w0;
  dst[1] = w1;
  dst[2] = w2;
  dst[3] = w3;
  dst[4] = w4;
}
