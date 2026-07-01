#include "globals.h"

extern volatile int D_80075B50[]; /* memcard op-in-progress flag */
extern void FUN_8005de8c(int, void *);
extern void MemCardTeardownEvents(void);

/* libmcrd session close: spin until the pending op flag clears, release the CD
   bus (FUN_8005de8c(7, 0)), then tear down the BIOS event handles.
   (0x8006631c, 64 bytes.) */
void MemCardCloseSession(void) {
  while (D_80075B50[0] != 0) {
  }
  FUN_8005de8c(7, 0);
  MemCardTeardownEvents();
}
