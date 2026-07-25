#include "globals.h"

extern unsigned char D_80011878[]; /* "%s:bad RECT" */
extern unsigned char D_80011884[]; /* "(%d,%d)-(%d,%d)\n" */
extern unsigned char D_80011898[]; /* "%s:" */

/* libgpu debug helper called by ClearImage/LoadImage/StoreImage/MoveImage.
   Validates that the RECT fits within the framebuffer bounds
   (g_nGpuScreenWidth/g_nGpuScreenHeight). Prints "<tag>:bad RECT" plus the
   rect via the debug-printf hook when g_bGpuDebugLevel==1 and the rect is
   out of bounds, or just "<tag>:" when g_bGpuDebugLevel==2.
   (0x8005f7d0, 296 bytes.) */
void ValidateGpuRect(char *name, RECT *rect) {
  unsigned char *msg;
  void *new_var;
  int w;
  short new_var3;
  int x;
  short *new_var2;
  int y;
  int h;
  new_var2 = &g_nGpuScreenWidth;
  switch (g_bGpuDebugLevel) {
  case 1:
    w = rect->w;
    new_var3 = g_nGpuScreenWidth;
    if (new_var3 < w) {
      goto bad_rect;
    }
    x = rect->x;
    if ((*new_var2) < (w + x)) {
      goto bad_rect;
    }
    y = rect->y;
    if (g_nGpuScreenHeight < y) {
      goto bad_rect;
    }
    h = rect->h;
    if (g_nGpuScreenHeight < (y + h)) {
      goto bad_rect;
    }
    if (w <= 0) {
      goto bad_rect;
    }
    if (x < 0) {
      goto bad_rect;
    }
    if (y < 0) {
      goto bad_rect;
    }
    if (h > 0) {
      return;
    }
  bad_rect:
    msg = D_80011878;

    break;

  case 2:
    msg = D_80011898;
    break;

  default:
    return;
  }

  ((void (*)())g_pfnGpuDebugPrintf)(msg, name);
  {
    int px = rect->x;
    int py = rect->y;
    int pw = rect->w;
    int ph = rect->h;
    do {
    } while (0);
    new_var = g_pfnGpuDebugPrintf;
    ((void (*)())new_var)(D_80011884, px, py, pw, ph);
  }
}
