#include "globals.h"

extern int CdInit(void);
extern void CdReadCallback(void *callback);
extern void func_80016490();
extern int FUN_80063bd8();
extern void CdRead(int count, void *dst, unsigned int mode);

/* 0x800163e4: watchdog-driven CD stall recovery. If a stall marker is armed
   and has exceeded the watchdog tick threshold, resets the CD subsystem,
   reinstalls the read callback, resets the music-stream state, busy-waits
   for the drive to reach state 2, then reissues the pending sector read. */
void RestartStalledCdRead(void) {
  int marker;

  marker = g_nCdStallMarker;
  if (marker != 0) {
    if (marker < g_nCdStallWatchdogTicks) {
      CdInit();
      CdReadCallback(func_80016490);
      g_dwMusicStreamStatus = 0x40;
      g_nLastCdMusicCommand = 0;
      g_nPendingMusicCommand = 0;

      do {
      } while (FUN_80063bd8(1, 0) != 2);

      g_nCdStallWatchdogTicks = 0;
      CdRead(g_nCdReadSectorCount, g_pCdReadDstBuffer, 0x80);
    }
  }
}
