#include "globals.h"

extern void SetDrawMode(int p, int dfe, int dtd, unsigned int tpage,
                        unsigned char *clip);
extern void AddPrimToOT(int prim);
extern void EmitRadialShadedLine(int x0, int y0, int x1, int y1);

/* Draw a menu rectangle (0x8001860c): semi-transparent POLY_F4 fill from
   (param_1,param_3) to (param_2,param_4) plus four EmitRadialShadedLine
   borders. Called by SaveLoadMenu_Draw. */
void DrawShadedMenuBox(int param_1, int param_2, int param_3, int param_4) {
  void *prim;

  prim = g_pPrimBufferWriteCursor;
  SetDrawMode((int)g_pPrimBufferWriteCursor, 1, 0, 0x40, (unsigned char *)0);
  AddPrimToOT((int)prim);
  *(int *)((int)prim + 0xc) = 0x5000000;
  *(unsigned char *)((int)prim + 0x13) = 0x2a;
  *(short *)((int)prim + 0x14) = param_1;
  *(short *)((int)prim + 0x18) = param_2;
  *(short *)((int)prim + 0x1c) = param_1;
  *(short *)((int)prim + 0x20) = param_2;
  *(short *)((int)prim + 0x16) = param_3;
  *(short *)((int)prim + 0x1a) = param_3;
  *(short *)((int)prim + 0x1e) = param_4;
  *(short *)((int)prim + 0x22) = param_4;
  *(unsigned char *)((int)prim + 0x10) = 0x70;
  *(unsigned char *)((int)prim + 0x11) = 0x70;
  *(unsigned char *)((int)prim + 0x12) = 0x70;
  AddPrimToOT((int)prim + 0xc);
  g_pPrimBufferWriteCursor = (void *)((int)prim + 0x24);
  EmitRadialShadedLine(param_1, param_3, param_2, param_3);
  EmitRadialShadedLine(param_2, param_3, param_2, param_4);
  EmitRadialShadedLine(param_2, param_4, param_1, param_4);
  EmitRadialShadedLine(param_1, param_4, param_1, param_3);
}
