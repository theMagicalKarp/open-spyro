/* spu.h — the PSY-Q SpuVoiceAttr record passed to ApplySpuVoiceAttr. */

#ifndef OPEN_SPYRO_SPU_H
#define OPEN_SPYRO_SPU_H

/* PSY-Q SpuVoiceAttr (0x40) — PlaySoundEffect writes voice/mask/volume/pitch/addr;
   InitSoundEngine fills a whole template (volume, pitch, ADSR). */
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

/* One of the 24 software mixer rows (g_aSpuVoiceRows, 0x80075F30, stride
 * 0x1C) — the voice shadow records that sit 0x28 into g_abSpuCommonAttr. */
typedef struct SpuVoiceRow {
  /* 0x00 */ unsigned char *pOwner;
  /* 0x04 */ short nVolL;
  /* 0x06 */ short nVolR;
  /* 0x08 */ int nPitch;
  /* 0x0C */ unsigned char nPriority;
  /* 0x0D */ unsigned char nSampleId;
  /* 0x0E */ unsigned short nFlags;
  /* 0x10 */ short anPanCache[2];
  /* 0x14 */ int nPitchStep;
  /* 0x18 */ char *pPlaying;
} SPUVOICEROW;

/* Full SPU volume (voice / master / CD volume words, and the SFX override
 * pair g_nSfxOverrideVolL/R). */
#define SPU_VOLUME_MAX 0x3FFF
/* InitSoundEngine's master and CD volumes; PlaySoundEffect starts a flat /
 * panned sound at SFX_FLAT_VOLUME and a positional one at SFX_3D_VOLUME
 * before its pan and falloff. */
#define SPU_MASTER_VOLUME 0x3CCC
#define SPU_CD_VOLUME 0x5000
#define SFX_FLAT_VOLUME 0x2000
#define SFX_3D_VOLUME 0x3CCC
/* SpuCommonAttr mask bits: the master and CD volume pairs. */
#define SPU_COMMON_MVOL 0x03
#define SPU_COMMON_CDVOL 0xC0
/* g_anSpuCommonVolAttr (the shorts after the mask): mvol, mvolmode, mvolx,
 * then cd.volume, each a left / right pair. */
#define SPU_COMMON_MVOL_L 0
#define SPU_COMMON_MVOL_R 1
#define SPU_COMMON_CDVOL_L 6
#define SPU_COMMON_CDVOL_R 7
/* The options screens' volume settings run 0..VOLUME_STEPS; music maps a
 * step to CD volume << MUSIC_VOLUME_SHIFT (10 steps = SPU_CD_VOLUME). */
#define VOLUME_STEPS 10
#define MUSIC_VOLUME_SHIFT 11
/* CdMix attenuations for the speakers option: stereo passes each side
 * straight through at CD_MIX_FULL, mono sends both sides to both at
 * CD_MIX_HALF. */
#define CD_MIX_FULL 0x7F
#define CD_MIX_HALF 0x3F

/* The SPU's 24 voices, one mixer row each (g_aSpuVoiceRows). */
#define SPU_VOICE_COUNT 24
#define SPU_VOICE_ALL 0xFFFFFF /* every voice's key bit */
#define VOICE_ROW_STRIDE 0x1C  /* sizeof (SPUVOICEROW) */
/* SPUVOICEATTR.mask bits (the libspu voice-attribute selectors). */
#define SPU_ATTR_VOLL 0x01
#define SPU_ATTR_VOLR 0x02
#define SPU_ATTR_PITCH 0x10
#define SPU_ATTR_ADDR 0x80       /* sample start address */
#define SPU_ATTR_ADSR 0xFF00     /* the eight envelope fields */
#define SPU_ATTR_LOOP_ADDR 0x10000

/* SPU RAM layout: 512 KB, the level's sample bank uploaded from
 * SPU_SAMPLE_BASE to the end (bank entries hold addresses relative to it,
 * RelocateLevelSampleBank); a rescued dragon's voice line is written at the
 * very top (SPU_RAM_SIZE - its size) over the bank's tail. */
#define SPU_SAMPLE_BASE 0x1010
#define SPU_RAM_SIZE 0x80000

/* SpuVoiceRow.nFlags. A row is free while none of VOICE_ROW_BUSY is set;
 * bits 0x1C are the PlaySoundEffect mode (SFX_MODE_*, actor_type.h). */
#define VOICE_ROW_ACTIVE 0x01   /* allocated to a sound (PlaySoundEffect) */
#define VOICE_ROW_KEYED 0x02    /* the SPU has reported it keyed on; once its
                                 * envelope ends TickActiveSoundVoices stops it */
#define VOICE_ROW_STOPPED 0x40  /* stopped this frame; TickActiveSoundVoices
                                 * clears the row next tick */
#define VOICE_ROW_FLAG_80 0x80  /* counted busy by the allocator and
                                 * StopAllSoundExceptMask; nothing sets it */
#define VOICE_ROW_LOOPING 0x100 /* the sample loops (it has a loop address) */
#define VOICE_ROW_BUSY (VOICE_ROW_ACTIVE | VOICE_ROW_STOPPED | VOICE_ROW_FLAG_80)

/* g_aSpuVoiceRows (the 24 mixer rows) is declared per file: PlaySoundEffect
 * needs a view with volatile pan-cache fields. */

/* Field `field` of the voice row at byte offset `off` (voice * 0x1C), in
 * the byte-offset form from g_abSpuCommonAttr the sound-stop paths use. */
#define VOICE_ROW_OFFSET(field) (0x28 + (int)&((SPUVOICEROW *)0)->field)
#define VOICE_ROW_RAW(off, type, field) \
  (*(type *)((char *)g_abSpuCommonAttr + (off) + VOICE_ROW_OFFSET(field)))

#endif /* OPEN_SPYRO_SPU_H */
