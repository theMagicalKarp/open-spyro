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

extern int D_80075AE8;           /* VSync deadline stamp */
extern int D_80075AEC;           /* spin counter */
extern char *D_80075AF0;         /* op-name string for the timeout message */
extern int D_80074E5C[];         /* sync-status name table */
extern int D_80074EDC[];         /* ready-status name table */
extern unsigned char D_80074E55; /* sync-status index */
extern unsigned char
    D_80075114[]; /* [0] sync status, [1] ready status, [2] pending flag */

extern volatile unsigned char *D_800750FC; /* CDIO index register */
extern void (*D_80074E34)();               /* sync (ready) IRQ callback */
extern void (*D_80074E38)();               /* data-ready IRQ callback */
extern unsigned char D_80075AD0[];         /* sync result bytes */
extern unsigned char D_80075AD8[];         /* ready result bytes */
extern unsigned char D_80075AE0[];         /* pending result bytes */
extern char D_80011E8C[];                  /* "CD_ready" */

/* libcd CdReady (0x80064a20, 716 bytes): wait for a data-ready (or data-end)
   interrupt. Same timeout/drain frame as CdSync; returns the pending status
   with its 8 result bytes copied to `result`, 0 in poll mode while nothing
   is pending, -1 on timeout.

   The timeout printf block is CdSync's recipe (do/while(0) ref weight,
   volatile read order, direct name load). The do/while(0) around the
   drain arm lifts `save` over `result` in global-alloc so they take s1/s4,
   and the first copy tests `result` itself so `move a1,s4` fills the
   branch delay slot. */
int func_80064A20(int mode, unsigned char *result) {
  register unsigned char *idx asm("$18");
  register unsigned char *rdy asm("$22");
  register unsigned char *pend asm("$19");
  unsigned char *dst;
  register int *tbl asm("$21");
  int flag;
  int irq;
  int save;
  int c;
  int old;
  int n;
  unsigned char *src;

  D_80075AE8 = VSync(-1) + 0x3C0;
  D_80075AEC = 0;
  D_80075AF0 = D_80011E8C;
  tbl = D_80074EDC;
  idx = D_80075114;
  rdy = idx + 1;
  pend = idx + 2;
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
    do {
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
    } while (0);
  }
  if ((c = *(volatile unsigned char *)pend) != 0) {
    *(volatile unsigned char *)pend = 0;
    src = D_80075AE0;
    if (result != 0) {
      dst = result;
      n = 7;
      do {
        *dst++ = *src++;
      } while (n-- != 0);
    }
    return c;
  }
  if ((c = *(volatile unsigned char *)(pend - 1)) != 0) {
    *(volatile unsigned char *)(pend - 1) = 0;
    dst = result;
    src = D_80075AD8;
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
