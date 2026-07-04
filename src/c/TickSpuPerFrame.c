#include "globals.h"

extern void SyncSoundVoicesWithHardware(void);
extern void SetSpuKeyMask(int on, unsigned int voices);

extern int
    g_anSpuPendingKeyOnBlock[]; /* shared-base view of g_dwSpuPendingKeyOn */

/* Per-frame SPU maintenance: every 64 frames resync the software voice pool
   with the hardware, then flush any pending key-on / key-off requests (masked
   to the 24 voice bits) to the SPU and clear them. (0x80056ed0, 144 bytes.) */
void TickSpuPerFrame(void) {
  int *pon;
  unsigned int on;
  unsigned int off;
  if (!(g_dwGamestateFrames & 0x3F)) {
    SyncSoundVoicesWithHardware();
  }
  pon = g_anSpuPendingKeyOnBlock;
  on = pon[0];
  if (on != 0) {
    SetSpuKeyMask(1, on & 0xFFFFFF);
    pon[0] = 0;
  }
  off = g_dwSpuPendingKeyOff;
  if (off != 0) {
    SetSpuKeyMask(0, off & 0xFFFFFF);
    g_dwSpuPendingKeyOff = 0;
  }
}
