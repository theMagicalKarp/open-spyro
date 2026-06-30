#include "globals.h"

extern unsigned int BuildDrawModeWord(int dtd, int dfe, int tpage);
extern unsigned int BuildTextureWindow(unsigned char *tw);

/* libgpu SetDrawMode: fill a DR_MODE primitive (len byte = 2) with the
   draw-mode command word built from (dtd, dfe, tpage) and the texture-window
   word built from `tw`. (0x80060670, 88 bytes.) */
void SetDrawMode(unsigned char *p, int dtd, int dfe, int tpage,
                 unsigned char *tw) {
  p[3] = 2;
  *(unsigned int *)(p + 4) = BuildDrawModeWord(dtd, dfe, tpage & 0xFFFF);
  *(unsigned int *)(p + 8) = BuildTextureWindow(tw);
}
