#include "globals.h"

extern int CdControl();
extern unsigned char *CdIntToPos();
extern int CdRead();
extern int FUN_80063bd8();
extern void RestartStalledCdRead(void);
extern void TickActiveSoundVoices(void);
extern void TickCdMusicStream(void);

extern unsigned char g_abCdlLocScratchBlock[36];

/* 0x80016500 — blocking CD sector read, StartCdReadAsync's synchronous twin.
   Spins until the drive is idle (no read in flight, CdSync == 2, music
   stream table built) while servicing the stall watchdog, sound voices and
   the music stream; then sets 2x speed (CdlSetmode 0x80), seeks to
   lbaBase + offset/0x800, kicks CdRead of (size+0x7ff)/0x800 sectors to dst
   with the in-flight bookkeeping raised, and spins on the CdlLOC scratch
   completion word + CdSync until the read drains. */
void CdReadSyncSectors(int lbaBase, void *dst, int size, int offset,
                       int marker) {
  unsigned char mode[4];
  byte *loc;
  int pos;
  int sectors;
  int *flag;
  int *inflight;
  int idle;

  inflight = &g_nCdReadInFlight;
  idle = 2;
  mode[0] = 0x80;
  for (;;) {
    if (*inflight == 0 && FUN_80063bd8(1, 0) == idle &&
        (g_dwMusicStreamStatus & 0x40)) {
      break;
    }
    RestartStalledCdRead();
    TickActiveSoundVoices();
    TickCdMusicStream();
  }

  CdControl(0xE, mode, 0);
  g_nCdStallMarker = marker;
  g_nCdStallWatchdogTicks = 0;

  pos = lbaBase + offset / 0x800;
  loc = g_abCdlLocScratchBlock;
  CdIntToPos(pos, loc);
  CdControl(2, loc, 0);

  sectors = (size + 0x7FF) / 0x800;
  g_nCdReadSectorCount = sectors;
  g_pCdReadDstBuffer = dst;
  g_nCdReadInFlight = 1;
  CdRead(sectors, dst, 0x80);

  flag = (int *)(loc + 0x20);
  for (;;) {
    if (*flag == 0 && FUN_80063bd8(1, 0) == 2) {
      break;
    }
    RestartStalledCdRead();
  }
}
