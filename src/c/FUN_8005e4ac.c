#include "globals.h"

/* Zero `n` consecutive words starting at `p` — libapi callback-registry clear
   helper (0x8005e4ac, 44 bytes). */
void FUN_8005e4ac(int *p, int n) {
  while (n--) {
    *p++ = 0;
  }
}
