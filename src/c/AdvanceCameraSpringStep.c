#include "globals.h"
#include "sdata.h"

extern int abs(int x);

/* Held-base view of g_anCameraSpringDeltaState. */
extern int g_anCameraSpringDeltaStateBlock[];
/* Held-base view of the spring angle set (yaw, pitch, distance, roll, pitch
   bias, yaw bias). */
extern int g_anCameraSpringYawBlock[];
/* Scalar view of g_anCameraSpringDeltaState[0]. */
extern int g_nCameraSpringYawDelta;
extern int g_nCameraSpringPitchDelta;

SDATA(g_nCameraLosClear, 4);
SDATA(g_nCameraManualYawInput, 4);
SDATA(g_nCameraSpringReengageCountdown, 4);

/* Wrapped 12-bit angle difference in -0x7FF..0x800. */
#define ANGLE_DELTA(target, spring)                                            \
  {                                                                            \
    delta = ((target) - (spring)) & 0xFFF;                                     \
    if (delta > 0x800) {                                                       \
      delta -= 0x1000;                                                         \
    }                                                                          \
  }

/* One axis of the spring: accumulate drive*delta - damping*state (>> 8),
   snap small states to zero (or to +-0x100 toward the target right after a
   camera cut). `drive` is already loaded. */
#define SPRING(k)                                                              \
  {                                                                            \
    int *p = &g_anCameraSpringDeltaStateBlock[k];                              \
    int v;                                                                     \
                                                                               \
    v = *p + ((drive * delta -                                                 \
               g_anCameraSpringDampingGains[(k) * 6 + column] * *p) >>         \
              8);                                                              \
    *p = v;                                                                    \
    if (abs(v) < g_anCameraSpringSnapThresholds[(k) * 6 + column]) {           \
      *p = 0;                                                                  \
      if (g_nCameraSnapped != 0) {                                             \
        if (delta > 0) {                                                       \
          *p = 0x100;                                                          \
        } else if (delta < 0) {                                                \
          *p = -0x100;                                                         \
        }                                                                      \
      }                                                                        \
    }                                                                          \
  }

#define CLAMP_STATE(x, lim)                                                    \
  {                                                                            \
    if ((x) < -(lim)) {                                                        \
      (x) = -(lim);                                                            \
    }                                                                          \
    if ((x) > (lim)) {                                                         \
      (x) = (lim);                                                             \
    }                                                                          \
  }

#define APPLY(k)                                                               \
  {                                                                            \
    int *s = &g_anCameraSpringYawBlock[k];                                     \
                                                                               \
    *s += g_anCameraSpringDeltaState[k] >> 8;                                  \
  }

#define PASSIVE_HOLD()                                                         \
  (((g_dwActiveCameraOptions & 0x10) || g_nCameraLookAroundActive) &&          \
   g_nCameraCurrentMode == 0x80000011)

/* Per-frame 6-axis camera spring step (0x80034ce8, 2748 bytes). `column` picks
   the tuning set (0..5) in the drive / damping / snap tables (one 6-entry row
   per axis). Axes: yaw (manual R1/L1 override, passive hold, idle look-around
   freeze), pitch, distance, roll, pitch bias, yaw bias; each springs its
   state toward the target and adds state >> 8 to the spring angle. The yaw
   state is clamped in the headbash state, with option 0x40 in modes 9/3, and
   in passive look-around; the pitch state with option 0x40 in mode 9. */
