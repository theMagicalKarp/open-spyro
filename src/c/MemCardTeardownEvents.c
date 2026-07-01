#include "globals.h"

extern int EnterCriticalSection(void);
extern void ExitCriticalSection(void);
extern long CloseEvent(long);
extern long D_80075B0C; /* memcard BIOS event handles (8): per-slot */
extern long D_80075B10; /*   ReadEnd/WriteEnd/Done/Error for slot 1+2 */
extern long D_80075B14;
extern long D_80075B18;
extern long D_80075B1C;
extern long D_80075B20;
extern long D_80075B24;
extern long D_80075B28;

/* libmcrd session teardown: CloseEvent on the 8 BIOS event handles, bracketed
   in a critical section (only exited if this call actually entered it).
   Counterpart to MemCardInit. Called by MemCardCloseSession. (0x800680a4,
   184 bytes.) */
void MemCardTeardownEvents(void) {
  int entered = EnterCriticalSection();

  CloseEvent(D_80075B0C);
  CloseEvent(D_80075B10);
  CloseEvent(D_80075B14);
  CloseEvent(D_80075B18);
  CloseEvent(D_80075B1C);
  CloseEvent(D_80075B20);
  CloseEvent(D_80075B24);
  CloseEvent(D_80075B28);

  if (entered == 1) {
    ExitCriticalSection();
  }
}
