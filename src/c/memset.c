#include "globals.h"

/* libc-style byte fill: write c into n bytes at p; no return value
   (0x8006230c, 44 bytes). */
void memset(unsigned char *p, unsigned char c, int n) {
  while (n--) {
    *p++ = c;
  }
}
