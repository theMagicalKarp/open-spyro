#include "globals.h"

extern int SetInterruptMask(int mask);
extern void DmaCallback();

/* libgpu DMA-queue drain (0x80061b00, 748 bytes). Called whenever the GPU may
   have completed an op. While DMA channel 2 (GPU CHCR bit 0x1000000) is idle
   and slots are queued, spins on the GP1 ready bit (0x4000000) and invokes each
   pending slot's func(args, arg4) in FIFO order until the ring drains or the
   GPU goes busy again; then, if the ring is empty and a one-shot completion
   callback is armed, fires it once. Returns the remaining ring depth. The ring
   head/tail indices are shared with the DMA-completion IRQ handler, hence
   volatile. The empty asm statements in the dispatch block are zero-byte
   scheduling barriers that fix its load order. */

extern unsigned int g_adwLastGpuOp[]; /* alias @ g_dwLastGpuOpFunc — held base
                                         for the Func store */

/* Distinct word[] views of the three used ring-slot fields (ring+0/+4/+8).
   Referencing them by distinct symbols keeps cse from hoisting one shared &ring
   base into a register: each access re-materializes %hi(ring+off) fresh,
   matching the original. */
extern unsigned int g_adwGpuRingFunc[]; /* ring + 0x0 */
extern unsigned int g_adwGpuRingArgs[]; /* ring + 0x4 */
extern unsigned int g_adwGpuRingArg4[]; /* ring + 0x8 */

/* Second views of the same two slots, for the POST-call re-reads. Naming them
   with a different base symbol stops cse value-numbering each pair into one
   pseudo; a shared pseudo would be live across the dispatch call, reload would
   un-allocate it and alter_reg would buy it an 8-byte BLKmode frame slot
   (two pairs = vars= 16). With distinct bases the frame is vars= 0, the
   original's. */
extern unsigned int g_adwGpuRingArgsPost[]; /* ring + 0x4 */
extern unsigned int g_adwGpuRingArg4Post[]; /* ring + 0x8 */

/* Incomplete-array view of the pending flag: the read and the clear share one
   materialized base in the original (addiu v1,%lo(); lw 0(v1) … sw zero,0(v1)),
   which a plain 4-byte extern loses to the sdata cost tie. */
extern int g_anGpuQueueCompletionPending[];

#define WIDX (*(volatile int *)&g_nGpuQueueWriteIndex)
#define RIDX (*(volatile int *)&g_nGpuQueueReadIndex)
#define CHCR (*(volatile unsigned int *)g_pDmaGpuChcrReg)

unsigned int FlushGpuQueue(void) {
  volatile unsigned int *last;

  if ((CHCR & 0x1000000) != 0) {
    return 1;
  }
  g_nFlushGpuSavedIMask = SetInterruptMask(0);
  if (WIDX != RIDX && (CHCR & 0x1000000) == 0) {
    last = (volatile unsigned int *)g_adwLastGpuOp;
  drain:
    if (((RIDX + 1) & 0x3F) == WIDX && g_pfnGpuQueueCompletionCallback == 0) {
      DmaCallback(2, 0);
    }
    while ((*(volatile unsigned int *)g_pGpuStatReg & 0x4000000) == 0) {
      ;
    }
    {
      register int r1 asm("$5") = RIDX;
      register int r2 asm("$3") = RIDX;
      unsigned int a;
      register int f asm("$3");

      __asm__ volatile("" : : "r"(r2 * 0x60));
      a = g_adwGpuRingArgs[r2 * 0x18];
      f = r1 * 3;

      __asm__ volatile("" : : "r"(a), "r"(f));
      {
        register int r3 asm("$5") = RIDX;

        f <<= 5;
        (*(void (**)(unsigned int, unsigned int))(g_abGpuOpQueueRing + f))(
            a, g_adwGpuRingArg4[r3 * 0x18]);
      }
    }
    last[0] = g_adwGpuRingFunc[RIDX * 0x18];
    *(volatile unsigned int *)&g_dwLastGpuOpArg1 =
        g_adwGpuRingArgsPost[RIDX * 0x18];
    *(volatile unsigned int *)&g_dwLastGpuOpArg2 =
        g_adwGpuRingArg4Post[RIDX * 0x18];
    RIDX = (RIDX + 1) & 0x3F;
    if (WIDX != RIDX && (CHCR & 0x1000000) == 0) {
      goto drain;
    }
  }
  SetInterruptMask(g_nFlushGpuSavedIMask);
  if (WIDX == RIDX && (CHCR & 0x1000000) == 0 &&
      g_anGpuQueueCompletionPending[0] != 0 &&
      g_pfnGpuQueueCompletionCallback != 0) {
    *(volatile int *)g_anGpuQueueCompletionPending = 0;
    (*(void (*)(void))g_pfnGpuQueueCompletionCallback)();
  }
  return (WIDX - RIDX) & 0x3F;
}
