#include "globals.h"

extern int D_80075B2C;
extern int D_80075B30;
extern int D_80075B34;
extern int D_80075B38;

/* Packs the slot-1 memory-card event counters into a single status word:
   base + step1*2 + step2*4 + step3*8. (0x8006841c, 60 bytes.) */
int GetMemCardEventStatusSlot1(void) {
  return D_80075B2C + (D_80075B30 << 1) + (D_80075B34 << 2) + (D_80075B38 << 3);
}
