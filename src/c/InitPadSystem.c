#include "globals.h"

extern void PadInitDirect(unsigned char *report1, unsigned char *report2);
extern void ResetPadStickCalibrationDefaults(int pad);
extern void FinalizePadCalibration(unsigned int *pad);
extern void PadStartCom(void);
extern void PollPadAndDistributeInput(void);
extern void InstallVSyncCallback(unsigned int cb);

/* Bring up the controller subsystem at boot: init the BIOS pad driver with the
   two raw report buffers, seed both pads' stick calibration, clear the rumble
   state, then start pad comms, do a first poll, and install the per-VSync poll
   callback. (0x800123c8, 152 bytes.) */
void InitPadSystem(void) {
  unsigned int *pad = &g_dwPad2Buttons;
  PadInitDirect(g_abPadRawReport, g_abPad2RawReport);
  ResetPadStickCalibrationDefaults((int)pad);
  FinalizePadCalibration(pad);
  g_nHitRumbleTimer = 0;
  g_nVibrationLevel = 0;
  g_nPulseRumbleTimer = 0;
  g_nPulseRumbleAmount = 0;
  g_nPadSetMainModePending = 1;
  PadStartCom();
  PollPadAndDistributeInput();
  InstallVSyncCallback((unsigned int)PollPadAndDistributeInput);
}
