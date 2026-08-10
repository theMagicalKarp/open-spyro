#include "globals.h"

extern unsigned short *D_80073554;

/* Encode a byte address in SPU sound RAM into SPU register units and stamp it
   into voice-register slot `idx` (0x8005c588, 164 bytes). When address
   alignment is enabled a non-multiple is rounded up to the next alignment unit.
   Sentinel slots skip the write: -2 returns the aligned byte address, -1 the
   encoded short. */
uint WriteSpuVoiceAddrRegister(int idx, uint addr) {
  uint enc;

  if (g_nSpuAddrAlignEnable != 0) {
    int unit = g_nSpuAddrAlignUnit;
    if (addr % unit != 0) {
      addr += unit;
      addr &= ~g_dwSpuAddrAlignMask;
    }
  }
  enc = addr >> g_nSpuAddrShift;
  if (idx == -2) {
    goto raw;
  }
  if (idx != -1) {
    goto store;
  }
  return enc & 0xFFFF;
raw:
  return addr;
store:
  D_80073554[idx] = enc;
  return addr;
}
