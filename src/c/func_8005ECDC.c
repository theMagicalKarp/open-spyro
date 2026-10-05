#include "globals.h"

/* libgpu primitive tag: 24-bit next-pointer and 8-bit word count. */
typedef struct {
  unsigned addr : 24;
  unsigned len : 8;
} P_TAG;

/* 0x8005ecdc (0x3c) — libgpu AddPrim: link primitive `p` into the
   ordering-table entry `ot` (p's tag takes ot's old next-pointer, ot points
   at p). Written with the P_TAG bitfield, as libgpu's setaddr/getaddr macros
   do; the explicit-mask spelling gets the two mask constants in swapped
   registers. */
void func_8005ECDC(P_TAG *ot, P_TAG *p) {
  p->addr = ot->addr;
  ot->addr = (unsigned int)p;
}
