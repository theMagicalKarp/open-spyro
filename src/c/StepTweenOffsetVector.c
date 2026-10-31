#include "funcs.h"
#include "globals.h"
#include "main_shared.h"

/* 0x8003a850 — clamp the record's step counter (byte +2) from byte +3,
   then rebuild its offset vector: copy, scale by half the counter, add the
   actor anchor (+0xC), subtract the indexed sub-record vector, and blend
   by the remaining step fraction. */
void StepTweenOffsetVector(Actor *actor, unsigned char *tween, int *out,
                           int *base) {
  unsigned int t;

  t = tween[3] & 0xFE;
  tween[2] = t;
  if (t >= 9) {
    tween[2] = 8;
  }

  CopyVector(base, out);
  ScaleVector3Sat(out, base, tween[2] >> 1);
  AddVector(out, out, &actor->posX);
  SubtractVector(out, (int *)(tween + ((tween[1] << 4) + 8)), out);
  DivideVector3(out, out, tween[3] - (tween[2] >> 1));
}
