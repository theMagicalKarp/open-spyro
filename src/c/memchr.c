#include "globals.h"

/* libc memchr (0x80062ec0, 80 bytes; splat used to split it into three
   pieces at its jump targets): first byte equal to (unsigned char)c in the
   n bytes at s, or NULL -- also NULL for a NULL s or n <= 0. */
char *memchr(char *s, int c, int n) {
  if (s == 0 || n <= 0) {
    return 0;
  }
  while (--n >= 0) {
    if (*s++ == (unsigned char)c) {
      return s - 1;
    }
  }
  return 0;
}
