#include "globals.h"

extern unsigned char D_80074E55;

/* libcd accessor: return the cached CD status byte at 0x80074e55
   (0x80063aac, 16 bytes). */
int FUN_80063aac(void) { return D_80074E55; }
