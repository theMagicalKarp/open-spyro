#include "globals.h"

/* Return the libapi in-ISR reentrancy flag as an unsigned halfword
   (0x8005df1c, 16 bytes). */
int FUN_8005df1c(void) { return *(unsigned short *)&g_nInIsrFlag; }
