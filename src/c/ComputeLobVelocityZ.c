#include "funcs.h"
#include "globals.h"

/* Initial Z velocity for a lob from `pos` that lands on `target` (0x8003891c):
   the flight time is the XY distance over `speed` frames (written to *pOutTime
   when non-null), and vz solves dz + gravity * t^2 / 2 over that time; 0 when
   the flight takes under a frame. */
int ComputeLobVelocityZ(int *pos, int *target, int speed, int gravity,
                        int *pOutTime) {
  int v[2];
  int t;
  int r;

  v[0] = pos[0] - target[0];
  v[1] = pos[1] - target[1];
  t = (int)VectorLength(v, VEC_LEN_XY) / speed;
  if (pOutTime != 0) {
    *pOutTime = t;
  }
  if (t != 0) {
    return -((pos[2] - target[2]) + ((gravity * (t * t)) >> 1)) / t;
  }
  return 0;
}
