#include "globals.h"

/* Fill a one-word GP0(0xe1) draw-mode primitive at `p` (tag length byte = 1):
   base 0xe1000000, +0x200 when dithering, the tpage bits (low 9 + bit 11), and
   +0x400 when drawing to the display area is allowed. (0x8005efb4, 44 bytes.)
 */
void func_8005EFB4(unsigned char *p, int dfe, int dtd, int tpage) {
  unsigned int mode;
  unsigned int bits;

  p[3] = 1;
  mode = dtd ? 0xE1000200 : 0xE1000000;
  bits = dfe ? ((tpage & 0x9FF) | 0x400) : (tpage & 0x9FF);
  *(unsigned int *)(p + 4) = mode | bits;
}
