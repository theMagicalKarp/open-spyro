#include "globals.h"

/* Reset all per-level / per-world progress state for a new game (0x80012604,
   376 bytes). Zeros the per-level gem/dragon/egg tallies and the visited/gem-%
   byte tables, resets the save-options block, clears the per-world persistent
   flags + per-level byte rows, then reinitializes the scalar inventory globals
   and FillWord-clears the spawn-anchor block and the level kill bitmaps. */
extern void FillWord(void *dst, unsigned int value, int byte_count);

extern int D_80075838;
extern int D_8007583C;

void ResetLevelState(void) {
  int i;
  int *eggs;
  int *dragons;
  int *gems;
  int n;
  unsigned char *p;
  unsigned char *row;
  int off;
  int *flags;
  unsigned char *q;
  int *anchor;
  int v;

  i = 0;
  eggs = g_anLevelEggsRecovered;
  dragons = g_anLevelDragonsRescued;
  gems = g_anLevelGemsCollected;
  do {
    *gems = 0;
    *dragons = 0;
    if (i < 0x12) {
      *eggs = 0;
    }
    eggs += 1;
    dragons += 1;
    g_abLevelVisitedFlag[i] = 0;
    g_abLevelGemPercent[i] = 0;
    i += 1;
    gems += 1;
  } while (i < 0x24);

  n = 5;
  p = &g_abSaveOptionsBlock[5];
  do {
    *p = 0;
    n -= 1;
    p -= 1;
  } while (n >= 0);
  g_abSaveOptionsBlock[0] = 2;

  row = g_abWorldPerLevelByteTable;
  off = 0;
  flags = g_anWorldPersistentFlags;
  do {
    *flags = 0;
    n = 4;
    q = row + 4;
    do {
      *q = 0;
      n -= 1;
      q -= 1;
    } while (n >= 0);
    row += 5;
    off += 5;
    flags += 1;
  } while (off < 0x1E);

  anchor = &g_nSpawnAnchorValid;
  v = 0;
  g_nLevelReadyFlag = 3;
  g_nExtraLives = 4;
  g_nWorldTreasureCollected = 0;
  g_nWorldDragonsRescued = 0;
  g_nWorldEggsRecovered = 0;
  g_nLevelGemsAtEntry = 0;
  g_nGemTallyIconCount = 0;
  g_nLevelEggCount = 0;
  g_nSparxFoodCount = 0;
  g_nLevelReadyFlagStash = -1;
  D_80075838 = 0;
  D_8007583C = 0;
  FillWord(anchor, v, 0x68);
  FillWord(g_anLevelKillBitmapTable, 0, 0x480);
}
