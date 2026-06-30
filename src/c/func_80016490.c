#include "globals.h"

extern int CdControl(int com, unsigned char *param, unsigned char *result);
extern int CdRead(int count, void *buf, int mode);

/* CD read-completion callback (registered via CdReadCallback). On CdlComplete
   (intr == 2) the read finished cleanly, so clear the in-flight flag and the
   stall-watchdog state; on any other interrupt the read stalled, so re-seek
   (CdControl) and re-issue the original CdRead. (0x80016490, 112 bytes.) */
void func_80016490(unsigned char intr) {
  if (intr == 2) {
    g_nCdReadInFlight = 0;
    g_nCdStallWatchdogTicks = 0;
    g_nCdStallMarker = 0;
  } else {
    CdControl(2, &g_abCdlLocScratch, 0);
    CdRead(g_nCdReadSectorCount, g_pCdReadDstBuffer, 0x80);
  }
}
