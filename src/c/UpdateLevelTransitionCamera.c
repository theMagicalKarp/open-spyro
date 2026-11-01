#include "cd_stream.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "sdata.h"
#include "vecmath.h"

SDATA(g_nCameraManualYawInput, 4);

/* Level-transition camera, mode 0x80000012 (0x80037a20, 436 bytes), set with
   GS_LEVEL_LOAD by portal entry, BeginWorldHubReturn and level select:
   anchors on Spyro with the body yaw as the yaw bias and picks the
   target-params block (portal exit: by CD stream state; otherwise once, by
   which side of the camera yaw Spyro faces). Springs column 0 once per frame
   step with no manual yaw, seats the eye, then copies the recomputed angles
   into the spring set. */
void UpdateLevelTransitionCamera(void) {
  int i;
  int yaw = g_nSpyroBodyYaw;

  g_pCameraAnchorPos = g_anSpyroWorldPos;
  g_nCameraTargetYawBiasFromMode = yaw;
  if (g_nReturnPortalLevelId != 0) {
    if (g_nCdStreamState >= LSTREAM_RELOCATE) {
      g_pCameraTargetParams = g_anCameraParamMode2Reverse;
    } else {
      g_pCameraTargetParams = g_anCameraParamTransitionStreaming;
    }
  } else if (g_pCameraTargetParams == 0) {
    if (yaw < g_nCameraEulerYaw) {
      g_pCameraTargetParams = g_anCameraParamTransitionYawBelow;
    } else {
      g_pCameraTargetParams = g_anCameraParamTransitionYawAbove;
    }
  }
  LoadCameraTargetAnglesFromMode();
  g_nCameraManualYawInput = 0;
  for (i = 0; i < g_nFrameStep; i++) {
    AdvanceCameraSpringStep(0);
  }
  ComputeCameraOrbitOffset(g_anCameraEyeTarget);
  AddVector(g_anCameraEyeTarget, g_anCameraEyeTarget, g_pCameraAnchorPos);
  SnapCameraToTarget();
  UpdateCameraEulerAngles();
  g_nCameraTargetInverted = ComputeCameraToTargetAngle(g_anCameraEyeTarget - 3);
  g_nCameraSpringYaw = g_nCameraYaw;
  g_nCameraSpringPitch = g_nCameraPitch;
  g_nCameraSpringDistance = g_nCameraDistance;
  g_nCameraSpringRoll = g_nCameraDraftRoll;
  g_nCameraSpringPitchBias = g_nCameraDraftPitchBias;
  g_nCameraSpringYawBias = g_nCameraDraftYawBias;
}
