#include "globals.h"

/* libgpu SetLineF3: stamp a flat 3-vertex line primitive — len=5 words,
   code=0x48, terminator pad=0x55555555 (0x8005ef34, 32 bytes). */
void SetLineF3(LINE_F3 *p) {
  *((unsigned char *)p + 3) = 5;
  p->code = 0x48;
  p->pad = 0x55555555;
}
