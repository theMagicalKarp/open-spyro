#include "funcs.h"
#include "globals.h"
#include "level_sfx.h"
#include "main_shared.h"
#include "model.h"
#include "pad_buttons.h"
#include "sample_bank.h"
#include "spyro_block.h"
#include "spyro_state.h"

/* 0x8003cb24: Adds delta to the sub-frame timer g_bSpyroSubFrameTimer. On
   overflow past 0xF, advances g_bSpyroAnimFrame by 1 within the current anim,
   wrapping back to the loop-start frame g_abSpyroAnimDescTable[anim*4] when it
   reaches the loop-end [anim*4+1]. Pure incremental step; does not change
   state (cf. StepSpyroAnimAndCommitTransition at 0x8003cbb8). */
void AdvanceSpyroAnimFrame(int delta) {
  unsigned char *timer = g_abSpyroSubFrameTimerBlock;
  unsigned char frame;
  unsigned char anim;
  unsigned char newFrame;
  unsigned char loopEnd;
  int idx;

  *timer = *timer + delta;
  if (*timer >= 0x10) {
    *timer = *timer - 0x10;

    frame = g_bSpyroAnimFrame;
    anim = g_bSpyroAnimCurrent;
    newFrame = frame + 1;
    g_bSpyroAnimPrev = anim;
    idx = anim * 4;
    g_bSpyroFramePrev = frame;
    g_bSpyroAnimFrame = newFrame;

    loopEnd = g_abSpyroAnimDescTable[idx + SPYRO_ANIM_DESC_LOOP_END];
    if (newFrame >= loopEnd) {
      g_bSpyroAnimFrame = g_abSpyroAnimDescTable[idx];
    }
  }
}

/* Advance Spyro's animation sub-frame timer by `step`; when a whole frame
   (0x10 sub-steps) elapses, advance the frame counter and, past the current
   animation's frame count, commit the pending state-transition animation
   (0x8003cbb8, 0x12C). Returns 1 when a transition was committed, else 0. */

