#include "globals.h"

/* Spyro state-delay updater (0x8002e084): tick Spyro with no input and the
   camera, counting idle frames. At 0x20 frames the counter wraps and bumps
   the transition counter (the second wrap returns to the world hub); at
   exactly 0x10 frames with no transition pending, kick Spyro to state 0xF. */
extern void TickSpyroFrameNoInput(void);
extern void UpdateCameraFrame(void);
extern void BeginWorldHubReturn(void);
extern void ChangeSpyroState();

void SpyroStateDelay_Update(void) {
  int frames;
  TickSpyroFrameNoInput();
  UpdateCameraFrame();
  frames = g_nPauseMenuIdleFrames + 1;
  g_nPauseMenuIdleFrames = frames;
  if (frames >= 0x20) {
    int passes;
    g_nPauseMenuIdleFrames = 0;
    passes = g_nPauseMenuTransitionFrames + 1;
    g_nPauseMenuTransitionFrames = passes;
    if (passes == 2) {
      BeginWorldHubReturn();
    }
  } else if (frames == 0x10) {
    if (g_nPauseMenuTransitionFrames == 0) {
      ChangeSpyroState(0xF);
    }
  }
}
