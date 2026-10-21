#include "globals.h"

extern int StepAngle8Toward(int a, int b, int c, int d);
extern int func_80038120(int a, int b, int c);

/* Step byte angle `b` toward `a` by limit `c` (StepAngle8Toward), then ease the
   result the rest of the way by fraction `d`/256 (func_80038120). */
int func_80038178(int a, int b, int c, int d) {
  return func_80038120(StepAngle8Toward(a, b, c, d), b, d & 0xFF);
}
