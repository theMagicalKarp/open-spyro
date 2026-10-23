#include "funcs.h"
#include "globals.h"

/* Interpolate byte angle `b` toward `a` by the byte fraction `c`/256: take the
   shortest signed delta (a - b), scale it by c, and wrap the scaled step back
   into byte range around `b`. */
int LerpAngle8(int a, int b, unsigned char c) {
  return WrapAddAngle8(b, (WrapAngleDelta8(b, a) * c) >> 8);
}
