#include "globals.h"

extern unsigned char g_abSpyroStateTransitionMatrixBlock[45][45];
extern unsigned char
    g_bSpyroAnimFrame_blk[]; /* held-base view of g_bSpyroAnimFrame */

/* 0x8003cce4 (0x4b0): applies the pending Spyro state change. The restart
   style comes from the transition matrix entry for (prev state, state):
   most styles swap in the new state's default anim (g_abSpyroStateAnimIndexMap)
   at a given frame / sub-frame phase and set the anim mode that
   TickSpyroAnimStateMachine then runs; style 4 holds the old anim's
   loop-end frame (with a landing rumble out of the hard-fall state 6),
   style 2 waits for the current anim to reach its loop end. */
void ApplySpyroStateTransition(void) {
  switch (
      g_abSpyroStateTransitionMatrixBlock[g_nSpyroPrevState][g_nSpyroState]) {
  case 1: {
    unsigned char cur;
    unsigned char frame;
    unsigned char next;
    int state;

    cur = g_bSpyroAnimCurrent;
    frame = g_bSpyroAnimFrame;
    state = g_nSpyroState;
    g_nSpyroAnimMode = 1;
    g_bSpyroAnimPrev = cur;
    g_bSpyroFramePrev = frame;
    next = g_abSpyroStateAnimIndexMap[state];
    g_bSpyroAnimFrame = 0;
    g_bSpyroSubFrameTimer = 2;
    g_nSpyroPrevState = state;
    g_bSpyroAnimCurrent = next;
  } break;
  case 8: {
    unsigned char cur;
    unsigned char frame;
    unsigned char next;
    int state;

    cur = g_bSpyroAnimCurrent;
    frame = g_bSpyroAnimFrame;
    state = g_nSpyroState;
    g_nSpyroAnimMode = 8;
    g_bSpyroAnimPrev = cur;
    g_bSpyroFramePrev = frame;
    next = g_abSpyroStateAnimIndexMap[state];
    g_bSpyroAnimFrame = 0;
    g_bSpyroSubFrameTimer = 4;
    g_nSpyroPrevState = state;
    g_bSpyroAnimCurrent = next;
  } break;
  case 2: {
    unsigned char *pf;
    unsigned char cur;
    unsigned char frame;
    int idx;

    pf = g_bSpyroAnimFrame_blk;
    cur = g_bSpyroAnimCurrent;
    frame = *pf;
    idx = cur * 4;
    if (frame >= g_abSpyroAnimDescTable[idx + 1]) {
      g_nSpyroAnimMode = 2;
      g_bSpyroAnimPrev = cur;
      g_bSpyroFramePrev = frame;
      frame = g_abSpyroAnimDescTable[idx];
      g_bSpyroSubFrameTimer = 2;
      *pf = frame;
    } else {
      g_nSpyroAnimMode = 0;
    }
  } break;
  case 5: {
    unsigned char cur;
    unsigned char frame;
    unsigned char next;
    int state;
    int idx;

    cur = g_bSpyroAnimCurrent;
    frame = g_bSpyroAnimFrame;
    state = g_nSpyroState;
    g_bSpyroAnimPrev = cur;
    g_bSpyroFramePrev = frame;
    next = g_abSpyroStateAnimIndexMap[state];
    g_bSpyroAnimFrame = frame + 1;
    g_bSpyroAnimCurrent = next;
    idx = next * 4;
    if (g_bSpyroAnimFrame >= g_abSpyroAnimDescTable[idx + 1]) {
      g_bSpyroAnimFrame = g_abSpyroAnimDescTable[idx];
    }
    g_bSpyroSubFrameTimer = 4;
    g_nSpyroAnimMode = 5;
    g_nSpyroPrevState = state;
  } break;
  case 3:
  case 10:
    g_nSpyroAnimMode = 3;
    break;
  case 4: {
    unsigned char next;
    int state;

    g_bSpyroAnimPrev = g_bSpyroAnimCurrent;
    g_bSpyroFramePrev = g_bSpyroAnimFrame;
    if (g_nSpyroPrevState == 6 && g_nSpyroPrevStateTimer < 0x18) {
      state = g_nSpyroState;
      g_nSpyroAnimMode = 1;
      next = g_abSpyroStateAnimIndexMap[state];
      g_bSpyroAnimFrame = 0;
      g_bSpyroSubFrameTimer = 2;
      g_nSpyroPrevState = state;
      g_bSpyroAnimCurrent = next;
      break;
    }
    {
      unsigned char end = g_abSpyroAnimDescTable[g_bSpyroAnimCurrent * 4 + 1];

      g_bSpyroSubFrameTimer = 4;
      g_nSpyroAnimMode = 4;
      g_bSpyroAnimFrame = end;
    }
    if (g_nSpyroPrevState == 6) {
      g_nPulseRumbleAmount = 0xA0;
      if (g_nPulseRumbleTimer < 15) {
        g_nPulseRumbleTimer = 15;
      }
    }
  } break;
  case 6: {
    unsigned char cur;
    unsigned char frame;
    unsigned char next;
    unsigned char first;
    int state;

    cur = g_bSpyroAnimCurrent;
    frame = g_bSpyroAnimFrame;
    state = g_nSpyroState;
    g_nSpyroAnimMode = 6;
    g_bSpyroAnimPrev = cur;
    g_bSpyroFramePrev = frame;
    next = g_abSpyroStateAnimIndexMap[state];
    g_bSpyroAnimCurrent = next;
    first = g_abSpyroAnimDescTable[next * 4];
    g_bSpyroSubFrameTimer = 2;
    g_nSpyroPrevState = state;
    g_bSpyroAnimFrame = first;
  } break;
  case 7: {
    unsigned char *map;
    unsigned char first;
    unsigned char next;
    int state;

    state = g_nSpyroState;
    map = &g_abSpyroStateAnimIndexMap[state];
    first = *map;
    g_bSpyroFramePrev = 0;
    g_bSpyroAnimPrev = first;
    next = *map;
    g_bSpyroAnimFrame = 1;
    g_bSpyroSubFrameTimer = 0;
    g_nSpyroAnimMode = 0;
    g_nSpyroPrevState = state;
    g_bSpyroAnimCurrent = next;
  } break;
  case 11: {
    unsigned char *map;
    unsigned char next;
    unsigned char first;
    int idx;
    int state;

    state = g_nSpyroState;
    map = &g_abSpyroStateAnimIndexMap[state];
    next = *map;
    g_bSpyroAnimPrev = next;
    idx = next * 4;
    g_bSpyroFramePrev = g_abSpyroAnimDescTable[idx];
    g_bSpyroAnimCurrent = *map;
    first = g_abSpyroAnimDescTable[idx];
    g_bSpyroSubFrameTimer = 0;
    g_nSpyroAnimMode = 0;
    g_nSpyroPrevState = state;
    g_bSpyroAnimFrame = first + 1;
  } break;
  }
}
