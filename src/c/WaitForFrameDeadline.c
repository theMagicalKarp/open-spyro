#include "globals.h"

extern void WriteString(unsigned char *msg);
extern void ChangeClearPAD(long enable);
extern void ChangeClearRCnt();
extern unsigned char D_800115FC[];

/* libgpu VSync helper: spins until the vblank tick g_nRcntAuxState reaches
   `deadline`, with a timeout budget of `mode << 15` polls; on timeout prints
   the "VSync: timeout" message and resets the pad/root-counter callbacks.
   (0x8005dd0c, 156 bytes.) The tick read is volatile as well as the budget:
   a volatile store followed by a volatile load keeps source order, which
   holds the budget init above the first tick load and lets reorg put the
   prologue `sw ra` into the guard's delay slot. */
void WaitForFrameDeadline(int deadline, int mode) {
  volatile int budget = mode << 15;
  while (*(volatile int *)&g_nRcntAuxState < deadline) {
    if (--budget == -1) {
      WriteString(D_800115FC);
      ChangeClearPAD(0);
      ChangeClearRCnt(3, 0);
      return;
    }
  }
}
