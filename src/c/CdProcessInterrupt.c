#include "globals.h"

extern int WritePrintf();
extern int WriteString(char *s);

extern volatile unsigned char *D_800750FC; /* CDIO index register */
extern volatile unsigned char
    *D_80075100; /* CDIO data register 1 (response FIFO) */
extern volatile unsigned char *D_80075104; /* CDIO data register 2 */
extern volatile unsigned char
    *D_80075108; /* CDIO data register 3 (interrupt flags) */
extern volatile unsigned char D_80075114[]; /* sync interrupt result */
extern volatile unsigned char D_80075115;   /* ready interrupt result */
extern volatile unsigned char D_80075116;   /* pending-command flag */
extern unsigned char D_80075AD0[];          /* sync result buffer */
extern unsigned char D_80075AD8[];          /* ready result buffer */
extern unsigned char D_80075AE0[];          /* data-end result buffer */
extern int D_80074E40;                      /* libcd debug level */
extern int D_80074E44;                      /* latched controller status */
extern int D_80074E48;                      /* latched error code */
extern int D_80074E4C;                      /* shell-open count */
extern unsigned char D_80074E55;            /* last command */
extern char *D_80074E5C[];                  /* command-name table */
extern int D_80074EFC[];  /* per-command "acknowledge only" flags */
extern int D_80074FFC[];  /* per-command "complete on ack" flags */
extern char D_80011E28[]; /* "DiskError: " */
extern char D_80011E34[]; /* "com=%s,code=(%02x:%02x)\n" */
extern char D_80011E50[]; /* "CDROM: unknown intr" */
extern char D_80011E64[]; /* "(%d)\n" */

#define COPY_RESULT(dst)                                                       \
  {                                                                            \
    unsigned char *d = dst;                                                    \
    volatile unsigned char *s = result;                                        \
    int n = 8;                                                                 \
    if (d) {                                                                   \
      while (n--) {                                                            \
        *d++ = *s++;                                                           \
      }                                                                        \
    }                                                                          \
  }

/* libcd interrupt reader (0x80064218, 1416 bytes): latches the CD controller
   interrupt type and its response bytes, clears the interrupt, records the
   status / error for the last command, and posts the sync or ready result
   (copying the response into the matching result buffer). Returns the
   interrupt class (1/2 sync, 4 ready, 6 both), 0 if none was pending. */
int CdProcessInterrupt(void) {
  volatile unsigned char intr;
  volatile unsigned char result[8];
  int i;
  int j;
  int err;
  int status;

  *D_800750FC = 1;
  intr = *D_80075108 & 7;
  err = 0;
  if (intr == 0) {
    return 0;
  }
  while (intr != (*D_80075108 & 7)) {
    intr = *D_80075108 & 7;
  }
  for (i = 0; i < 8; i++) {
    if ((*D_800750FC & 0x20) == 0) {
      break;
    }
    result[i] = *D_80075100;
  }
  for (j = i; j < 8; j++) {
    result[j] = 0;
  }
  *D_800750FC = 1;
  *D_80075108 = 7;
  *D_80075104 = 7;

  if (intr != 3 || D_80074FFC[D_80074E55] != 0) {
    if (!(D_80074E44 & 0x10) && (result[0] & 0x10)) {
      D_80074E4C++;
    }
    status = result[0];
    err = status & 0x1D;
    D_80074E44 = status;
    D_80074E48 = result[1];
  }
  if (intr == 5) {
    if (D_80074E40 > 0) {
      WritePrintf(D_80011E28);
    }
    if (D_80074E40 > 0) {
      WritePrintf(D_80011E34, D_80074E5C[D_80074E55], D_80074E44, D_80074E48);
    }
  }

  switch (intr) {
  case 3:
    if (err) {
      D_80075114[0] = 5;
      COPY_RESULT(D_80075AD0);
      return 2;
    }
    if (D_80074EFC[D_80074E55] != 0) {
      D_80075114[0] = 3;
      COPY_RESULT(D_80075AD0);
      return 1;
    }
    D_80075114[0] = 2;
    COPY_RESULT(D_80075AD0);
    return 2;
  case 2:
    D_80075114[0] = err ? 5 : 2;
    COPY_RESULT(D_80075AD0);
    return 2;
  case 1:
    if (err && i == 1) {
      err = 0;
    }
    D_80075115 = err ? 5 : 1;
    COPY_RESULT(D_80075AD8);
    *D_800750FC = 0;
    *D_80075108 = 0;
    return 4;
  case 4:
    D_80075116 = 4;
    D_80075115 = D_80075116;
    COPY_RESULT(D_80075AE0);
    COPY_RESULT(D_80075AD8);
    return 4;
  case 5:
    D_80075115 = 5;
    D_80075114[0] = D_80075115;
    COPY_RESULT(D_80075AD0);
    COPY_RESULT(D_80075AD8);
    return 6;
  default:
    WriteString(D_80011E50);
    WritePrintf(D_80011E64, intr);
    return 0;
  }
}
