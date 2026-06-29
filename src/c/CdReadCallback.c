#include "globals.h"

extern void *D_80075144;

/* libcd CdReadCallback: install the per-sector data-arrival callback for libcd
   reads at DAT_80075144 and return the previous one for save/restore
   (0x8006623c, 24 bytes). */
void *CdReadCallback(void *func) {
  void *prev = D_80075144;
  D_80075144 = func;
  return prev;
}
