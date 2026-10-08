extern int VSync(int mode);
extern void WriteString(char *s);
extern int WritePrintf();
extern void CdResetController(void);
extern int CdSync(int mode, unsigned char *result);
extern int FUN_8005df1c(void);
extern int CdProcessInterrupt(void);

extern char D_80011DFC[]; /* "CD timeout: " */
extern char D_80011E0C[]; /* "%s:(%s) Sync=%s, Ready=%s\n" */
extern char D_80011E98[]; /* "%s...\n" */
extern char D_80011EA0[]; /* "%s: no param\n" */
extern char D_80011EB0[]; /* "CD_cw" */

extern int D_80074E40;             /* libcd debug level */
extern unsigned char D_80074E50[]; /* last setloc position */
extern unsigned char D_80074E54;   /* saved mode byte */
extern unsigned char D_80074E55;   /* last command */
extern int D_80074E5C[];           /* command name table */
extern int D_80074EDC[];           /* status name table */
extern int D_80074F7C[]; /* [cmd] clears ready; [64 + cmd] param count */
extern int D_8007507C[]; /* [cmd] command needs a parameter */
extern volatile unsigned char D_80075114[]; /* [0] sync, [1] ready status */

extern volatile unsigned char *D_800750FC; /* CDIO index register */
extern volatile unsigned char *D_80075100; /* CDIO command register */
extern volatile unsigned char *D_80075104; /* CDIO parameter register */
extern void (*D_80074E34)();               /* sync IRQ callback */
extern void (*D_80074E38)();               /* data-ready IRQ callback */
extern unsigned char D_80075AD0[];         /* sync result bytes */
extern unsigned char D_80075AD8[];         /* ready result bytes */

extern int D_80075AE8;   /* VSync deadline stamp */
extern int D_80075AEC;   /* spin counter */
extern char *D_80075AF0; /* op-name string for the timeout message */

/* libcd command issue (0x80064cec, 1052 bytes): wait for the previous command,
   latch setloc/setmode parameters, write the command and its parameter bytes
   to the CDIO registers, then (unless `async`) spin like CdSync until the
   sync status is non-zero, copying the 8 sync result bytes to `result`.
   Returns -2 when a parameterised command has no `param`, -1 on timeout or a
   CdlDiskError status, 0 otherwise.

   The status bytes are a volatile array: every access goes through a `la`
   (`sb zero,0(v0)`, `lbu 0(v0)`), which is what lets loop.c hoist the wait
   loop's bases into s2/s3/s4 (cse2 then turns the s2 init into a copy of
   the entry test's a0). The [1] clear is a plain store. Unlike CdSync, the
   wait loop is a real `while`, so its refs already carry loop-depth 2 and
   the printf block needs no do/while(0). */
int CdCommandSync(unsigned char com, unsigned char *param,
                  unsigned char *result, int async) {
  unsigned char *dst;
  unsigned char *src;
  int flag;
  int irq;
  int save;
  int old;
  int i;
  int n;

  if (D_80074E40 >= 2) {
    WritePrintf(D_80011E98, D_80074E5C[com]);
  }
  if (D_8007507C[com] != 0 && param == 0) {
    if (D_80074E40 > 0) {
      WritePrintf(D_80011EA0, D_80074E5C[com]);
    }
    return -2;
  }
  CdSync(0, 0);
  if (com == 2) {
    for (i = 0; i < 4; i++) {
      D_80074E50[i] = param[i];
    }
  }
  if (com == 14) {
    D_80074E54 = param[0];
  }
  D_80075114[0] = 0;
  if (D_80074F7C[com] != 0) {
    ((unsigned char *)D_80075114)[1] = 0;
  }
  *D_800750FC = 0;
  for (i = 0; i < D_80074F7C[com + 64]; i++) {
    *D_80075104 = param[i];
  }
  D_80074E55 = com;
  *D_80075100 = com;
  if (async != 0) {
    return 0;
  }
  D_80075AE8 = VSync(-1) + 0x3C0;
  D_80075AEC = 0;
  D_80075AF0 = D_80011EB0;
  while (D_80075114[0] == 0) {
    if (D_80075AE8 < VSync(-1) ||
        (old = D_80075AEC, D_80075AEC = old + 1, 0x3C0000 < old)) {
      WriteString(D_80011DFC);
      {
        int i0 = D_80075114[0];
        int i1 = D_80075114[1];
        WritePrintf(D_80011E0C, *(char *volatile *)&D_80075AF0,
                    D_80074E5C[D_80074E55], D_80074EDC[i0], D_80074EDC[i1]);
      }
      CdResetController();
      flag = -1;
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
          D_80074E38(D_80075114[1], D_80075AD8);
        }
        if ((irq & 2) && D_80074E34 != 0) {
          D_80074E34(D_80075114[0], D_80075AD0);
        }
      }
      *D_800750FC = save;
    }
  }
  dst = result;
  src = D_80075AD0;
  if (dst != 0) {
    n = 7;
    do {
      *dst++ = *src++;
    } while (n-- != 0);
  }
  return -(D_80075114[0] == 5);
}
