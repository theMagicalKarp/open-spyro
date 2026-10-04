#include "globals.h"

extern int LookupCosine(int angle);
extern int LookupSine(int angle);

/* Held base at g_nSpyroBodyPitch: pitch at [0], roll at [-1]. */
extern int g_anSpyroBodyPitchBlock[];

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
  int d[3];
  int cp;
  int sp;
  int cr;
  int a;
  int b;
  int pitch;

  cp = LookupCosine(g_anSpyroBodyPitchBlock[0]);
  pitch = g_anSpyroBodyPitchBlock[0];
  do {
  } while (0);
  sp = LookupSine(pitch);
  cr = LookupCosine(-g_anSpyroBodyPitchBlock[-1] & 0xfff);
  if (cr == 0) {
    cr = 1;
  }
  a = rate * LookupSine(-g_anSpyroBodyPitchBlock[-1] & 0xfff) / cr;
  b = rate * cp / cr;
  d[1] = (rate * sp) >> 12;
  g_anSpyroBodyPitchBlock[-1] = (g_anSpyroBodyPitchBlock[-1] + d[1]) & 0xfff;
  g_abSpyroPersistentEuler[1] = g_anSpyroBodyPitchBlock[-1] >> 4;
  d[2] = b;
  YAW = (YAW + d[2]) & 0xfff;
  g_abSpyroPersistentEuler[2] = YAW >> 4;
  d[0] = (a * cp) >> 12;
  g_anSpyroBodyPitchBlock[0] = (g_anSpyroBodyPitchBlock[0] + d[0]) & 0xfff;
  g_abSpyroPersistentEuler[0] = g_anSpyroBodyPitchBlock[0] >> 4;
}
