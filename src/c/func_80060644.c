#include "globals.h"

/* libgpu SetDrawMask: stamp a GP0(0xe6) mask-bit primitive (len byte = 2). Bit
   1 (set-mask-while-drawing) is taken from `sb`, bit 0 (check-mask-before-draw)
   from `mb`; the trailing word is cleared (0x80060644, 40 bytes). */
void func_80060644(unsigned char *p, int sb, int mb) {
  unsigned int code;
  *(p + 3) = 2;
  code = 0xE6000000;
  if (sb) {
    code |= 2;
  }
  if (mb) {
    code |= 1;
  }
  *(unsigned int *)(p + 4) = code;
  *(unsigned int *)(p + 8) = 0;
}
