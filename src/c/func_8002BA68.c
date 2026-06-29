#include "globals.h"

extern void func_8005DEBC(void);

/* Brings the console hardware subsystems back to a known idle state: blanks the
   display, re-inits the CD, clears the CD read callback, the VSync callback and
   pad comms, resets the GPU, then runs the libapi reset helper.
   (0x8002ba68, 80 bytes.) */
void func_8002BA68(void) {
  SetDispMask(0);
  CdInit();
  CdReadCallback(0);
  InstallVSyncCallback(0);
  PadStopCom();
  ResetGraph(3);
  func_8005DEBC();
}
