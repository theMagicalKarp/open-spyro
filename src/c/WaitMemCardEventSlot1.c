#include "globals.h"

extern long TestEvent(long);
extern long D_80075B1C; /* memcard BIOS event handles, slot 1 */
extern long D_80075B20;
extern long D_80075B24;
extern long D_80075B28;
extern volatile int D_80075B2C; /* slot-1 event-status flags (x4) */
extern volatile int D_80075B30;
extern volatile int D_80075B34;
extern volatile int D_80075B38;

/* libmcrd slot-1 event wait: busy-spins on the combined slot-1 status until
   any bit is set, then TestEvent on the slot-1 BIOS handles and clears the
   flags. Returns combined>>1 (drops the "request issued" bit).
   (0x80068264, 220 bytes.) */
int WaitMemCardEventSlot1(void) {
  int status;

  do {
    status = D_80075B2C + D_80075B30 * 2 + D_80075B34 * 4 + D_80075B38 * 8;
  } while (status == 0);

  TestEvent(D_80075B1C);
  TestEvent(D_80075B20);
  TestEvent(D_80075B24);
  TestEvent(D_80075B28);
  D_80075B2C = D_80075B30 = D_80075B34 = D_80075B38 = 0;
  return status >> 1;
}
