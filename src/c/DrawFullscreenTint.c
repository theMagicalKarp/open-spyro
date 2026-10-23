#include "globals.h"
#include "gpu_prim.h"

extern void SetDrawMode(int p, int dfe, int dtd, unsigned int tpage,
                        unsigned char *clip);
extern void AddPrimToOT(int prim);

/* Submit a semi-transparent fullscreen POLY_F4 (rect (0,8)-(512,232)) of color
   (r,g,b), blend mode `abr` (tpage bits 5..6), into the active OT slot
   (0x800190d4). Allocates from the primitive buffer and chains via AddPrimToOT.
   Used for fade/tint overlays. */
void DrawFullscreenTint(int abr, int r, int g, int b) {
  ModePolyF4 *prim;

  prim = g_pPrimBufferWriteCursor;
  SetDrawMode((int)g_pPrimBufferWriteCursor, 1, 0, abr << 5,
              (unsigned char *)0);
  AddPrimToOT((int)prim);
  prim->poly.tag = 0x5000000;
  prim->poly.code = 0x2a;
  prim->poly.y0 = 8;
  prim->poly.y1 = 8;
  prim->poly.x0 = 0;
  prim->poly.x1 = 0x200;
  prim->poly.x2 = 0;
  prim->poly.y2 = 0xe8;
  prim->poly.x3 = 0x200;
  prim->poly.y3 = 0xe8;
  prim->poly.r0 = r;
  prim->poly.g0 = g;
  prim->poly.b0 = b;
  AddPrimToOT((int)&prim->poly);
  g_pPrimBufferWriteCursor = (void *)((int)prim + 0x24);
}
