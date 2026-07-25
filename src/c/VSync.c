#include "globals.h"

extern volatile unsigned int
    *D_800738BC; /* GPU_STAT-shadow reg ptr (interlace/field bit) */
extern volatile unsigned int *D_800738C0; /* TMR_HRETRACE_VAL reg ptr */
extern int WaitForFrameDeadline(int deadline, int mode);

/* libgpu VSync: mode<0 returns the free-running HRETRACE tick delta since the
   last call without waiting; mode==0/1 waits for the next vblank (or field,
   mode==1) via WaitForFrameDeadline, then for interlaced fields waits out
   the field-parity flip on the GPU status shadow register; mode>0 waits for
   the mode'th future vblank. Returns g_nRcntAuxState (the current vblank
   tick) on the waiting paths. (0x8005dbc4, 328 bytes.) */
int VSync(int mode) {
  unsigned int delta;
  unsigned int stat;
  int target;
  int new_var;
  int wait;
  stat = *D_800738BC;
  delta = ((*D_800738C0) - g_nVsyncPrevHRetraceCount) & 0xFFFF;
  if (mode < 0) {
    return g_nRcntAuxState;
  }
  if (mode == 1) {
    return delta;
  }
  new_var = mode > 0;
  if (new_var) {
    target = 1;
    target = (g_nVsyncPrevVblankCount - target) + mode;
  } else {
    target = g_nVsyncPrevVblankCount;
  }
  do {
    wait = 0;
    if (mode > 0) {
      wait = mode - 1;
    }
    WaitForFrameDeadline(target, wait);
    stat = *D_800738BC;
  } while (0);
  {
    int one = 1;
    WaitForFrameDeadline(g_nRcntAuxState + 1, one);
  }
  if (((stat & 0x400000) != 0) && (((int)(stat ^ (*D_800738BC))) >= 0)) {
    if (1) {
      do {
      } while (((stat ^ (*D_800738BC)) & 0x80000000) == 0);
    }
  }
  g_nVsyncPrevVblankCount = g_nRcntAuxState;
  g_nVsyncPrevHRetraceCount = *D_800738C0;
  return delta;
}
