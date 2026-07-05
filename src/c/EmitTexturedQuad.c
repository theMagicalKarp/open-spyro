#include "globals.h"

extern void AddPrimToOT(int prim);

/* Emit a textured POLY_FT4 quad (0x8001919c): rect (x,y,w,h)=param_1[0..3],
   UV+CLUT=param_2[0..1], RGB=param_3[0..2] (or default 0x80). Chains via
   AddPrimToOT and advances the primitive buffer by 0x28. */
void EmitTexturedQuad(short *param_1, unsigned int *param_2,
                      unsigned int *param_3) {
  void *prim;

  prim = g_pPrimBufferWriteCursor;
  *(int *)g_pPrimBufferWriteCursor = 0x9000000;
  *(unsigned char *)((int)prim + 7) = 0x2c;
  if (param_3 == 0) {
    *(unsigned char *)((int)prim + 4) = 0x80;
    *(unsigned char *)((int)prim + 5) = 0x80;
    *(unsigned char *)((int)prim + 6) = 0x80;
  } else {
    *(unsigned char *)((int)prim + 4) = param_3[0];
    *(unsigned char *)((int)prim + 5) = param_3[1];
    *(unsigned char *)((int)prim + 6) = param_3[2];
  }
  *(short *)((int)prim + 8) = param_1[0];
  *(short *)((int)prim + 10) = param_1[1];
  *(short *)((int)prim + 0x10) = param_1[0] + param_1[2];
  *(short *)((int)prim + 0x12) = param_1[1];
  *(short *)((int)prim + 0x18) = param_1[0];
  *(short *)((int)prim + 0x1a) = param_1[1] + param_1[3];
  *(short *)((int)prim + 0x20) = param_1[0] + param_1[2];
  *(short *)((int)prim + 0x22) = param_1[1] + param_1[3];
  *(int *)((int)prim + 0xc) = param_2[0];
  *(int *)((int)prim + 0x14) = param_2[1];
  *(char *)((int)prim + 0x14) = *(char *)((int)prim + 0xc) + (char)param_1[2];
  *(unsigned char *)((int)prim + 0x1c) = *(unsigned char *)((int)prim + 0xc);
  *(unsigned char *)((int)prim + 0x24) = *(unsigned char *)((int)prim + 0x14);
  *(char *)((int)prim + 0x1d) = *(char *)((int)prim + 0xd) + (char)param_1[3];
  *(char *)((int)prim + 0x25) = *(char *)((int)prim + 0xd) + (char)param_1[3];
  AddPrimToOT((int)prim);
  g_pPrimBufferWriteCursor = (void *)((int)prim + 0x28);
}
