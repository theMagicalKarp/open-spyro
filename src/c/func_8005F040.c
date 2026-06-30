#include "globals.h"

/* Build a GP0(0xa0) "load image" primitive at `p` from rect `r`: tag length =
   (w*h+1)/2 + 4 words (0 when the payload would exceed the 12-word packet
   body), followed by the command word, the rect's x/y and w/h words, and a
   trailing 0x01000000 end marker after the payload. (0x8005f040, 108 bytes.) */
void func_8005F040(unsigned char *p, RECT *r) {
  unsigned int half = (r->w * r->h + 1) / 2;
  int len = half + 4;
  if (half >= 13) {
    len = 0;
  }
  p[3] = len;
  *(unsigned int *)(p + 4) = 0xA0000000;
  *(unsigned int *)(p + 8) = ((unsigned int *)r)[0];
  *(unsigned int *)(p + 12) = ((unsigned int *)r)[1];
  *(unsigned int *)(p + (len << 2)) = 0x1000000;
}
