#include "globals.h"

extern void InitSpu(void);
extern void SetSpuCommonAttr(unsigned char *attr);
extern void ApplySpuVoiceAttr(int *attr);
extern void SetSpuKeyMask(int on, unsigned int voices);
extern void SetSpuTransferMode(int mode);
extern void FillWord(void *dst, unsigned int value, int byte_count);

extern int D_80076224;               /* CD-DA mix volume */
extern int D_80076228;               /* sfx mix volume */
extern short g_anSpuCommonVolAttr[]; /* g_abSpuCommonAttr + 0x4 (see alias) */

/* 0x8005595c (0x11c) — top-level sound init (after SpuInit): reset the SPU,
   seed the common-attr block (master volume 0x3CCC L/R, CD volume 0x5000
   L/R) + the master/mix volume globals, push it, silence all 24 voices with
   one template SpuVoiceAttr (vol 0x2FFF, pitch 0x400, ADSR modes 1/1/3,
   sustain 0xF), clear the key mask, set DMA transfer mode, and zero the
   voice-state block at common+0x28. */
void InitSoundEngine(void) {
  int pad[2];
  int a[16];
  unsigned char *common;
  int cd;

  InitSpu();
  common = g_abSpuCommonAttr;
  *(int *)common = 0xC3;
  D_80076228 = 0x3FFF;
  g_nSoundMasterVolume = 0x1000;
  g_anSpuCommonVolAttr[0] = 0x3CCC;
  g_anSpuCommonVolAttr[1] = 0x3CCC;
  cd = 0x5000;
  D_80076224 = cd;
  g_anSpuCommonVolAttr[6] = cd;
  g_anSpuCommonVolAttr[7] = cd;
  g_nMusicVolumeFadeTarget = -1;
  SetSpuCommonAttr(common);

  a[1] = 0xFF13;
  *(short *)((char *)a + 0x8) = 0x2FFF;
  *(short *)((char *)a + 0xA) = 0x2FFF;
  *(short *)((char *)a + 0x14) = 0x400;
  a[9] = 1;
  a[10] = 1;
  a[11] = 3;
  a[0] = 0xFFFFFF;
  *(short *)((char *)a + 0x30) = 0;
  *(short *)((char *)a + 0x32) = 0;
  *(short *)((char *)a + 0x34) = 0;
  *(short *)((char *)a + 0x36) = 0;
  *(short *)((char *)a + 0x38) = 0xF;
  ApplySpuVoiceAttr(a);

  SetSpuKeyMask(0, 0xFFFFFF);
  SetSpuTransferMode(0);
  FillWord(common + 0x28, 0, 0x2A0);
  g_nSoundMonoMixFlag = 0;
}
