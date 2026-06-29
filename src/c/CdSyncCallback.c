#include "globals.h"

extern void *D_80074E34;

/* libcd CdSyncCallback: install the command-complete callback (invoked from
   CdCommandSync's response drain when status bit 0x02 fires) and return the
   previous one for save/restore (0x80063c18, 24 bytes). */
void *CdSyncCallback(void *func) {
  void *prev = D_80074E34;
  D_80074E34 = func;
  return prev;
}
