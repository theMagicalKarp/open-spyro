#include "globals.h"

extern unsigned int ControlSpuDmaTransfer();
extern void WriteSpuRamPio();
extern int D_80073570;            /* SPU transfer mode: 0 = DMA, else PIO */
extern unsigned short D_8007356C; /* SPU transfer-addr shadow (encoded units) */

/* SPU RAM write dispatcher (CPU->SPU), DMA or PIO by transfer-mode flag.
   DMA path: re-stamp the current transfer address (decoded via the SPU
   address shift) with subcmd 2, switch the channel to write direction with
   subcmd 1, then kick the DMA with subcmd 3 (buffer, byte count). PIO path:
   direct FIFO upload via WriteSpuRamPio. Returns the byte count.
   (0x8005c450, 136 bytes.) */
unsigned int WriteSpuRamInternal(unsigned short *dst, unsigned int len,
                                 unsigned int mode) {
  if (D_80073570 == 0) {
    ControlSpuDmaTransfer(2, D_8007356C << g_nSpuAddrShift);
    ControlSpuDmaTransfer(1);
    ControlSpuDmaTransfer(3, dst, len);
  } else {
    WriteSpuRamPio(dst, len);
  }
  return len;
}
