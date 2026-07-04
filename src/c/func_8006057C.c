#include "globals.h"

extern unsigned int BuildDrawAreaTopLeft(unsigned int x, unsigned int y);
extern unsigned int BuildDrawAreaBottomRight(unsigned int x, unsigned int y);

/* Build the top-left / bottom-right GPU draw-area commands for a clip RECT
   (x,y,w,h shorts) into a packet: byte+3 = command tag 2, word+4 = top-left
   corner, word+8 = bottom-right corner (x+w-1, y+h-1). (0x8006057c.) */
void func_8006057C(void *packet, void *rect) {
  short *s = (short *)rect;
  unsigned short *u = (unsigned short *)rect;
  *((unsigned char *)packet + 3) = 2;
  *(unsigned int *)((char *)packet + 4) = BuildDrawAreaTopLeft(s[0], s[1]);
  *(unsigned int *)((char *)packet + 8) = BuildDrawAreaBottomRight(
      (short)(u[0] + u[2] - 1), (short)(u[1] + u[3] - 1));
}
