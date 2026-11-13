#include "actor_type.h"
#include "cd_music.h"
#include "funcs.h"
#include "globals.h"
#include "libcd.h"
#include "main_shared.h"
#include "spu.h"
#include "spyro_block.h"

/* 0x8005595c (0x11c) — top-level sound init (after SpuInit): reset the SPU,
   seed the common-attr block (master volume 0x3CCC L/R, CD volume 0x5000
   L/R) + the master/mix volume globals, push it, silence all 24 voices with
   one template SpuVoiceAttr (vol 0x2FFF, pitch 0x400, ADSR modes 1/1/3,
   sustain 0xF), clear the key mask, set DMA transfer mode, and zero the
   voice-state block at common+0x28. */
void InitSoundEngine(void) {
  int pad[2];
  SPUVOICEATTR a;
  unsigned char *common;
  int cd;

  InitSpu();
  common = g_abSpuCommonAttr;
  *(int *)common = SPU_COMMON_MVOL | SPU_COMMON_CDVOL;
  g_nSfxMixVolume_blk[0] = SPU_VOLUME_MAX;
  g_nSoundMasterVolume = 0x1000;
  g_anSpuCommonVolAttr[SPU_COMMON_MVOL_L] = SPU_MASTER_VOLUME;
  g_anSpuCommonVolAttr[SPU_COMMON_MVOL_R] = SPU_MASTER_VOLUME;
  cd = SPU_CD_VOLUME;
  g_nCdAudioVolume = cd;
  g_anSpuCommonVolAttr[SPU_COMMON_CDVOL_L] = cd;
  g_anSpuCommonVolAttr[SPU_COMMON_CDVOL_R] = cd;
  g_nMusicVolumeFadeTarget = -1;
  SetSpuCommonAttr(common);

  a.mask = SPU_ATTR_VOLL | SPU_ATTR_VOLR | SPU_ATTR_PITCH | SPU_ATTR_ADSR;
  a.volL = 0x2FFF;
  a.volR = 0x2FFF;
  a.pitch = 0x400;
  a.a_mode = 1;
  a.s_mode = 1;
  a.r_mode = 3;
  a.voice = SPU_VOICE_ALL;
  a.ar = 0;
  a.dr = 0;
  a.sr = 0;
  a.rr = 0;
  a.sl = 0xF;
  ApplySpuVoiceAttr((int *)&a);

  SetSpuKeyMask(0, SPU_VOICE_ALL);
  SetSpuTransferMode(0);
  FillWord(common + 0x28, 0, 0x2A0);
  g_nSoundMonoMixFlag = 0;
}

/* Queue an SPU key-off for voice `index` (0x80056200): set the pending key-off
   bit, and when `mode` is 2 or 4 also tear down the voice's common-attr shadow
   record (voice*28 stride): free the sample-callback slot, reset the ADSR/mode
   fields, and mark it stopped. */

void StopSoundVoiceByIndex(int index, int mode) {
  unsigned int voice = index & 0x7F;
  int off;
  char *cb;

  if (voice < SPU_VOICE_COUNT) {
    unsigned int *keyoff = g_anSpuPendingKeyOffBlock;
    *keyoff |= 1 << voice;
    if (mode == 2 || mode == 4) {
      off = voice * 28;
      cb = VOICE_ROW_RAW(off, char *, pPlaying);
      if (cb != 0) {
        *cb = VOICE_ROW_NONE;
      }
      VOICE_ROW_RAW(off, short, nFlags) = VOICE_ROW_STOPPED;
      VOICE_ROW_RAW(off, int, pPlaying) = 0;
      VOICE_ROW_RAW(off, int, pOwner) = 0;
      VOICE_ROW_RAW(off, char, nSampleId) = 0xFF;
    }
  }
}

extern SPUVOICEROW g_aSpuVoiceRows[]; /* the 24 mixer rows */

/* Stop every software voice row owned by `owner` (0x800562a4). mode 1
   (VOICE_STOP_ALL) stops unconditionally; mode 2 (VOICE_STOP_LOOPING) only
   stops rows whose nFlags has VOICE_ROW_LOOPING. A stopped row silences its
   live sample byte (0x7F), clears pPlaying and pOwner, parks nFlags at
   VOICE_ROW_STOPPED, zeros nPitchStep, resets nSampleId to 0xFF, and
   accumulates the voice bit into the pending hardware key-off mask. 24 rows of
   g_aSpuVoiceRows, walked as three field pointers. */

