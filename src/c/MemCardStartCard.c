#include "globals.h"

extern void EnterCriticalSection(void);
extern void ExitCriticalSection(void);
extern void StartCARD2(void);
extern void ChangeClearPAD(long enable);

/* libmcrd StartCARD2 wrapper: start the memcard driver inside a critical
   section, then re-arm the pad's clear-callback. (0x800689b0, 56 bytes.) */
void MemCardStartCard(void) {
  EnterCriticalSection();
  StartCARD2();
  ChangeClearPAD(0);
  ExitCriticalSection();
}
