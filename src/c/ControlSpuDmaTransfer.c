#include "globals.h"

extern void SetSpuDmaReadStateBits(void);
extern void SetSpuDmaWriteStateBits(void);

extern char *D_80073554;          /* SPU hardware register block base */
extern volatile int *D_80073558;  /* SPU DMA channel MADR register */
extern volatile int *D_8007355C;  /* SPU DMA channel BCR register */
extern volatile int *D_80073560;  /* SPU DMA channel CHCR register */
extern unsigned short D_8007356C; /* encoded SPU transfer address shadow */

/* gcc 2.7.2 stdarg (no system headers in this build): the `...` in the
   signature triggers the register-save prologue; va_arg is the standard
   pointer-bump expansion. */
#define VA_START(ap, last) ((ap) = (char *)__builtin_next_arg(last))
#define VA_ARG_U32(ap) ((ap) += 4, *(unsigned int *)((ap) - 4))

/* 0x8005c1c0 (656 bytes) — SPU DMA transfer controller. op 2 (addr) stamps the
   encoded transfer address into the SPU address register (+0x1a6); op 1 / op 0
   wait for that register to settle, then set the SPUCNT (+0x1aa) transfer-mode
   bits (write 0x20 / read 0x30) and the pending-direction flag; op 3
   (madr, size) waits for the mode bits, fires the direction's state-bit
   callback, then kicks the DMA channel through the MADR/BCR/CHCR shadows.
   Returns -2 if any wait spins past 0xF00 iterations, else 0.

   Matching notes (load-bearing, do not "clean up"):
   - The empty asm copy of the entry wait-test read keeps the original's
     `andi 0xFFFF` after the `lhu` (combine otherwise deletes the redundant
     mask on a single-use zero-extending load; cookbook B22 / A383).
   - That asm is a stop for reorg's backward delay-slot search, so the wait
     loops are written as `if (cur != target) { n = 0; do ... while }`: the
     `move v1,zero` then reaches the guard's delay slot from the fall-through.
   - With `n = 0` inside the guard, n no longer conflicts with the hoisted
     constant 1, which would take v1; the `$3` reservation (k3, live from
     entry to case 3) pushes it to a2 as in the original (A384).
   - A `break` + one shared `return 0` puts `move v0,zero` after the CHCR
     store; reorg copies it into each arm's `j` delay slot (A186). */
int ControlSpuDmaTransfer(int op, ...) {
  char *ap;
  int target;
  char *base;
  int cur;
  unsigned short want;
  unsigned int n;
  unsigned int size;
  unsigned int blocks;
  unsigned int frac;
  int cmd;
  register int k3 asm("$3");

  __asm__("" : "=r"(k3));
  VA_START(ap, op);
  (void)(&op);

  switch (op) {
  case 2:
    size = VA_ARG_U32(ap);
    cur = size >> g_nSpuAddrShift;
    {
      char *b = D_80073554;

      D_8007356C = cur;
      *(volatile short *)(b + 0x1A6) = cur;
    }
    break;

  case 1:
    base = D_80073554;
    target = D_8007356C;
    cur = *(volatile unsigned short *)(base + 0x1A6);
    __asm__("" : "=r"(cur) : "0"(cur));
    g_nSpuPendingDmaDirection = 0;
    if (cur != target) {
      n = 0;
      do {
        if (++n >= 0xF01) {
          return -2;
        }
        cur = *(volatile unsigned short *)(base + 0x1A6);
      } while (cur != target);
    }
    {
      char *b = D_80073554;

      size = *(unsigned short *)(b + 0x1AA);
      size = (size & 0xFFCF) | 0x20;
      *(volatile unsigned short *)(b + 0x1AA) = size;
    }
    break;

  case 0:
    base = D_80073554;
    target = D_8007356C;
    cur = *(volatile unsigned short *)(base + 0x1A6);
    __asm__("" : "=r"(cur) : "0"(cur));
    g_nSpuPendingDmaDirection = 1;
    if (cur != target) {
      n = 0;
      do {
        if (++n >= 0xF01) {
          return -2;
        }
        cur = *(volatile unsigned short *)(base + 0x1A6);
      } while (cur != target);
    }
    {
      char *b = D_80073554;

      size = *(unsigned short *)(b + 0x1AA);
      size |= 0x30;
      *(volatile unsigned short *)(b + 0x1AA) = size;
    }
    break;

  case 3:
    __asm__ volatile("" : : "r"(k3));
    want = 0x20;
    if (g_nSpuPendingDmaDirection == 1) {
      want = 0x30;
    }
    base = D_80073554;
    n = 0;
    while ((*(volatile unsigned short *)(base + 0x1AA) & 0x30) != want) {
      n += 1;
      if (n >= 0xF01) {
        return -2;
      }
    }

    if (g_nSpuPendingDmaDirection == 1) {
      SetSpuDmaReadStateBits();
    } else {
      SetSpuDmaWriteStateBits();
    }
    cmd = 0x1000201;
    size = VA_ARG_U32(ap);
    g_dwSpuDmaMadrShadow = size;
    size = VA_ARG_U32(ap);
    blocks = size >> 6;
    frac = (size & 0x3F) != 0;
    g_nSpuDmaBcrBlockCount = blocks + frac;
    *D_80073558 = *(volatile unsigned int *)(&g_dwSpuDmaMadrShadow);
    *D_8007355C = ((*(volatile int *)(&g_nSpuDmaBcrBlockCount)) << 16) | 0x10;
    if (g_nSpuPendingDmaDirection == 1) {
      cmd = 0x1000200;
    }
    *D_80073560 = cmd;
    break;
  }

  return 0;
}
