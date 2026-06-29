#include "globals.h"

/* Init a GPU primitive header at p: word-length at +3, primitive code at +7. */
void func_8005EE6C(unsigned char *p) {
  p[3] = 0xC;
  p[7] = 0x3C;
}
