#include "globals.h"

extern int FUN_8006570c(void *buf, unsigned int nword);

/* libcd CdGetSector: synchronous DMA read of one sector into (buf, nword).
   Returns true when the underlying transfer reports success (0)
   (0x80064010, 32 bytes). */
int CdGetSector(void *buf, unsigned int nword) {
  return FUN_8006570c(buf, nword) == 0;
}
