#include "globals.h"

extern volatile unsigned short *D_80073554;

/* Write one short into SPU register slot `idx` (D_80073554 base, 2 bytes/slot).
   When `encode` is 0 the value is stored raw; otherwise it is right-shifted by
   g_nSpuAddrShift to encode a byte-address into SPU register units (0x8005c540,
   72 bytes). */
void WriteSpuRegister(int idx, unsigned int val, int encode) {
  if (encode == 0) {
    D_80073554[idx] = val;
  } else {
    D_80073554[idx] = val >> g_nSpuAddrShift;
  }
}
