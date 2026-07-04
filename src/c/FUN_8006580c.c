#include "globals.h"

extern volatile unsigned char *D_800750FC; /* CDIO index register */
extern volatile unsigned char *D_80075108; /* CDIO data register 3 */
extern volatile unsigned int *D_8007510C;  /* CDIO DMA/config register */
extern volatile unsigned int *D_80075130;  /* COMDELAY register */
extern volatile unsigned int *D_80075134;  /* DMA control (DPCR) */
extern volatile unsigned int *D_80075138;  /* CD DMA base address (MADR) */
extern volatile unsigned int *D_8007513C;  /* CD DMA block control (BCR) */
extern volatile unsigned int *D_80075140;  /* CD DMA channel control (CHCR) */

/* libcd sector DMA (chained/slice variant): same CDIO arming as FUN_8006570c
   but with comdelay 0x21020843 and config 0x1325, waits for data-request
   status (bit 6), fires CHCR 0x11400100 (linked mode) and reads the channel
   control back to flush. (0x8006580c, 240 bytes.) */
int FUN_8006580c(unsigned int madr, unsigned int bcr) {
  volatile unsigned int dummy;

  *D_800750FC = 0;
  *D_80075108 = 0x80;
  *D_80075130 = 0x21020843;
  *D_8007510C = 0x1325;
  *D_80075134 |= 0x8000;
  *D_80075138 = madr;
  *D_8007513C = bcr | 0x10000;
  while ((*D_800750FC & 0x40) == 0) {
  }
  *D_80075140 = 0x11400100;
  dummy = *D_80075140;
  return 0;
}
