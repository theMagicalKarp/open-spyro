#include "globals.h"

/* CD-DA music command dispatcher (0x800567F4, 0x334). Called from
   TickCdMusicStream with the track index and one of four commands:
   1 = start track, 2/4 = stop (fade out), 8 = resume from the stored read
   head. Commands are ignored outright while the stream is busy (status bit
   0x80) and are re-queued in g_nPendingMusicCommand when the drive is not
   ready (status bit 0x40 clear) or the music volume is 0.

   The two play paths issue the same libcd triple — CdlSetmode 0xC8 (double
   speed, CD-DA, report), CdlSetfilter with file 1 / channel track%8, then
   CdlReadS at the track LBA (fresh track) or at the saved read head (resume).
   On a successful ReadS the fade ramps back up to D_80076224; a rejected
   command re-queues itself. */
extern int CdControlBlocking(int cmd, char *param, char *result);
extern char *CdIntToPos(int lba, char *loc);

extern short D_80075F18; /* live CD-DA volume (fade ramp current value) */
extern int D_80076224;   /* CD-DA volume fade ceiling */

extern unsigned int
    g_adwMusicStreamStatusBlock[]; /* alias of g_dwMusicStreamStatus */
extern unsigned int
    g_adwMusicStopStatusBlock[]; /* alias of g_dwMusicStreamStatus (stop arm) */
extern int g_anPendingMusicCommandBlock[]; /* alias of g_nPendingMusicCommand */

void HandleMusicCommand(int track, int cmd) {
  unsigned int *status;
  unsigned int *stopStatus;
  int *queued;
  unsigned int st;
  int pending;
  char mode[8];
  char loc[8];
  char filter[8];

  status = g_adwMusicStreamStatusBlock;
  st = *status;
  if ((st & 0x80) != 0) {
    return;
  }
  switch (cmd) {
  case 1:
    if ((st & 0x40) != 0 && g_nMusicVolume != 0) {
      mode[0] = 0xC8;
      CdControlBlocking(0xE, mode, 0);
      filter[0] = 1;
      filter[1] = track % 8;
      CdControlBlocking(0xD, filter, 0);
      CdIntToPos(g_anMusicTrackLbaTable[track * 2], loc);
      if (CdControlBlocking(0x1B, loc, 0) != 0) {
        g_nLastCdMusicCommand = 0;
        g_nPendingMusicCommand = 0;
        *status = 0x10;
        g_nMusicVolumeFadeTarget = D_80076224;
        g_nMusicVolumeFadeStep = (D_80076224 - D_80075F18) >> 3;
      } else {
        g_nPendingMusicCommand = cmd;
      }
    } else {
      queued = g_anPendingMusicCommandBlock;
      pending = *queued;
      if (pending != 0 || g_nLastCdMusicCommand != 0) {
        if (pending != 1) {
          *queued = 1;
        }
      }
    }
    break;
  case 2:
  case 4:
    stopStatus = g_adwMusicStopStatusBlock;
    if ((*stopStatus & 0x10) != 0) {
      if (D_80075F18 != 0) {
        g_nMusicVolumeFadeStep = -D_80075F18 >> 3;
        g_nLastCdMusicCommand = 9;
        g_nMusicVolumeFadeTarget = 0;
        *stopStatus = 0x200;
      } else {
        *stopStatus = 0x40;
      }
    } else {
      pending = g_nPendingMusicCommand;
      if (pending != 0 || g_nLastCdMusicCommand != 0) {
        if (pending != 4 && pending != 2) {
          g_nPendingMusicCommand = 2;
        }
      }
    }
    break;
  case 8:
    if ((st & 0x40) != 0 && g_nMusicVolume != 0) {
      mode[0] = 0xC8;
      CdControlBlocking(0xE, mode, 0);
      filter[0] = 1;
      filter[1] = track % 8;
      CdControlBlocking(0xD, filter, 0);
      CdIntToPos(g_dwCdMusicReadHead, loc);
      if (CdControlBlocking(0x1B, loc, 0) != 0) {
        g_nLastCdMusicCommand = 0;
        g_nPendingMusicCommand = 0;
        *status = 0x10;
        g_nMusicVolumeFadeTarget = D_80076224;
        g_nMusicVolumeFadeStep = (D_80076224 - D_80075F18) >> 3;
      } else {
        g_nPendingMusicCommand = cmd;
      }
    } else {
      queued = g_anPendingMusicCommandBlock;
      pending = *queued;
      if (pending != 0 || g_nLastCdMusicCommand != 0) {
        if (pending != 8) {
          *queued = 8;
        }
      }
    }
    break;
  }
}
