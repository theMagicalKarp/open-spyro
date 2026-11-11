#include "funcs.h"
#include "globals.h"
#include "gpu_prim.h"

/* Submit a semi-transparent fullscreen POLY_F4 (rect (0,8)-(512,232)) of color
   (r,g,b), blend mode `abr` (tpage bits 5..6), into the active OT slot
   (0x800190d4). Allocates from the primitive buffer and chains via AddPrimToOT.
   Used for fade/tint overlays. */
void DrawFullscreenTint(int abr, int r, int g, int b) {
  ModePolyF4 *prim;

  prim = g_pPrimBufferWriteCursor;
  SetDrawMode(g_pPrimBufferWriteCursor, 1, 0, abr << 5, 0);
  AddPrimToOT((int)prim);
  prim->poly.tag = PRIM_TAG(5);
  prim->poly.code = (GPU_CODE_POLY_F4 | GPU_SEMI_TRANS);
  prim->poly.y0 = DRAW_Y;
  prim->poly.y1 = DRAW_Y;
  prim->poly.x0 = 0;
  prim->poly.x1 = SCREEN_W;
  prim->poly.x2 = 0;
  prim->poly.y2 = DRAW_Y + DRAW_H;
  prim->poly.x3 = SCREEN_W;
  prim->poly.y3 = DRAW_Y + DRAW_H;
  prim->poly.r0 = r;
  prim->poly.g0 = g;
  prim->poly.b0 = b;
  AddPrimToOT((int)&prim->poly);
  g_pPrimBufferWriteCursor = prim + 1;
}
