#include "globals.h"

extern int func_800381BC(int a, int b);
extern int func_80038074(int a, int b);

/* Interpolate byte angle `b` toward `a` by the byte fraction `c`/256: take the
   shortest signed delta (a - b), scale it by c, and wrap the scaled step back
   into byte range around `b`. */
int func_80038120(int a, int b, unsigned char c) {
  return func_80038074(b, (func_800381BC(b, a) * c) >> 8);
}
