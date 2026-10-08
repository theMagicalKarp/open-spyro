#include "globals.h"

extern void *D_80074E34;
extern unsigned char D_80074E44;
extern int D_80074DAC[];
extern int CdCommandSync(unsigned char com, unsigned char *param,
                         unsigned char *result, int mode);

/* libcd CdControl: issue a command with up to four attempts. Parks the sync
   callback while the CdlNop abort and the optional CdlSetloc preamble run,
   restores it for the real command. Returns 1 on success, 0 when every
   attempt failed (0x80063c48, 312 bytes). The loop-top locals make loop.c
   hoist the zext, table base and index add ahead of `ret = 0`. */
int CdControl(unsigned char com, unsigned char *param, unsigned char *result) {
  void *old = D_80074E34;
  int cnt = 4;
  int ret;

  while (cnt--) {
    int c = com;
    int *tbl = D_80074DAC;
    int *p = tbl + c;
    ret = 0;
    D_80074E34 = 0;
    if (com != 1 && (D_80074E44 & 0x10)) {
      CdCommandSync(1, 0, 0, 0);
    }
    if (param == 0 || *p == 0 || CdCommandSync(2, param, result, 0) == 0) {
      D_80074E34 = old;
      if (CdCommandSync(com, param, result, 0) == 0) {
        goto out;
      }
    }
  }
  D_80074E34 = old;
  ret = -1;
out:
  return ret + 1;
}
