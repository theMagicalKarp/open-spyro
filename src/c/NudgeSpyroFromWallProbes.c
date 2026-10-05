#include "globals.h"

/* The tail's `lh; sra 4` pair is A233 (shift counts sh0/sh1 assigned
   outside the if-block). The loop is a goto loop: the original strength-
   reduces nothing (cos through a hand-advanced pointer, sine indexed by `a`)
   and recomputes `la g_anSineLut` inside the body for the tail's sine read.
   `a` is reused for the nudge angle (the original keeps both in s0).
   Allocation: sinTab is a `$18` register local (as a pseudo, its `la` is a
   3-ref cse temp that ranks last) and `pto` a `$20` one, set before `cosp`. */

extern void AddVector(int *dst, int *a, int *b);
extern int CastRayWorldAndActors(int *from, int *to);
extern short g_anCosineLut[];

/* 0x8003e90c (0x15c) — probe four rays around Spyro (at angles 0x20 +
   0x40*k, from 0x124 to 0x1a4 below his position); the bitmask of blocked
   probes selects a nudge angle from g_anSpyroWallNudgeAngleByProbeMask, and
   a non-negative angle pushes the motion vector (world pos +0xF4/+0xF8)
   1/16 along it. */
void NudgeSpyroFromWallProbes(void) {
  int from[3];
  int pad;
  int to[3];
  int mask;
  int i;
  int a;
  short *cosp;
  register short *sinTab asm("$18");
  int *pos;
  register int *pto asm("$20");
  int sh0;
  int sh1;

  mask = 0;
  a = 0x20;
  i = 0;
  pos = g_anSpyroWorldPos;
  pto = to;
  cosp = &g_anCosineLut[0x20];
  sh0 = 4;
  sh1 = 4;
probe:
  from[0] = to[0] = *cosp >> 4;
  from[1] = to[1] = g_anSineLut[a] >> 4;
  from[2] = -0x124;
  to[2] = -0x1a4;
  AddVector(from, from, pos);
  AddVector(pto, pto, pos);
  sinTab = g_anSineLut;
  if (CastRayWorldAndActors(from, pto) != 0) {
    mask |= 1 << i;
  }
  cosp += 0x40;
  i++;
  a += 0x40;
  if (i < 4) {
    goto probe;
  }
  a = g_anSpyroWallNudgeAngleByProbeMask[mask];
  if (a >= 0) {
    pos[0x3d] += g_anCosineLut[a] >> sh0;
    pos[0x3e] += sinTab[a] >> sh1;
  }
}
