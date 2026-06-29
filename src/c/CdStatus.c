#include "globals.h"

extern unsigned char D_80074E44;

/* libcd CdStatus: return the latest CD-ROM status byte (0x80063a8c, 16 bytes).
   Bit 0x10 means a data fetch is in progress. */
int CdStatus(void) { return D_80074E44; }
