#include "globals.h"

extern void ApplyEulerRotation();
extern void SubtractVector();
extern void RotateVectorByMatrix();

/* 0x8003bed8 — classify the actor's position relative to Spyro into a
   quadrant mask: compute actor(+0xC) - Spyro (optionally un-rotated by the
   actor's yaw byte at +0x46), then AND away the halves of 0xF that the
   X/Y signs rule out. */
int func_8003BED8(unsigned char *actor) {
  int mtx[8];
  signed char euler[8];
  int delta[4];
  int mask;

  mask = 0xF;
  if (actor[0x46] != 0) {
    euler[0] = 0;
    euler[1] = 0;
    euler[2] = -actor[0x46];
    ApplyEulerRotation(euler, mtx, 0);
    SubtractVector(delta, actor + 0xC, g_anSpyroWorldPos);
    RotateVectorByMatrix(mtx, delta, delta);
  } else {
    SubtractVector(delta, actor + 0xC, g_anSpyroWorldPos);
  }

  if (delta[0] > 0) {
    mask &= 6;
  } else {
    mask &= 9;
  }
  if (delta[1] > 0) {
    mask &= 3;
  } else {
    mask &= 0xC;
  }
  return mask;
}
