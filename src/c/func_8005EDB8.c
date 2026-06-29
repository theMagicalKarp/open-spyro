#include "globals.h"

/* Set (flag) or clear bit 0x01 of the byte at p+7. */
void func_8005EDB8(unsigned char *p, int flag) {
  if (flag) {
    p[7] |= 1;
  } else {
    p[7] &= 0xFE;
  }
}
