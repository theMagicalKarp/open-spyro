#include "globals.h"

/* PSY-Q get_tw: pack a texture-window RECT into a GP0(0xE2) command word —
   5-bit fields (y>>3)<<15 | (x>>3)<<10 | (-h>>3)<<5 | (-w>>3), all taken
   mod 256. Returns 0 for a null rect pointer. (0x80060da4, 132 bytes.) */
unsigned int BuildTextureWindow(RECT *tw) {
  int t[4];
  unsigned int y;
  unsigned int r;
  unsigned int code;

  if (tw == 0) {
    code = 0;
  } else {
    t[0] = (tw->x & 0xFF) >> 3;
    t[2] = (-tw->w & 0xFF) >> 3;
    t[1] = (tw->y & 0xFF) >> 3;
    t[3] = (-tw->h & 0xFF) >> 3;
    y = t[1] << 15;
    r = (t[0] << 10) | 0xE2000000;
    code = y | r | (t[3] << 5) | t[2];
  }
  return code;
}
