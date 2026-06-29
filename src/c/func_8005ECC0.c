#include "globals.h"

/* True when the low 24 bits of *p are all set (== 0xFFFFFF). */
int func_8005ECC0(int *p) { return (*p & 0xFFFFFF) == 0xFFFFFF; }
