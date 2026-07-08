#include "globals.h"

extern void StopAllSoundExceptMask(unsigned int mask);
extern void CdReadSyncSectors(int lba, void *dst, int size, int offset,
                              int stall);
extern void BuildLoadedLevelRuntime();
extern void RestartSpyroAnimWithState();

/* 0x800144c8: loads + starts a level from CD. Silences sound, resolves the
   level's archive LBA (base + per-level offset table, stride 0x10), syncs
   it in, builds the runtime level, restarts Spyro's anim state (flight
   levels pass an extra 0x20 flag), then marks the level ready. */
void LoadAndStartLevelFromCd(void) {
  int pad[8];
  int level;
  int base;
  int archiveBase;
  void *dst;
  int size;
  int offset;

  (void)pad;

  StopAllSoundExceptMask(0);
  level = *(volatile int *)&g_nLevelIntroIndex;
  base = *(volatile int *)&g_nCdBaseLba;
  archiveBase = *(volatile int *)&g_nLevelArchiveBaseLba;
  dst = *(void *volatile *)&g_pPathTableBuffer;
  size = *(volatile int *)&g_nLevelArchiveByteSize;
  offset = *(int *)((char *)&g_nLevelArchiveOffset + level * 0x10);
  CdReadSyncSectors(base, dst, size, archiveBase + offset, 0x258);
  BuildLoadedLevelRuntime(1);

  if (g_nFlightLevelActive) {
    RestartSpyroAnimWithState(0x20);
  } else {
    RestartSpyroAnimWithState(0);
  }

  g_nLevelReadyFlag = 3;
}
