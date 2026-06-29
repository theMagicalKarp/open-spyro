#include "globals.h"

/* Take the low 24 bits of *p and force the top bit (0x80000000) set. */
int func_8005ECA4(int *p) { return (*p & 0xFFFFFF) | 0x80000000; }
