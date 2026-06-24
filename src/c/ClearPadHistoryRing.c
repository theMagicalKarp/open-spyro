#include "globals.h"

/* Zero the 16-slot pad-press history ring and reset its write index. */
void ClearPadHistoryRing(void) {
  int i;

  for (i = 15; i >= 0; i--) {
    g_anPadHistoryRing[i] = 0;
  }
  g_nPadHistoryIndex = 0;
}
