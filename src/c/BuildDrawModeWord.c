#include "globals.h"

/* libgpu get_mode equivalent: builds the GP0(0xE1) draw-mode command word.
   Old-type GPU (graph type 1/2) packs dither at bit 11 with 13 texpage bits;
   new-type packs dither at bit 9 with 10 texpage bits. dfe = draw to display
   area, dtd = dither enable. tp doubles as the graph-type test temp — that
   register reuse is what the original regalloc keys on.
   (0x80060b70, 88 bytes.) */
unsigned int BuildDrawModeWord(int dfe, int dtd, unsigned int tpage) {
  unsigned int b;
  unsigned int tp;

  tp = g_bGpuDebugType - 1;
  if ((unsigned int)tp < 2) {
    b = 0xE1000000;
    if (dtd) {
      b |= 0x800;
    }
    tp = tpage & 0x27FF;
    if (dfe) {
      tp |= 0x1000;
    }
  } else {
    b = 0xE1000000;
    if (dtd) {
      b |= 0x200;
    }
    tp = tpage & 0x9FF;
    if (dfe) {
      tp |= 0x400;
    }
  }
  return b | tp;
}
