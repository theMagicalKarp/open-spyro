#include "globals.h"

extern int FUN_80063bd8(int mode, unsigned char *result);
extern int FUN_80063aac(void);
extern void HandleMusicCommand(int track, int cmd);
extern void CdIntToPos(int lba, char *pos);
extern int CdPosToInt(unsigned char *pos);
extern int CdControl(unsigned char com, char *param, char *result);
extern int CdControlBlocking(unsigned char com, char *param, char *result);
extern int CdControlForce(unsigned char com, char *param);
extern void SetSpuCommonAttr(unsigned int *attr);
extern unsigned int GetRandomU32(void);

extern unsigned char D_800776BC[];
extern unsigned int D_800776CC;
extern int D_8006F05C[];
extern short D_80075F18;
extern short D_80075F1A;

/* held-base view of g_nMusicVolumeFadeTarget (0x800761d8): [0] is the target,
   [0x16] (+0x58) is g_nMusicVolumeFadeStep, and the SPU master-volume short
   pair sits at -0x2C0. */
extern int g_anMusicTrackEndLbaTable[];
extern int g_anMusicFadeBlock[];
/* held-base view of g_dwMusicStreamStatus (0x800774b4); +0x20D reaches the
   last CdlLOC the drive reported. */
extern unsigned int g_adwMusicStreamBlock[];
extern int
    g_anMusicTrackLbaTablePost[]; /* g_anMusicTrackLbaTable, post-call view */

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
   (g_anMusicTrackEndLbaTable); `if (g_nVblankTickCount > D_800776CC)`
   (A120 load order); the fade tail's held base + empty do/while(0) (A200)
   and its per-arm "store the new volume" blocks (A201).
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

  sync = FUN_80063bd8(1, D_800776BC);
  cmd = g_nLastCdMusicCommand;
  if (cmd != 0) {
    switch ((unsigned int)cmd) {
    case 8:
    case 9:
      if ((g_dwMusicStreamStatus & 0x200) == 0 && sync == 2) {
        g_dwMusicStreamStatus = 0x40;
        g_nLastCdMusicCommand = 0;
      }
      break;
    case 0x1B:
      g_dwMusicStreamStatus = 0x10;
      g_nLastCdMusicCommand = 0;
      break;
    }
  } else if (g_nPendingMusicCommand != 0) {
    HandleMusicCommand(g_nCurrentMusicTrack, g_nPendingMusicCommand);
    g_nPendingMusicCommand = 0;
  }

  if (g_adwMusicStreamBlock[0] & 0x10) {
    if (sync != 2) {
      if (sync == 5) {
        CdIntToPos(g_anMusicTrackLbaTable[g_nCurrentMusicTrack * 2], pos);
        CdControl(0x1B, pos, 0);
        trk = g_nCurrentMusicTrack;
        ((struct { int v; } *)&g_nLastCdMusicCommand)->v = 0x1B;
        g_dwCdMusicReadHead = g_anMusicTrackLbaTablePost[trk * 2];
      }
    } else {
      if ((unsigned int)g_anMusicTrackEndLbaTable[g_nCurrentMusicTrack * 2] <
          g_dwCdMusicReadHead) {
        if (g_nVblankTickCount > D_800776CC) {
          pick = (int)GetRandomU32();
          {
            int *base = D_8006F05C;
            tbl = &base[g_nLevelIntroIndex * 3];
          }
          D_800776CC = g_nVblankTickCount + 0x7080;
          g_nCurrentMusicTrack = (&g_nLevelMusicTrack)[tbl[pick % 3]];
        }
        g_dwCdMusicReadHead = g_anMusicTrackLbaTable[g_nCurrentMusicTrack * 2];
        CdIntToPos(g_dwCdMusicReadHead, pos);
        filt[0] = 1;
        filt[1] = g_nCurrentMusicTrack & 7;
        CdControlBlocking(0xD, filt, 0);
        CdControl(0x1B, pos, 0);
      } else {
        if (FUN_80063aac() == 0x11) {
          r = CdPosToInt((unsigned char *)g_adwMusicStreamBlock + 0x20D);
          if (r > 0) {
            g_dwCdMusicReadHead = r;
          }
        }
        CdControlForce(0x11, 0);
      }
    }
  }

  target = g_anMusicFadeBlock[0];
  if (target >= 0) {
    do {
    } while (0);
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
        case 8:
        case 9:
          CdControl(g_nLastCdMusicCommand, 0, 0);
          g_dwMusicStreamStatus = 0x100;
          break;
        case 0x1B:
          g_dwMusicStreamStatus = 0x10;
          break;
        }
      }
      vol = g_nMusicVolumeFadeTarget;
      g_nMusicVolumeFadeStep = 0;
      g_nMusicVolumeFadeTarget = -1;
      D_80075F18 = vol;
      goto store_right;
    }
    if (vol < target) {
      *(short *)&g_anMusicFadeBlock[-0xB0] = vol;
      goto store_right;
    }
    g_anMusicFadeBlock[0x16] = 0;
    *(short *)&g_anMusicFadeBlock[-0xB0] = target;
    D_80075F1A = target;
    g_anMusicFadeBlock[0] = -1;
    goto spu;
  store_right:
    D_80075F1A = vol;
  spu:
    *(unsigned int *)g_abSpuCommonAttr = 0xC0;
    SetSpuCommonAttr((unsigned int *)g_abSpuCommonAttr);
  } else {
    if (g_adwMusicStreamBlock[0] & 0x200) {
      g_adwMusicStreamBlock[0] = 0x100;
    }
  }
}
