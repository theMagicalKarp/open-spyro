#include "globals.h"

/* 0x800638ec, 60 bytes -- PSY-Q setjmp: store $ra, $gp, $sp, $fp and
   $s0-$s7 into the jump buffer and return 0.

   Matching notes (ex-wall "runtime primitive"): every saved register is a
   file-scope register variable, so gcc reads it directly and never treats it
   as a callee-saved register of its own (no prologue). The first empty asm
   keeps the return-value move below the stores; the second (a use of $v0)
   keeps reorg from filling the return's delay slot with it. */
register int jb_ra asm("$31");
register int jb_gp asm("$28");
register int jb_sp asm("$29");
register int jb_fp asm("$30");
register int jb_s0 asm("$16");
register int jb_s1 asm("$17");
register int jb_s2 asm("$18");
register int jb_s3 asm("$19");
register int jb_s4 asm("$20");
register int jb_s5 asm("$21");
register int jb_s6 asm("$22");
register int jb_s7 asm("$23");

int setjmp(__jmp_buf_tag *__env) {
  int *buf = (int *)__env;
  register int rv asm("$2");

  buf[0] = jb_ra;
  buf[11] = jb_gp;
  buf[1] = jb_sp;
  buf[2] = jb_fp;
  buf[3] = jb_s0;
  buf[4] = jb_s1;
  buf[5] = jb_s2;
  buf[6] = jb_s3;
  buf[7] = jb_s4;
  buf[8] = jb_s5;
  buf[9] = jb_s6;
  buf[10] = jb_s7;
  __asm__ volatile("");
  rv = 0;
  __asm__ volatile("" : : "r"(rv));
  return rv;
}
