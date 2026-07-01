#include "globals.h"

extern void ResetMemCardEventStack(void);
extern void MemCardInit(void);
extern void FUN_8005de8c(int, void *);
extern void func_80067CD4(void);
extern volatile int D_80075B50[]; /* memcard op state */
extern volatile int D_80075B54;   /* memcard op result code */
extern volatile int D_80075B58;   /* memcard op-complete flag */
extern volatile int D_80075B60;   /* memcard session slot/handle (-1 = none) */

/* libmcrd session open: resets the event-handler stack, clears the pending-op
   block, opens the BIOS events (MemCardInit) and claims the CD bus
   (FUN_8005de8c(7, ...)) with func_80067CD4 as the bus event callback.
   Counterpart to MemCardCloseSession. (0x800662bc, 96 bytes.) */
void MemCardOpenSession(void) {
  ResetMemCardEventStack();
  D_80075B50[0] = 0;
  D_80075B54 = 0;
  D_80075B58 = 0;
  D_80075B60 = -1;
  MemCardInit();
  FUN_8005de8c(7, func_80067CD4);
}
