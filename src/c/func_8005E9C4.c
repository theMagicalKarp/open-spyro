#include "globals.h"

extern int func_8005EBEC(int x, int y);

/* Upload a 0x100x1 texture/CLUT strip to VRAM at (x, y): build the destination
   RECT, DMA the source bytes in via LoadImage, then return the framebuffer
   tpage/clut id computed by func_8005EBEC for that location. (0x8005e9c4,
   104 bytes.) */
int func_8005E9C4(void *data, int x, int y) {
  RECT rect;
  rect.x = x;
  rect.y = y;
  rect.w = 0x100;
  rect.h = 1;
  LoadImage((short *)&rect, (undefined4)data);
  return func_8005EBEC(x, y) & 0xFFFF;
}
