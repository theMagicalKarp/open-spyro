#include "globals.h"

/* libc srand: seed the linear-congruential RNG used by GetRandomU32
   (0x8006275c, 16 bytes). */
void srand(unsigned int seed) { g_dwRandSeed = seed; }
