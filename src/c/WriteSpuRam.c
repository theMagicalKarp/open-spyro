#include "globals.h"

extern int D_8007358C; /* SPU DMA in-flight flag */
extern unsigned int WriteSpuRamInternal(unsigned short *dst, unsigned int len,
                                        unsigned int mode);

/* PSY-Q SpuWrite equivalent: clamp the byte count to SPU RAM size (0x7eff0),
   forward to the internal writer, then clear the per-voice transfer-running
   shadow if the sync transfer already completed. Returns the clamped count.
   (0x8005cac4, 96 bytes.) */
unsigned int WriteSpuRam(unsigned short *dst, unsigned int len,
                         unsigned int mode) {
  if (len > 0x7EFF0) {
    len = 0x7EFF0;
  }
  WriteSpuRamInternal(dst, len, mode);
  if (D_8007358C == 0) {
    g_nSpuTransferDoneFlag = 0;
  }
  return len;
}
