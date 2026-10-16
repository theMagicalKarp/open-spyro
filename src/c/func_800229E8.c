#include "globals.h"

/* 0x800229e8, 68 bytes -- store the value in $v0 through the pointer the
   caller left in $at, then restore ra/fp/sp/gp/s7-s0 from
   g_anStaticRegSaveArea and return through the restored ra.

   Matching notes (ex-wall "handwritten register-restore blob"): the restored
   registers are file-scope register variables (setjmp/longjmp recipe, A411),
   so gcc writes them directly with no prologue; the incoming $at/$v0 are
   local register variables seeded by an empty asm (A410), and $at is then
   reused as the save-area base through an asm-opaque copy (A383). The trailing empty asm keeps reorg out of
   the return's delay slot. */
register int rr_ra asm("$31");
register int rr_gp asm("$28");
register int rr_sp asm("$29");
register int rr_fp asm("$30");
register int rr_s0 asm("$16");
register int rr_s1 asm("$17");
register int rr_s2 asm("$18");
register int rr_s3 asm("$19");
register int rr_s4 asm("$20");
register int rr_s5 asm("$21");
register int rr_s6 asm("$22");
register int rr_s7 asm("$23");

void func_800229E8(void) {
  register int *at asm("$1");
  register int v0 asm("$2");

  __asm__("" : "=r"(at), "=r"(v0));
  *at = v0;
  __asm__("" : "=r"(at) : "0"(g_anStaticRegSaveArea));
  rr_ra = at[11];
  rr_fp = at[10];
  rr_sp = at[9];
  rr_gp = at[8];
  rr_s7 = at[7];
  rr_s6 = at[6];
  rr_s5 = at[5];
  rr_s4 = at[4];
  rr_s3 = at[3];
  rr_s2 = at[2];
  rr_s1 = at[1];
  rr_s0 = at[0];
  __asm__ volatile("");
}
