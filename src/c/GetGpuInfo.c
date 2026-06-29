#include "globals.h"

/* PSY-Q GPUgetINFO: issue GP1(0x10) info command `cmd` and read back the 24-bit
   reply from the GPU data port (0x800617cc, 48 bytes). */
int GetGpuInfo(int cmd) {
  *(volatile int *)g_pGpuStatReg = cmd | 0x10000000;
  return *(volatile int *)g_pGpuDataReg & 0xFFFFFF;
}
