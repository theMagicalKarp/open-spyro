#include "globals.h"

extern int D_8007358C; /* SPU DMA in-flight flag */
extern unsigned int ReadSpuRamDma(unsigned int src, unsigned int len,
                                  unsigned int mode);

/* PSY-Q SpuRead equivalent: clamp the byte count to SPU RAM size (0x7eff0),
   forward to the DMA reader, then clear the per-voice transfer-running shadow
   if the sync transfer already completed. Returns the clamped count.
   (0x8005ca64, 96 bytes.) */
unsigned int ReadSpuRam(unsigned int src, unsigned int len, unsigned int mode) {
  if (len > 0x7EFF0) {
    len = 0x7EFF0;
  }
  ReadSpuRamDma(src, len, mode);
  if (D_8007358C == 0) {
    g_nSpuTransferDoneFlag = 0;
  }
  return len;
}