int StepSpyroAnimAndCommitTransition(int step) {
  unsigned char *timer = g_abSpyroSubFrameTimerBlock;
  unsigned char frame;
  int cur;
  int next;
  int state;
  int prev;
  int sum;
  unsigned char *idx;
  int wrap;
  sum = timer[0] + step;
  timer[0] = sum;
  wrap = 0x10;
  if (((unsigned int)(sum & 0xFF)) >= wrap) {
    timer[0] = sum - wrap;
    /* The do / while (0) ends cse's extended basic block (A200): the
       match needs the block boundary, not the loop. */
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

/* 0x8003cce4 (0x4b0): applies the pending Spyro state change. The restart
   style comes from the transition matrix entry for (prev state, state):
   most styles swap in the new state's default anim (g_abSpyroStateAnimIndexMap)
   at a given frame / sub-frame phase and set the anim mode that
   TickSpyroAnimStateMachine then runs; style 4 holds the old anim's
   loop-end frame (with a landing rumble out of the hard-fall SPYRO_ST_FALL),
   style 2 waits for the current anim to reach its loop end. */
void ApplySpyroStateTransition(void) {
  switch (
      g_abSpyroStateTransitionMatrixBlock[g_nSpyroPrevState][g_nSpyroState]) {
  case SPYRO_ANIM_BLEND_IN: {
    unsigned char cur;
    unsigned char frame;
    unsigned char next;
    int state;

    cur = g_bSpyroAnimCurrent;
    frame = g_bSpyroAnimFrame;
    state = g_nSpyroState;
    g_nSpyroAnimMode = SPYRO_ANIM_BLEND_IN;
    g_bSpyroAnimPrev = cur;
    g_bSpyroFramePrev = frame;
    next = g_abSpyroStateAnimIndexMap[state];
    g_bSpyroAnimFrame = 0;
    g_bSpyroSubFrameTimer = 2;
    g_nSpyroPrevState = state;
    g_bSpyroAnimCurrent = next;
  } break;
  case SPYRO_ANIM_BLEND_FAST: {
    unsigned char cur;
    unsigned char frame;
    unsigned char next;
    int state;

    cur = g_bSpyroAnimCurrent;
    frame = g_bSpyroAnimFrame;
    state = g_nSpyroState;
    g_nSpyroAnimMode = SPYRO_ANIM_BLEND_FAST;
    g_bSpyroAnimPrev = cur;
    g_bSpyroFramePrev = frame;
    next = g_abSpyroStateAnimIndexMap[state];
    g_bSpyroAnimFrame = 0;
    g_bSpyroSubFrameTimer = 4;
    g_nSpyroPrevState = state;
    g_bSpyroAnimCurrent = next;
  } break;
  case SPYRO_ANIM_LOOP_WRAP: {
    unsigned char *pf;
    unsigned char cur;
    unsigned char frame;
    int idx;

    pf = g_bSpyroAnimFrame_blk;
    cur = g_bSpyroAnimCurrent;
    frame = *pf;
    idx = cur * 4;
    if (frame >= g_abSpyroAnimDescTable[idx + SPYRO_ANIM_DESC_LOOP_END]) {
      g_nSpyroAnimMode = SPYRO_ANIM_LOOP_WRAP;
      g_bSpyroAnimPrev = cur;
      g_bSpyroFramePrev = frame;
      frame = g_abSpyroAnimDescTable[idx];
      g_bSpyroSubFrameTimer = 2;
      *pf = frame;
    } else {
      g_nSpyroAnimMode = SPYRO_ANIM_PLAY;
    }
  } break;
  case SPYRO_ANIM_BLEND_KEEP_FRAME: {
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
    if (g_bSpyroAnimFrame >=
        g_abSpyroAnimDescTable[idx + SPYRO_ANIM_DESC_LOOP_END]) {
      g_bSpyroAnimFrame = g_abSpyroAnimDescTable[idx];
    }
    g_bSpyroSubFrameTimer = 4;
    g_nSpyroAnimMode = SPYRO_ANIM_BLEND_KEEP_FRAME;
    g_nSpyroPrevState = state;
  } break;
  case SPYRO_ANIM_FINISH:
  case SPYRO_ANIM_FINISH_10:
    g_nSpyroAnimMode = SPYRO_ANIM_FINISH;
    break;
  case SPYRO_ANIM_HOLD_END: {
    unsigned char next;
    int state;

    g_bSpyroAnimPrev = g_bSpyroAnimCurrent;
    g_bSpyroFramePrev = g_bSpyroAnimFrame;
    if (g_nSpyroPrevState == SPYRO_ST_FALL && g_nSpyroPrevStateTimer < 0x18) {
      state = g_nSpyroState;
      g_nSpyroAnimMode = SPYRO_ANIM_BLEND_IN;
      next = g_abSpyroStateAnimIndexMap[state];
      g_bSpyroAnimFrame = 0;
      g_bSpyroSubFrameTimer = 2;
      g_nSpyroPrevState = state;
      g_bSpyroAnimCurrent = next;
      break;
    }
    {
      unsigned char end =
          SPYRO_ANIM_DESC(g_bSpyroAnimCurrent, SPYRO_ANIM_DESC_LOOP_END);

      g_bSpyroSubFrameTimer = 4;
      g_nSpyroAnimMode = SPYRO_ANIM_HOLD_END;
      g_bSpyroAnimFrame = end;
    }
    if (g_nSpyroPrevState == SPYRO_ST_FALL) {
      g_nPulseRumbleAmount = LAND_RUMBLE_AMOUNT;
      if (g_nPulseRumbleTimer < LAND_RUMBLE_FRAMES) {
        g_nPulseRumbleTimer = LAND_RUMBLE_FRAMES;
      }
    }
  } break;
  case SPYRO_ANIM_BLEND_LOOP_START: {
    unsigned char cur;
    unsigned char frame;
    unsigned char next;
    unsigned char first;
    int state;

    cur = g_bSpyroAnimCurrent;
    frame = g_bSpyroAnimFrame;
    state = g_nSpyroState;
    g_nSpyroAnimMode = SPYRO_ANIM_BLEND_LOOP_START;
    g_bSpyroAnimPrev = cur;
    g_bSpyroFramePrev = frame;
    next = g_abSpyroStateAnimIndexMap[state];
    g_bSpyroAnimCurrent = next;
    first = SPYRO_ANIM_DESC(next, SPYRO_ANIM_DESC_LOOP_START);
    g_bSpyroSubFrameTimer = 2;
    g_nSpyroPrevState = state;
    g_bSpyroAnimFrame = first;
  } break;
  case SPYRO_ANIM_CUT: {
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
    g_nSpyroAnimMode = SPYRO_ANIM_PLAY;
    g_nSpyroPrevState = state;
    g_bSpyroAnimCurrent = next;
  } break;
  case SPYRO_ANIM_CUT_LOOP_START: {
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
    g_nSpyroAnimMode = SPYRO_ANIM_PLAY;
    g_nSpyroPrevState = state;
    g_bSpyroAnimFrame = first + 1;
  } break;
  }
}

/* 0x8003d194 (0x224): per-frame Spyro animation state-machine tick, routed on
   g_nSpyroAnimMode. Modes 1/2/6 and 5 run the sub-frame timer at +2/+4 and
   drop back to mode 0 on overflow; 3/10 step a pending transition when the
   transition-anim matrix entry is 3/4/10 (SFX_SPYRO_HURT_EXIT when
   anim 0x1A, SPYRO_ST_HURT_PINNED's, ends); 4 waits
   for timer overflow then enters mode 3. */
void TickSpyroAnimStateMachine(void) {
  unsigned char *timer;
  unsigned char *prev;
  unsigned char m;

  switch (g_nSpyroAnimMode) {
  case SPYRO_ANIM_PLAY:
    if (g_nSpyroState == g_nSpyroPrevState) {
      AdvanceSpyroAnimFrame(g_nSpyroAnimPlayRate);
      break;
    }
    goto transition;
  case SPYRO_ANIM_BLEND_IN:
  case SPYRO_ANIM_LOOP_WRAP:
  case SPYRO_ANIM_BLEND_LOOP_START:
    if ((g_abSpyroSubFrameTimerBlock[0] += 2) >= 0x10) {
      AdvanceSpyroAnimFrame(0);
      g_nSpyroAnimMode = SPYRO_ANIM_PLAY;
    }
    break;
  case SPYRO_ANIM_BLEND_KEEP_FRAME:
    if ((g_abSpyroSubFrameTimerBlock[0] += 4) >= 0x10) {
      AdvanceSpyroAnimFrame(0);
      g_nSpyroAnimMode = SPYRO_ANIM_PLAY;
    }
    break;
  case SPYRO_ANIM_BLEND_FAST:
    AdvanceSpyroAnimFrame(2);
    g_nSpyroAnimMode = SPYRO_ANIM_PLAY;
    break;
  case SPYRO_ANIM_FINISH:
  case SPYRO_ANIM_FINISH_10:
    m = g_abSpyroStateTransitionMatrixBlock[g_nSpyroPrevState][g_nSpyroState];
    if (m == SPYRO_ANIM_FINISH || m == SPYRO_ANIM_FINISH_10 ||
        m == SPYRO_ANIM_HOLD_END) {
      goto step;
    }
  transition:
    ApplySpyroStateTransition();
    break;
  step:
    prev = g_abSpyroAnimPrevBlock;
    if (StepSpyroAnimAndCommitTransition(
            SPYRO_ANIM_DESC(*prev, SPYRO_ANIM_DESC_RATE)) != 0) {
      if (*prev == SPYRO_ANIMID_HURT_PINNED) {
        PlaySoundEffect(LEVEL_SFX(SFX_SPYRO_HURT_EXIT),
                        (Actor *)SPY_FROM_ANIM_PREV(prev, 0), SFX_MODE_PANNED,
                        0);
      }
      g_nSpyroAnimMode = SPYRO_ANIM_PLAY;
    }
    break;
  case SPYRO_ANIM_HOLD_END:
    timer = g_abSpyroSubFrameTimerBlock;
    *timer += g_nSpyroAnimPlayRate;
    if (*timer >= 0x10) {
      *timer = 0;
      g_nSpyroAnimMode = SPYRO_ANIM_FINISH;
      g_bSpyroAnimPrev = g_bSpyroAnimCurrent;
      g_bSpyroFramePrev = g_bSpyroAnimFrame;
      g_bSpyroAnimFrame++;
    }
    break;
  }
}

/* 0x8003fdc8 (0x78) — switch Spyro to `state` and restart his animation from
   that state's default clip: frame 1, sub-frame timer and anim mode cleared,
   the new state recorded as the previous one, and both the previous and
   current clip set from g_abSpyroStateAnimIndexMap.

   The clip table is re-read for each copy: a byte store may alias the byte
   table (sched.c true/anti_dependence exempt QImode from the in-struct
   escape), so each read is pinned between the byte stores around it. */
void RestartSpyroAnimWithState(int state) {
  int prev;
  int anim;

  ChangeSpyroState(state);
  prev = g_abSpyroStateAnimIndexMap[g_nSpyroState];
  g_bSpyroFramePrev = 0;
  g_bSpyroAnimPrev = prev;
  anim = g_abSpyroStateAnimIndexMap[g_nSpyroState];
  g_bSpyroAnimFrame = 1;
  g_bSpyroSubFrameTimer = 0;
  g_nSpyroAnimMode = SPYRO_ANIM_PLAY;
  g_nSpyroPrevState = g_nSpyroState;
  g_bSpyroAnimCurrent = anim;
}

/* Advance Spyro's idle-anim sequence cursor to the next state whose animation
   mesh is actually loaded, then enter that state (0x80041558, 280 bytes).
   Walks g_anSpyroIdleAnimStateSeq (wrapping at 9) via
   g_abSpyroStateAnimIndexMap into Spyro's model (g_apActorMeshTable[0])
   animations; if the walk comes back around to the starting cursor with nothing
   loaded, just rearms the idle timeout (0x2710) instead. */

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
      if (((Model *)mesh)
              ->m_Animations[*(unsigned char *)(seq[cur] + (int)map)] == 0) {
        g_nSpyroIdleAnimTimeout = timeout;
        return;
      }
      break;
    }
    if (((Model *)mesh)
            ->m_Animations[*(unsigned char *)(seq[cur] + (int)map)] != 0) {
      break;
    }
  }
  ChangeSpyroState(g_anSpyroIdleAnimStateSeq[g_nSpyroIdleAnimSeqCursor]);
}

