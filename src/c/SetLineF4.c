#include "globals.h"

/* libgpu SetLineF4: stamp a flat 4-vertex line primitive — len=6 words,
   code=0x4c, terminator pad=0x55555555 (0x8005ef74, 32 bytes). */
void SetLineF4(LINE_F4 *p) {
  *((unsigned char *)p + 3) = 6;
  p->code = 0x4C;
  p->pad = 0x55555555;
}
