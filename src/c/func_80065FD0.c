#include "globals.h"

extern void FUN_80064074(int);
extern void FUN_80064050(void *);
extern void *CdSyncCallback(void *func);
extern void *CdReadyCallback(void *func);
extern int CdControlForce(int cmd, void *param);
extern int D_80075178[]; /* libcd driver mode flags (bit 0 = DMA delivery) */
extern volatile int D_8007515C;   /* libcd busy/pending flag */
extern void *volatile D_8007516C; /* saved sync callback */
extern void *D_80075170;          /* saved ready callback */
extern void *D_80075174;          /* saved DMA completion handler */

/* libcd read teardown: stops the DMA engine when mode bit 0 is set, clears the
   pending flag, restores the saved sync/ready callbacks (and the DMA handler
   in DMA mode), then issues Pause (cmd 9) via CdControlForce.
   (0x80065fd0, 156 bytes.) */
void func_80065FD0(void) {
  if (D_80075178[0] & 1) {
    FUN_80064074(0);
  }
  D_8007515C = 0;
  CdSyncCallback(D_8007516C);
  CdReadyCallback(D_80075170);
  if (D_80075178[0] & 1) {
    FUN_80064050(D_80075174);
  }
  CdControlForce(9, 0);
}
