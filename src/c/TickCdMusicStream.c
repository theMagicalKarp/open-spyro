#include "cd_music.h"
#include "funcs.h"
#include "globals.h"
#include "libcd.h"
#include "main_shared.h"
#include "match_asm.h"
#include "vecmath.h"

/* The CdlLOC bytes at 0x800776c1 reached from the music-stream block's
   held base, as the original addresses it. */
#define CD_MUSIC_POS ((unsigned char *)g_adwMusicStreamBlock + 0x20D)

/* held-base view of g_nMusicVolumeFadeTarget (0x800761d8): [0] is the target,
   [0x16] (+0x58) is g_nMusicVolumeFadeStep, and the SPU master-volume short
   pair sits at -0x2C0. */
/* held-base view of g_dwMusicStreamStatus (0x800774b4); +0x20D reaches the
   last CdlLOC the drive reported. */

/* Levers (all load-bearing):
     - frame 0x38 -> 0x30: A231. The CdSync==5 arm's two LBA-table reads
       straddle CdIntToPos/CdControl; with one base symbol cse makes them one
       pseudo live across the calls and reload buys it an unreferenced 8-byte
       slot. The post-call read uses g_anMusicTrackLbaTablePost.
     - old RESIDUE 1 (the `li 0x1B; sw g_nLastCdMusicCommand` triple filling
       the track-load delay slot): read the track into its own local `trk`
       AFTER CdControl, then store the command through a one-member struct
       view (A230: an in-struct store keeps the in-struct array read of the
       table below it).
   Earlier levers still load-bearing: the end-LBA half has its own symbol
   (g_anMusicTrackEndLbaTable); `if (g_nVblankTickCount >
   g_nCdMusicStreamTimeout)` (A120 load order); the fade tail's held base +
   empty do/while(0) (A200) and its per-arm "store the new volume" blocks
   (A201).
   - the random-track table base is a block-local pointer (`base`): cse keeps
     the cheaper register, so the `la` is emitted with the index load and
     fills its load-delay bubble, as in the original. */

/* 0x8002bbe0 (1024 bytes) — per-frame CD-DA music stream tick plus the volume
   fade ramp.  Polls the in-flight CD command, retires it (or dispatches a
   queued one), keeps the current track streaming — advancing to a random track
   from the world's triple once the read head passes the track end, or
   re-syncing the read head from the drive while still inside it — and finally
   steps the SPU master volume one frame toward its fade target. */
void TickCdMusicStream(void) {
  char pos[8];
  char filt[8];
  int sync;
  int cmd;
  int target;
  int vol;
  int step;
  int r;
  int trk;
  int pick;
  int *tbl;

  sync = CallCdSync(1, g_abCdSyncResult);
  cmd = g_nLastCdMusicCommand;
  if (cmd != 0) {
    switch ((unsigned int)cmd) {
    case CdlStop:
    case CdlPause:
      if ((g_dwMusicStreamStatus & MUSIC_ST_FADING) == 0 && sync == 2) {
        g_dwMusicStreamStatus = MUSIC_ST_IDLE;
        g_nLastCdMusicCommand = 0;
      }
      break;
    case CdlReadS:
      g_dwMusicStreamStatus = MUSIC_ST_PLAYING;
      g_nLastCdMusicCommand = 0;
      break;
    }
  } else if (g_nPendingMusicCommand != 0) {
    HandleMusicCommand(g_nCurrentMusicTrack, g_nPendingMusicCommand);
    g_nPendingMusicCommand = 0;
  }

  if (g_adwMusicStreamBlock[0] & MUSIC_ST_PLAYING) {
    if (sync != 2) {
      if (sync == 5) {
        CdIntToPos(g_anMusicTrackLbaTable[g_nCurrentMusicTrack * 2], pos);
        CdControl(CdlReadS, pos, 0);
        trk = g_nCurrentMusicTrack;
        ((struct { int v; } *)&g_nLastCdMusicCommand)->v = CdlReadS;
        g_dwCdMusicReadHead = g_anMusicTrackLbaTablePost[trk * 2];
      }
    } else {
      if ((unsigned int)g_anMusicTrackEndLbaTable[g_nCurrentMusicTrack * 2] <
          g_dwCdMusicReadHead) {
        if (g_nVblankTickCount > g_nCdMusicStreamTimeout) {
          pick = (int)GetRandomU32();
          {
            int *base = g_anLevelMusicPickTable;
            tbl = &base[g_nLevelIndex * 3];
          }
          g_nCdMusicStreamTimeout = g_nVblankTickCount + 0x7080;
          g_nCurrentMusicTrack = (&g_nLevelMusicTrack)[tbl[pick % 3]];
        }
        g_dwCdMusicReadHead = g_anMusicTrackLbaTable[g_nCurrentMusicTrack * 2];
        CdIntToPos(g_dwCdMusicReadHead, pos);
        filt[0] = 1;
        filt[1] = g_nCurrentMusicTrack & 7;
        CdControlBlocking(CdlSetfilter, filt, 0);
        CdControl(CdlReadS, pos, 0);
      } else {
        if (GetCdStatusByte() == 0x11) {
          r = CdPosToInt(CD_MUSIC_POS);
          if (r > 0) {
            g_dwCdMusicReadHead = r;
          }
        }
        CdControlForce(CdlGetlocP, 0);
      }
    }
  }

  target = g_anMusicFadeBlock[0];
  if (target >= 0) {
    EBB_BREAK();
    step = g_anMusicFadeBlock[0x16];
    vol = *(short *)&g_anMusicFadeBlock[-0xB0];
    vol = vol + step;
    if (step < 0) {
      if (target < vol) {
        *(short *)&g_anMusicFadeBlock[-0xB0] = vol;
        goto store_right;
      }
      if (target == 0) {
        switch ((unsigned int)g_nLastCdMusicCommand) {
        case CdlStop:
        case CdlPause:
          CdControl(g_nLastCdMusicCommand, 0, 0);
          g_dwMusicStreamStatus = MUSIC_ST_PAUSED;
          break;
        case CdlReadS:
          g_dwMusicStreamStatus = MUSIC_ST_PLAYING;
          break;
        }
      }
      vol = g_nMusicVolumeFadeTarget;
      g_nMusicVolumeFadeStep = 0;
      g_nMusicVolumeFadeTarget = -1;
      g_nCdDaVolumeCurrent = vol;
      goto store_right;
    }
    if (vol < target) {
      *(short *)&g_anMusicFadeBlock[-0xB0] = vol;
      goto store_right;
    }
    g_anMusicFadeBlock[0x16] = 0;
    *(short *)&g_anMusicFadeBlock[-0xB0] = target;
    g_nSpuCommonCdVolumeR = target;
    g_anMusicFadeBlock[0] = -1;
    goto spu;
  store_right:
    g_nSpuCommonCdVolumeR = vol;
  spu:
    *(unsigned int *)g_abSpuCommonAttr = 0xC0;
    SetSpuCommonAttr((unsigned int *)g_abSpuCommonAttr);
  } else {
    if (g_adwMusicStreamBlock[0] & MUSIC_ST_FADING) {
      g_adwMusicStreamBlock[0] = MUSIC_ST_PAUSED;
    }
  }
}
