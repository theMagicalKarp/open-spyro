#include "globals.h"

/* Build a DR_MOVE primitive: a VRAM-to-VRAM block move of `rect` to (x,y).
   The tag length byte is 5 words unless the rect is degenerate (zero width
   or height), in which case it is zeroed so the packet is skipped.
   (0x8005efe0, 0x60) */
void SetDrawMove(DR_MOVE *p, RECT *rect, int x, int y) {
  unsigned char len;
  u_long src;

  len = 5;
  if (rect->w == 0 || rect->h == 0) {
    len = 0;
  }
  p->code[0] = 0x1000000;
  p->code[1] = 0x80000000;
  *((unsigned char *)&p->tag + 3) = len;
  src = *(u_long *)rect;
  p->code[3] = y << 16 | (x & 0xffff);
  p->code[2] = src;
  p->code[4] = *(u_long *)&rect->w;
}
