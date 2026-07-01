#include "globals.h"

extern int D_80075B4C[]; /* memcard session retry counter */
extern int D_80075B90[]; /* memcard session retry counter */
extern int MemCardBuInit(void);

/* libmcrd one-shot reset + init: clears the session retry counters and
   re-inits the card driver via MemCardBuInit. (0x8006626c, 48 bytes.) */
void MemCardReset(void) {
  D_80075B4C[0] = 0;
  D_80075B90[0] = 0;
  MemCardBuInit();
}
