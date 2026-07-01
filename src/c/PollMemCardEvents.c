#include "globals.h"

extern long TestEvent(long);
extern long D_80075B0C; /* memcard BIOS event handles (8): per-slot */
extern long D_80075B10; /*   ReadEnd/WriteEnd/Done/Error for slot 1+2 */
extern long D_80075B14;
extern long D_80075B18;
extern long D_80075B1C;
extern long D_80075B20;
extern long D_80075B24;
extern long D_80075B28;
extern volatile int D_80075B2C; /* slot-1 event-status flags (x4) */
extern volatile int D_80075B30;
extern volatile int D_80075B34;
extern volatile int D_80075B38;
extern volatile int D_80075B3C; /* slot-2 event-status flags (x4) */
extern volatile int D_80075B40;
extern volatile int D_80075B44;
extern volatile int D_80075B48;

/* libmcrd event drain: TestEvent on all 8 BIOS event handles and clear both
   slots' status flags. Run by MemCardInit to flush stale events.
   (0x8006815c, 264 bytes.) */
void PollMemCardEvents(void) {
  TestEvent(D_80075B0C);
  TestEvent(D_80075B10);
  TestEvent(D_80075B14);
  TestEvent(D_80075B18);
  TestEvent(D_80075B1C);
  TestEvent(D_80075B20);
  TestEvent(D_80075B24);
  TestEvent(D_80075B28);
  D_80075B2C = D_80075B30 = D_80075B34 = D_80075B38 = 0;
  D_80075B3C = D_80075B40 = D_80075B44 = D_80075B48 = 0;
}
