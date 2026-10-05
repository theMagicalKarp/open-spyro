#include "globals.h"

extern int CdReset(int mode);
extern void *CdSyncCallback(void *func);
extern void *CdReadyCallback(void *func);
extern void *CdReadCallback(void *func);
extern int FUN_80066254(int mode);
extern int WritePrintf();
extern void func_80063A14(void); /* default sync callback */
extern void D_80063A3C(void);    /* default ready callback */
extern void D_80063A64(void);    /* default read callback */
extern char D_80011CA0[];        /* "CdInit: Init failed\n" */

/* libcd CdInit: reset the drive (up to five tries) and install the default
   sync/ready/read callbacks. Returns 1 on success, 0 on failure
   (0x8006397c, 152 bytes). */
int CdInit(void) {
  int cnt = 4;

retry:
  if (CdReset(1) != 1) {
    if (cnt--) {
      goto retry;
    }
    WritePrintf(D_80011CA0);
    return 0;
  }
  CdSyncCallback(func_80063A14);
  CdReadyCallback(D_80063A3C);
  CdReadCallback(D_80063A64);
  FUN_80066254(0);
  return 1;
}
