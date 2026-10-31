#include "funcs.h"
#include "globals.h"

/* Interpolate byte angle `from` toward `to` by the byte fraction `frac`/256:
   take the shortest signed delta (to - from), scale it by frac, and wrap the
   scaled step back into byte range around `from`. */
int LerpAngle8(int to, int from, unsigned char frac) {
  return WrapAddAngle8(from, (WrapAngleDelta8(from, to) * frac) >> 8);
}
