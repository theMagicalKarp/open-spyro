#include "globals.h"

extern int GetSoundVoiceStatusByOwner(unsigned char *owner,
                                      unsigned int sampleId);
extern void SubtractVector(int *dst, int *a, int *b);
extern void RShiftVector3(int *vec, unsigned int bits);
extern unsigned int VectorLength(int *vec, int includeZ);
extern unsigned int GetRandomU32(void);
extern int ArcTan2(int y, int x, int highPrecision);
extern unsigned int AbsAngleDelta8(int a, int b);
extern void ComputeVoicePanAndFalloff(short *vol, int dist,
                                      unsigned char bearing, unsigned int range,
                                      short *cached);
extern void ApplySpuVoiceAttr(unsigned int *attr);

/* One of the 24 software mixer rows at 0x80075F30 (stride 0x1C). */
typedef struct {
  /* 0x00 */ unsigned char *pOwner;
  /* 0x04 */ volatile short nVolL;
  /* 0x06 */ volatile short nVolR;
  /* 0x08 */ int nPitch;
  /* 0x0C */ unsigned char nPriority;
  /* 0x0D */ unsigned char nSampleId;
  /* 0x0E */ unsigned short nFlags;
  /* 0x10 */ volatile short anPanCache[2];
  /* 0x14 */ int nPitchStep;
  /* 0x18 */ unsigned char *pPlaying;
} SPUVOICEROW;

/* PSY-Q SpuVoiceAttr (0x40) — only voice/mask/volume/pitch/addr are written. */
typedef struct {
  /* 0x00 */ unsigned int voice;
  /* 0x04 */ unsigned int mask;
  /* 0x08 */ short volL;
  /* 0x0A */ short volR;
  /* 0x0C */ short volmodeL;
  /* 0x0E */ short volmodeR;
  /* 0x10 */ short volxL;
  /* 0x12 */ short volxR;
  /* 0x14 */ unsigned short pitch;
  /* 0x16 */ unsigned short note;
  /* 0x18 */ unsigned short sample_note;
  /* 0x1A */ short envx;
  /* 0x1C */ unsigned int addr;
  /* 0x20 */ unsigned int loop_addr;
  /* 0x24 */ int a_mode;
  /* 0x28 */ int s_mode;
  /* 0x2C */ int r_mode;
  /* 0x30 */ unsigned short ar;
  /* 0x32 */ unsigned short dr;
  /* 0x34 */ unsigned short sr;
  /* 0x36 */ unsigned short rr;
  /* 0x38 */ unsigned short sl;
  /* 0x3A */ unsigned short adsr1;
  /* 0x3C */ unsigned short adsr2;
} SPUVOICEATTR;

/* One entry of the per-level sample bank (stride 0x14). */
typedef struct {
  /* 0x00 */ unsigned int nSampleAddr;
  /* 0x04 */ unsigned int nLoopAddr;
  /* 0x08 */ unsigned short unk08;
  /* 0x0A */ unsigned short nPitch;
  /* 0x0C */ unsigned short nPitchSpread;
  /* 0x0E */ unsigned short nPitchStep;
  /* 0x10 */ int nPitchMode;
} SAMPLEENTRY;

extern SPUVOICEROW D_80075F30[];        /* the 24 mixer rows */
extern short g_anCameraEulerYawBlock[]; /* g_nCameraEulerYaw, held-base view */
extern int D_800761DC[]; /* one-shot override flags: 1 = vol pair, 2 = pitch */
extern unsigned short D_800761F4; /* override pitch */

/* The one-shot override pair and the row volume fields it feeds are all
   volatile: the mixer reads the row back on the next tick, so every read and
   write in the override arms below has to keep its own load and its program
   order slot (see nVolL/nVolR/anPanCache above). */
extern volatile unsigned short D_800761E8; /* one-shot override left volume */
extern volatile unsigned short D_800761EA; /* one-shot override right volume */

#define ENT ((SAMPLEENTRY *)g_pLevelSampleBankEntries + sampleId)

/* Start sample `sampleId` on a free software mixer row, owned by entity
   `owner`. `mode` is the row's flag group: 4 = flat volume, 8 = 3D positional,
   0x10 = caller-supplied pan. Modes 4 and 8 refuse to double-trigger a sample
   the owner is already playing, and mode 8 additionally drops the sound when
   the camera is outside the owner's audible range (owner+0x55 << 10, default
   0x4000) — reporting 0x7F ("finished") through `pPlaying` so the caller stops
   waiting on it.

   The chosen row is stamped with owner/sample/flags and a pitch that may be
   randomized around the bank entry's nominal value (nPitchMode 0 = centred
   spread, 1 = up only, 2 = down only, each in nPitchStep units). The volume
   pair comes from the row's flag group — the one-shot overrides at D_800761E8
   win once when armed — and is scaled by g_nSoundMasterVolume and pushed to the
   SPU via ApplySpuVoiceAttr, then the voice is queued for key-on.

   `pPlaying` receives the row index (or 0x7F when the sound was dropped); rows
   whose bank entry has a loop address are marked 0x100 so the mixer knows they
   will not end on their own. (0x80055A78, 1928 bytes.) */
