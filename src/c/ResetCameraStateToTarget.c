#include "globals.h"

/* Snap the spring camera to its per-mode target (0x80034358, 0x128).
   Loads the target angles for the current mode, copies all six target
   angles into the spring set, zeroes the spring delta state and the
   collision/stuck counters, recomputes the eye position (orbit offset +
   anchor), seats the smoothed position on it, refreshes the euler angles,
   and clears any camera shake. */
extern void LoadCameraTargetAnglesFromMode(void);
extern void ComputeCameraOrbitOffset(int *out);
extern void AddVector();
extern void CopyVector(int *dst, int *src);
extern int ComputeCameraToTargetAngle(int *pos);
extern void UpdateCameraEulerAngles(void);

extern int g_anCameraSpringYawBlock[];

/* The spring-block base is taken twice (`springBase = (springAlias = ...)`)
   on purpose: one pseudo alone is copy-propagated into the prologue and the
   `lui/addiu` pair then lands ahead of the register saves. */
void ResetCameraStateToTarget(void) {
  int *yaw;
  int y;
  int p;
  int d;
  int r;
  int pb;
  int *springBase;
  int *eye;
  int yb;
  volatile int *pSpringPitch;
  int *springAlias;
  springBase = (springAlias = g_anCameraSpringYawBlock);
  LoadCameraTargetAnglesFromMode();
  yaw = springBase;
  eye = &yaw[-17];
  y = *((volatile int *)(&g_nCameraTargetYaw));
  p = *((volatile int *)(&g_nCameraTargetPitch));
  d = *((volatile int *)(&g_nCameraTargetDistance));
  r = *((volatile int *)(&g_nCameraTargetRoll));
  pb = *((volatile int *)(&g_nCameraTargetPitchBias));
  yb = *((volatile int *)(&g_nCameraTargetYawBias));
  g_anCameraSpringDeltaState[0] = 0;
  g_anCameraSpringDeltaState[1] = 0;
  g_anCameraSpringDeltaState[2] = 0;
  g_anCameraSpringDeltaState[3] = 0;
  g_anCameraSpringDeltaState[4] = 0;
  g_anCameraSpringDeltaState[5] = 0;
  g_nCameraCollisionRetries = 0;
  g_nCameraStuckFrames = 0;
  pSpringPitch = (volatile int *)(&g_nCameraSpringPitch);
  *((volatile int *)yaw) = y;
  *pSpringPitch = p;
  *((volatile int *)(&g_nCameraSpringDistance)) = d;
  *((volatile int *)(&g_nCameraSpringRoll)) = r;
  *((volatile int *)(&g_nCameraSpringPitchBias)) = pb;
  *((volatile int *)(&g_nCameraSpringYawBias)) = yb;
  ComputeCameraOrbitOffset(eye);
  AddVector(eye, eye, g_pCameraAnchorPos);
  yaw = yaw - 0x14;
  CopyVector(yaw, eye);
  g_nCameraTargetInverted = ComputeCameraToTargetAngle(yaw);
  UpdateCameraEulerAngles();
  g_nCameraShakeMagnitude = 0;
  g_nCameraShakeDuration = 0;
}
