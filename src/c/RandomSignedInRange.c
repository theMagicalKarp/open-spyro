#include "globals.h"
#include "vecmath.h"

/* Random signed offset whose magnitude is uniform in [lo, hi]: draw once,
   reduce it modulo the span (hi - lo + 1) biased by lo for the magnitude, then
   flip the sign on the draw's low bit. */
int RandomSignedInRange(int lo, int hi) {
  int r = (int)GetRandomU32();
  int v = r % (hi - lo + 1) + lo;
  if (r & 1) {
    return v;
  }
  return -v;
}
