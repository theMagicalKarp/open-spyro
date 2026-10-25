/* camera.h — the camera's six-word angle sets.
 *
 * The camera keeps three copies of the same six words: the target
 * (g_nCameraTargetYaw, 0x80076e30), the spring (g_nCameraSpringYaw,
 * 0x80076e48) and the draft (g_nCameraYaw, 0x80076e60). Mode changes re-sync
 * one set from another with a whole-struct copy. */

#ifndef OPEN_SPYRO_CAMERA_H
#define OPEN_SPYRO_CAMERA_H

typedef struct CameraAngles {
  int yaw;       /* 0x00 */
  int pitch;     /* 0x04 */
  int distance;  /* 0x08 */
  int roll;      /* 0x0C */
  int pitchBias; /* 0x10 */
  int yawBias;   /* 0x14 */
} CameraAngles;

#endif /* OPEN_SPYRO_CAMERA_H */
