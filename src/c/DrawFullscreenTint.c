#include "globals.h"

extern void SetDrawMode(int p, int dfe, int dtd, unsigned int tpage,
                        unsigned char *clip);
extern void AddPrimToOT(int prim);

/* Submit a semi-transparent fullscreen POLY_F4 (rect (0,8)-(512,232)) of color
   (r,g,b) into the active OT slot (0x800190d4). Allocates from the primitive
   buffer and chains via AddPrimToOT. Used for fade/tint overlays. */
void DrawFullscreenTint(int param_1, int r, int g, int b) {
  void *prim;

  prim = g_pPrimBufferWriteCursor;
  SetDrawMode((int)g_pPrimBufferWriteCursor, 1, 0, param_1 << 5,
              (unsigned char *)0);
  AddPrimToOT((int)prim);
  *(int *)((int)prim + 0xc) = 0x5000000;
  *(unsigned char *)((int)prim + 0x13) = 0x2a;
  *(short *)((int)prim + 0x16) = 8;
  *(short *)((int)prim + 0x1a) = 8;
  *(short *)((int)prim + 0x14) = 0;
  *(short *)((int)prim + 0x18) = 0x200;
  *(short *)((int)prim + 0x1c) = 0;
  *(short *)((int)prim + 0x1e) = 0xe8;
  *(short *)((int)prim + 0x20) = 0x200;
  *(short *)((int)prim + 0x22) = 0xe8;
  *(unsigned char *)((int)prim + 0x10) = r;
  *(unsigned char *)((int)prim + 0x11) = g;
  *(unsigned char *)((int)prim + 0x12) = b;
  AddPrimToOT((int)prim + 0xc);
  g_pPrimBufferWriteCursor = (void *)((int)prim + 0x24);
}
