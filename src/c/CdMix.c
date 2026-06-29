#include "globals.h"

extern int FUN_80065108(void *vol);

/* libcd CdMix: set CD-DA volume mixing from a CdlATV (0x80063ff0, 32 bytes).
   Delegates the mixer-register pokes to FUN_80065108 and always reports
   success. */
int CdMix(void *vol) {
  FUN_80065108(vol);
  return 1;
}
