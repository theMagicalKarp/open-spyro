#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "spyro_block.h"
#include "spyro_state.h"
#include "vecmath.h"

/* shared-base view of g_dwSpyroRequestMask (0x80078c4c): the mask read and
   the FindGroundHeightBelow position arg (-0x1F4 = g_anSpyroWorldPos) are
   both reached off one held register. */

/* 0x80048b9c (372 bytes) — per-frame Spyro physics driver: zero the motion
   accumulator, run the per-substep integrate loop, the footing-state
   dispatch, the per-substep finalize loop, then probe the ground below and
   fire the trigger-region event under Spyro: the ground he stands on, else
   the first surface he touched this frame (skipped under SPYRO_RQ_NO_CLIP). */
void AdvanceSpyroFrameMotion(void) {
  int i;
  int ground;
  int surface;
  int contact;
  unsigned int *req;

  ZeroVector(g_anSpyroVelocityFp6);
  for (i = 0; i < g_nFrameStep; i++) {
    IntegrateSpyroMotionForSubstep(i);
  }
  DispatchSpyroPhysicsByState();
  for (i = 0; i < g_nFrameStep; i++) {
    AdvanceSpyroSubstepState();
  }
  req = g_adwSpyroRequestMaskBlock;
  if (!(req[0] & SPYRO_RQ_NO_CLIP)) {
    ground =
        FindGroundHeightBelow((int *)(req - (SPY_REQUEST - SPY_POS)), 0x10000);
    g_nSpyroGroundHeightZ = ground;
    g_nSpyroTriggerCurrentEventId = 0;
    if (g_nSpyroAirborneFrames == 0 &&
        (surface = g_nGroundSurfaceType & 0x3F) != 0x3F &&
        g_anSpyroWorldPos[2] - ground < 0x201) {
      DispatchSpyroTriggerEvent(surface, 2);
    } else if ((contact = g_nSpyroFirstContactSurfaceType & 0x3F) != 0x3F) {
      DispatchSpyroTriggerEvent(contact, 0);
    } else if (g_nSpyroGroundHeightZ > 0 &&
               (surface = g_nGroundSurfaceType & 0x3F) != contact) {
      DispatchSpyroTriggerEvent(surface, 1);
    }
  }
}
