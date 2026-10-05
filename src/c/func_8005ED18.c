#include "globals.h"

/* libgpu primitive tag: 24-bit next-pointer and 8-bit word count. */
typedef struct {
  unsigned addr : 24;
  unsigned len : 8;
} P_TAG;

/* 0x8005ed18 (0x3c) — libgpu AddPrims: link an already-chained run of
   primitives `p0`..`p1` into the ordering-table entry `ot` (the last one
   takes ot's old next-pointer, ot points at the first). */
void func_8005ED18(P_TAG *ot, P_TAG *p0, P_TAG *p1) {
  p1->addr = ot->addr;
  ot->addr = (unsigned int)p0;
}
