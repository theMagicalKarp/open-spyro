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

#endif /* OPEN_SPYRO_SPU_H */
