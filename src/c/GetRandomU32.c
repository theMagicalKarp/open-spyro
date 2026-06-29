#include "globals.h"

/* Advance the linear-congruential RNG (seed = seed*0x41c64e6d + 0x3039) and
   return bits 16..30 of the new seed (0x8006272c, 48 bytes). */
unsigned int GetRandomU32(void) {
  g_dwRandSeed = g_dwRandSeed * 0x41C64E6D + 0x3039;
  return (g_dwRandSeed >> 16) & 0x7FFF;
}
