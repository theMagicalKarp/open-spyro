#include "funcs.h"
#include "globals.h"
#include "gpu_prim.h"

/* Emit a textured POLY_FT4 quad (0x8001919c): rect (x,y,w,h) = rect[0..3],
   UV+CLUT and UV+tpage words = uv[0..1], RGB = rgb[0..2] (or 0x80 grey when
   rgb is NULL). The other three corners' UVs are derived from u0/v0 plus the
   rect size. Chains via AddPrimToOT and advances the primitive buffer by
   0x28. */
void EmitTexturedQuad(short *rect, unsigned int *uv, unsigned int *rgb) {
  PolyFT4 *q;

  q = g_pPrimBufferWriteCursor;
  q->tag = 0x9000000;
  q->code = 0x2c;
  if (rgb == 0) {
    q->r0 = 0x80;
    q->g0 = 0x80;
    q->b0 = 0x80;
  } else {
    q->r0 = rgb[0];
    q->g0 = rgb[1];
    q->b0 = rgb[2];
  }
  q->x0 = rect[0];
  q->y0 = rect[1];
  q->x1 = rect[0] + rect[2];
  q->y1 = rect[1];
  q->x2 = rect[0];
  q->y2 = rect[1] + rect[3];
  q->x3 = rect[0] + rect[2];
  q->y3 = rect[1] + rect[3];
  *(int *)&q->u0 = uv[0];
  *(int *)&q->u1 = uv[1];
  q->u1 = q->u0 + rect[2];
  q->u2 = q->u0;
  q->u3 = q->u1;
  q->v2 = q->v0 + rect[3];
  q->v3 = q->v0 + rect[3];
  AddPrimToOT((int)q);
  g_pPrimBufferWriteCursor = q + 1;
}
