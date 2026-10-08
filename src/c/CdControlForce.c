#include "globals.h"

extern void *D_80074E34;
extern unsigned char D_80074E44;
extern int D_80074DAC[];
extern int CdCommandSync(unsigned char com, unsigned char *param,
                         unsigned char *result, int mode);

/* libcd CdControlF: like CdControl, but issues the command without waiting
   for its result (CdCommandSync mode 1). Returns 1 on success, 0 after four
   failed attempts (0x80063d80, 300 bytes). */
int CdControlForce(unsigned char com, unsigned char *param) {
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
    if (param == 0 || *p == 0 || CdCommandSync(2, param, 0, 0) == 0) {
      D_80074E34 = old;
      if (CdCommandSync(com, param, 0, 1) == 0) {
        goto out;
      }
    }
  }
  D_80074E34 = old;
  ret = -1;
out:
  return ret + 1;
}
