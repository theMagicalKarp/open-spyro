#include "globals.h"

extern unsigned char D_80074D15[];

/* ASCII case folding via the ctype class table (libc toupper/tolower pair,
   emitted as one 0x60-byte block). toupper: if the lowercase bit (0x2) is set,
   subtract 0x20. tolower: if the uppercase bit (0x1) is set, add 0x20.
   (0x80062e60, 96 bytes.) */
int func_80062E60(unsigned char c) {
  unsigned char r = c;
  if (D_80074D15[c] & 2) {
    r = c - 0x20;
  }
  return r;
}

int func_80062E90(unsigned char c) {
  unsigned char r = c;
  if (D_80074D15[c] & 1) {
    r = c + 0x20;
  }
  return r;
}
