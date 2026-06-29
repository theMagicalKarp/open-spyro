#include "globals.h"

/* Merge primitive-count of `src` into `dst` (counts at +3, cap 16): if the
   combined count + 1 still fits (< 0x11), bump dst's count and clear src's head
   word, returning 0; otherwise leave them and return -1. */
int func_8005F0AC(unsigned char *dst, unsigned char *src) {
  int n = dst[3] + src[3] + 1;
  if (n >= 0x11) {
    return -1;
  }
  dst[3] = n;
  *(int *)src = 0;
  return 0;
}
