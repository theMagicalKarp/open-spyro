#include "globals.h"

extern void CopyVector();
extern void ScaleVector3Sat();
extern void AddVector();
extern void SubtractVector();
extern void func_800177F8();

/* 0x8003a850 — clamp the record's step counter (byte +2) from byte +3,
   then rebuild its offset vector: copy, scale by half the counter, add the
   actor anchor (+0xC), subtract the indexed sub-record vector, and blend
   by the remaining step fraction. */
void func_8003A850(int *arg0, unsigned char *arg1, int *arg2, int *arg3) {
  unsigned int t;

  t = arg1[3] & 0xFE;
  arg1[2] = t;
  if (t >= 9) {
    arg1[2] = 8;
  }

  CopyVector(arg3, arg2);
  ScaleVector3Sat(arg2, arg3, arg1[2] >> 1);
  AddVector(arg2, arg2, arg0 + 3);
  SubtractVector(arg2, (int *)(arg1 + ((arg1[1] << 4) + 8)), arg2);
  func_800177F8(arg2, arg2, arg1[3] - (arg1[2] >> 1));
}
