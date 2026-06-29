#include "globals.h"

extern int D_80075B3C;
extern int D_80075B40;
extern int D_80075B44;
extern int D_80075B48;

/* Packs the slot-2 memory-card event counters into a single status word:
   base + step1*2 + step2*4 + step3*8. (0x80068458, 60 bytes.) */
int GetMemCardEventStatusSlot2(void) {
  return D_80075B3C + (D_80075B40 << 1) + (D_80075B44 << 2) + (D_80075B48 << 3);
}
