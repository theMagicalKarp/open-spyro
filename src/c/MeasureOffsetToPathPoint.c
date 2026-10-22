#include "globals.h"

extern unsigned int VectorLength(int *vec, int include_z);

/* 0x8003a79c: builds a 3-component offset vector (path[idx].field - actor.field
   for fields +8/+0xc/+0x10 vs +0xc/+0x10/+0x14, idx = path[1], stride 0x10)
   and returns its length divided by divisor. */
int MeasureOffsetToPathPoint(unsigned char *actor, unsigned char *path,
                             int divisor) {
  int v[3];
  int idx;

  idx = path[1];
  v[0] = *(int *)((idx << 4) + path + 8) - *(int *)(actor + 0xc);
  idx = path[1];
  v[1] = *(int *)((idx << 4) + path + 0xc) - *(int *)(actor + 0x10);
  idx = path[1];
  v[2] = *(int *)((idx << 4) + path + 0x10) - *(int *)(actor + 0x14);

  return (int)VectorLength(v, 1) / divisor;
}
