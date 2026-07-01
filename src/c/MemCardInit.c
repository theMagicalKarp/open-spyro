#include "globals.h"

extern int EnterCriticalSection(void);
extern void ExitCriticalSection(void);
extern long OpenEvent(unsigned long desc, long spec, long mode,
                      void (*handler)());
extern long EnableEvent(long ev);
extern void PollMemCardEvents(void);
extern void FUN_80067dd0();
extern void FUN_80067de4();
extern void FUN_80067df8();
extern void FUN_80067e0c();
extern void FUN_80067e20();
extern void FUN_80067e34();
extern void FUN_80067e48();
extern void FUN_80067e5c();
extern long D_80075B0C; /* memcard BIOS event handles (8): per-slot */
extern long D_80075B10; /*   ReadEnd/WriteEnd/Done/Error for slot 1+2 */
extern long D_80075B14;
extern long D_80075B18;
extern long D_80075B1C;
extern long D_80075B20;
extern long D_80075B24;
extern long D_80075B28;

/* libmcrd event setup: OpenEvent x8 (ReadEnd/WriteEnd/Done/Error for the two
   card descriptors) with per-event status-flag handlers, EnableEvent x8, then
   PollMemCardEvents to drain stale events. Bracketed in a critical section
   (only exited if this call actually entered it). Counterpart to
   MemCardTeardownEvents. (0x80067ea0, 484 bytes.) */
void MemCardInit(void) {
  int entered = EnterCriticalSection();

  D_80075B0C = OpenEvent(0xF4000001, 0x4, 0x1000, FUN_80067dd0);
  D_80075B10 = OpenEvent(0xF4000001, 0x8000, 0x1000, FUN_80067de4);
  D_80075B14 = OpenEvent(0xF4000001, 0x100, 0x1000, FUN_80067df8);
  D_80075B18 = OpenEvent(0xF4000001, 0x2000, 0x1000, FUN_80067e0c);
  D_80075B1C = OpenEvent(0xF0000011, 0x4, 0x1000, FUN_80067e20);
  D_80075B20 = OpenEvent(0xF0000011, 0x8000, 0x1000, FUN_80067e34);
  D_80075B24 = OpenEvent(0xF0000011, 0x100, 0x1000, FUN_80067e48);
  D_80075B28 = OpenEvent(0xF0000011, 0x2000, 0x1000, FUN_80067e5c);

  EnableEvent(D_80075B0C);
  EnableEvent(D_80075B10);
  EnableEvent(D_80075B14);
  EnableEvent(D_80075B18);
  EnableEvent(D_80075B1C);
  EnableEvent(D_80075B20);
  EnableEvent(D_80075B24);
  EnableEvent(D_80075B28);

  PollMemCardEvents();

  if (entered == 1) {
    ExitCriticalSection();
  }
}
