#include "globals.h"
#include "main_shared.h"
#include "match_asm.h"
#include "spyro_block.h"
#include "vecmath.h"

/* Yaw written as a record member, so its in-struct load stays below the
   in-struct roll store (sched.c true_dependence, A230). */
#define YAW (((struct { int v; } *)&g_nSpyroBodyYaw)->v)

/* 0x8003d52c (0x1a4) — integrate Spyro's body euler angles by `rate` of
   angular velocity: roll advances by rate*sin(pitch), yaw by
   rate*cos(pitch)/cos(roll) and pitch by rate*sin(roll)*cos(pitch)/cos(roll)
   (cos(roll) clamped away from 0), each wrapped to 12 bits and mirrored into
   g_abSpyroPersistentEuler as a byte. The deltas are also kept in a local
   triple that nothing reads.

   The two empty do/while(0) blocks end cse's extended basic block (A200) so
   the roll accesses stay off the held pitch base instead of folding to
   absolute addresses; the first one sits between the pitch read for
   LookupSine and the call itself, which leaves the call's result copy free to
   follow the first roll load. */
void IntegrateSpyroBodyEuler(int rate) {
  int delta[3];
  int cosPitch;
  int sinPitch;
  int cosRoll;
  int rollTerm;
  int yawStep;
  int pitch;

  cosPitch = LookupCosine(g_anSpyroBodyPitchBlock[0]);
  pitch = g_anSpyroBodyPitchBlock[0];
  EBB_BREAK();
  sinPitch = LookupSine(pitch);
  cosRoll =
      LookupCosine(-g_anSpyroBodyPitchBlock[SPY_ROLL - SPY_PITCH] & 0xfff);
  if (cosRoll == 0) {
    cosRoll = 1;
  }
  rollTerm =
      rate *
      LookupSine(-g_anSpyroBodyPitchBlock[SPY_ROLL - SPY_PITCH] & 0xfff) /
      cosRoll;
  yawStep = rate * cosPitch / cosRoll;
  delta[1] = (rate * sinPitch) >> 12;
  g_anSpyroBodyPitchBlock[SPY_ROLL - SPY_PITCH] =
      (g_anSpyroBodyPitchBlock[SPY_ROLL - SPY_PITCH] + delta[1]) & 0xfff;
  g_abSpyroPersistentEuler[1] =
      g_anSpyroBodyPitchBlock[SPY_ROLL - SPY_PITCH] >> 4;
  delta[2] = yawStep;
  YAW = (YAW + delta[2]) & 0xfff;
  g_abSpyroPersistentEuler[2] = YAW >> 4;
  delta[0] = (rollTerm * cosPitch) >> 12;
  g_anSpyroBodyPitchBlock[0] = (g_anSpyroBodyPitchBlock[0] + delta[0]) & 0xfff;
  g_abSpyroPersistentEuler[0] = g_anSpyroBodyPitchBlock[0] >> 4;
}
