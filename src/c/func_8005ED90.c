#include "globals.h"

/* Set (flag) or clear bit 0x02 of the byte at p+7. */
void func_8005ED90(unsigned char *p, int flag) {
  if (flag) {
    p[7] |= 2;
  } else {
    p[7] &= 0xFD;
  }
}
