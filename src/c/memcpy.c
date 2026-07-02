#include "globals.h"

/* libc memcpy (BIOS-style byte copy: NULL dst returns 0, n <= 0 is a no-op).
   (0x800626f8, 52 bytes.) */
char *memcpy(char *dst, const char *src, int n) {
  char *ret;

  if (dst == 0) {
    return 0;
  }
  ret = dst;
  for (; n > 0; n--) {
    *dst++ = *src++;
  }
  return ret;
}
