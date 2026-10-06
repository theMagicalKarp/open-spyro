#include "globals.h"

extern void AdvanceSpyroAnimFrame(int delta);
extern int StepSpyroAnimAndCommitTransition(int step);
extern void ApplySpyroStateTransition(void);
extern int PlaySoundEffect(unsigned int id, void *owner, unsigned int mode,
                           void *pos);

extern unsigned char g_abSpyroSubFrameTimerBlock[];
extern unsigned char g_abSpyroAnimPrevBlock[];
extern unsigned char g_abSpyroStateTransitionMatrixBlock[45][45];

/* 0x8003d194 (0x224): per-frame Spyro animation state-machine tick, routed on
   g_nSpyroAnimMode. Modes 1/2/6 and 5 run the sub-frame timer at +2/+4 and
   drop back to mode 0 on overflow; 3/10 step a pending transition when the
   transition-anim matrix entry is 3/4/10 (landing sfx for anim 0x1a); 4 waits
   for timer overflow then enters mode 3. */
void TickSpyroAnimStateMachine(void) {
  unsigned char *timer;
  unsigned char *prev;
  unsigned char m;

  switch (g_nSpyroAnimMode) {
  case 0:
    if (g_nSpyroState == g_nSpyroPrevState) {
      AdvanceSpyroAnimFrame(g_nSpyroAnimPlayRate);
      break;
    }
    goto transition;
  case 1:
  case 2:
  case 6:
    if ((g_abSpyroSubFrameTimerBlock[0] += 2) >= 0x10) {
      AdvanceSpyroAnimFrame(0);
      g_nSpyroAnimMode = 0;
    }
    break;
  case 5:
    if ((g_abSpyroSubFrameTimerBlock[0] += 4) >= 0x10) {
      AdvanceSpyroAnimFrame(0);
      g_nSpyroAnimMode = 0;
    }
    break;
  case 8:
    AdvanceSpyroAnimFrame(2);
    g_nSpyroAnimMode = 0;
    break;
  case 3:
  case 10:
    m = g_abSpyroStateTransitionMatrixBlock[g_nSpyroPrevState][g_nSpyroState];
    if (m == 3 || m == 10 || m == 4) {
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
        PlaySoundEffect(((unsigned char *)g_pLevelSampleBankHeader)[0x1B],
                        prev - 0x18, 0x10, 0);
      }
      g_nSpyroAnimMode = 0;
    }
    break;
  case 4:
    timer = g_abSpyroSubFrameTimerBlock;
    *timer += g_nSpyroAnimPlayRate;
    if (*timer >= 0x10) {
      *timer = 0;
      g_nSpyroAnimMode = 3;
      g_bSpyroAnimPrev = g_bSpyroAnimCurrent;
      g_bSpyroFramePrev = g_bSpyroAnimFrame;
      g_bSpyroAnimFrame++;
    }
    break;
  }
}
