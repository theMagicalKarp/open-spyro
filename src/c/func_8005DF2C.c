#include "globals.h"

/* Read the 16-bit interrupt-mask register (I_MASK) via g_pIMaskReg. */
int func_8005DF2C(void) { return *(unsigned short *)g_pIMaskReg; }
