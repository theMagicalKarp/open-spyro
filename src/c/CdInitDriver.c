#include "globals.h"

extern int WriteString(char *s);
extern int WritePrintf();
extern int CdCommandSync(int com, unsigned char *param, unsigned char *result,
                         int mode);
extern int CdSync(int mode, unsigned char *result);
extern void FUN_8005ddc8(void);
extern void FUN_8005ddf8(int slot, void *table);

extern char D_80011EEC[]; /* "CD_init:" */
extern char D_80011EF8[]; /* "addr=%08x\n" */
extern int D_80075118;    /* driver base address (printed by the banner) */
extern int D_8006590C;    /* libapi slot[2] callback table */

extern int D_80074E34; /* CD sync/ready/data callback slots */
extern int D_80074E38;
extern int D_80074E44; /* latched controller status word */
extern int D_80074E48;
extern unsigned char D_80074E54; /* saved status bytes */
extern unsigned char D_80074E55;

extern volatile unsigned char *D_800750FC;  /* CDIO index register */
extern volatile unsigned char *D_80075104;  /* CDIO data register 2 */
extern volatile unsigned char *D_80075108;  /* CDIO data register 3 */
extern volatile unsigned int *D_8007510C;   /* CDIO DMA/config register */
extern volatile unsigned char D_80075114[]; /* libcd controller state */
extern volatile unsigned char D_80075115;   /* saved status byte */
extern volatile unsigned char D_80075116;   /* pending-command flag */

/* libcd low-level CD-ROM controller init worker (CdReset's "CD_init:" path):
   clears the callback slots + status bytes, re-arms the interrupt subsystem,
   drains the response fifo (ack IRQ 7 via index 1) until status bits 0..2 are
   clear, re-arms controller state to 2 (ready), writes the CDIO config word
   0x1325, then issues CdlNop / CdlGetTN / CdlGetTD + a final CdSync(0).
   Returns 0 on success, -1 on failure. (0x800653b4, 492 bytes.) */
int CdInitDriver(void) {
  unsigned char status;

  WriteString(D_80011EEC);
  WritePrintf(D_80011EF8, &D_80075118);
  D_80074E55 = 0;
  D_80074E54 = 0;
  D_80074E38 = 0;
  D_80074E34 = 0;
  D_80074E48 = 0;
  D_80074E44 = 0;
  FUN_8005ddc8();
  FUN_8005ddf8(2, &D_8006590C);
  *D_800750FC = 1;
  while ((status = *D_80075108) & 7) {
    *D_800750FC = 1;
    *D_80075108 = 7;
    *D_80075104 = 7;
  }
  D_80075116 = 0;
  D_80075115 = D_80075116;
  D_80075114[0] = 2;
  *D_800750FC = 0;
  *D_80075108 = 0;
  *D_8007510C = 0x1325;
  CdCommandSync(1, 0, 0, 0);
  if (D_80074E44 & 0x10) {
    CdCommandSync(1, 0, 0, 0);
  }
  if (CdCommandSync(0xA, 0, 0, 0) != 0) {
    return -1;
  }
  if (CdCommandSync(0xC, 0, 0, 0) != 0) {
    return -1;
  }
  return -(CdSync(0, 0) != 2);
}
