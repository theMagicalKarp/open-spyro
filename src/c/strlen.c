#include "globals.h"

/* libc strlen (BIOS-style: NULL returns 0). while(*s++) keeps the pointer
   bump in the test so it lands in the loop branch delay slot.
   (0x8006276c, 48 bytes.) */
int strlen(const char *s) {
  int n;

  n = 0;
  if (s == 0) {
    return 0;
  }
  while (*s++) {
    n++;
  }
  return n;
}
