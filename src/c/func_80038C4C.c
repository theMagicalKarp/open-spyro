#include "globals.h"

extern void SubtractVector(int *dst, int *a, int *b);
extern int abs(int n);

extern short g_anCosineLut[];

/* 0x80038c4c (0x108) — rotate the XY delta between `pos` and record `rec`
   into the record's frame using its yaw angle (+0x14, sine-LUT index), and
   return whether the rotated point lies inside the record's box extents
   (+0xC half-width, +0x10 half-depth). The rotated |x|/|y| pair is also
   written to a scratch pair (never read back). */
int func_80038C4C(int *pos, int *rec) {
  volatile int out[4];
  int delta[3];
  short *cosp;
  short *sinp;
  int angle;
  int dx;
  int dy;
  int rx;
  int ry;
  int ay;

  angle = rec[5];
  SubtractVector(delta, pos, rec);
  cosp = &g_anCosineLut[angle];
  sinp = &g_anSineLut[angle];
  dx = delta[0];
  dy = delta[1];

  rx = ((dx * *cosp) >> 12) - ((dy * *sinp) >> 12);
  out[0] = rx;
  ry = ((dx * *sinp) >> 12) + ((dy * *cosp) >> 12);
  rx = abs(rx);
  out[0] = rx;
  ay = abs(ry);
  out[1] = ry;
  out[1] = ay;

  {
    register int r asm("$2") = 0;

    if (rx < rec[3]) {
      r = ay < rec[4];
    }
    return r;
  }
}

/* Load-bearing forms: volatile out[4] keeps both dead stores per slot; abs()
   (not if/ternary) expands via single-insn abssi2 so the ry mult pipeline
   interleaves across the x-abs + store; g_anCosineLut alias (sym at
   lut+0x80) stops cse deriving the sine base via addiu -0x80. The result is
   a `$2` register variable set to 0 BEFORE the rec[3] load: with $2 live
   there, local-alloc cannot give rec[3] and rec[4] the same v0, so rec[3]
   takes v1 and ay drops to a1. As a pseudo the same `r = 0` form costs a
   move. */
