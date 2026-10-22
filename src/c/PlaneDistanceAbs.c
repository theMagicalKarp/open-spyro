#include "globals.h"

typedef struct {
  int x, y, z, w;
} IVec4;

int PlaneDistanceAbs(IVec4 *pos, IVec4 *plane) {
  int r;
  int t[4];
  r = ((plane->x * pos->x) + (plane->y * pos->y) + (plane->z * pos->z)) >> 10;
  r += plane->w;
  return r >= 0 ? r : -r;
}
