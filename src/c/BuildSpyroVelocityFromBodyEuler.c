#include "globals.h"

extern void ApplyEulerRotation(signed char *euler, int *mtx, int flag);
extern void RotateVectorByMatrix(int *mtx, int *in, int *out);
extern int g_anSpyroBodyPitchBlock[]; /* +0 pitch, [-12] g_anSpyroVelocity */

/* 0x8003d978 (0x90) — build Spyro's world-space velocity: snapshot the body
   Euler angles (>> 4) into a byte triple, expand them into a rotation matrix
   on the stack, then rotate (speed, 0, 0) through it into g_anSpyroVelocity.
   The matrix is a 32-byte local declared ahead of the angle bytes (frame
   0x40: matrix at sp+0x10, angles at sp+0x30). Pitch and the velocity's x
   component / both RotateVectorByMatrix pointers go through the pitch-block
   alias, so &pitch is held in s0 across the first call and the velocity is
   reached at -0x30; the y/z zero stores stay direct. */
void BuildSpyroVelocityFromBodyEuler(void) {
  int mtx[8];
  signed char euler[3];

  euler[0] = (signed char)(g_anSpyroBodyPitchBlock[0] >> 4);
  euler[1] = (signed char)(g_nSpyroBodyRoll >> 4);
  euler[2] = (signed char)(g_nSpyroBodyYaw >> 4);
  ApplyEulerRotation(euler, mtx, 0);

  g_anSpyroVelocity[1] = 0;
  g_anSpyroVelocity[2] = 0;
  g_anSpyroBodyPitchBlock[-12] = g_nSpyroMotionSpeed;
  RotateVectorByMatrix(mtx, &g_anSpyroBodyPitchBlock[-12],
                       &g_anSpyroBodyPitchBlock[-12]);
}
