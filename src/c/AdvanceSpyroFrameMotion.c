#include "globals.h"

extern void ZeroVector(int *vec);
extern void IntegrateSpyroMotionForSubstep(int substep);
extern void DispatchSpyroPhysicsByState(void);
extern void AdvanceSpyroSubstepState(void);
extern int FindGroundHeightBelow(int *pos, int maxDown);
extern void DispatchSpyroTriggerEvent(int surface, int kind);

/* shared-base view of g_dwSpyroRequestMask (0x80078c4c): the mask read and
   the FindGroundHeightBelow position arg (-0x1F4 = g_anSpyroWorldPos) are
   both reached off one held register. */
extern unsigned int g_adwSpyroRequestMaskBlock[];

/* 0x80048b9c (372 bytes) — per-frame Spyro physics driver: zero the motion
   accumulator, run the per-substep integrate loop, the footing-state
   dispatch, the per-substep finalize loop, then probe the ground below and
   fire the surface/chunk trigger event (skipped while request bit 0x4000
   is set). */
void AdvanceSpyroFrameMotion(void) {
  int i;
  int ground;
  int surface;
  int chunk;
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
  if (!(req[0] & 0x4000)) {
    ground = FindGroundHeightBelow((int *)((char *)req - 0x1F4), 0x10000);
    g_nSpyroGroundHeightZ = ground;
    g_nSpyroTriggerCurrentEventId = 0;
    if (g_nSpyroAirborneFrames == 0 &&
        (surface = g_nGroundSurfaceType & 0x3F) != 0x3F &&
        g_anSpyroWorldPos[2] - ground < 0x201) {
      DispatchSpyroTriggerEvent(surface, 2);
    } else if ((chunk = g_nSpyroFirstContactChunkId & 0x3F) != 0x3F) {
      DispatchSpyroTriggerEvent(chunk, 0);
    } else if (g_nSpyroGroundHeightZ > 0 &&
               (surface = g_nGroundSurfaceType & 0x3F) != chunk) {
      DispatchSpyroTriggerEvent(surface, 1);
    }
  }
}
