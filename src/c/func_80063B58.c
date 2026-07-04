#include "globals.h"

extern int D_80074E40;     /* libcd debug level */
extern char *D_80074E5C[]; /* command-name string table (0x1C entries) */
extern char *D_80074EDC[]; /* interrupt-name string table (7 entries) */
extern char D_80011CB8[];  /* "unknown" fallback string */

/* libcd CdSetDebug: set the debug print level, returning the previous one.
   (0x80063b58, 0x80 bytes across 3 funcs.) */
int func_80063B58(int level) {
  int old = D_80074E40;
  D_80074E40 = level;
  return old;
}

/* libcd CdComstr: command byte -> command-name string (or "unknown"). */
char *func_80063B70(unsigned int cmd) {
  char *s;
  cmd &= 0xFF;
  if (cmd < 0x1C) {
    s = D_80074E5C[cmd];
  } else {
    s = D_80011CB8;
  }
  return s;
}

/* libcd CdIntstr: interrupt byte -> interrupt-name string (or "unknown"). */
char *func_80063BA4(unsigned int intr) {
  char *s;
  intr &= 0xFF;
  if (intr < 7) {
    s = D_80074EDC[intr];
  } else {
    s = D_80011CB8;
  }
  return s;
}
