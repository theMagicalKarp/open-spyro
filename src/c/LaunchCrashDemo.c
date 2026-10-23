#include "globals.h"

extern void QuiesceHardwareForExec(void);

/* Tears the running game down and chain-loads the crash/demo executable: logs
   "Shutting Down", quiesces the hardware (QuiesceHardwareForExec), deinits and
   re-inits the BIOS 96 CD-FS, logs "Launching Crash Demo", then LoadExec's
   CRASH.EXE. (0x8002bab8, 104 bytes.) */
void LaunchCrashDemo(void) {
  WritePrintf(g_szShuttingDownMsg);
  QuiesceHardwareForExec();
  Bios96Remove();
  _96_init();
  WritePrintf(g_szLaunchingCrashDemoMsg);
  LoadExec(g_szCrashDemoExePath, 0x801FFF00, 0);
}
