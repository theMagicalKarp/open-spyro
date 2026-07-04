#include "globals.h"

extern volatile unsigned char *D_800750FC; /* CDIO index register */
extern volatile unsigned char *D_80075108; /* CDIO data register 3 */
extern volatile unsigned int *D_8007510C;  /* CDIO DMA/config register */
extern volatile unsigned int *D_80075130;  /* COMDELAY register */
extern volatile unsigned int *D_80075134;  /* DMA control (DPCR) */
extern volatile unsigned int *D_80075138;  /* CD DMA base address (MADR) */
extern volatile unsigned int *D_8007513C;  /* CD DMA block control (BCR) */
extern volatile unsigned int *D_80075140;  /* CD DMA channel control (CHCR) */

/* libcd sector DMA: arm the CDIO for a data transfer (index 0, request 0x80,
   comdelay 0x20943, config 0x1323), enable the CD DMA channel in DPCR, set
   MADR/BCR, busy-wait for data-request status (bit 6), fire CHCR 0x11000000,
   drain until the channel busy bit clears, then restore config 0x1325.
   (0x8006570c, 256 bytes.) */
int FUN_8006570c(unsigned int madr, unsigned int bcr) {
  *D_800750FC = 0;
  *D_80075108 = 0x80;
  *D_80075130 = 0x20943;
  *D_8007510C = 0x1323;
  *D_80075134 |= 0x8000;
  *D_80075138 = madr;
  *D_8007513C = bcr | 0x10000;
  do {
  } while ((*D_800750FC & 0x40) == 0);
  *D_80075140 = 0x11000000;
  while (*D_80075140 & 0x1000000) {
  }
  *D_8007510C = 0x1325;
  return 0;
}
