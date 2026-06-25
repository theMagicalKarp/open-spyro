#include "globals.h"

/* Zero a single word at the given pointer. The store rides the jr delay slot,
   so the whole body is `jr $ra; sw $zero, 0($a0)` at 0x80058B60. */
void func_80058B60(int *p) { *p = 0; }
