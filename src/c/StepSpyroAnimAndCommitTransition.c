#include "globals.h"

/* Advance Spyro's animation sub-frame timer by `step`; when a whole frame
   (0x10 sub-steps) elapses, advance the frame counter and, past the current
   animation's frame count, commit the pending state-transition animation
   (0x8003cbb8, 0x12C). Returns 1 when a transition was committed, else 0. */

extern unsigned char g_abSpyroSubFrameTimerBlock[];
extern unsigned char g_abSpyroStateTransitionMatrixBlock[45][45];

int StepSpyroAnimAndCommitTransition(int step) {
  unsigned char *timer = g_abSpyroSubFrameTimerBlock;
  unsigned char frame;
  int cur;
  int next;
  int state;
  int prev;
  int sum;
  unsigned char *idx;
  int new_var;
  sum = timer[0] + step;
  timer[0] = sum;
  new_var = 0x10;
  if (((unsigned int)(sum & 0xFF)) >= new_var) {
    timer[0] = sum - new_var;
    do {
      frame = g_bSpyroAnimFrame;
      cur = g_bSpyroAnimCurrent;
      next = frame + 1;
      g_bSpyroAnimPrev = cur;
    } while (0);
    cur <<= 2;
    g_bSpyroFramePrev = frame;
    g_bSpyroAnimFrame = next;
    if (((unsigned int)(next & 0xFF)) < g_abSpyroAnimDescTable[cur + 2]) {
      return 0;
    }
    state = g_nSpyroState;
    idx = (unsigned char *)(state + ((int)g_abSpyroStateAnimIndexMap));
    *((volatile unsigned char *)(&g_bSpyroAnimCurrent)) = idx[0];
    prev = *((volatile int *)(&g_nSpyroPrevState));
    if (g_abSpyroStateTransitionMatrixBlock[prev][state] == 0xA) {
      g_bSpyroAnimFrame = g_abSpyroAnimDescTable[idx[0] << 2];
    } else {
      g_bSpyroAnimFrame = 1;
      timer[0] = 4;
    }
    g_nSpyroPrevState = *((volatile int *)(&g_nSpyroState));
    return 1;
  }
  return 0;
}
