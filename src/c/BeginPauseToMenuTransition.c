#include "globals.h"

extern void StopAllSoundExceptMask(int mask);
extern void ResetSpyroEntity(int param);
extern void ChangeSpyroState(int state);

/* Leave gameplay for the pause/menu gamestate (0xA): kill all sound, reset the
   pause-menu frame timers, and park Spyro in his idle state. */
void BeginPauseToMenuTransition(void) {
  StopAllSoundExceptMask(0);
  g_nGamestate = 10;
  g_nPauseMenuIdleFrames = 0;
  g_nPauseMenuTransitionFrames = 0;
  ResetSpyroEntity(0);
  ChangeSpyroState(0);
}
