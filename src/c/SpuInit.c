#include "funcs.h"
#include "globals.h"

/* Spyro audio bring-up: resets the memory-card subsystem (mode 0).
   (0x80012460, 32 bytes.) */
void SpuInit(void) { MemCardReset(0); }
