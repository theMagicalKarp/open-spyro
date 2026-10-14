#include "globals.h"

/* BIOS call B(0x19) (0x8005e4f8, 12 bytes): jump to the kernel's 0xB0
   dispatcher with the function number in $t1, arguments passed through.
   gcc has no tail calls, so the jump is a computed goto; bios_fn is a
   file-scope register variable, which is always live, so its store is kept
   and reorg moves it into the jump's delay slot. */
register int bios_fn asm("$9");

void HookEntryInt(void) {
  register void *vec asm("$10");

  vec = (void *)0xB0;
  bios_fn = 0x19;
  goto *vec;
}
