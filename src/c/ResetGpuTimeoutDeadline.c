#include "globals.h"

extern int VSync(int mode);

/* Arm the GPU watchdog: deadline = current VSync count + 0xf0 frames, and clear
   the spin counter (0x80062090, 52 bytes). */
void ResetGpuTimeoutDeadline(void) {
  g_nGpuWatchdogDeadline = VSync(-1) + 0xF0;
  g_nGpuWatchdogSpinCount = 0;
}
