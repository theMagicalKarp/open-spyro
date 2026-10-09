#include "globals.h"

extern void ResetGpuTimeoutDeadline(void);
extern int CheckGpuTimeout(void);
extern int SetInterruptMask(int mask);
extern void DmaCallback();
extern unsigned int FlushGpuQueue(void);

/* libgpu DMA-queue enqueue (0x80061820, 736 bytes). If the GPU + DMA + 64-entry
   ring are all idle, invoke func(args, arg4) immediately; otherwise append the
   call to the ring for later replay by FlushGpuQueue. The ring head/tail
   indices are shared with the DMA-completion IRQ handler, hence volatile.

   nbytes/arg4 are pinned to s1/s2 and the copy loop's slot address to v1/v0
   to reproduce the original's allocation. The ring stores go through
   byte-offset casts: a non-struct store keeps the next WIDX reload behind
   it, where an array-index store would let the load pass (A357). */

extern unsigned int g_adwLastGpuOp[]; /* alias @ g_dwLastGpuOpFunc — held base
                                         for the Func store */

/* Distinct word[] views of the three used ring-slot fields (ring+0/+4/+8) and
   of the inline-args area (ring+0xC); see FlushGpuQueue for why each field
   needs its own symbol. */
extern unsigned int g_adwGpuRingFunc[];  /* ring + 0x0 */
extern unsigned int g_adwGpuRingArgs[];  /* ring + 0x4 */
extern unsigned int g_adwGpuRingArg4[];  /* ring + 0x8 */
extern unsigned char g_abGpuOpArgArea[]; /* ring + 0xC */

#define WIDX (*(volatile int *)&g_nGpuQueueWriteIndex)
#define RIDX (*(volatile int *)&g_nGpuQueueReadIndex)
#define CHCR (*(volatile unsigned int *)g_pDmaGpuChcrReg)

unsigned int EnqueueGpuOp(void *func, unsigned int *args, int nbytes,
                          unsigned int arg4) {
  unsigned int *src;
  char *area;
  int i;
  int n;
  register int nb asm("$17") = nbytes;
  register unsigned int a4 asm("$18") = arg4;

  ResetGpuTimeoutDeadline();
  while (((WIDX + 1) & 0x3F) == RIDX) {
    if (CheckGpuTimeout() != 0) {
      return -1;
    }
    FlushGpuQueue();
  }
  g_nEnqueueGpuSavedIMask = SetInterruptMask(0);
  g_nGpuQueueCompletionPending = 1;
  if (g_bGpuQueueModeActive == 0 || (WIDX == RIDX && (CHCR & 0x1000000) == 0 &&
                                     g_pfnGpuQueueCompletionCallback == 0)) {
    while ((*(volatile unsigned int *)g_pGpuStatReg & 0x4000000) == 0) {
      ;
    }
    ((void (*)(unsigned int *, unsigned int))func)(args, a4);
    *(volatile unsigned int *)g_adwLastGpuOp = (unsigned int)func;
    *(volatile unsigned int *)&g_dwLastGpuOpArg1 = (unsigned int)args;
    *(volatile unsigned int *)&g_dwLastGpuOpArg2 = a4;
    SetInterruptMask(g_nEnqueueGpuSavedIMask);
    return 0;
  }
  DmaCallback(2, FlushGpuQueue);
  if (nb != 0) {
    i = 0;
    area = (char *)g_abGpuOpArgArea;
    src = args;
  copy:
    n = nb / 4;
    if (i < n) {
      {
        register int wi asm("$3") = WIDX;
        register int w asm("$2") = wi * 0x60;

        w += (int)area;
        *(volatile unsigned int *)(i * 4 + w) = *src++;
      }
      i++;
      goto copy;
    }
    *(unsigned int *)((char *)g_adwGpuRingArgs + WIDX * 0x60) =
        (unsigned int)((char *)g_abGpuOpArgArea + WIDX * 0x60);
  } else {
    *(unsigned int *)((char *)g_adwGpuRingArgs + WIDX * 0x60) =
        (unsigned int)args;
  }
  *(unsigned int *)((char *)g_adwGpuRingArg4 + WIDX * 0x60) = a4;
  *(unsigned int *)((char *)g_adwGpuRingFunc + WIDX * 0x60) =
      (unsigned int)func;
  WIDX = (WIDX + 1) & 0x3F;
  SetInterruptMask(g_nEnqueueGpuSavedIMask);
  FlushGpuQueue();
  return (WIDX - RIDX) & 0x3F;
}
