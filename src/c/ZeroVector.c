#include "globals.h"

/* Hard-zero an int[3] vector. The third store rides the jr delay slot. */
void ZeroVector(int *vec) {
  vec[0] = 0;
  vec[1] = 0;
  vec[2] = 0;
}
