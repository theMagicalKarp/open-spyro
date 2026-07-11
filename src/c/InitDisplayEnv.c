#include "globals.h"

/* libgpu display-env init (called by ResetGraphImpl mode==0). Applies the
   initial GP1(0x10000007) get-info command, then if GPU mode is 24-bit picks
   a sub-mode; otherwise writes an initial GP0(0xe1) draw-mode word (bit
   0x1000 = display-disable check) and optionally GP1(0x20000504) for
   interlace setup. Returns a mode code 0..4 indicating display configuration.
   (0x80062230, 220 bytes.) */
unsigned int InitDisplayEnv(unsigned int param_1) {
  *(volatile unsigned int *)g_pGpuStatReg = 0x10000007;
  if ((*(volatile unsigned int *)g_pGpuDataReg & 0xFFFFFF) != 2) {
    *(volatile unsigned int *)g_pGpuDataReg =
        (*(volatile unsigned int *)g_pGpuStatReg & 0x3FFF) | 0xE1001000;
    *(volatile unsigned int *)g_pGpuDataReg;
    if ((*(volatile unsigned int *)g_pGpuStatReg & 0x1000) == 0) {
      return 0;
    }
    if ((param_1 & 8) == 0) {
      return 1;
    }
    *(volatile unsigned int *)g_pGpuStatReg = 0x20000504;
    return 2;
  }
  if ((param_1 & 8) == 0) {
    return 3;
  }
  *(volatile unsigned int *)g_pGpuStatReg = 0x9000001;
  return 4;
}