void AdvanceCameraSpringStep(int column) {
  int drive;
  int delta;

  drive = g_anCameraSpringDriveGains[column];
  if (g_nCameraCurrentMode == 3 && drive < 0x400) {
    drive = 0x400;
  }
  if (g_nCameraManualYawInput != 0) {
    delta = g_nCameraManualYawInput;
  } else {
    if (PASSIVE_HOLD() && g_nCameraLosClear != 0) {
      delta = 0;
    } else if ((((g_dwActiveCameraOptions & 0x10) &&
                 (unsigned)(g_nSpyroState - 5) < 2) ||
                g_nCameraLookAroundActive) &&
               g_nSpyroMotionSpeed < 0x400 && g_nCameraCollisionRetries == 0 &&
               g_nCameraSpringSuppressFlag == 0) {
      delta = 0;
      g_anCameraSpringDeltaState[0] = 0;
    } else if (g_nCameraCurrentMode == 6) {
      delta = 0;
    } else {
      ANGLE_DELTA(g_nCameraTargetYaw, g_nCameraSpringYaw);
    }
  }
  SPRING(0);
  if (g_nSpyroState == 0xF && g_nSpyroStateFlags == 0xB) {
    int *p = g_anCameraSpringDeltaStateBlock;

    if (*p < -0x800) {
      *p = -0x800;
    }
    if (*p > 0x800) {
      *p = 0x800;
    }
  }
  if ((g_dwActiveCameraOptions & 0x40) && g_nCameraCollisionRetries == 0 &&
      (g_nCameraCurrentMode == 0x80000009 || g_nCameraCurrentMode == 3)) {
    CLAMP_STATE(g_nCameraSpringYawDelta, 0x2000);
  }
  if (((g_dwActiveCameraOptions & 0x10) || g_nCameraLookAroundActive) &&
      g_nCameraSpringReengageCountdown == 0 && g_nCameraLosClear != 0 &&
      g_nCameraSpringSuppressFlag == 0 &&
      (g_nCameraCurrentMode == 0 || g_nCameraCurrentMode == 0x80000000)) {
    CLAMP_STATE(g_nCameraSpringYawDelta, 0x800);
  }
  APPLY(0);

  if (PASSIVE_HOLD()) {
    goto hold;
  }
  if ((g_dwActiveCameraOptions & 0x10) && g_nSpyroState == 6 &&
      g_nCameraCollisionRetries == 0) {
    delta = 0;
    g_anCameraSpringDeltaState[1] = 0;
  } else if (g_nCameraCurrentMode == 6) {
  hold:
    delta = 0;
  } else {
    ANGLE_DELTA(g_nCameraTargetPitch, g_nCameraSpringPitch);
  }
  drive = g_anCameraSpringDriveGains[6 + column];
  SPRING(1);
  if ((g_dwActiveCameraOptions & 0x40) && g_nCameraCurrentMode == 0x80000009 &&
      g_nCameraCollisionRetries == 0) {
    CLAMP_STATE(g_nCameraSpringPitchDelta, 0x2000);
  }
  APPLY(1);

  if (PASSIVE_HOLD() || g_nCameraCurrentMode == 6) {
    delta = 0;
  } else {
    delta = g_nCameraTargetDistance - g_nCameraSpringDistance;
  }
  drive = g_anCameraSpringDriveGains[12 + column];
  SPRING(2);
  APPLY(2);

  if (PASSIVE_HOLD() || g_nCameraCurrentMode == 6) {
    delta = 0;
  } else {
    ANGLE_DELTA(g_nCameraTargetRoll, g_nCameraSpringRoll);
  }
  drive = g_anCameraSpringDriveGains[18 + column];
  SPRING(3);
  APPLY(3);

  if (PASSIVE_HOLD() || g_nCameraCurrentMode == 6) {
    delta = 0;
  } else {
    ANGLE_DELTA(g_nCameraTargetPitchBias, g_nCameraSpringPitchBias);
  }
  drive = g_anCameraSpringDriveGains[24 + column];
  SPRING(4);
  APPLY(4);

  if (PASSIVE_HOLD() || g_nCameraCurrentMode == 6) {
    delta = 0;
  } else {
    ANGLE_DELTA(g_nCameraTargetYawBias, g_nCameraSpringYawBias);
  }
  drive = g_anCameraSpringDriveGains[30 + column];
  SPRING(5);
  APPLY(5);
}
