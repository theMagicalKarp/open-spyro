#include "globals.h"

/* Sum two values and wrap into a single signed byte's range via (x + 0x100) %
   0x100. The +0x100 biases the sum before the signed remainder so the result
   tracks an 8-bit angle/offset. */
int func_80038074(int a, int b) { return (a + b + 0x100) % 0x100; }
