#include "globals.h"

extern void FUN_8005ddc8(void);
extern void InitSpuHardware(int mode);
extern void InstallSpuDmaEventHandler(void);
extern void WriteSpuRegister(int reg, int value, int flags);

extern int D_800730F4;
extern int D_800730F8;
extern int D_80073104;
extern short D_80073108;
extern short D_8007310A;
extern int D_8007310C;
extern int D_80073110;
extern int D_800735B0;
extern int D_800735B4;
extern int D_800735B8;
extern int D_80073570;
extern int D_8007354C;

/* 0x8005ba80 (0xF8) — SPU driver-state (re)init: reset the DMA callback
   plumbing and hardware, on a cold init (mode 0) preset all 24 voice note
   shadows to 0xC000 (descending fill), hook the DMA event handler, zero the
   transfer/keyon bookkeeping block, and program the reverb ESA register from
   its default. */
void InitSpuSubsystem(int mode) {
  int esa;

  FUN_8005ddc8();
  InitSpuHardware(mode);
  if (mode == 0) {
    int val = 0xC000;
    int i = 0x17;
    unsigned short *p = &g_anSpuVoiceNoteShadow[0x17];
    do {
      *p = val;
      p -= 1;
    } while (--i >= 0);
  }
  InstallSpuDmaEventHandler();

  esa = g_nSpuReverbEsaDefault;
  D_800730F4 = 0;
  D_800730F8 = 0;
  D_80073104 = 0;
  D_80073108 = 0;
  D_8007310A = 0;
  D_8007310C = 0;
  D_80073110 = 0;
  g_nSpuReverbEsaShadow = esa;
  WriteSpuRegister(0xD1, esa, 0);
  D_800735B0 = 0;
  D_800735B4 = 0;
  D_800735B8 = 0;
  g_nSpuTransferMode = 0;
  D_80073570 = 0;
  g_dwSpuLiveKeyOnMask = 0;
  g_dwSpuDeferredFlushPending = 0;
  g_dwSpuDeferredKeyOnMask = 0;
  D_8007354C = 0;
}
