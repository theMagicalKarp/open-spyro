#include "globals.h"

register void *ra asm("$31");

/* 0x800168dc, 56 bytes -- append a primitive to the active OT depth slot.

   Matching notes (ex-"handwritten" $at-as-data): the slot pointer and the
   tail prim are pinned to $v1/$at with local register variables (A374);
   an asm-opaque copy keeps the `lui/addiu` + `lw 0` base form (A383); the
   mid-function `jr ra` is a computed goto through a file-scope `$31`
   register variable (no ra save) (gcc 2.7.2 never emits a return insn before
   the epilogue). */
void AddPrimToOT(unsigned int prim) {
  register int *slot asm("$3");
  register unsigned char *tail asm("$1");

  __asm__("" : "=r"(slot) : "0"(&g_pOtActiveDepthSlot));
  slot = *(int **)slot;
  tail = (unsigned char *)slot[0];
  slot[0] = prim;
  if (tail != 0) {
    *(unsigned short *)tail = prim;
    prim >>= 16;
    tail[2] = prim;
    goto *ra;
  }
  slot[1] = prim;
}
