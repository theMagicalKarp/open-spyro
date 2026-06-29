#include "globals.h"

/* Init a GPU primitive header at p: word-length at +3, primitive code at +7. */
void func_8005EEA8(unsigned char *p) {
  p[3] = 0x4;
  p[7] = 0x64;
}
