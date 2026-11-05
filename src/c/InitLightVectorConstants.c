#include "globals.h"

/* Seed both light directions (sprite queue [0..2], secondary actors [3..5])
   with the same default. */
void InitLightVectorConstants(void) {
  g_anWorldLightDirVec[0] = -0x9D5;
  g_anWorldLightDirVec[1] = -0x99A;
  g_anWorldLightDirVec[2] = -0xB20;
  g_anWorldLightDirVec[3] = -0x9D5;
  g_anWorldLightDirVec[4] = -0x99A;
  g_anWorldLightDirVec[5] = -0xB20;
}
