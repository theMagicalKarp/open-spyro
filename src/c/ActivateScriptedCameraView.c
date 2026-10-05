#include "globals.h"

extern void CopyVector(int *dst, int *src);
extern void ZeroVector(int *vec);

/* The active scripted-camera record pointer (D_80076EBC) and the camera
   position 0xc4 below it are reached through one held base register. */
extern int *g_apScriptedCameraViewBlock[];
#define VIEW (g_apScriptedCameraViewBlock[0])
#define CAMPOS ((int *)&g_apScriptedCameraViewBlock[-49])
extern int
    *D_80076EBC; /* the same pointer, absolute form for the post-call re-read */

/* The camera's euler angles: three consecutive shorts from g_nCameraEulerRoll.
 */
struct CameraEuler {
  short roll;  /* 0x0 g_nCameraEulerRoll */
  short pitch; /* 0x2 g_nCameraEulerPitch */
  short yaw;   /* 0x4 g_nCameraEulerYaw */
};
#define CAMERA_EULER (*(struct CameraEuler *)&g_nCameraEulerRoll)

/* 0x80037714 (0x94) — make `view` the active scripted-camera record: seat the
   camera at the record's position (+0x18), stop Spyro with a fixed downward
   velocity, copy the record's euler angles (+0x24/+0x28/+0x2c) into the
   camera and switch both camera modes to the scripted vantage (0x8000000e).

   The euler angles are written as record members, so each in-struct store
   depends on the next in-struct record read and the original's load/store
   interleave survives (sched.c true_dependence). */
void ActivateScriptedCameraView(int *view) {
  int *rec;

  VIEW = view;
  CopyVector(CAMPOS, view + 6);
  ZeroVector(g_anSpyroVelocity);
  g_anSpyroVelocity[2] = -0x2300;
  rec = D_80076EBC;
  CAMERA_EULER.roll = rec[9];
  CAMERA_EULER.pitch = rec[10];
  g_nCameraCurrentMode = 0x8000000e;
  g_nCameraNextMode = 0x8000000e;
  CAMERA_EULER.yaw = rec[11];
}
