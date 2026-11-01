#include "globals.h"

/* On death: spend a life and go to the respawn gamestate (4); with no lives
   left, go to game-over (5). Either way reset the gameplay draw cursor. */
void TriggerRespawnOrGameOver(void) {
  if (g_nExtraLives != 0) {
    g_nExtraLives--;
    g_nGamestate = GS_RESPAWN;
  } else {
    g_nGamestate = GS_GAME_OVER;
  }
  g_nRespawnOrGameOverMode = 0;
  g_nRespawnOrGameOverFrame = 0;
}
