#include "globals.h"

extern int func_8005EBEC(int x, int y);

/* Upload a 0x10x1 texture/CLUT strip to VRAM at (x, y): build the destination
   RECT, DMA the source bytes in via LoadImage, then return the framebuffer
   tpage/clut id computed by func_8005EBEC for that location. Twin of
   func_8005E9C4 with a 16-wide strip. (0x8005ea2c, 104 bytes.) */
int func_8005EA2C(void *data, int x, int y) {
  RECT rect;
  rect.x = x;
  rect.y = y;
  rect.w = 0x10;
  rect.h = 1;
  LoadImage((short *)&rect, (undefined4)data);
  return func_8005EBEC(x, y) & 0xFFFF;
}
