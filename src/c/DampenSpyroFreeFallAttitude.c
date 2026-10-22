#include "globals.h"
#include "main_shared.h"

/* 0x8003da08 (0xDC) — spring Spyro's body pitch and roll back toward level
   while free-falling: wrap each angle's error into [-0x800,0x800), run it
   through a critically-damped spring (velocity in g_nSpyroPitchSpringVel/CB4),
   and apply a quarter of the spring velocity to the angle each frame. */
/* Statement order carries the spring section: the chain-1 error shift is
   split (`<< 2` right after chain 0's spring update, `>> 4` after the
   g_nSpyroRollSpringVel load) so sched1 does not hoist it to the head of the
   block, the g_nSpyroRollSpringVel load is written ahead of the
   g_nSpyroPitchSpringVel store, and each angle's final sum goes to its own
   single-set local (`out0`/`out1`) so the addu keeps the original's `addu
   a1,t0,a1` operand order instead of accumulating in place. */

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
  k0 = g_nSpyroPitchSpringVel;
  d0 = d0 - ((k0 << 4) >> 6);
  k0 = k0 + d0;
  d1 = e[1] << 2;
  new_var = g_nSpyroRollSpringVel;
  d1 = d1 >> 4;
  g_nSpyroPitchSpringVel = k0;
  k0 = k0 >> 2;
  e[0] = k0;
  out0 = pitch + k0;
  k1 = new_var;
  d1 = d1 - ((k1 << 4) >> 6);
  k1 = k1 + d1;
  g_nSpyroRollSpringVel = k1;
  k1 = k1 >> 2;
  e[1] = k1;
  out1 = roll + k1;
  g_anSpyroBodyPitchBlock[0] = out0;
  g_anSpyroBodyPitchBlock[-1] = out1;
}
