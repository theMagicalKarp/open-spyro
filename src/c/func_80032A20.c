#include "globals.h"

/* Poll the pending memcard sync (0x80032a20): MemCardSync(1, &ring[4],
   &ring[5]). When it reports "in progress" (return 1) commit the finished entry
   into the ring slot indexed by D_80078DA4, flip that index (1 - it), then hand
   the ring index (<<4) to the card-info op handler func_8006635C. */
extern int MemCardSync();
extern void func_8006635C();
extern volatile int D_80078DA4;
extern volatile int D_80078DA8[];

void func_80032A20(void) {
  int r = MemCardSync(1, &D_80078DA8[4], &D_80078DA8[5]);
  if (r == 0) {
    return;
  }
  if (r == 1) {
    D_80078DA8[D_80078DA4] = D_80078DA8[5];
    D_80078DA4 = r - D_80078DA4;
  }
  func_8006635C(D_80078DA4 << 4);
}
