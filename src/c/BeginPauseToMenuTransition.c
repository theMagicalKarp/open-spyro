#include "funcs.h"
#include "globals.h"
#include "spyro_state.h"

/* Leave gameplay for the pause/menu gamestate (0xA): kill all sound, reset the
   pause-menu frame timers, and park Spyro in his idle state. */
void BeginPauseToMenuTransition(void) {
  StopAllSoundExceptMask(0);
  g_nGamestate = GS_MENU_TRANSITION;
  g_nPauseMenuIdleFrames = 0;
  g_nPauseMenuTransitionFrames = 0;
  ResetSpyroEntity(0);
  ChangeSpyroState(SPYRO_ST_IDLE);
}
