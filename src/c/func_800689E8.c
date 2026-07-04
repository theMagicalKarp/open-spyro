#include "globals.h"

extern void StopCARD2(void);
extern void func_80068EB0(void);

/* libmcrd stop wrapper: halt the memcard driver, then run the local
   teardown helper. (0x800689e8, 40 bytes.) */
void func_800689E8(void) {
  StopCARD2();
  func_80068EB0();
}