/* 0x800495d8 */
void AdvanceSpyroAnimLayer1Frame(int step) {
  unsigned char *sub_ptr = g_abSpyroAnimLayer1SubstepBlock;
  unsigned char frame;
  unsigned char anim;
  unsigned char newFrame;
  unsigned char len;

  *sub_ptr = *sub_ptr + step;
  if (*sub_ptr >= 0x10) {
    *sub_ptr = *sub_ptr - 0x10;

    frame = g_bSpyroAnimLayer1Frame;
    anim = g_bSpyroAnimLayer1Current;
    newFrame = frame + 1;
    g_bSpyroAnimLayer1Prev = anim;
    g_bSpyroAnimLayer1FramePrev = frame;
    g_bSpyroAnimLayer1Frame = newFrame;

    len = g_abSpyroAnimDescTable[anim * 4 + SPYRO_ANIM_DESC_FRAMES];
    if (newFrame >= len) {
      g_bSpyroAnimLayer1Frame = len - 1;
    }
  }
}

extern unsigned char g_bSpyroAnimLayer1Substep;

/* g_anSpyroFlamePhaseBlock[0], and the g_abSpyroFlameTransitionModes entry
 * that starts a phase on a flame-state change. The blend timer
 * (g_bSpyroAnimLayer1Substep) runs to 0x10. */
