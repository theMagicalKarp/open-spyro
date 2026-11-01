#include "funcs.h"
#include "globals.h"
#include "libcd.h"

/* CD read-completion callback (registered via CdReadCallback). On CdlComplete
   (intr == 2) the read finished cleanly, so clear the in-flight flag and the
   stall-watchdog state; on any other interrupt the read stalled, so re-seek
   (CdControl) and re-issue the original CdRead. (0x80016490, 112 bytes.) */
void CdReadCompleteCallback(unsigned char intr) {
  if (intr == 2) {
    g_nCdReadInFlight = 0;
    g_nCdStallWatchdogTicks = 0;
    g_nCdStallMarker = 0;
  } else {
    CdControl(CdlSetloc, &g_abCdlLocScratch, 0);
    CdRead(g_nCdReadSectorCount, g_pCdReadDstBuffer, CdlModeSpeed);
  }
}
