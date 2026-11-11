#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "spu.h"

/* Per-frame SPU maintenance: every 64 frames resync the software voice pool
   with the hardware, then flush any pending key-on / key-off requests (masked
   to the 24 voice bits) to the SPU and clear them. (0x80056ed4, 144 bytes.) */
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
    SetSpuKeyMask(1, on & SPU_VOICE_ALL);
    pon[0] = 0;
  }
  off = g_dwSpuPendingKeyOff;
  if (off != 0) {
    SetSpuKeyMask(0, off & SPU_VOICE_ALL);
    g_dwSpuPendingKeyOff = 0;
  }
}
