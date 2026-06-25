#include "globals.h"

/* Signed 8-bit angle delta: wrap (b - a) into the range (-128, 128]. Used to
   find the shortest turn between two byte angles. */
int func_800381BC(int a, int b) {
  int d = b - a;

  if (d > 0x80) {
    d -= 0x100;
  }
  if (d < -0x80) {
    d += 0x100;
  }
  return d;
}
