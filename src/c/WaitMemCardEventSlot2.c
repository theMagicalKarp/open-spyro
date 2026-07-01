#include "globals.h"

extern long TestEvent(long);
extern long D_80075B0C; /* memcard BIOS event handles, slot 2 */
extern long D_80075B10;
extern long D_80075B14;
extern long D_80075B18;
extern volatile int D_80075B3C; /* slot-2 event-status flags (x4) */
extern volatile int D_80075B40;
extern volatile int D_80075B44;
extern volatile int D_80075B48;

/* libmcrd slot-2 event wait: busy-spins on the combined slot-2 status until
   any bit is set, then TestEvent on the slot-2 BIOS handles and clears the
   flags. Returns combined>>1 (drops the "request issued" bit). Counterpart to
   WaitMemCardEventSlot1. (0x80068340, 220 bytes.) */
int WaitMemCardEventSlot2(void) {
  int status;

  do {
    status = D_80075B3C + D_80075B40 * 2 + D_80075B44 * 4 + D_80075B48 * 8;
  } while (status == 0);

  TestEvent(D_80075B0C);
  TestEvent(D_80075B10);
  TestEvent(D_80075B14);
  TestEvent(D_80075B18);
  D_80075B3C = D_80075B40 = D_80075B44 = D_80075B48 = 0;
  return status >> 1;
}