enum FlameLayerPhase {
  FLAME_LAYER_TRACK = 0, /* follows g_nSpyroFlameState */
  FLAME_LAYER_BLEND = 1, /* blends in from the body's pose, 2 per tick from 2 */
  FLAME_LAYER_SWITCH =
      2, /* switches to the state's flame anim, 4 per tick from 4 */
};

/* 0x80049660 — per-frame layer-1 (flame-breath overlay) anim driver: phase 0
   tracks the flame-breath state (snapshot the primary anim bytes when idle,
   advance the layer frame otherwise, or start a blend/hard-switch phase from
   the transition table on a state change); phases 1/2 run the blend timer and
   drop back to phase 0 when it expires. */
void TickSpyroAnimLayer1(void) {
  int state;
  int mode;
  int timer;
  unsigned char prev;
  unsigned char framePrev;
  unsigned char cur;
  unsigned char frame;
  unsigned char sub;
  unsigned char oldAnim;
  unsigned char oldFrame;
  unsigned char newAnim;
  unsigned char blendAnim;
  unsigned char blendFrame;
  unsigned char blendCur;
  unsigned char blendCurFrame;

  switch (g_anSpyroFlamePhaseBlock[0]) {
  case FLAME_LAYER_TRACK:
    state = g_nSpyroFlameState;
    if (state ==
        g_anSpyroFlamePhaseBlock[SPY_FLAME_LAYER1_STATE - SPY_FLAME_PHASE]) {
      if (state == 0) {
        /* volatile accesses pin the original's load-all-then-store-all
           schedule (the -g3 scheduler otherwise pairs each store with
           its load). */
        prev = *(volatile byte *)&g_bSpyroAnimPrev;
        framePrev = *(volatile byte *)&g_bSpyroFramePrev;
        cur = *(volatile byte *)&g_bSpyroAnimCurrent;
        frame = *(volatile byte *)&g_bSpyroAnimFrame;
        sub = *(volatile byte *)&g_bSpyroSubFrameTimer;
        *(volatile unsigned char *)&g_bSpyroAnimLayer1Prev = prev;
        *(volatile unsigned char *)&g_bSpyroAnimLayer1FramePrev = framePrev;
        *(volatile unsigned char *)&g_bSpyroAnimLayer1Current = cur;
        *(volatile unsigned char *)&g_bSpyroAnimLayer1Frame = frame;
        *(volatile unsigned char *)&g_bSpyroAnimLayer1Substep = sub;
      } else {
        AdvanceSpyroAnimLayer1Frame(g_nSpyroFlameAnimRate);
      }
    } else {
      mode = g_abSpyroFlameTransitionModes[g_nSpyroFlameLayer1State][state];
      switch (mode) {
      case FLAME_LAYER_SWITCH:
        oldAnim = *(volatile byte *)&g_bSpyroAnimLayer1Current;
        oldFrame = *(volatile byte *)&g_bSpyroAnimLayer1Frame;
        g_anSpyroFlamePhaseBlock[0] = mode;
        *(volatile unsigned char *)&g_bSpyroAnimLayer1Prev = oldAnim;
        *(volatile unsigned char *)&g_bSpyroAnimLayer1FramePrev = oldFrame;
        newAnim = g_abSpyroFlameAnimTable[state];
        *(volatile unsigned char *)&g_bSpyroAnimLayer1Frame = 0;
        *(volatile unsigned char *)&g_bSpyroAnimLayer1Substep = 4;
        g_anSpyroFlamePhaseBlock[SPY_FLAME_LAYER1_STATE - SPY_FLAME_PHASE] =
            state;
        *(volatile unsigned char *)&g_bSpyroAnimLayer1Current = newAnim;
        break;
      case FLAME_LAYER_BLEND:
        *(volatile int *)&g_anSpyroFlamePhaseBlock[0] = mode;
        blendAnim = *(volatile byte *)&g_bSpyroAnimLayer1Current;
        blendFrame = *(volatile byte *)&g_bSpyroAnimLayer1Frame;
        blendCur = *(volatile byte *)&g_bSpyroAnimCurrent;
        blendCurFrame = *(volatile byte *)&g_bSpyroAnimFrame;
        *(volatile unsigned char *)&g_bSpyroAnimLayer1Substep = 2;
        g_anSpyroFlamePhaseBlock[SPY_FLAME_LAYER1_STATE - SPY_FLAME_PHASE] =
            state;
        *(volatile unsigned char *)&g_bSpyroAnimLayer1Prev = blendAnim;
        *(volatile unsigned char *)&g_bSpyroAnimLayer1FramePrev = blendFrame;
        *(volatile unsigned char *)&g_bSpyroAnimLayer1Current = blendCur;
        *(volatile unsigned char *)&g_bSpyroAnimLayer1Frame = blendCurFrame;
        break;
      }
    }
    break;
  case FLAME_LAYER_SWITCH:
    timer = g_bSpyroAnimLayer1Substep + 4;
    g_bSpyroAnimLayer1Substep = timer;
    if ((unsigned char)timer >= 0x10) {
      AdvanceSpyroAnimLayer1Frame(0);
      g_anSpyroFlamePhaseBlock[0] = FLAME_LAYER_TRACK;
    }
    break;
  case FLAME_LAYER_BLEND:
    timer = g_bSpyroAnimLayer1Substep + 2;
    g_bSpyroAnimLayer1Substep = timer;
    if ((unsigned char)timer >= 0x10) {
      AdvanceSpyroAnimLayer1Frame(0);
      g_anSpyroFlamePhaseBlock[0] = FLAME_LAYER_TRACK;
    }
    break;
  }
}

