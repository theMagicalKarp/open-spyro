#include "funcs.h"
#include "globals.h"
#include "libcd.h"

/* 0x80016698 — fire an async CD sector read without waiting: set 2x speed
   (CdlSetmode 0x80), seek to lbaBase + offset/0x800, then CdRead
   (size+0x7ff)/0x800 sectors to dst. Stashes the in-flight bookkeeping
   (sector count / dst / marker for RestartStalledCdRead) and raises the
   in-flight flag; callers poll it themselves. */
void StartCdReadAsync(int lbaBase, void *dst, int size, int offset,
                      int marker) {
  unsigned char mode[4];
  byte *loc;
  int pos;
  int sectors;

  mode[0] = CdlModeSpeed;
  CdControl(CdlSetmode, mode, 0);
  g_nCdStallWatchdogTicks = 0;
  g_nCdStallMarker = marker;

  pos = lbaBase + offset / 0x800;
  loc = &g_abCdlLocScratch;
  CdIntToPos(pos, loc);
  CdControl(CdlSetloc, loc, 0);

  sectors = (size + 0x7FF) / 0x800;
  g_nCdReadSectorCount = sectors;
  g_pCdReadDstBuffer = dst;
  g_nCdReadInFlight = 1;
  CdRead(sectors, dst, CdlModeSpeed);
}
