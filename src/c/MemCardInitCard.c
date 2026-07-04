#include "globals.h"

extern void EnterCriticalSection(void);
extern void ExitCriticalSection(void);
extern void ChangeClearPAD(long enable);
extern int ReadInitPadFlag(void);
extern void InitCARD2(int state);

/* libmcrd InitCARD2 wrapper: re-arm the pad clear-callback, then init the
   memcard driver inside a critical section. If the pad flag was not yet set,
   the requested initial pad state is forced to 0. (0x80068958, 88 bytes.) */
void MemCardInitCard(int state) {
  ChangeClearPAD(0);
  EnterCriticalSection();
  if (ReadInitPadFlag() == 0) {
    state = 0;
  }
  InitCARD2(state);
  ExitCriticalSection();
}
