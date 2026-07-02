#include "globals.h"

extern unsigned int ControlSpuDmaTransfer();
extern unsigned short D_8007356C; /* SPU transfer-addr shadow (encoded units) */

/* DMA SPU->CPU read implementation (no PIO fallback): re-stamps the current
   transfer address (decoded via the SPU address shift) with subcmd 2, switches
   the channel to read direction with subcmd 0, then kicks the DMA with
   subcmd 3 (buffer, byte count). Returns the byte count.
   (0x8005c4d8, 104 bytes.) */
unsigned int ReadSpuRamDma(char *buf, unsigned int len) {
  ControlSpuDmaTransfer(2, D_8007356C << g_nSpuAddrShift);
  ControlSpuDmaTransfer(0);
  ControlSpuDmaTransfer(3, buf, len);
  return len;
}
