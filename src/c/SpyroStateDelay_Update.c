#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "spyro_state.h"

/* Spyro state-delay updater (0x8002e084): tick Spyro with no input and the
   camera, counting idle frames. At 0x20 frames the counter wraps and bumps
   the transition counter (the second wrap returns to the world hub); at
   exactly 0x10 frames with no transition pending, kick Spyro to state 0xF. */

void SpyroStateDelay_Update(void) {
  int frames;
  TickSpyroFrameNoInput();
  UpdateCameraFrame();
  frames = g_nMenuTickFrames + 1;
  g_nMenuTickFrames = frames;
  if (frames >= 0x20) {
    int passes;
    g_nMenuTickFrames = 0;
    passes = g_nMenuDrawFrames + 1;
    g_nMenuDrawFrames = passes;
    if (passes == 2) {
      BeginWorldHubReturn();
    }
  } else if (frames == 0x10) {
    if (g_nMenuDrawFrames == 0) {
      ChangeSpyroState(SPYRO_ST_GLIDE);
    }
  }
}
