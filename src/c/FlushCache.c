#include "globals.h"

/* BIOS call A(0x44) (0x800626e8, 12 bytes): jump to the kernel's 0xA0
   dispatcher with the function number in $t1, arguments passed through.
   gcc has no tail calls, so the jump is a computed goto; bios_fn is a
   file-scope register variable, which is always live, so its store is kept
   and reorg moves it into the jump's delay slot. */
register int bios_fn asm("$9");

void FlushCache(void) {
  register void *vec asm("$10");

  vec = (void *)0xA0;
  bios_fn = 0x44;
  goto *vec;
}
