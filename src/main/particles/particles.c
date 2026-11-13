#include "angle.h"
#include "funcs.h"
#include "globals.h"
#include "vecmath.h"

/* The world sparkle table: 16 records of 0x24 bytes (+0 kind, +4 shape
   table, +0x20 age). */
#define T g_abWorldSparkleParticleTable

/* 0x80058ae8 (0x78) — claim the first free world sparkle record: mark it kind
   0x11 with the sparkle shape table and a zero age, and return it (NULL
   when all 16 are in use).

   The record offset is a block-local `k = i * 0x24`: loop.c turns the kind
   access, the returned address and `k` itself into three separate givs, the
   original's a0/a1/v1 trio. */
void *AllocWorldSparkle(void) {
  int i;

  for (i = 0; i < 16; i++) {
    int k = i * 0x24;

    if (*(int *)&T[k] == 0) {
      *(int *)&T[k] = 0x11;
      *(void **)&T[k + 4] = g_anWorldSparkleShape;
      *(int *)&T[k + 0x20] = 0;
      return &T[k];
    }
  }
  return 0;
}

#undef T

/* Zero a single word at the given pointer. The store rides the jr delay slot,
   so the whole body is `jr $ra; sw $zero, 0($a0)` at 0x80058B60. */
void ClearWord(int *p) { *p = 0; }

/* Resets the world particle/billboard tables on level load: zeroes the point
   count (FxNode.points) of each of the 16 0x24-byte world sparkles and the
   life byte (RotBillboard.life) of each of the 8 0x18-byte rotating
   billboards, which frees every slot. Sole caller is the level
   loader (BuildLoadedLevelRuntime). (0x80058b68, 64 bytes.) */
/* Head word of the sparkle-emitter record at byte offset `off`; the loop
   counts the byte offset down directly (a record index changes the loop). */
#define SPARKLE_HEAD(off) (*(int *)&g_abWorldSparkleParticleTable[off])

void ResetWorldParticleTables(void) {
  int i;

  for (i = 0x21C; i >= 0; i -= 0x24) {
    SPARKLE_HEAD(i) = 0;
  }
  for (i = 0xA8; i >= 0; i -= 0x18) {
    g_abRotBillboardLife[i] = 0;
  }
}

#undef SPARKLE_HEAD

/* Draws the per-level world particle effects: the sparkle particles followed by
   the rotating billboards (advanced by g_nFrameStep). (0x80058ba8, 48 bytes.)
 */
void DrawWorldParticleEmitters(void) {
  DrawWorldSparkleParticles();
  DrawWorldRotatingBillboards(g_nFrameStep);
}

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

/* g_anLightSweepAngleBlock (globals.h) is an array alias of
   g_nLightSweepAngle: through the bare 4-byte extern int (sdata cost 1) both
   halves of the accumulator RMW fold to $at absolutes; the array view holds
   one base register for the pair (A22 / B-i rule 2). */
extern volatile int g_nLightSweepAngle;

/* 0x80058cc0: spins the sprite-queue light direction
   (g_anWorldLightDirVec[0..2]) around the vertical (Z) axis while a menu, card
   or cutscene is up (pause, inventory, save, title, credits, respawn). Advances
   the byte angle accumulator g_nLightSweepAngle by `steps` (wraps mod 256),
   then looks the rotated X/Y up in the cosine/sine LUTs scaled by 1983/6345; Z
   is pinned at -0x400 (flat overhead light). */
void RotateLightVectorXZ(int steps) {
  int angle;

  g_anLightSweepAngleBlock[0] =
      (g_anLightSweepAngleBlock[0] + steps) & ANGLE8_MASK;
  angle = g_nLightSweepAngle;
  g_anWorldLightDirVec[0] = (g_anCosineLut[angle] * 1983) >> 11;
  g_anWorldLightDirVec[1] = (g_anSineLut[angle] * 6345) >> 12;
  g_anWorldLightDirVec[2] = -0x400;
}
