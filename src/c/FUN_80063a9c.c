#include "globals.h"

extern unsigned char D_80074E54;

/* libcd accessor: return the cached CD status byte at 0x80074e54
   (0x80063a9c, 16 bytes). */
int FUN_80063a9c(void) { return D_80074E54; }
