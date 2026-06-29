#include "globals.h"

extern void DmaCallback(int channel, void *cb);

/* Install `cb` as the SPU DMA channel-4 completion callback (0x8005c788,
   36 bytes). */
void InstallSpuDmaCallback(void *cb) { DmaCallback(4, cb); }
