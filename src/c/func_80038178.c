#include "globals.h"

extern int func_80038098(int a, int b, int c, int d);
extern int func_80038120(int a, int b, int c);

/* Step byte angle `b` toward `a` by limit `c` (func_80038098), then ease the
   result the rest of the way by fraction `d`/256 (func_80038120). */
int func_80038178(int a, int b, int c, int d) {
  return func_80038120(func_80038098(a, b, c, d), b, d & 0xFF);
}
