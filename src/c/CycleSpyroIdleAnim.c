#include "globals.h"

/* Advance Spyro's idle-anim sequence cursor to the next state whose animation
   mesh is actually loaded, then enter that state (0x80041558, 280 bytes).
   Walks g_anSpyroIdleAnimStateSeq (wrapping at 9) via
   g_abSpyroStateAnimIndexMap into g_apActorMeshTable (+0x38 per anim slot);
   if the walk comes back around to the starting cursor with nothing loaded,
   just rearms the idle timeout (0x2710) instead. */
extern void ChangeSpyroState(int state);

void CycleSpyroIdleAnim(void) {
  int *seq;
  unsigned char *map;
  char *mesh;
  int timeout;
  int cur;
  int start;

  seq = g_anSpyroIdleAnimStateSeq;
  map = g_abSpyroStateAnimIndexMap;
  start = g_nSpyroIdleAnimSeqCursor;
  mesh = g_apActorMeshTable;
  timeout = 0x2710;
  for (;;) {
    g_nSpyroIdleAnimSeqCursor += 1;
    if (g_nSpyroIdleAnimSeqCursor >= 9) {
      g_nSpyroIdleAnimSeqCursor = 0;
    }
    cur = g_nSpyroIdleAnimSeqCursor;
    if (cur == start) {
      if (*(int *)(mesh + *(unsigned char *)(seq[cur] + (int)map) * 4 + 0x38) ==
          0) {
        g_nSpyroIdleAnimTimeout = timeout;
        return;
      }
      break;
    }
    if (*(int *)(mesh + *(unsigned char *)(seq[cur] + (int)map) * 4 + 0x38) !=
        0) {
      break;
    }
  }
  ChangeSpyroState(g_anSpyroIdleAnimStateSeq[g_nSpyroIdleAnimSeqCursor]);
}
