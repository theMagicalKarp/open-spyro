#include "globals.h"

extern int D_80073570;

/* libspu SetTransferMode: record the SPU transfer mode and cache its DMA/IO
   selector (1 for mode 1, else 0), returning that selector (0x8005cb7c, 52
   bytes). */
int SetSpuTransferMode(int mode) {
  int sel;
  switch (mode) {
  case 0:
    sel = 0;
    break;
  case 1:
    sel = 1;
    break;
  default:
    sel = 0;
    break;
  }
  g_nSpuTransferMode = mode;
  D_80073570 = sel;
  return sel;
}
