#include "globals.h"

/* Init a GPU primitive header at p: word-length at +3, primitive code at +7. */
void func_8005EE08(unsigned char *p) {
  p[3] = 0x6;
  p[7] = 0x30;
}
