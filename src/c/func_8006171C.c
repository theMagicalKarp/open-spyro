#include "globals.h"

/* Read the shadowed GPU status byte at index `idx`. */
int func_8006171C(int idx) { return g_abGpuStatusShadow[idx]; }
