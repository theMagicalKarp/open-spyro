#include "globals.h"

extern int VSync(int mode);
extern void WriteString(char *s);
extern int WritePrintf();
extern void CdResetController(void);
extern int FUN_8005df1c(void);
extern int CdProcessInterrupt(void);

extern char D_80011DFC[]; /* "CD timeout: " */
extern char D_80011E0C[]; /* "%s:(%s) Sync=%s, Ready=%s\n" */
extern char D_80011E84[]; /* "CD_sync" */

extern int D_80075AE8;             /* VSync deadline stamp */
extern int D_80075AEC;             /* spin counter */
extern char *D_80075AF0;           /* op-name string for the timeout message */
extern int D_80074E5C[];           /* sync-status name table */
extern int D_80074EDC[];           /* ready-status name table */
extern unsigned char D_80074E55;   /* sync-status index */
extern unsigned char D_80075114[]; /* [0] sync status, [1] ready status */

extern volatile unsigned char *D_800750FC; /* CDIO index register */
extern void (*D_80074E34)();               /* sync (ready) IRQ callback */
extern void (*D_80074E38)();               /* data-ready IRQ callback */
extern unsigned char D_80075AD0[];         /* sync result bytes */
extern unsigned char D_80075AD8[];         /* ready result bytes */

/* libcd CdSync (0x800647a0, 640 bytes): wait for the drive to reach a terminal
   state. Spin up to ~0x3C0 VSyncs (or 0x3C0000 iterations); on timeout print
   "CD timeout: <op>:(<sync>) Sync=.., Ready=.." and reset the controller
   (return -1). Each pass drains pending controller interrupts, dispatching the
   data-ready and sync callbacks. Returns the status once it is Complete (2) or
   Error (5), copying the 8 sync result bytes to `result`; in poll mode
   (mode != 0) returns 0 while still busy.

   The timeout arm's do/while(0) gives its refs loop-depth 2 weight, so
   local-alloc ranks the stack-arg temp above the long idx[0] address chain
   (which then takes a0). The volatile idx/name reads pin the original's
   load order, and the direct name load into a1 leaves the format-string set
   alone in the last slot before the call. */
int CdSync(int mode, unsigned char *result) {
  register unsigned char *idx asm("$18");
  register unsigned char *rdy asm("$20");
  register int *tbl asm("$19");
  int flag;
  int irq;
  int save;
  int c;
  int old;
  int n;
  unsigned char *src;
  unsigned char *dst;

  D_80075AE8 = VSync(-1) + 0x3C0;
  D_80075AEC = 0;
  D_80075AF0 = D_80011E84;
  tbl = D_80074EDC;
  idx = D_80075114;
  rdy = idx + 1;
top:
  if (D_80075AE8 < VSync(-1) ||
      (old = D_80075AEC, D_80075AEC = old + 1, 0x3C0000 < old)) {
    do {
      WriteString(D_80011DFC);
      {
        int i0 = ((volatile unsigned char *)idx)[0];
        int i1 = ((volatile unsigned char *)idx)[1];
        WritePrintf(D_80011E0C, *(char *volatile *)&D_80075AF0,
                    D_80074E5C[D_80074E55], tbl[i0], tbl[i1]);
      }
      CdResetController();
      flag = -1;
    } while (0);
  } else {
    flag = 0;
  }
  if (flag != 0) {
    return -1;
  }
  if (FUN_8005df1c() != 0) {
    save = *D_800750FC & 3;
    while ((irq = CdProcessInterrupt()) != 0) {
      if ((irq & 4) && D_80074E38 != 0) {
        D_80074E38(rdy[0], D_80075AD8);
      }
      if ((irq & 2) && D_80074E34 != 0) {
        D_80074E34(idx[0], D_80075AD0);
      }
    }
    *D_800750FC = save;
  }
  c = *(volatile unsigned char *)idx;
  if (c == 2 || c == 5) {
    *(volatile unsigned char *)idx = 2;
    dst = result;
    src = D_80075AD0;
    if (dst != 0) {
      n = 7;
      do {
        *dst++ = *src++;
      } while (n-- != 0);
    }
    return c;
  }
  if (mode == 0) {
    goto top;
  }
  return 0;
}
