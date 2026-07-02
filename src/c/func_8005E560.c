#include "globals.h"

/* Root-counter aux dispatch: bumps the aux tick counter, then walks the 8
   root-counter callback slots and calls each installed one.
   (0x8005e560, 120 bytes.) */
void func_8005E560(void) {
  volatile int *vp = (volatile int *)&g_nRcntAuxState;
  int i;
  void (**p)();

  (*vp)++;
  for (i = 0, p = (void (**)())g_apfnRcntCallbacks; i < 8; i++, p++) {
    if (*p) {
      (*p)();
    }
  }
}
