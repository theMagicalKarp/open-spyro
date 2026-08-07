#include "globals.h"

/* Align Spyro's body pitch/roll toward his velocity direction (0x8003de44,
   284 bytes). Computes the target pitch from ArcTan2(speed, vz) + 0x8E (roll
   target is 0), wraps each delta to a signed 12-bit angle, then advances the
   two spring velocities (D_80078CB0/CB4) toward the deltas and steps the body
   euler angles by spring/4. */
extern unsigned int VectorLength(int *vec, int include_z);
extern int ArcTan2(int y, int x, int high_precision);

extern int D_80078CB0;
extern int D_80078CB4;

/* Scheduling levers: the pitch delta is written as four separate statements
   (stack read, global read, subtract, mask). The array read must come FIRST in
   source order — an embedded `(pitch = g_nSpyroBodyPitch)` operand has side
   effects, so expand evaluates it before `d[0]` and sched1 then hoists the
   global's lui/lw above the stack reload — and the subtract/mask must be their
   own statements or the result lands in a fresh register instead of reusing
   the loaded one. The roll delta below wants the opposite (embedded) form. */

void AlignSpyroBodyToVelocity(void) {
  int d[3];
  int len;
  int roll;
  int pitch;
  int v;
  int step2;
  int u;
  int step;
  d[0] = 0;
  len = VectorLength(g_anSpyroVelocity, 0);
  d[2] = len;
  d[1] = ArcTan2(len, g_anSpyroVelocity[2], 1) + 0x8E;
  v = d[0];
  pitch = g_nSpyroBodyPitch;
  v -= pitch;
  v &= 0xFFF;
  d[0] = v;
  if (v >= 0x800) {
    d[0] = v - 0x1000;
  }
  v = (d[1] - (roll = g_nSpyroBodyRoll)) & 0xFFF;
  d[1] = v;
  if (v >= 0x800) {
    d[1] = v - 0x1000;
  }
  step = D_80078CB0 + (((d[0] << 2) >> 4) - ((D_80078CB0 << 4) >> 6));
  u = (d[1] << 2) >> 4;
  D_80078CB0 = step;
  step2 = D_80078CB4 + (u - ((D_80078CB4 << 4) >> 6));
  step >>= 2;
  d[0] = step;
  g_nSpyroBodyPitch = pitch + step;
  D_80078CB4 = step2;
  step2 >>= 2;
  d[1] = step2;
  g_nSpyroBodyRoll = roll + step2;
}
