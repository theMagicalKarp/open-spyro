#include "globals.h"

extern char D_80010D78[]; /* "Shutting Down\n" */
extern char D_80010D88[]; /* "Launching Crash Demo\n" */
extern char D_80010DA0[]; /* "cdrom:\\S0\\CRASH.EXE;1" */
extern void func_8002BA68(void);

/* Tears the running game down and chain-loads the crash/demo executable: logs
   "Shutting Down", quiesces the hardware (func_8002BA68), deinits and re-inits
   the BIOS 96 CD-FS, logs "Launching Crash Demo", then LoadExec's CRASH.EXE.
   (0x8002bab8, 104 bytes.) */
void func_8002BAB8(void) {
  WritePrintf(D_80010D78);
  func_8002BA68();
  Bios96Remove();
  _96_init();
  WritePrintf(D_80010D88);
  LoadExec(D_80010DA0, 0x801FFF00, 0);
}
