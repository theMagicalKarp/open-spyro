#include "globals.h"

/* libgpu LoadTPage (0x8005e8d8, 0xEC). Upload a texture page image: build the
   VRAM RECT for pixel-mode `tp` (0 = 4-bit so w/4, 1 = 8-bit so w/2,
   2 = 16-bit direct), LoadImage the pixels, and return the texture-page id
   from func_8005EBB0 (GetTPage). */
extern void LoadImage(RECT *rect, unsigned int *pix);
extern int func_8005EBB0(int tp, int abr, int x, int y);

int func_8005E8D8(unsigned int *pix, int tp, int abr, int x, int y, int w,
                  int h) {
  RECT rect;

  rect.x = x;
  rect.h = h;
  rect.y = y;
  switch (tp) {
  case 0:
    rect.w = w / 4;
    break;
  case 1:
    rect.w = w / 2;
    break;
  case 2:
    rect.w = w;
    break;
  }
  LoadImage(&rect, pix);
  return func_8005EBB0(tp, abr, x, y) & 0xFFFF;
}