extern unsigned char g_abSpyroAnimLayer2SubstepBlock[]; /* layer-2 substep
   accumulator — incomplete array so the base address is materialized once and
   held (single `a1` across the load + both stores), matching the original. */

/* 0x80049dfc — layer-2 sibling of AdvanceSpyroAnimLayer1Frame: accumulate
   sub-frame steps; every 0x10 substeps advance the layer-2 anim frame,
   looping back to the anim's start frame at its loop point. */
void AdvanceSpyroAnimLayer2Frame(int step) {
  unsigned char *sub_ptr = g_abSpyroAnimLayer2SubstepBlock;
  int sum;
  int off;
  unsigned char frame;
  unsigned char anim;
  unsigned char newFrame;

  sum = *sub_ptr + step;
  *sub_ptr = sum;
  if ((unsigned char)sum >= 0x10) {
    *sub_ptr = sum - 0x10;

    frame = g_bSpyroAnimLayer2Frame;
    anim = g_bSpyroAnimLayer2Current;
    newFrame = frame + 1;
    g_bSpyroAnimLayer2Prev = anim;
    off = anim * 4;
    g_bSpyroAnimLayer2FramePrev = frame;
    g_bSpyroAnimLayer2Frame = newFrame;

    if (newFrame == g_abSpyroAnimDescTable[off + SPYRO_ANIM_DESC_LOOP_END]) {
      g_bSpyroAnimLayer2Frame = g_abSpyroAnimDescTable[off];
    }
  }
}

