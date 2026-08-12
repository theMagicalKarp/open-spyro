#include "globals.h"

extern int VSync(int mode);
extern int WritePrintf(char *fmt, ...);
extern int SetInterruptMask(int mask);

extern unsigned int *D_80074B54;      /* DPCR register pointer */
extern unsigned int g_adwLastGpuOp[]; /* alias @ g_dwLastGpuOpFunc */
extern char D_8001195C[];             /* "GPU timeout:que=%d,stat=%08x,..." */
extern char D_80011990[];             /* last-op diagnostic format */

/* libgpu GPU watchdog: called from EnqueueGpuOp when the 64-entry DMA ring is
   full. Returns 0 while there is still time on the current frame budget
   (g_nGpuWatchdogDeadline vs VSync(-1), plus g_nGpuWatchdogSpinCount under
   0xf0000), -1 once the GPU has wedged. On timeout it prints the watchdog
   diagnostics, hard-resets DMA channel 2 (GPU CHCR=0x401, DPCR bit 0x800),
   issues the GP1 resets (0x02000000, 0x01000000) and clears the ring
   head/tail. (0x800620c4, 364 bytes.) */
int CheckGpuTimeout(void) {
  int mask;

  if (VSync(-1) > g_nGpuWatchdogDeadline ||
      g_nGpuWatchdogSpinCount++ > 0xF0000) {
    *((volatile unsigned int *)g_pGpuStatReg);
    WritePrintf(D_8001195C,
                (*((volatile int *)&g_nGpuQueueWriteIndex) -
                 *((volatile int *)&g_nGpuQueueReadIndex)) &
                    0x3F,
                *((volatile unsigned int *)g_pGpuStatReg),
                *((volatile unsigned int *)g_pDmaGpuChcrReg),
                *((volatile unsigned int *)g_pDmaGpuMadrReg));
    WritePrintf(D_80011990, *((volatile unsigned int *)g_adwLastGpuOp),
                g_dwLastGpuOpArg1, g_dwLastGpuOpArg2);

    mask = SetInterruptMask(0);
    *((volatile int *)&g_nGpuQueueReadIndex) = 0;
    g_nGpuQueueWriteIndex = *((volatile int *)&g_nGpuQueueReadIndex);
    g_nResetGraphSavedIMask = mask;
    *((volatile unsigned int *)g_pDmaGpuChcrReg) = 0x401;
    *D_80074B54 |= 0x800;
    *((volatile unsigned int *)g_pGpuStatReg) = 0x2000000;
    *((volatile unsigned int *)g_pGpuStatReg) = 0x1000000;
    SetInterruptMask(g_nResetGraphSavedIMask);
    return -1;
  }
  return 0;
}
