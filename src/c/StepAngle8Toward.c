#include "globals.h"

extern int WrapAngleDelta8(int a, int b);
extern int WrapAddAngle8(int a, int b);

/* Step byte angle `b` toward `a` by at most `c`: if the shortest signed delta
   (b - a) is larger in magnitude than the limit `c`, move `b` by `c` in the
   delta's direction; otherwise it is already within range, so return `a`. */
int StepAngle8Toward(int a, int b, int c, int d) {
  int delta = WrapAngleDelta8(a, b);
  int absd = delta >= 0 ? delta : -delta;
  if (c < absd) {
    if (delta < 0) {
      return WrapAddAngle8(b, c);
    }
    return WrapAddAngle8(b, -c);
  }
  return a;
}
