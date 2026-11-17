/* Native replacement for src/main/render/ot.c. The PS1 C
 * returns through a file-scope `$31` register variable; this is the same
 * function in plain C. */

#include "globals.h"

/* 0x800168dc: append a primitive to the active OT depth slot. slot[0] is the
 * slot's tail prim, slot[1] its head; a non-empty slot links the old tail to
 * `prim` (24-bit address in the tag's low three bytes). */
void AddPrimToOT(unsigned int prim) {
  int *slot = g_pOtActiveDepthSlot;
  unsigned char *tail = (unsigned char *)slot[0];

  slot[0] = prim;
  if (tail != 0) {
    *(unsigned short *)tail = prim;
    tail[2] = prim >> 16;
    return;
  }
  slot[1] = prim;
}
