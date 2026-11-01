#include "cd_music.h"
#include "funcs.h"
#include "globals.h"
#include "libcd.h"
#include "main_shared.h"

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
      CdReadCallback(CdReadCompleteCallback);
      g_dwMusicStreamStatus = MUSIC_ST_IDLE;
      g_nLastCdMusicCommand = 0;
      g_nPendingMusicCommand = 0;

      do {
      } while (CallCdSync(1, 0) != 2);

      g_nCdStallWatchdogTicks = 0;
      CdRead(g_nCdReadSectorCount, g_pCdReadDstBuffer, CdlModeSpeed);
    }
  }
}
