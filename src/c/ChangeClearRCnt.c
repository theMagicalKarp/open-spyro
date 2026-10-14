#include "globals.h"

/* BIOS call C(0xA) (0x8005ddb8, 12 bytes): jump to the kernel's 0xC0
   dispatcher with the function number in $t1, arguments passed through.
   gcc has no tail calls, so the jump is a computed goto; bios_fn is a
   file-scope register variable, which is always live, so its store is kept
   and reorg moves it into the jump's delay slot. */
register int bios_fn asm("$9");

void ChangeClearRCnt(void) {
  register void *vec asm("$10");

  vec = (void *)0xC0;
  bios_fn = 0xA;
  goto *vec;
}
