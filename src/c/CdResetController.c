#include "globals.h"

extern volatile unsigned char *D_800750FC;  /* CDIO index register */
extern volatile unsigned char *D_80075104;  /* CDIO data register 2 */
extern volatile unsigned char *D_80075108;  /* CDIO data register 3 */
extern volatile unsigned int *D_8007510C;   /* CDIO DMA/config register */
extern volatile unsigned char D_80075114[]; /* libcd controller state */
extern volatile unsigned char D_80075115;   /* saved status byte */
extern volatile unsigned char D_80075116;   /* pending-command flag */

/* libcd hard CD-ROM controller reset: drains the response fifo (ack IRQ 7 via
   index 1) until status bits 0..2 clear, clears the pending/saved state,
   re-arms the controller state to 2 (ready) and writes the CDIO config word
   0x1325. Recovery path from command-sync timeout. (0x80065190, 224 bytes.) */
void CdResetController(void) {
  unsigned char status;

  *D_800750FC = 1;
  while ((status = *D_80075108) & 7) {
    *D_800750FC = 1;
    *D_80075108 = 7;
    *D_80075104 = 7;
  }
  D_80075115 = D_80075116 = 0;
  D_80075114[0] = 2;
  *D_800750FC = 0;
  *D_80075108 = 0;
  *D_8007510C = 0x1325;
}
