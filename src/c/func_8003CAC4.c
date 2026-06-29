#include "globals.h"

extern int D_80078630[];
extern int D_80078608[];

/* Bumps the per-id usage counter D_80078630[id] and registers `id` into the
   4-slot active table D_80078608 (claiming the first free slot, marked by a
   negative entry; no-op if already present or the table is full).
   (0x8003cac4, 96 bytes.) */
void func_8003CAC4(int id) {
  int i;

  D_80078630[id]++;
  for (i = 0; i < 4; i++) {
    if (D_80078608[i] < 0) {
      D_80078608[i] = id;
      return;
    }
    if (D_80078608[i] == id) {
      return;
    }
  }
}
