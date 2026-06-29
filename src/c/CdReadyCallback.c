#include "globals.h"

extern void *D_80074E38;

/* libcd CdReadyCallback: install the data-ready callback at DAT_80074e38 and
   return the previous one for save/restore (0x80063c30, 24 bytes). */
void *CdReadyCallback(void *func) {
  void *prev = D_80074E38;
  D_80074E38 = func;
  return prev;
}
