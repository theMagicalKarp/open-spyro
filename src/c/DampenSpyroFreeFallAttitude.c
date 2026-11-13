#include "angle.h"
#include "globals.h"
#include "main_shared.h"
#include "spyro_block.h"

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
  int err[2];
  int pitch;
  int roll;
  int wrapped;
  int pitchAccel;
  int rollAccel;
  int pitchVel;
  int rollStepVel;
  int pad[2];
  int rollVel;
  int out0;
  int out1;
  err[0] = 0;
  err[1] = 0;
  pitch = g_anSpyroBodyPitchBlock[0];
  wrapped = (-pitch) & ANGLE12_MASK;
  err[0] = wrapped;
  if (wrapped >= 0x800) {
    err[0] = wrapped - 0x1000;
  }
  roll = g_anSpyroBodyPitchBlock[SPY_ROLL - SPY_PITCH];
  wrapped = (-roll) & ANGLE12_MASK;
  err[1] = wrapped;
  if (wrapped >= 0x800) {
    err[1] = wrapped - 0x1000;
  }
  pitchAccel = (err[0] << 2) >> 4;
  pitchVel = g_nSpyroPitchSpringVel;
  pitchAccel = pitchAccel - ((pitchVel << 4) >> 6);
  pitchVel = pitchVel + pitchAccel;
  rollAccel = err[1] << 2;
  rollVel = g_nSpyroRollSpringVel;
  rollAccel = rollAccel >> 4;
  g_nSpyroPitchSpringVel = pitchVel;
  pitchVel = pitchVel >> 2;
  err[0] = pitchVel;
  out0 = pitch + pitchVel;
  rollStepVel = rollVel;
  rollAccel = rollAccel - ((rollStepVel << 4) >> 6);
  rollStepVel = rollStepVel + rollAccel;
  g_nSpyroRollSpringVel = rollStepVel;
  rollStepVel = rollStepVel >> 2;
  err[1] = rollStepVel;
  out1 = roll + rollStepVel;
  g_anSpyroBodyPitchBlock[0] = out0;
  g_anSpyroBodyPitchBlock[SPY_ROLL - SPY_PITCH] = out1;
}
