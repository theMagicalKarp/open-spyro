#include "globals.h"

/* Refresh the HUD level-gem counters (0x80054578): caches the collected
   count for the current level intro index and re-derives the displayed
   digits and the counter state bytes that sit just below the cache. */
extern void SetHudCounterDigits();
extern int g_anHudLevelGemCachedBlock[];

void func_80054578(void) {
  int *pc = g_anHudLevelGemCachedBlock;
  unsigned char *ps = ((unsigned char *)g_anHudLevelGemCachedBlock) - 0x20;
  unsigned char *pr = ((unsigned char *)g_anHudLevelGemCachedBlock) - 0x1B;
  unsigned char state;
  int gems = g_anLevelGemsCollected[g_nLevelIntroIndex];
  *pc = gems;
  SetHudCounterDigits(0, 4, gems, 1);
  /* the do/while(0) is load-bearing: it pins the state load below the
     SetHudCounterDigits call instead of letting it hoist into the arg block */
  do {
    state = *ps;
  } while (0);
  if (((state == 1) || (state == 2)) || (state == 4)) {
    *ps = 3;
    *pr = 0xD;
  }
}
