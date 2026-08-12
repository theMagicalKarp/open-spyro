#include "globals.h"

/* 0x80063830, 108 bytes — libc memmove (overlap-safe byte copy). dst >= src
   copies backward from the tail through a fresh pair of walkers; dst < src
   copies forward with dst itself walked in place, which is why the returned
   pointer is the advanced one on that path. */

void *memmove(void *dst, void *src, int n) {
  unsigned char *d = dst;
  unsigned char *s = src;
  unsigned char *p;
  unsigned char *q;

  if ((unsigned int)d >= (unsigned int)s) {
    if (n-- > 0) {
      p = (unsigned char *)(n + (int)d);
      q = (unsigned char *)(n + (int)s);
      do {
        *p-- = *q--;
      } while (n-- > 0);
    }
  } else {
    while (n-- > 0) {
      *d++ = *s++;
    }
  }
  return d;
}
