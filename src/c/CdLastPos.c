#include "globals.h"

extern unsigned char D_80074E50;

/* libcd CdLastPos: return a pointer to the last seek position, a 3-byte BCD
   CdlLOC (0x80063abc, 16 bytes). */
unsigned char *CdLastPos(void) { return &D_80074E50; }
