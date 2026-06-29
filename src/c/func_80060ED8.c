#include "globals.h"

/* Read the GPU status register (GPUSTAT) via g_pGpuStatReg. */
int func_80060ED8(void) { return *(int *)g_pGpuStatReg; }