void StopSoundVoicesByOwner(Actor *owner, int mode) {
  unsigned int kmask = 0;
  int i = 0;
  int one = 1;
  int two = 2;
  int silence = VOICE_ROW_NONE;
  int idle = VOICE_ROW_STOPPED;
  int noSample = 0xFF;
  int *ownerp = (int *)&g_aSpuVoiceRows[0].pOwner;
  unsigned short *flagsp = &g_aSpuVoiceRows[0].nFlags;
  unsigned char **playingp = (unsigned char **)&g_aSpuVoiceRows[0].pPlaying;
  int off = 0;
top:
  if (*ownerp == (int)owner) {
    if (mode == one || (mode == two && (*flagsp & VOICE_ROW_LOOPING) != 0)) {
      kmask |= one << i;
      if (*playingp != 0) {
        **playingp = silence;
      }
      *playingp = 0;
      *flagsp = idle;
      *ownerp = 0;
      VOICE_ROW_RAW(off, int, nPitchStep) = 0;
      VOICE_ROW_RAW(off, unsigned char, nSampleId) = noSample;
    }
  }
  ownerp = (int *)((SPUVOICEROW *)ownerp + 1);
  flagsp = (unsigned short *)((SPUVOICEROW *)flagsp + 1);
  playingp = (unsigned char **)((SPUVOICEROW *)playingp + 1);
  i += 1;
  off += VOICE_ROW_STRIDE;
  if (i < SPU_VOICE_COUNT) {
    goto top;
  }
  {
    unsigned int *pending = g_adwSpuPendingKeyOffBlock;
    *pending |= kmask;
  }
}

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
  /* 0x00 */ char pad[0x2C];
  /* 0x2C */ short nVolL;
  /* 0x2E */ short nVolR;
} SPUFARROW;

extern SPUVOICEROW g_aSpuVoiceRows[]; /* the 24 mixer rows */

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
  for (; i < SPU_VOICE_COUNT; i++) {
    if (g_aSpuVoiceRows[i].nFlags & VOICE_ROW_STOPPED) {
      g_aSpuVoiceRows[i].nFlags = 0;
      continue;
    }
    if (!(g_aSpuVoiceRows[i].nFlags & VOICE_ROW_ACTIVE)) {
      continue;
    }
    status = GetSpuVoiceKeyStatus(bit << i);
    if (g_aSpuVoiceRows[i].nFlags & VOICE_ROW_KEYED) {
      if ((status & 0xFF) == 3) {
        playing = g_aSpuVoiceRows[i].pPlaying;
        keyOff |= bit << i;
        if (playing != 0) {
          *playing = VOICE_ROW_NONE;
        }
        g_aSpuVoiceRows[i].nFlags = VOICE_ROW_STOPPED;
        g_aSpuVoiceRows[i].pPlaying = 0;
        g_aSpuVoiceRows[i].pOwner = 0;
        g_aSpuVoiceRows[i].nSampleId = 0xFF;
        continue;
      }
    } else if (((status & 0xFF) == bit) || ((status & 0xFF) == 3)) {
      g_aSpuVoiceRows[i].nFlags |= VOICE_ROW_KEYED;
    }
    if (!(g_aSpuVoiceRows[i].nFlags & VOICE_ROW_ACTIVE)) {
      continue;
    }
    mode = g_aSpuVoiceRows[i].nFlags & SFX_MODE_MASK;
    switch (mode) {
    case SFX_MODE_FLAT:
      yc = SFX_FLAT_VOLUME;
      attr.volR = yc;
      attr.volL = yc;
      break;

    case SFX_MODE_3D:
      pVol = &g_aSpuVoiceRows[i].nVolL;
      SpuGetVoiceVolume(i, pVol, pVol + 1);
      owner = g_aSpuVoiceRows[i].pOwner;
      range = ((Actor *)owner)->soundRange << SOUND_RANGE_SHIFT;
      if (range == 0) {
        range = SOUND_RANGE_DEFAULT;
      }
      SubtractVector(vec, &((Actor *)owner)->posX, g_anCameraPos);
      dist = VectorLength(vec, VEC_LEN_XYZ);
      if (dist >= range) {
        far = (SPUFARROW *)((char *)g_aSpuVoiceRows - 0x28 +
                            i * VOICE_ROW_STRIDE);
        attr.volR = (far->nVolR = g_aSpuVoiceRows[i].nVolR >> 1);
        vol = g_aSpuVoiceRows[i].nVolL >> 1;
        attr.volL = (far->nVolL = vol);
        if ((attr.volR < 0x40) && (vol < 0x40)) {
          playing = g_aSpuVoiceRows[i].pPlaying;
          keyOff |= bit << i;
          if (playing != 0) {
            *playing = VOICE_ROW_NONE;
          }
          g_aSpuVoiceRows[i].nFlags = VOICE_ROW_STOPPED;
          g_aSpuVoiceRows[i].pPlaying = 0;
          g_aSpuVoiceRows[i].pOwner = 0;
          g_aSpuVoiceRows[i].nSampleId = 0xFF;
        } else {
          g_aSpuVoiceRows[i].nVolL =
              (g_aSpuVoiceRows[i].nVolL *
               (*((volatile int *)((char *)g_anSoundMasterVolumeBlock)))) >>
              12;
          g_aSpuVoiceRows[i].nVolR =
              (g_aSpuVoiceRows[i].nVolR *
               (*((volatile int *)((char *)g_anSoundMasterVolumeBlock)))) >>
              12;
          SetSpuVoiceVolumeLR(i, g_aSpuVoiceRows[i].nVolL,
                              g_aSpuVoiceRows[i].nVolR);
        }
        continue;
      };
      ComputeVoicePanAndFalloff(
          &attr.volL, dist,
          (unsigned char)(ArcTan2(
                              ((int *)(g_aSpuVoiceRows[i].pOwner + 0xC))[0] -
                                  g_anCameraPos[0],
                              ((int *)(g_aSpuVoiceRows[i].pOwner + 0xC))[1] -
                                  g_anCameraPos[1],
                              ATAN_ANGLE8) -
                          yaw),
          range, g_aSpuVoiceRows[i].anPanCache);
      break;

    case SFX_MODE_PANNED:
      attr.volR = g_aSpuVoiceRows[i].nVolR;
      attr.volL = g_aSpuVoiceRows[i].nVolL;
      break;
    }

    attr.mask = SPU_ATTR_VOLL | SPU_ATTR_VOLR;
    attr.voice = bit << i;
    attr.volL = (attr.volL * g_nSoundMasterVolume) >> 12;
    attr.volR = (attr.volR * g_nSoundMasterVolume) >> 12;
    g_aSpuVoiceRows[i].nPitch += g_aSpuVoiceRows[i].nPitchStep;
    g_aSpuVoiceRows[i].nPitchStep = 0;
    attr.mask |= SPU_ATTR_PITCH;
    attr.pitch = g_aSpuVoiceRows[i].nPitch;
    ApplySpuVoiceAttr(&attr.voice);
  }

  g_adwSpuPendingKeyOffBlock[0] |= keyOff;
  return 0;
}

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
   g_nCdAudioVolume; a rejected command re-queues itself. */

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
        g_nMusicVolumeFadeTarget = g_nCdAudioVolume;
        g_nMusicVolumeFadeStep = (g_nCdAudioVolume - g_nCdDaVolumeCurrent) >> 3;
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
        g_nMusicVolumeFadeTarget = g_nCdAudioVolume;
        g_nMusicVolumeFadeStep = (g_nCdAudioVolume - g_nCdDaVolumeCurrent) >> 3;
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

