#include "funcs.h"
#include "globals.h"

/* Poll the pending memcard sync (0x80032a20): MemCardSync(1, &ring[4],
   &ring[5]). When it reports "in progress" (return 1) commit the finished entry
   into the ring slot indexed by g_nMemCardRingIndex, flip that index (1 - it),
   then hand the ring index (<<4) to the card-info op handler RunMemCardInfoOp.
 */
extern void RunMemCardInfoOp();
extern volatile int g_nMemCardRingIndex;
extern volatile int g_anMemCardSyncRing[];

void PollMemCardSyncRing(void) {
  int r = MemCardSync(1, &g_anMemCardSyncRing[4], &g_anMemCardSyncRing[5]);
  if (r == 0) {
    return;
  }
  if (r == 1) {
    g_anMemCardSyncRing[g_nMemCardRingIndex] = g_anMemCardSyncRing[5];
    g_nMemCardRingIndex = r - g_nMemCardRingIndex;
  }
  RunMemCardInfoOp(g_nMemCardRingIndex << 4);
}
