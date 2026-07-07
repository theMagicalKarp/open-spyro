#include "globals.h"

/* SPU transfer completion poll (0x8005cbb0, SpuIsTransferCompleted-style).
   DMA-write mode (g_nSpuTransferMode == 1) or an already-latched done flag
   short-circuits to 1. Otherwise tests the SPU DMA event: mode 1 blocks,
   re-testing until the event fires; mode 0 returns the raw test result,
   latching the done flag only on success. */
extern int TestEvent();

int GetSpuTransferStatus(int mode) {
  int r;
  if (g_nSpuTransferMode == 1 || g_nSpuTransferDoneFlag == 1) {
    return 1;
  }
  r = TestEvent(g_nSpuDmaEventHandle);
  if (mode == 1) {
    while (r == 0) {
      r = TestEvent(g_nSpuDmaEventHandle);
    }
    r = 1;
  } else {
    if (r != 1) {
      return r;
    }
  }
  g_nSpuTransferDoneFlag = r;
  return r;
}
