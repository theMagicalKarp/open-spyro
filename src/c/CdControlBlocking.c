#include "globals.h"

extern void *D_80074E34;
extern unsigned char D_80074E44;
extern int D_80074DAC[];
extern int CdCommandSync(unsigned char com, unsigned char *param,
                         unsigned char *result, int mode);
extern int CdSync(int mode, unsigned char *result);

/* libcd CdControlB: CdControl, then block in CdSync until the command
   completes. Returns 1 when it completed (CdlComplete), 0 on failure
   (0x80063eac, 324 bytes). */
int CdControlBlocking(unsigned char com, unsigned char *param,
                      unsigned char *result) {
  void *old = D_80074E34;
  int cnt = 4;
  int ret;

  while (cnt--) {
    D_80074E34 = 0;
    if (com != 1 && (D_80074E44 & 0x10)) {
      CdCommandSync(1, 0, 0, 0);
    }
    if (param == 0 || D_80074DAC[com] == 0 ||
        CdCommandSync(2, param, result, 0) == 0) {
      D_80074E34 = old;
      if (CdCommandSync(com, param, result, 0) == 0) {
        ret = 0;
        goto out;
      }
    }
  }
  D_80074E34 = old;
  ret = -1;
out:
  if (ret != 0) {
    return 0;
  }
  return CdSync(0, result) == 2;
}
