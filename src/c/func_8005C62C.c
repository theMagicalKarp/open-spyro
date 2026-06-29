#include "globals.h"

extern unsigned short *D_80073554;

/* Read one short from SPU register slot `idx` (D_80073554 base). When `mode` is
   -1 the raw value is returned; otherwise it is left-shifted by g_nSpuAddrShift
   to decode a byte-address into SPU register units (0x8005c62c, 60 bytes). */
int func_8005C62C(int idx, int mode) {
  unsigned short v = D_80073554[idx];
  if (mode == -1) {
    return v;
  }
  return v << g_nSpuAddrShift;
}
