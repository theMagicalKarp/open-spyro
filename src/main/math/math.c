#include "funcs.h"
#include "globals.h"
#include "match_asm.h"

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
  SCHED_FENCE();
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

/* 0x80017294, 8 bytes -- tail of a GTE helper: return the value the
   caller left in $t0, scaled down by 12 fractional bits.

   Matching notes (ex-wall "GTE-neighbor epilogue fragment"): $t0 is read as
   a local register variable seeded through an empty asm, so gcc emits only
   the shift into $v0 and the return. */
unsigned int ReturnT0Fixed12(void) {
  register unsigned int t0 asm("$8");

  REG_RESERVE(t0);
  return t0 >> 12;
}

/* Zero the int field at +0x8 of the record pointed at by a0. A single-store
   leaf: the `sw $zero` rides the jr delay slot. (Unnamed in the Ghidra seed;
   the surrounding cluster at 0x80017xxx is small actor/record helpers.)
   Unreferenced: no call, branch, pointer or table entry in main or any overlay
   (xref). */
void ZeroRecordField08(int *rec) { rec[2] = 0; }

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

/* 0x800176c8, 40 bytes -- arithmetic-shift each component of a 3-int vector
   right by n, in place.

   Matching notes (ex-"handwritten" $at-as-data): the three components are
   pinned to $at/$v0/$v1 with local register variables (A374/A405). */
void RShiftVector3(int *vec, int shift) {
  register int a asm("$1");
  register int b asm("$2");
  register int c asm("$3");

  a = vec[0];
  b = vec[1];
  c = vec[2];
  a >>= shift;
  b >>= shift;
  c >>= shift;
  vec[0] = a;
  vec[1] = b;
  vec[2] = c;
}

/* Hard-zero an int[3] vector. The third store rides the jr delay slot. */
void ZeroVector(int *vec) {
  vec[0] = 0;
  vec[1] = 0;
  vec[2] = 0;
}

/* 0x80017700, 28 bytes -- copy a 3-int vector.

   Matching notes (ex-"handwritten" $at-as-data): the three components are
   pinned to $at/$v0/$v1 with local register variables (A374/A405). */
void CopyVector(int *dst, int *src) {
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

/* 0x80017a9c, 8 bytes -- tail of a GTE helper: return the value the
   caller left in $t0, scaled down by 12 fractional bits.

   Matching notes (ex-wall "GTE-neighbor epilogue fragment"): $t0 is read as
   a local register variable seeded through an empty asm, so gcc emits only
   the shift into $v0 and the return. */
unsigned int ReturnT0Fixed12Dup(void) {
  register unsigned int t0 asm("$8");

  REG_RESERVE(t0);
  return t0 >> 12;
}

/* 0x80017bfc, 40 bytes -- narrow a 3-int vector to 3 shorts, dropping 2
   fractional bits.

   Matching notes (ex-"handwritten" $at-as-data): the three components are
   pinned to $at/$v0/$v1 with local register variables (A374/A405). */
void VectorToSVectorShr2(short *dst, int *src) {
  register int a asm("$1");
  register int b asm("$2");
  register int c asm("$3");

  a = src[0];
  b = src[1];
  c = src[2];
  a >>= 2;
  b >>= 2;
  c >>= 2;
  dst[0] = a;
  dst[1] = b;
  dst[2] = c;
}

/* 0x80017c24, 40 bytes -- widen a 3-ushort vector to 3 ints, adding 2
   fractional bits.

   Matching notes (ex-"handwritten" $at-as-data): the three components are
   pinned to $at/$v0/$v1 with local register variables (A374/A405). */
void SVectorToVectorShl2(int *dst, unsigned short *src) {
  register int a asm("$1");
  register int b asm("$2");
  register int c asm("$3");

  a = src[0];
  b = src[1];
  c = src[2];
  a <<= 2;
  b <<= 2;
  c <<= 2;
  dst[0] = a;
  dst[1] = b;
  dst[2] = c;
}

/* 0x80017c4c, 28 bytes -- widen a 3-short vector to 3 ints.

   Matching notes (ex-"handwritten" $at-as-data): the three components are
   pinned to $at/$v0/$v1 with local register variables (A374/A405). */
void SVectorToVector(int *dst, short *src) {
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

  OPAQUE_COPY(io, 0x1F800000);
  return io[0x1120 / 2];
}
