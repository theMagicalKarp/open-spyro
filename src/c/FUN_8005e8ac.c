#include "globals.h"

/* Zero `n` consecutive words starting at `p` — libapi callback-registry clear
   helper (0x8005e8ac, 44 bytes). */
void FUN_8005e8ac(int *p, int n) {
  while (n--) {
    *p++ = 0;
  }
}
