#include "globals.h"
#include "vecmath.h"

/* Signed 8-bit angle delta: wrap (b - a) into the range (-128, 128]. Used to
   find the shortest turn between two byte angles. */
int WrapAngleDelta8(int a, int b) {
  int d = b - a;

  if (d > ANGLE8_HALF) {
    d -= ANGLE8_TURN;
  }
  if (d < -ANGLE8_HALF) {
    d += ANGLE8_TURN;
  }
  return d;
}
