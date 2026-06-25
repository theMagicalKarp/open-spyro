#include "globals.h"

extern int func_800381BC(int a, int b);
extern int func_80038074(int a, int b);

/* Step byte angle `b` toward `a` by at most `c`: if the shortest signed delta
   (b - a) is larger in magnitude than the limit `c`, move `b` by `c` in the
   delta's direction; otherwise it is already within range, so return `a`. */
int func_80038098(int a, int b, int c, int d) {
  int delta = func_800381BC(a, b);
  int absd = delta >= 0 ? delta : -delta;
  if (c < absd) {
    if (delta < 0) {
      return func_80038074(b, c);
    }
    return func_80038074(b, -c);
  }
  return a;
}
