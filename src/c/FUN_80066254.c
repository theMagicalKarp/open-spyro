#include "globals.h"

extern void *volatile D_80075178[];

/* Install `cb` as the libcd read-break callback slot (D_80075178), returning
   the previous value; CdInit clears it by passing 0 (0x80066254, 24 bytes). */
void *FUN_80066254(void *cb) {
  void *prev = D_80075178[0];
  D_80075178[0] = cb;
  return prev;
}
