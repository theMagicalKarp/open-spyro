#include "cd_stream.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"

/* GS_LEVEL_LOAD per-frame update (the level-intro variant of the gameplay
   tick): runs the glide-hold Spyro tick (TickSpyroFrameGlideHold), camera, and
   gem-tally particles, then advances the level-transition stream unless the CD
   stream is past stage 0xB and the tally is still active. (0x8002df9c, 100
   bytes.) */
void LevelLoad_Update(void) {
  TickSpyroFrameGlideHold();
  UpdateCameraFrame();
  AnimateGemTallyParticles();
  if (g_nCdStreamState < LSTREAM_READ_PATHS ||
      g_nLevelTransitionTallyActive == 0) {
    TickLevelTransitionStream(1);
  }
}