/* 0x80056b28 (0x15c) — stop every active software voice whose bit is NOT set
   in `mask` (low 24 bits). Each stopped voice is accumulated into the pending
   hardware key-off word; mode-4 (one-shot) and mode-8 (owner-attached) rows
   also silence the owner's live sample byte (Spyro's g_bSpyroSfxVoiceMarker /
   Actor voiceRow). The row is then parked idle exactly like
   StopSoundVoicesByOwner (flags 0x40, owner and sample pointer cleared,
   priority 0xFF). Finally the pending key-on word is dropped and the music
   engine is told to stop (command 4). */
/* The row bit is computed BETWEEN the two guards rather than inside the body:
   it is what reorg puts in the second beqz's delay slot, and it can only get
   there from the guard block. Written inside the body it never reaches the
   slot - the body's first insn is then the volatile key-off load, which reorg
   will not execute speculatively, so the slot takes a nop and the shift falls
   into the load shadow instead. (sched1 orders them that way for a reason no
   source form inside the body can undo: the load and the shift feed the same
   `or`, both are birthing defs, and after the `or` is scheduled the load
   becomes ready one cycle LATER than the shift, so the shift wins the earlier
   slot and the load heads the block.) */

void StopAllSoundExceptMask(int mask) {
  int i;
  int off;
  int one;
  int rowOff;
  int bit;
  int eight;
  int silence;
  volatile unsigned int *keyoff;
  mask = mask ^ SPU_VOICE_ALL;
  i = 0;
  keyoff = g_adwSpuPendingKeyOffBlock;
  one = 1;
  eight = 8;
  silence = VOICE_ROW_NONE;
  off = 0;
top:
  if (((VOICE_ROW_RAW(off, unsigned short, nFlags)) &
       (VOICE_ROW_ACTIVE | VOICE_ROW_KEYED | VOICE_ROW_FLAG_80)) != 0) {
    int rowAt;
    int mode;
    int row;
    int parkOff;
    unsigned char *owner;
    bit = one << i;
    if (((mask >> i) & 1) != 0) {
      rowOff = (i & 0x7F) * VOICE_ROW_STRIDE;
      *keyoff |= bit;
      rowAt = rowOff;
      mode = (VOICE_ROW_RAW(rowAt, unsigned short, nFlags)) & SFX_MODE_MASK;
      if (mode == eight) { /* SFX_MODE_3D */
        goto mode3d;
      }
      if (mode < 9) {
        if (mode == SFX_MODE_FLAT) {
          owner = VOICE_ROW_RAW(rowAt, unsigned char *, pOwner);
          if (owner != 0) {
            owner[SPYB_VOICE_MARKER] = silence; /* every SFX_MODE_FLAT caller
                                               owns the row by Spyro's block */
          }
        }
      }
      goto park;
    mode3d:
      owner = VOICE_ROW_RAW(rowAt, unsigned char *, pOwner);

      if (owner != 0) {
        ((Actor *)owner)->voiceRow = silence;
      }
    park:
      row = i & 0x7F;

      parkOff = row * VOICE_ROW_STRIDE;
      VOICE_ROW_RAW(parkOff, unsigned short, nFlags) = VOICE_ROW_STOPPED;
      VOICE_ROW_RAW(parkOff, int, pOwner) = 0;
      VOICE_ROW_RAW(parkOff, int, pPlaying) = 0;
      VOICE_ROW_RAW(parkOff, unsigned char, nSampleId) = 0xFF;
    }
  }

  i += 1;
  off += VOICE_ROW_STRIDE;
  if (i < SPU_VOICE_COUNT) {
    goto top;
  }
  g_dwSpuPendingKeyOn = 0;
  HandleMusicCommand(0, MUSIC_CMD_STOP_4);
}

