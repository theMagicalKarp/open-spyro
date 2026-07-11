#include "globals.h"

/* libgpu DMA read-back kick (0x80060ef0, 0xE8). Program the GPU DMA channel
   through the register-pointer globals (D_80074B54 = GPU control, D_80074B50 =
   channel control, D_80074B48 = DMA base address, D_80074B4C = block count):
   enable DMA in the GPU control word, point the channel at the END of the
   destination buffer and start a reverse VRAM->CPU transfer (0x11000002),
   then poll busy (bit 24) under the GPU timeout watchdog. Returns `len` on
   completion, -1 on timeout. */
extern void ResetGpuTimeoutDeadline(void);
extern int CheckGpuTimeout(void);

extern volatile int *D_80074B48;
extern volatile int *D_80074B4C;
extern volatile int *D_80074B50;
extern volatile int *D_80074B54;

int func_80060EF0(char *addr, int len) {
  *D_80074B54 |= 0x8000000;
  *D_80074B50 = 0;
  *D_80074B48 = (int)(addr + ((len << 2) - 4));
  *D_80074B4C = len;
  *D_80074B50 = 0x11000002;
  ResetGpuTimeoutDeadline();
  while (*D_80074B50 & 0x1000000) {
    if (CheckGpuTimeout() != 0) {
      return -1;
    }
  }
  return len;
}