extern unsigned char g_bSpyroAnimLayer2Substep;

/* 0x80049e8c — per-frame layer-2 anim driver: while the gate counter is
   synced, either seed the layer-2 anim state from Spyro's layer-1 state
   (gate == 0) or advance the layer-2 frame. */
void TickSpyroAnimLayer2(void) {
  unsigned char prev;
  unsigned char framePrev;
  unsigned char cur;
  unsigned char frame;
  unsigned char sub;

  if (g_nSpyroAnimLayer2Phase == 0) {
    if (g_nSpyroAnimLayer2Request == g_nSpyroAnimLayer2Active) {
      if (g_nSpyroAnimLayer2Request == 0) {
        /* volatile accesses pin the original's load-all-then-store-all
           schedule (the -g3 scheduler otherwise pairs each store with
           its load). */
        prev = *(volatile byte *)&g_bSpyroAnimPrev;
        framePrev = *(volatile byte *)&g_bSpyroFramePrev;
        cur = *(volatile byte *)&g_bSpyroAnimCurrent;
        frame = *(volatile byte *)&g_bSpyroAnimFrame;
        sub = *(volatile byte *)&g_bSpyroSubFrameTimer;
        *(volatile unsigned char *)&g_bSpyroAnimLayer2Prev = prev;
        *(volatile unsigned char *)&g_bSpyroAnimLayer2FramePrev = framePrev;
        *(volatile unsigned char *)&g_bSpyroAnimLayer2Current = cur;
        *(volatile unsigned char *)&g_bSpyroAnimLayer2Frame = frame;
        *(volatile unsigned char *)&g_bSpyroAnimLayer2Substep = sub;
      } else {
        AdvanceSpyroAnimLayer2Frame(g_nSpyroAnimLayer2Rate);
      }
    }
  }
}

/* Layer-2 anim gate timer. SPYRO_ANIM_NO_OVERLAY (the per-anim bytes after
   the g_abSpyroAnimDescTable records) marks anims that keep the overlay
   layers off; if neither the previous nor the current Spyro anim blocks layer
   2, the permissive-duration counter advances, otherwise the active layer-2
   anim is force-cancelled. (0x80049f3c, 112 bytes.) */
void TickSpyroAnimLayer2GateTimer(void) {
  if (SPYRO_ANIM_NO_OVERLAY(g_bSpyroAnimPrev) ||
      SPYRO_ANIM_NO_OVERLAY(g_bSpyroAnimCurrent)) {
    g_nSpyroAnimLayer2Request = 0;
  } else {
    g_nSpyroAnimLayer2GateFrames++;
  }
}