/* Scan the 0x18 SPU voice records (0x1C bytes each) for the one owned by
   `owner` playing sample `sampleId`. Returns VOICE_OWNER_LOOPING if that voice
   is VOICE_ROW_LOOPING, VOICE_OWNER_PLAYING if matched without the flag, or
   VOICE_OWNER_NONE. (0x80056dc4, 120 bytes.) */
int GetSoundVoiceStatusByOwner(Actor *owner, int sampleId) {
  int i;

  for (i = 0; i < SPU_VOICE_COUNT; i++) {
    int off = i * VOICE_ROW_STRIDE;
    if ((int)owner == VOICE_ROW_RAW(off, int, pOwner) &&
        VOICE_ROW_RAW(off, unsigned char, nSampleId) == sampleId) {
      if (VOICE_ROW_RAW(off, unsigned short, nFlags) & VOICE_ROW_LOOPING) {
        return VOICE_OWNER_LOOPING;
      }
      return VOICE_OWNER_PLAYING;
    }
  }
  return VOICE_OWNER_NONE;
}

/* Reconcile software voice rows with the SPU (0x80056e3c): probe all 24
   hardware voice key states into a stack buffer; for any voice the hardware
   reports keyed-on (1) whose row is free (no VOICE_ROW_BUSY bit in nFlags),
   mark the row VOICE_ROW_STOPPED and OR the voice bit into the pending hardware
   key-off mask. */

void SyncSoundVoicesWithHardware(void) {
  unsigned char status[24];
  unsigned int *pending;
  unsigned char *p;
  int i, one, idle, off;
  GetAllSpuVoiceKeyStatus(status);
  i = 0;
  one = 1;
  idle = VOICE_ROW_STOPPED;
  pending = &g_dwSpuPendingKeyOff;
  off = 0;
  p = status;
  do {
    int state = *p;
    if (state == one) {
      int mask = state << i;
      if ((VOICE_ROW_RAW(off, volatile unsigned short, nFlags) &
           VOICE_ROW_BUSY) == 0) {
        VOICE_ROW_RAW(off, volatile unsigned short, nFlags) = idle;
        *pending |= mask;
      }
    }
    off += VOICE_ROW_STRIDE;
    i += 1;
    p += 1;
  } while (i < SPU_VOICE_COUNT);
}

/* Per-frame SPU maintenance: every 64 frames resync the software voice pool
   with the hardware, then flush any pending key-on / key-off requests (masked
   to the 24 voice bits) to the SPU and clear them. (0x80056ed4, 144 bytes.) */
void TickSpuPerFrame(void) {
  int *pon;
  unsigned int on;
  unsigned int off;
  if (!(g_dwGamestateFrames & 0x3F)) {
    SyncSoundVoicesWithHardware();
  }
  pon = g_anSpuPendingKeyOnBlock;
  on = pon[0];
  if (on != 0) {
    SetSpuKeyMask(1, on & SPU_VOICE_ALL);
    pon[0] = 0;
  }
  off = g_dwSpuPendingKeyOff;
  if (off != 0) {
    SetSpuKeyMask(0, off & SPU_VOICE_ALL);
    g_dwSpuPendingKeyOff = 0;
  }
}
