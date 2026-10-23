#include "globals.h"
#include "main_shared.h"
#include "spu.h"

extern void InitSpu(void);
extern void ApplySpuVoiceAttr(int *attr);
extern void SetSpuKeyMask(int on, unsigned int voices);
extern void SetSpuTransferMode(int mode);
extern void FillWord(void *dst, unsigned int value, int byte_count);

extern int g_nSfxMixVolume_blk;      /* sfx mix volume */
extern short g_anSpuCommonVolAttr[]; /* g_abSpuCommonAttr + 0x4 (see alias) */

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
  *(int *)common = 0xC3;
  g_nSfxMixVolume_blk = 0x3FFF;
  g_nSoundMasterVolume = 0x1000;
  g_anSpuCommonVolAttr[0] = 0x3CCC;
  g_anSpuCommonVolAttr[1] = 0x3CCC;
  cd = 0x5000;
  g_nCdAudioVolume_blk = cd;
  g_anSpuCommonVolAttr[6] = cd;
  g_anSpuCommonVolAttr[7] = cd;
  g_nMusicVolumeFadeTarget = -1;
  SetSpuCommonAttr(common);

  a.mask = 0xFF13;
  a.volL = 0x2FFF;
  a.volR = 0x2FFF;
  a.pitch = 0x400;
  a.a_mode = 1;
  a.s_mode = 1;
  a.r_mode = 3;
  a.voice = 0xFFFFFF;
  a.ar = 0;
  a.dr = 0;
  a.sr = 0;
  a.rr = 0;
  a.sl = 0xF;
  ApplySpuVoiceAttr((int *)&a);

  SetSpuKeyMask(0, 0xFFFFFF);
  SetSpuTransferMode(0);
  FillWord(common + 0x28, 0, 0x2A0);
  g_nSoundMonoMixFlag = 0;
}
