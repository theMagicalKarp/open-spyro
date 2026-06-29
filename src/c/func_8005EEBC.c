#include "globals.h"

/* Init a GPU primitive header at p: word-length at +3, primitive code at +7. */
void func_8005EEBC(unsigned char *p) {
  p[3] = 0x2;
  p[7] = 0x68;
}
