#include "globals.h"

extern char *D_80073554;

/* 0x8005d91c — fill out[] with a status code per SPU voice in [lo, hi]:
   1 = keyed-on + owner record live, 3 = keyed-on only, 2 = record live
   only, 0 = idle. Clamps the range to 0..0x17; returns -3 on a bad
   range, else 0. */
int func_8005D91C(int lo, int hi, unsigned char *out) {
  int i;

  if (lo < 0) {
    lo = 0;
  }
  if (lo >= 0x18) {
    return -3;
  }
  if (hi >= 0x18) {
    hi = 0x17;
  }
  if (hi < 0) {
    return -3;
  }
  if (hi < lo) {
    return -3;
  }

  i = lo;
  hi = hi + 1;
  if (i < hi) {
    do {
      int off = i << 4;
      char *rec = (char *)(off + (int)D_80073554);
      unsigned int live = g_dwSpuLiveKeyOnMask & (1 << i);
      unsigned short st = *(unsigned short *)(rec + 0xC);
      if (live) {
        if (st) {
          out[i] = 1;
        } else {
          out[i] = 3;
        }
      } else {
        if (st) {
          out[i] = 2;
        } else {
          out[i] = 0;
        }
      }
      i += 1;
    } while (i < hi);
  }
  return 0;
}
