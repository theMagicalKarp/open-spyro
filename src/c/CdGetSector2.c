#include "globals.h"

extern int FUN_8006580c(void *buf, unsigned int nword);

/* libcd CdGetSector2: chained-mode DMA read into (buf, nword) for continuous
   reads. Returns true on successful issue (0x80064030, 32 bytes). */
int CdGetSector2(void *buf, unsigned int nword) {
  return FUN_8006580c(buf, nword) == 0;
}
