#include "globals.h"

extern void ZeroVector(int *v);
extern void WorldToCameraRotate(int *out, int *in);
extern void VectorToSVector(void *dst, int *src);

/* 0x80058bd8: precomputes the 3 camera-space basis vectors (X/Y/-Z axes)
   used by the billboard sprite emitter, storing each as an SVECTOR in
   g_abBillboardBasisVectors. */
void ComputeBillboardBasisVectors(void) {
  int local[3];

  ZeroVector(local);
  local[0] = 0x1000;
  WorldToCameraRotate(local, local);
  VectorToSVector(g_abBillboardBasisVectors, local);

  ZeroVector(local);
  local[1] = 0x1000;
  WorldToCameraRotate(local, local);
  VectorToSVector(g_abBillboardBasisVectors + 6, local);

  ZeroVector(local);
  local[2] = -0x1000;
  WorldToCameraRotate(local, local);
  VectorToSVector(g_abBillboardBasisVectors + 0xc, local);
}
