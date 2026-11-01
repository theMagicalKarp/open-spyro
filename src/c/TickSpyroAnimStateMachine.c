#include "funcs.h"
#include "globals.h"
#include "level_sfx.h"
#include "main_shared.h"
#include "sample_bank.h"
#include "spyro_block.h"
#include "spyro_state.h"

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
            g_abSpyroAnimDescTable[*prev * 4 + 3]) != 0) {
      if (*prev == 0x1A) {
        PlaySoundEffect(LEVEL_SFX(SFX_SPYRO_HURT_EXIT),
                        SPY_FROM_ANIM_PREV(prev, 0), SFX_MODE_PANNED, 0);
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