unsigned int PlaySoundEffect(unsigned int sampleId, unsigned char *owner,
                             unsigned int mode, unsigned char *pPlaying) {
  SPUVOICEATTR attr;
  int delta[3];
  int voice;
  int quietest;
  int i;
  int dist;
  int range;
  short *cam;

  cam = g_anCameraEulerYawBlock;
  voice = -1;
  switch (mode) {
  case 4:
    if (GetSoundVoiceStatusByOwner(owner, sampleId) == 2) {
      return 0;
    }
    break;
  case 8:
    if (GetSoundVoiceStatusByOwner(owner, sampleId) == 2) {
      return 0;
    }
    SubtractVector(delta, (int *)(owner + 0xC), (int *)(cam - 0x14));
    RShiftVector3(delta, 4);
    dist = VectorLength(delta, 1) * 16;
    range = owner[0x55] << 10;
    if (range == 0) {
      range = 0x4000;
    }
    if (range < dist) {
      if (pPlaying != 0) {
        *pPlaying = 0x7F;
      }
      return 0x7F;
    }
    break;
  case 0x10:
    break;
  }

  quietest = 0x100;
  for (i = 0; i < 0x18; i++) {
    if ((D_80075F30[i].nFlags & 0xC1) == 0) {
      voice = i;
      break;
    }
    if (D_80075F30[i].nPriority < D_80075F30[quietest].nPriority) {
      quietest = i;
    }
  }

  if (voice >= 0) {
    if (voice != 0x18) {
      D_80075F30[voice].nFlags = mode | 1;
      D_80075F30[voice].pOwner = owner;
      D_80075F30[voice].nSampleId = sampleId;
      {
        int *ovr = D_800761DC;
        int pending = ovr[0];

        if (pending & 2) {
          ovr[0] = pending & ~2;
          D_80075F30[voice].nPitch = D_800761F4;
        } else {
          D_80075F30[voice].nPitch = ENT->nPitch;
        }
      }
      attr.mask = 0x10093;
      attr.voice = 1 << voice;
      attr.addr = ENT->nSampleAddr;
      attr.pitch = ENT->nPitch;
      if (ENT->nPitchSpread != 0) {
        switch (ENT->nPitchMode) {
        case 0:
          attr.pitch = attr.pitch - ENT->nPitchStep * (ENT->nPitchSpread >> 1);
          attr.pitch = attr.pitch + ((int)GetRandomU32() % ENT->nPitchSpread) *
                                        ENT->nPitchStep;
          break;
        case 1:
          attr.pitch = attr.pitch + ((int)GetRandomU32() % ENT->nPitchSpread) *
                                        ENT->nPitchStep;
          break;
        case 2:
          attr.pitch = attr.pitch - ((int)GetRandomU32() % ENT->nPitchSpread) *
                                        ENT->nPitchStep;
          break;
        }
      }
      attr.loop_addr = ENT->nLoopAddr;
      switch (D_80075F30[i].nFlags & 0x1C) {
      case 4:
      case 0x10: {
        int *ovr = D_800761DC;
        int pending = ovr[0];

        if (pending & 1) {
          ovr[0] = pending & ~1;
          D_80075F30[voice].anPanCache[0] = D_800761E8;
          D_80075F30[voice].anPanCache[1] = D_800761E8;
          attr.volL = D_800761E8;
          attr.volR = D_800761EA;
          D_80075F30[i].nVolR = attr.volR;
          D_80075F30[i].nVolL = D_800761E8;
        } else {
          D_80075F30[voice].anPanCache[0] = 0x2000;
          D_80075F30[voice].anPanCache[1] = 0x2000;
          attr.volR = 0x2000;
          attr.volL = 0x2000;
          D_80075F30[i].nVolR = 0x2000;
          D_80075F30[i].nVolL = 0x2000;
        }
        break;
      }
      case 8: {
        int *ovr = D_800761DC;
        int pending = ovr[0];

        if (pending & 1) {
          ovr[0] = pending & ~1;
          D_80075F30[voice].anPanCache[0] = D_800761E8;
          D_80075F30[voice].anPanCache[1] = D_800761EA;
          attr.volR = D_800761EA;
          attr.volL = D_800761E8;
        } else {
          D_80075F30[voice].anPanCache[0] = 0x3CCC;
          D_80075F30[voice].anPanCache[1] = 0x3CCC;
          attr.volR = 0x3CCC;
          attr.volL = 0x3CCC;
        }
        ComputeVoicePanAndFalloff(
            &attr.volL, dist,
            AbsAngleDelta8(
                g_nCameraEulerYaw >> 4,
                ArcTan2(g_anCameraPos[0] -
                            *(int *)(D_80075F30[voice].pOwner + 0xC),
                        g_anCameraPos[1] -
                            *(int *)(D_80075F30[voice].pOwner + 0x10),
                        0) &
                    0xFF) &
                0xFF,
            range, &D_80075F30[voice].anPanCache[0]);
        break;
      }
      }
      attr.volL = (attr.volL * g_nSoundMasterVolume) >> 12;
      attr.volR = (attr.volR * g_nSoundMasterVolume) >> 12;
      ApplySpuVoiceAttr(&attr.voice);
      g_dwSpuPendingKeyOn |= 1 << voice;
    }
    D_80075F30[voice].pPlaying = pPlaying;
    if (pPlaying != 0) {
      *pPlaying = voice;
      if (ENT->nLoopAddr != 0) {
        D_80075F30[voice].nFlags |= 0x100;
      }
    }
  }
  return 0;
}
