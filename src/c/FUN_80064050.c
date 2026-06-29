#include "globals.h"

extern void DmaCallback(int channel, void *cb);

/* libcd: install `cb` as the DMA channel-3 (CD-ROM) completion callback
   (0x80064050, 36 bytes). */
void FUN_80064050(void *cb) { DmaCallback(3, cb); }
