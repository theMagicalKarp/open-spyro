#include "globals.h"

/* Pack a CLUT id from x (arg0) and y (arg1): (y << 6) | ((x >> 4) & 0x3F),
   truncated to 16 bits. */
int func_8005EBEC(int x, int y) {
  return ((y << 6) | ((x >> 4) & 0x3F)) & 0xFFFF;
}
