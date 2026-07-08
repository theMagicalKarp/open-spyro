#include "globals.h"

extern unsigned int VectorLength(int *vec, int include_z);

/* 0x8003a79c: builds a 3-component offset vector (arg1[idx].field - arg0.field
   for fields +8/+0xc/+0x10 vs +0xc/+0x10/+0x14, idx = arg1[1], stride 0x10)
   and returns its length divided by arg2. */
int func_8003A79C(unsigned char *arg0, unsigned char *arg1, int arg2) {
  int v[3];
  int idx;

  idx = arg1[1];
  v[0] = *(int *)((idx << 4) + arg1 + 8) - *(int *)(arg0 + 0xc);
  idx = arg1[1];
  v[1] = *(int *)((idx << 4) + arg1 + 0xc) - *(int *)(arg0 + 0x10);
  idx = arg1[1];
  v[2] = *(int *)((idx << 4) + arg1 + 0x10) - *(int *)(arg0 + 0x14);

  return (int)VectorLength(v, 1) / arg2;
}
