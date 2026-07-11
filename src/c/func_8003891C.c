#include "globals.h"

extern unsigned int VectorLength(int *vec, int include_z);

int func_8003891C(int *pos, int *target, int speed, int gravity,
                  int *pOutTime) {
  int v[2];
  int t;
  int r;

  v[0] = pos[0] - target[0];
  v[1] = pos[1] - target[1];
  t = (int)VectorLength(v, 0) / speed;
  if (pOutTime != 0) {
    *pOutTime = t;
  }
  if (t != 0) {
    return -((pos[2] - target[2]) + ((gravity * (t * t)) >> 1)) / t;
  }
  return 0;
}
