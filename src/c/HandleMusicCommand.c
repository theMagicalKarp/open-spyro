#include "cd_music.h"
#include "funcs.h"
#include "globals.h"
#include "libcd.h"
#include "main_shared.h"

/* XA music command dispatcher (0x800567F4, 0x334). Called from
   TickCdMusicStream with the track index and one of four commands:
   MUSIC_CMD_PLAY starts a track, MUSIC_CMD_STOP(_4) fades out and pauses,
   MUSIC_CMD_RESUME restarts from the stored read head. Commands are ignored
   outright while the stream is busy (status bit 0x80) and are re-queued in
   g_nPendingMusicCommand when the drive is not ready (status bit 0x40 clear) or
   the music volume is 0.

   The two play paths issue the same libcd triple — CdlSetmode 0xC8 (double
   speed, XA audio, subheader filter), CdlSetfilter with file 1 / channel
   track%8, then CdlReadS at the track LBA (fresh track) or at the saved read
   head (resume). On a successful ReadS the fade ramps back up to
   g_nCdAudioVolume_blk; a rejected command re-queues itself. */

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
  if ((st & MUSIC_ST_LOCKED) != 0) {
    return;
  }
  switch (cmd) {
  case MUSIC_CMD_PLAY:
    if ((st & MUSIC_ST_IDLE) != 0 && g_nMusicVolume != 0) {
      mode[0] = CdlModeSpeed | CdlModeRT | CdlModeSF;
      CdControlBlocking(CdlSetmode, mode, 0);
      filter[0] = 1;
      filter[1] = track % 8;
      CdControlBlocking(CdlSetfilter, filter, 0);
      CdIntToPos(g_anMusicTrackLbaTable[track * 2], loc);
      if (CdControlBlocking(CdlReadS, loc, 0) != 0) {
        g_nLastCdMusicCommand = 0;
        g_nPendingMusicCommand = 0;
        *status = MUSIC_ST_PLAYING;
        g_nMusicVolumeFadeTarget = g_nCdAudioVolume_blk;
        g_nMusicVolumeFadeStep =
            (g_nCdAudioVolume_blk - g_nCdDaVolumeCurrent) >> 3;
      } else {
        g_nPendingMusicCommand = cmd;
      }
    } else {
      queued = g_anPendingMusicCommandBlock;
      pending = *queued;
      if (pending != 0 || g_nLastCdMusicCommand != 0) {
        if (pending != MUSIC_CMD_PLAY) {
          *queued = MUSIC_CMD_PLAY;
        }
      }
    }
    break;
  case MUSIC_CMD_STOP:
  case MUSIC_CMD_STOP_4:
    stopStatus = g_adwMusicStopStatusBlock;
    if ((*stopStatus & MUSIC_ST_PLAYING) != 0) {
      if (g_nCdDaVolumeCurrent != 0) {
        g_nMusicVolumeFadeStep = -g_nCdDaVolumeCurrent >> 3;
        g_nLastCdMusicCommand = CdlPause;
        g_nMusicVolumeFadeTarget = 0;
        *stopStatus = MUSIC_ST_FADING;
      } else {
        *stopStatus = MUSIC_ST_IDLE;
      }
    } else {
      pending = g_nPendingMusicCommand;
      if (pending != 0 || g_nLastCdMusicCommand != 0) {
        if (pending != MUSIC_CMD_STOP_4 && pending != MUSIC_CMD_STOP) {
          g_nPendingMusicCommand = MUSIC_CMD_STOP;
        }
      }
    }
    break;
  case MUSIC_CMD_RESUME:
    if ((st & MUSIC_ST_IDLE) != 0 && g_nMusicVolume != 0) {
      mode[0] = CdlModeSpeed | CdlModeRT | CdlModeSF;
      CdControlBlocking(CdlSetmode, mode, 0);
      filter[0] = 1;
      filter[1] = track % 8;
      CdControlBlocking(CdlSetfilter, filter, 0);
      CdIntToPos(g_dwCdMusicReadHead, loc);
      if (CdControlBlocking(CdlReadS, loc, 0) != 0) {
        g_nLastCdMusicCommand = 0;
        g_nPendingMusicCommand = 0;
        *status = MUSIC_ST_PLAYING;
        g_nMusicVolumeFadeTarget = g_nCdAudioVolume_blk;
        g_nMusicVolumeFadeStep =
            (g_nCdAudioVolume_blk - g_nCdDaVolumeCurrent) >> 3;
      } else {
        g_nPendingMusicCommand = cmd;
      }
    } else {
      queued = g_anPendingMusicCommandBlock;
      pending = *queued;
      if (pending != 0 || g_nLastCdMusicCommand != 0) {
        if (pending != MUSIC_CMD_RESUME) {
          *queued = MUSIC_CMD_RESUME;
        }
      }
    }
    break;
  }
}
