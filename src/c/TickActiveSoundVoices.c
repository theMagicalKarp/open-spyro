#include "globals.h"

/* Matching notes (0x8005637C, 1144 B — the levers, so the shape is not lost):
   - The yaw read is an INLINE volatile cast-deref of the block symbol so the
     address stays in a register (`lui/addiu` + `lhu 0(reg)`, B16/A142); a plain
     pointer local is folded back to `lui;lw %lo` by cse.
   - That address must NOT be a single-set pseudo: sched.c's `birthing_insn_p`
     boosts a single-set def to LAUNCH_PRIORITY and drags it next to its
     consumer, nine slots late (F14). `yc` is the F14b DONOR — it also carries
     the flat 0x2000 volume pair in the `case 4` arm, so `reg_n_sets == 2`, the
     boost is gone, and its second range is short enough not to conflict with
     v0 (every other candidate carrier in this function does: playing/owner/
     status/mode -> v1, vol -> a1).
   - Position is then LUID order, and loop.c's hoists always outrank pre-loop
     statements, so the `1` of `1 << i` has to be demoted to an explicit
     pre-loop local (`bit`, A93) with the `for` init split out (`i = 0;`) for
     `move s5,zero; move s3,zero; li s7,1; lui v0` to come out in that order.
   - The out-of-range halving pair stores through `far`, a view of the row
     0x28 BELOW the array so cse derives the shared base off the live row base
     (`addiu a0,s8,-44`) — spelling it as the neighbouring g_abSpuCommonAttr
     symbol costs a whole `lui/addiu`, and a byte-offset form folds the field
     offset into the base and buys a second pointer giv (A204). */

/* Per-frame SPU voice mixer tick (0x8005637C, 0x478).

   Walks the 24 software voice rows (stride 0x1C). A row flagged 0x40 is the
   "just released" marker and is simply cleared. An active row (bit 0x1) is
   polled against the hardware key status: once the voice has been keyed on
   (status 1/3) the row latches bit 0x2, and a latched row that reports 3 again
   has finished playing, so the row is released — the owner's playing byte is
   stamped 0x7F, the row is blanked and the voice bit is queued into
   g_dwSpuPendingKeyOff.

   The row's 0x1C flag group selects how the frame's volume pair is produced:
   4 = flat 0x2000, 0x10 = hold the row's cached pair, 8 = 3D positional. The 3D
   case reads back the hardware volumes, then compares the camera distance to
   the owner's range (owner+0x55 << 10, default 0x4000): inside the range
   ComputeVoicePanAndFalloff pans by the owner's bearing relative to the camera
   yaw; outside it the pair is halved every frame (and pushed straight to the
   hardware) until both sides fall under 0x40, at which point the row is
   released.

   Every path that produces a pair scales it by g_nSoundMasterVolume (>>12),
   folds the row's pitch step into its running pitch and pushes voice + volume
   + pitch through ApplySpuVoiceAttr. */
typedef struct {
  /* 0x00 */ unsigned char *pOwner;
  /* 0x04 */ short nVolL;
  /* 0x06 */ short nVolR;
  /* 0x08 */ int nPitch;
  /* 0x0C */ unsigned char unk0C;
  /* 0x0D */ unsigned char nSampleId;
  /* 0x0E */ unsigned short nFlags;
  /* 0x10 */ short anPanCache[2];
  /* 0x14 */ int nPitchStep;
  /* 0x18 */ char *pPlaying;
} SPUVOICEROW;

/* PSY-Q SpuVoiceAttr (0x40) — only voice/mask/volume/pitch are written here. */
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

extern int GetSpuVoiceKeyStatus(unsigned int voiceBit);
extern void SpuGetVoiceVolume(int vNum, short *volL, short *volR);
extern void SetSpuVoiceVolumeLR(int vNum, short volL, short volR);
extern void ApplySpuVoiceAttr(unsigned int *attr);
extern void ComputeVoicePanAndFalloff(short *vol, int dist,
                                      unsigned char bearing, unsigned int range,
                                      short *cached);
extern void SubtractVector(int *dst, int *a, int *b);
extern unsigned int VectorLength(int *vec, int includeZ);
extern int ArcTan2(int y, int x, int highPrecision);

typedef struct {
  /* 0x00 */ char pad[0x2C];
  /* 0x2C */ short nVolL;
  /* 0x2E */ short nVolR;
} SPUFARROW;

extern SPUVOICEROW D_80075F30[];                  /* the 24 mixer rows */
extern unsigned short g_anCameraEulerYawBlock[];  /* camera euler yaw */
extern int g_anSoundMasterVolumeBlock[];          /* g_nSoundMasterVolume */
extern unsigned int g_adwSpuPendingKeyOffBlock[]; /* g_dwSpuPendingKeyOff */

