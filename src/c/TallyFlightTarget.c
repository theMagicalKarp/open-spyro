#include "globals.h"

extern int g_anFlightTargetTally[];

/* Bumps the per-id usage counter g_anFlightTargetTally[id] and registers `id`
   into the 4-slot active table g_anFlightTallyActiveSlots (claiming the first
   free slot, marked by a negative entry; no-op if already present or the table
   is full). (0x8003cac4, 96 bytes.) */
void TallyFlightTarget(int id) {
  int i;

  g_anFlightTargetTally[id]++;
  for (i = 0; i < 4; i++) {
    if (g_anFlightTallyActiveSlots[i] < 0) {
      g_anFlightTallyActiveSlots[i] = id;
      return;
    }
    if (g_anFlightTallyActiveSlots[i] == id) {
      return;
    }
  }
}
