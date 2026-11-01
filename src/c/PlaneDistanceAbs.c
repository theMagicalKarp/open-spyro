#include "globals.h"

typedef struct {
  int x, y, z, w;
} IVec4;

/* |dot(plane.xyz, pos) >> 10 + plane.w|: the distance of `pos` from a plane
   stored as a 1.10 normal plus offset (0x80038d54). The unused `t` keeps the
   original's stack frame size. */
int PlaneDistanceAbs(IVec4 *pos, IVec4 *plane) {
  int r;
  int t[4];
  r = ((plane->x * pos->x) + (plane->y * pos->y) + (plane->z * pos->z)) >> 10;
  r += plane->w;
  return r >= 0 ? r : -r;
}
