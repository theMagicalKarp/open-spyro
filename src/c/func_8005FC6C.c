#include "globals.h"

extern char D_800118E4[]; /* GPU debug tag */
extern int D_80074B20;    /* OT terminator sentinel */

/* Reset an ordering table to empty (0x8005fc6c): optionally trace, hand the
   table to the +0x2C command-table method, then write the terminator link
   into the head entry. Returns the table. */
/* The return copy is a `$2` register local so the terminator store goes
   through v0. The dead `ot = 0` kills the copy's equivalence, so cse2 cannot
   fold the store address back to the parameter. The 0xFFFFFF mask is a `$4`
   register local, as the original has it in a0. */
int *func_8005FC6C(int *ot, int n) {
  register int m asm("$4");
  register int *r asm("$2");

  if (g_bGpuDebugLevel >= 2) {
    ((void (*)(char *, int *, int))g_pfnGpuDebugPrintf)(D_800118E4, ot, n);
  }
  (*(void (**)(int *, int))((char *)g_pGpuDispatchTable + 0x2C))(ot, n);
  m = 0xFFFFFF;
  r = ot;
  ot = 0;
  *r = (int)&D_80074B20 & m;
  return r;
}
