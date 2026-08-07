#include "globals.h"

extern int g_anSpyroBodyPitchBlock[]; /* +0 pitch, [-1] roll */
extern int D_80078CB0;                /* pitch damping spring */
extern int D_80078CB4;                /* roll damping spring */

/* 0x8003da08 (0xDC) — spring Spyro's body pitch and roll back toward level
   while free-falling: wrap each angle's error into [-0x800,0x800), run it
   through a critically-damped spring (velocity in D_80078CB0/CB4), and apply
   a quarter of the spring velocity to the angle each frame. */
/* Statement order carries the spring section: the chain-1 error shift is
   split (`<< 2` right after chain 0's spring update, `>> 4` after the
   D_80078CB4 load) so sched1 does not hoist it to the head of the block, the
   D_80078CB4 load is written ahead of the D_80078CB0 store, and each angle's
   final sum goes to its own single-set local (`out0`/`out1`) so the addu keeps
   the original's `addu a1,t0,a1` operand order instead of accumulating in
   place. */

void DampenSpyroFreeFallAttitude(void) {
  int e[2];
  int pitch;
  int roll;
  int v;
  int d0;
  int d1;
  int k0;
  int k1;
  int pad[2];
  int new_var;
  int out0;
  int out1;
  e[0] = 0;
  e[1] = 0;
  pitch = g_anSpyroBodyPitchBlock[0];
  v = (-pitch) & 0xFFF;
  e[0] = v;
  if (v >= 0x800) {
    e[0] = v - 0x1000;
  }
  roll = g_anSpyroBodyPitchBlock[-1];
  v = (-roll) & 0xFFF;
  e[1] = v;
  if (v >= 0x800) {
    e[1] = v - 0x1000;
  }
  d0 = (e[0] << 2) >> 4;
  k0 = D_80078CB0;
  d0 = d0 - ((k0 << 4) >> 6);
  k0 = k0 + d0;
  d1 = e[1] << 2;
  new_var = D_80078CB4;
  d1 = d1 >> 4;
  D_80078CB0 = k0;
  k0 = k0 >> 2;
  e[0] = k0;
  out0 = pitch + k0;
  k1 = new_var;
  d1 = d1 - ((k1 << 4) >> 6);
  k1 = k1 + d1;
  D_80078CB4 = k1;
  k1 = k1 >> 2;
  e[1] = k1;
  out1 = roll + k1;
  g_anSpyroBodyPitchBlock[0] = out0;
  g_anSpyroBodyPitchBlock[-1] = out1;
}
