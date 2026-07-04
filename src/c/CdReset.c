#include "globals.h"

extern void FUN_80065364(void);
extern int FUN_80065270(void);
extern int CdInitDriver(void);

/* libcd CdReset mode dispatcher. mode==2 does a callback-only reset via
   FUN_80065364; otherwise brings up the controller with CdInitDriver, and
   mode==1 additionally applies the initial CD volume/buffer defaults via
   FUN_80065270. Returns 1 on success, 0 on failure. (0x80063acc, 108 bytes.) */
int CdReset(int mode) {
  if (mode == 2) {
    FUN_80065364();
    return 1;
  }
  if (CdInitDriver() != 0) {
    return 0;
  }
  if (mode == 1) {
    if (FUN_80065270() != 0) {
      return 0;
    }
  }
  return 1;
}
