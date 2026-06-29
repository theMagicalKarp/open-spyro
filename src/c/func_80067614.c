#include "globals.h"

extern int D_80075B90[];

int func_80067614(int val) {
  int old = D_80075B90[0];
  D_80075B90[0] = val;
  return old;
}