int TickActiveSoundVoices(void) {
  SPUVOICEATTR attr;
  int vec[4];
  unsigned char yaw;
  int i;
  int keyOff;
  int status;
  int mode;
  int range;
  int dist;
  int vol;
  short *pVol;
  unsigned char *owner;
  char *playing;
  int yc;
  unsigned int bit;
  SPUFARROW *far;
  keyOff = 0;
  i = 0;
  bit = 1;
  yc = (int)g_anCameraEulerYawBlock;
  yaw = (unsigned char)((*((volatile unsigned short *)yc)) >> 4);
  for (; i < 24; i++) {
    if (D_80075F30[i].nFlags & 0x40) {
      D_80075F30[i].nFlags = 0;
      continue;
    }
    if (!(D_80075F30[i].nFlags & 1)) {
      continue;
    }
    status = GetSpuVoiceKeyStatus(bit << i);
    if (D_80075F30[i].nFlags & 2) {
      if ((status & 0xFF) == 3) {
        playing = D_80075F30[i].pPlaying;
        keyOff |= bit << i;
        if (playing != 0) {
          *playing = 0x7F;
        }
        D_80075F30[i].nFlags = 0x40;
        D_80075F30[i].pPlaying = 0;
        D_80075F30[i].pOwner = 0;
        D_80075F30[i].nSampleId = 0xFF;
        continue;
      }
    } else if (((status & 0xFF) == bit) || ((status & 0xFF) == 3)) {
      D_80075F30[i].nFlags |= 2;
    }
    if (!(D_80075F30[i].nFlags & 1)) {
      continue;
    }
    mode = D_80075F30[i].nFlags & 0x1C;
    switch (mode) {
    case 4:
      yc = 0x2000;
      attr.volR = yc;
      attr.volL = yc;
      break;

    case 8:
      pVol = &D_80075F30[i].nVolL;
      SpuGetVoiceVolume(i, pVol, pVol + 1);
      owner = D_80075F30[i].pOwner;
      range = owner[0x55] << 10;
      if (range == 0) {
        range = 0x4000;
      }
      SubtractVector(vec, (int *)(owner + 0xC), g_anCameraPos);
      dist = VectorLength(vec, 1);
      if (dist >= range) {
        far = (SPUFARROW *)((char *)D_80075F30 - 0x28 + i * 0x1C);
        attr.volR = (far->nVolR = D_80075F30[i].nVolR >> 1);
        vol = D_80075F30[i].nVolL >> 1;
        attr.volL = (far->nVolL = vol);
        if ((attr.volR < 0x40) && (vol < 0x40)) {
          playing = D_80075F30[i].pPlaying;
          keyOff |= bit << i;
          if (playing != 0) {
            *playing = 0x7F;
          }
          D_80075F30[i].nFlags = 0x40;
          D_80075F30[i].pPlaying = 0;
          D_80075F30[i].pOwner = 0;
          D_80075F30[i].nSampleId = 0xFF;
        } else {
          D_80075F30[i].nVolL =
              (D_80075F30[i].nVolL *
               (*((volatile int *)((char *)g_anSoundMasterVolumeBlock)))) >>
              12;
          D_80075F30[i].nVolR =
              (D_80075F30[i].nVolR *
               (*((volatile int *)((char *)g_anSoundMasterVolumeBlock)))) >>
              12;
          SetSpuVoiceVolumeLR(i, D_80075F30[i].nVolL, D_80075F30[i].nVolR);
        }
        continue;
      };
      ComputeVoicePanAndFalloff(
          &attr.volL, dist,
          (unsigned char)(ArcTan2(((int *)(D_80075F30[i].pOwner + 0xC))[0] -
                                      g_anCameraPos[0],
                                  ((int *)(D_80075F30[i].pOwner + 0xC))[1] -
                                      g_anCameraPos[1],
                                  0) -
                          yaw),
          range, D_80075F30[i].anPanCache);
      break;

    case 0x10:
      attr.volR = D_80075F30[i].nVolR;
      attr.volL = D_80075F30[i].nVolL;
      break;
    }

    attr.mask = 3;
    attr.voice = bit << i;
    attr.volL = (attr.volL * g_nSoundMasterVolume) >> 12;
    attr.volR = (attr.volR * g_nSoundMasterVolume) >> 12;
    D_80075F30[i].nPitch += D_80075F30[i].nPitchStep;
    D_80075F30[i].nPitchStep = 0;
    attr.mask |= 0x10;
    attr.pitch = D_80075F30[i].nPitch;
    ApplySpuVoiceAttr(&attr.voice);
  }

  g_adwSpuPendingKeyOffBlock[0] |= keyOff;
  return 0;
}
