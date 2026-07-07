#include "globals.h"

/* Inventory completion percentage (0x8002bb20): gems are 50%, dragons 40%,
   eggs 10% — gems*50 + dragons*6000 + eggs*10000, out of 12000*100. Past the
   12000-gem cap (100%+ via cheats) it counts single percent per 100 extra
   gems above 0x2EE0. */
int ComputeInventoryProgressPercent(void) {
  int pct = (g_nWorldTreasureCollected * 50 + g_nWorldDragonsRescued * 6000 +
             g_nWorldEggsRecovered * 10000) /
            12000;
  if (g_nWorldTreasureCollected >= 0x2EE1) {
    pct = (g_nWorldTreasureCollected - 0x2EE0) / 100 + 100;
  }
  return pct;
}
