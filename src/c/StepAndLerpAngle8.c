#include "globals.h"

extern int StepAngle8Toward(int a, int b, int c, int d);
extern int LerpAngle8(int a, int b, int c);

/* Step byte angle `b` toward `a` by limit `c` (StepAngle8Toward), then ease the
   result the rest of the way by fraction `d`/256 (LerpAngle8). */
int StepAndLerpAngle8(int a, int b, int c, int d) {
  return LerpAngle8(StepAngle8Toward(a, b, c, d), b, d & 0xFF);
}
