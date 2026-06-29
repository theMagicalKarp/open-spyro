#include "globals.h"

extern volatile unsigned int *D_80073564;

/* Update the SPU control shadow (*D_80073564): clear the reverb-mode field
   (bits 16..18) then OR in 0x3 when `on` is set, otherwise 0x5, into that field
   (0x8005c668, 96 bytes). */
void func_8005C668(int on) {
  *D_80073564 &= 0xFFF8FFFF;
  if (on) {
    *D_80073564 |= 0x30000;
  } else {
    *D_80073564 |= 0x50000;
  }
}
