#include "globals.h"

/* PSY-Q GetGraphType companion: report the cached GPU debug/standard type byte
   (0x8005f64c, 16 bytes). */
int func_8005F64C(void) { return g_bGpuDebugType; }
