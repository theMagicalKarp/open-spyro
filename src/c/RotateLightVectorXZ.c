#include "globals.h"

/* Sized-array alias of g_nLightSweepAngle: a bare 4-byte extern int is sdata
   cost 1, so both halves of the accumulator RMW fold to $at absolutes; the
   sized view holds one base register for the pair (A22 / B-i rule 2). */
extern volatile int g_anLightSweepAngleBlock[8];
extern volatile int g_nLightSweepAngle;
extern short g_anCosineLut[];

/* 0x80058cc0: spins the primary light direction vector around the world Y
   axis. Advances the byte angle accumulator g_nLightSweepAngle by `step`
   (wraps mod 256), then looks the rotated X/Y up in the cosine/sine LUTs
   scaled by 1983/6345; Z is pinned at -0x400 (flat overhead light). */
void RotateLightVectorXZ(int step) {
  int angle;

  g_anLightSweepAngleBlock[0] = (g_anLightSweepAngleBlock[0] + step) & 0xFF;
  angle = g_nLightSweepAngle;
  g_anWorldLightDirVec[0] = (g_anCosineLut[angle] * 1983) >> 11;
  g_anWorldLightDirVec[1] = (g_anSineLut[angle] * 6345) >> 12;
  g_anWorldLightDirVec[2] = -0x400;
}
