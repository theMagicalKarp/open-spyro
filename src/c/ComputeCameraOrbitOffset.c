#include "globals.h"

/* Compute the camera's orbit offset vector from the spring parameters
   (0x80034204, 0xF4). out = spherical-to-cartesian of (pitch, yaw, distance)
   in 1.19.12 fixed point: x = dist*cos(pitch)*cos(yaw), y =
   -dist*cos(pitch)*sin(yaw), z = dist*sin(pitch). */
extern int LookupCosine(unsigned int angle);
extern int LookupSine(unsigned int angle);

extern int g_anCameraSpringPitchBlock[];

void ComputeCameraOrbitOffset(int *out) {
  int cx;
  int cy;
  int s;
  int t;
  int *pitch = g_anCameraSpringPitchBlock;

  cx = LookupCosine(pitch[0]);
  s = LookupCosine(g_nCameraSpringYaw);
  t = (g_nCameraSpringDistance * cx) >> 12;
  out[0] = (t * s) >> 12;

  cy = LookupCosine(pitch[0]);
  s = LookupSine(g_nCameraSpringYaw);
  t = -((g_nCameraSpringDistance * cy) >> 12);
  out[1] = (t * s) >> 12;

  s = LookupSine(pitch[0]);
  out[2] = (g_nCameraSpringDistance * s) >> 12;
}
