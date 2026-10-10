#include "globals.h"
#include "sdata.h"

extern void LoadCameraTargetAnglesFromMode(void);
extern void AdvanceCameraSpringStep(int column);
extern void ComputeCameraOrbitOffset(int *out);
extern void AddVector(int *dst, int *a, int *b);
extern void SnapCameraToTarget(void);
extern void UpdateCameraEulerAngles(void);
extern int ComputeCameraToTargetAngle(int *pos);

SDATA(g_nCameraManualYawInput, 4);

/* Ledge-grab camera, mode 0x80000012 (0x80037a20, 436 bytes): anchors on
   Spyro with the body yaw as the yaw bias and picks the target-params block
   (scripted: by CD stream state; otherwise once, by which side of the camera
   yaw Spyro faces). Springs column 0 once per frame step with no manual yaw,
   seats the eye, then copies the recomputed angles into the spring set. */
void UpdateLedgeGrabCamera(void) {
  int i;
  int yaw = g_nSpyroBodyYaw;

  g_pCameraAnchorPos = g_anSpyroWorldPos;
  g_nCameraTargetYawBiasFromMode = yaw;
  if (g_nScriptedMobTag != 0) {
    if (g_nCdStreamState >= 10) {
      g_pCameraTargetParams = g_anCameraParamMode2Reverse;
    } else {
      g_pCameraTargetParams = g_anCameraParamLedgeGrabStreaming;
    }
  } else if (g_pCameraTargetParams == 0) {
    if (yaw < g_nCameraEulerYaw) {
      g_pCameraTargetParams = g_anCameraParamLedgeGrabYawBelow;
    } else {
      g_pCameraTargetParams = g_anCameraParamLedgeGrabYawAbove;
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
