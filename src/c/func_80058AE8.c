#include "globals.h"

extern void D_8006E308(); /* sparkle particle update routine */

/* The world sparkle table: 16 records of 0x24 bytes (+0 kind, +4 update
   routine, +0x20 age). */
#define T g_abWorldSparkleParticleTable

/* 0x80058ae8 (0x78) — claim the first free world sparkle record: mark it kind
   0x11 with the sparkle update routine and a zero age, and return it (NULL
   when all 16 are in use).

   The record offset is a block-local `k = i * 0x24`: loop.c turns the kind
   access, the returned address and `k` itself into three separate givs, the
   original's a0/a1/v1 trio. */
void *func_80058AE8(void) {
  int i;

  for (i = 0; i < 16; i++) {
    int k = i * 0x24;

    if (*(int *)&T[k] == 0) {
      *(int *)&T[k] = 0x11;
      *(void **)&T[k + 4] = D_8006E308;
      *(int *)&T[k + 0x20] = 0;
      return &T[k];
    }
  }
  return 0;
}
