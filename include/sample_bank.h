/* sample_bank.h — one entry of the per-level sample bank
 * (g_pLevelSampleBankEntries, stride 0x14). PlaySoundEffect reads the
 * address/pitch fields; the cutscene and credits paths point the bank at a
 * single static entry and fill it in by hand. Those fills (CheckDeathPlane,
 * Credits_Update) stay byte-offset stores, spelled with SAMPLE_RAW: their
 * per-field volatile base reloads are part of the match, and a struct view
 * reschedules them. */

#ifndef OPEN_SPYRO_SAMPLE_BANK_H
#define OPEN_SPYRO_SAMPLE_BANK_H

typedef struct {
  /* 0x00 */ unsigned int nSampleAddr;
  /* 0x04 */ unsigned int nLoopAddr;     /* -1: no loop point */
  /* 0x08 */ short nVolume;              /* credits fade it 0xA per step */
  /* 0x0A */ unsigned short nPitch;
  /* 0x0C */ unsigned short nPitchSpread;
  /* 0x0E */ unsigned short nPitchStep;
  /* 0x10 */ int nPitchMode;
} SAMPLEENTRY;

/* Byte offset of a SAMPLEENTRY field, and that field accessed as a plain
 * byte-offset reference from `entry` (any pointer). */
#define SAMPLE_OFFSET(field) ((int)&((SAMPLEENTRY *)0)->field)
#define SAMPLE_RAW(entry, type, field) \
  (*(type *)((char *)(entry) + SAMPLE_OFFSET(field)))

#endif /* OPEN_SPYRO_SAMPLE_BANK_H */
