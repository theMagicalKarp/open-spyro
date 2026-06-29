#include "globals.h"

/* Resets the world particle/billboard tables on level load: zeroes the head
   word of each of the 16 0x24-byte sparkle-emitter records and the head byte of
   each of the 8 0x18-byte rotating-billboard records. Sole caller is the level
   loader (FUN_8001364c). (0x80058b68, 64 bytes.) */
void ResetWorldParticleTables(void) {
  int i;

  for (i = 0x21C; i >= 0; i -= 0x24) {
    *(int *)(g_abWorldSparkleParticleTable + i) = 0;
  }
  for (i = 0xA8; i >= 0; i -= 0x18) {
    g_abWorldRotatingBillboardTable[i] = 0;
  }
}
