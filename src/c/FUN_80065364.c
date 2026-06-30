#include "globals.h"

extern int D_80074E38;
extern int D_80074E34;
extern int D_80074E48;
extern int D_80074E44;
extern int D_8006590C;
extern void FUN_8005ddc8(void);
extern void FUN_8005ddf8(int slot, void *table);

/* libcd callback-only reset (CdReset mode 2): zero the four CD callback slots,
   then tear down the CD bus / interrupt arbitration via the libapi
   system-vector thunks. (0x80065364, 80 bytes.) */
void FUN_80065364(void) {
  D_80074E38 = 0;
  D_80074E34 = 0;
  D_80074E48 = 0;
  D_80074E44 = 0;
  FUN_8005ddc8();
  FUN_8005ddf8(2, &D_8006590C);
}
