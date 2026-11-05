#include "camera.h"
#include "globals.h"
#include "spyro_state.h"
#include "vecmath.h"

/* The active scripted-camera record pointer (g_pScriptedCameraView) and the
   camera position 0xc4 below it are reached through one held base register. */
#define VIEW (g_apScriptedCameraViewBlock[0])
#define CAMPOS ((int *)&g_apScriptedCameraViewBlock[-49])

/* The camera's euler angles: three consecutive shorts from g_nCameraEulerRoll.
 */
struct CameraEuler {
  short roll;  /* 0x0 g_nCameraEulerRoll */
  short pitch; /* 0x2 g_nCameraEulerPitch */
  short yaw;   /* 0x4 g_nCameraEulerYaw */
};
#define CAMERA_EULER (*(struct CameraEuler *)&g_nCameraEulerRoll)

/* 0x80037714 (0x94) — make `view` the active scripted-camera record: seat the
   camera at the record's camPos, stop Spyro with a fixed downward
   velocity, copy the record's euler angles into the
   camera and switch both camera modes to the scripted vantage (CAM_HELD |
   CAM_MODE_SCRIPTED_VANTAGE).

   The euler angles are written as record members, so each in-struct store
   depends on the next in-struct record read and the original's load/store
   interleave survives (sched.c true_dependence). */
void ActivateScriptedCameraView(ScriptedCamView *view) {
  ScriptedCamView *rec;

  VIEW = view;
  CopyVector(CAMPOS, view->camPos);
  ZeroVector(g_anSpyroVelocity);
  g_anSpyroVelocity[2] = -SPYRO_TERMINAL_SPEED;
  rec = g_pScriptedCameraView;
  CAMERA_EULER.roll = rec->camRoll;
  CAMERA_EULER.pitch = rec->camPitch;
  g_nCameraCurrentMode = (CAM_HELD | CAM_MODE_SCRIPTED_VANTAGE);
  g_nCameraNextMode = (CAM_HELD | CAM_MODE_SCRIPTED_VANTAGE);
  CAMERA_EULER.yaw = rec->camYaw;
}
