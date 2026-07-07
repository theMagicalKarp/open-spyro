#include "globals.h"

/* Apply a matched cheat code (0x8002d810). 2 = jump to the credits (remembering
   the level to return to); 3 = unlock: mark levels 0..0x1F visited and set all
   six save-options slots to 2, then play the confirm sound; 4 = 99 lives. */
extern void BeginCreditsSequence(int param);
extern int PlaySoundEffect(unsigned int sample, int a, unsigned int b, void *c);

void ApplyCheatCodeEffect(int code) {
  unsigned char val;
  int n;
  unsigned char *p;
  unsigned char *q;
  switch (code) {
  case 2:
    g_nCreditsReturnLevelId = g_nActiveLevelId;
    BeginCreditsSequence(0);
    PlaySoundEffect(*(unsigned char *)g_pLevelSampleBankHeader, 0, 0x10, 0);
    break;
  case 3:
    val = 1;
    n = 0x1F;
    p = &g_abLevelVisitedFlag[0x1F];
    do {
      *p = val;
      n -= 1;
      p -= 1;
    } while (n >= 0);
    val = 2;
    n = 5;
    q = &g_abSaveOptionsBlock[5];
    do {
      *q = val;
      n -= 1;
      q -= 1;
    } while (n >= 0);
    PlaySoundEffect(*(unsigned char *)g_pLevelSampleBankHeader, 0, 0x10, 0);
    break;
  case 4:
    g_nExtraLives = 0x63;
    break;
  }
}
