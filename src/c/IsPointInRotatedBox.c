#include "actor_states.h"
#include "globals.h"
#include "main_shared.h"
#include "vecmath.h"

/* 0x80038c4c (0x108) — rotate the XY delta between `pos` and the box's
   centre into the box's frame using its yaw (sine-LUT index), and return
   whether the rotated point lies inside the box's half-width / half-depth. The
   rotated |x|/|y| pair is also written to a scratch pair (never read back). */
int IsPointInRotatedBox(int *pos, TriggerBox *box) {
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

  angle = box->yaw;
  SubtractVector(delta, pos, &box->x);
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

    if (rx < box->halfWidth) {
      r = ay < box->halfDepth;
    }
    return r;
  }
}

/* Load-bearing forms: volatile out[4] keeps both dead stores per slot; abs()
   (not if/ternary) expands via single-insn abssi2 so the ry mult pipeline
   interleaves across the x-abs + store; g_anCosineLut alias (sym at
   lut+0x80) stops cse deriving the sine base via addiu -0x80. The result is
   a `$2` register variable set to 0 BEFORE the halfWidth load: with $2 live
   there, local-alloc cannot give halfWidth and halfDepth the same v0, so
   halfWidth takes v1 and ay drops to a1. As a pseudo the same `r = 0` form
   costs a move. */
