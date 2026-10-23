#include "funcs.h"
#include "globals.h"
#include "gpu_prim.h"

/* Draw a menu rectangle (0x8001860c): semi-transparent POLY_F4 fill from
   (left,top) to (right,bottom) plus four EmitRadialShadedLine
   borders. Called by SaveLoadMenu_Draw. */
void DrawShadedMenuBox(int left, int right, int top, int bottom) {
  ModePolyF4 *prim;

  prim = g_pPrimBufferWriteCursor;
  SetDrawMode((int)g_pPrimBufferWriteCursor, 1, 0, 0x40, (unsigned char *)0);
  AddPrimToOT((int)prim);
  prim->poly.tag = 0x5000000;
  prim->poly.code = 0x2a;
  prim->poly.x0 = left;
  prim->poly.x1 = right;
  prim->poly.x2 = left;
  prim->poly.x3 = right;
  prim->poly.y0 = top;
  prim->poly.y1 = top;
  prim->poly.y2 = bottom;
  prim->poly.y3 = bottom;
  prim->poly.r0 = 0x70;
  prim->poly.g0 = 0x70;
  prim->poly.b0 = 0x70;
  AddPrimToOT((int)&prim->poly);
  g_pPrimBufferWriteCursor = (void *)((int)prim + 0x24);
  EmitRadialShadedLine(left, top, right, top);
  EmitRadialShadedLine(right, top, right, bottom);
  EmitRadialShadedLine(right, bottom, left, bottom);
  EmitRadialShadedLine(left, bottom, left, top);
}
