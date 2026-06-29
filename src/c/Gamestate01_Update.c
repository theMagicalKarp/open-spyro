#include "globals.h"

extern void TickSpyroFrameForceJump(void);
extern void UpdateCameraFrame(void);
extern void AnimateGemTallyParticles(void);
extern void TickLevelTransitionStream(int param_1);

/* GS 1 per-frame update (intro/forced-jump variant of the gameplay tick): runs
   the forced-jump spyro tick, camera, and gem-tally particles, then advances
   the level-transition stream unless the CD stream is past stage 0xB and the
   tally is still active. (0x8002df9c, 100 bytes.) */
void Gamestate01_Update(void) {
  TickSpyroFrameForceJump();
  UpdateCameraFrame();
  AnimateGemTallyParticles();
  if (g_nCdStreamState < 0xB || g_nLevelTransitionTallyActive == 0) {
    TickLevelTransitionStream(1);
  }
}
