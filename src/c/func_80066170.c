#include "globals.h"

extern long VSync(long mode);
extern int CdReadStart(int resume);
extern void func_80063BF8(int, int);
extern volatile int
    D_80075164[]; /* CD-read watchdog anchor: [0]=read-start frame,
                     [-1]=last-sector frame, [-2]=sectors remaining,
                     [-7]=pending sector count */

/* CD-read completion wait: each VSync, gives up (-1) if the whole read has
   run past 1200 frames; restarts the read (CdReadStart(1)) if the remaining
   count went negative or no sector arrived for 60 frames. mode 0 blocks until
   the read drains or fails; nonzero samples once. Always hands off through
   func_80063BF8(1, arg). Returns sectors remaining or -1.
   (0x80066170, 204 bytes.) */
int func_80066170(int mode, int arg) {
  int remaining;

  do {
    if (VSync(-1) > D_80075164[0] + 1200) {
      remaining = -1;
    } else if (D_80075164[-2] < 0 || VSync(-1) > D_80075164[-1] + 60) {
      CdReadStart(1);
      remaining = D_80075164[-7];
    } else {
      remaining = D_80075164[-2];
    }
  } while (mode == 0 && remaining > 0);

  func_80063BF8(1, arg);
  return remaining;
}
