#include "funcs.h"
#include "globals.h"
#include "vecmath.h"

/* 0x80058bd8: precomputes the 3 camera-space basis vectors (X/Y/-Z axes)
   used by the billboard sprite emitter, storing each as an SVECTOR in
   g_anBillboardBasisVectors. */
void ComputeBillboardBasisVectors(void) {
  int local[3];

  ZeroVector(local);
  local[0] = 0x1000;
  WorldToCameraRotate(local, local);
  VectorToSVector(g_anBillboardBasisVectors, local);

  ZeroVector(local);
  local[1] = 0x1000;
  WorldToCameraRotate(local, local);
  VectorToSVector(g_anBillboardBasisVectors + 3, local);

  ZeroVector(local);
  local[2] = -0x1000;
  WorldToCameraRotate(local, local);
  VectorToSVector(g_anBillboardBasisVectors + 6, local);
}
