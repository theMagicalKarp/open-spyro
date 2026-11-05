#include "globals.h"

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
