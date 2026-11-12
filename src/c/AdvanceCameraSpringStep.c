#include "camera.h"
#include "globals.h"
#include "main_shared.h"
#include "sdata.h"
#include "spyro_state.h"
#include "vecmath.h"

/* Held-base view of g_anCameraSpringDeltaState. */
/* Scalar view of g_anCameraSpringDeltaState[0]. */

SDATA(g_nCameraLosClear, 4);
SDATA(g_nCameraManualYawInput, 4);
SDATA(g_nCameraSpringReengageCountdown, 4);

/* Wrapped 12-bit angle difference in -0x7FF..0x800. */
#define ANGLE_DELTA(target, spring)                                            \
  {                                                                            \
    delta = ((target) - (spring)) & ANGLE12_MASK;                              \
    if (delta > ANGLE12_HALF) {                                                \
      delta -= ANGLE12_TURN;                                                   \
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
    v = *p +                                                                   \
        ((drive * delta -                                                      \
          g_anCameraSpringDampingGains[CAM_SPRING_ROW(k) + column] * *p) >>    \
         8);                                                                   \
    *p = v;                                                                    \
    if (abs(v) < g_anCameraSpringSnapThresholds[CAM_SPRING_ROW(k) + column]) { \
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
  (((g_dwActiveCameraOptions & CAM_OPT_PASSIVE_FOLLOW) ||                      \
    g_nCameraForcePassiveFollow) &&                                            \
   g_nCameraCurrentMode == (CAM_HELD | CAM_MODE_PASSIVE_SETTLE))

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
  if (g_nCameraCurrentMode == CAM_MODE_CHARGE && drive < CAM_CHARGE_MIN_DRIVE) {
    drive = CAM_CHARGE_MIN_DRIVE;
  }
  if (g_nCameraManualYawInput != 0) {
    delta = g_nCameraManualYawInput;
  } else {
    if (PASSIVE_HOLD() && g_nCameraLosClear != 0) {
      delta = 0;
    } else if ((((g_dwActiveCameraOptions & CAM_OPT_PASSIVE_FOLLOW) &&
                 (unsigned)(g_nSpyroState - SPYRO_ST_JUMP) <
                     2) || /* JUMP or FALL */
                g_nCameraForcePassiveFollow) &&
               g_nSpyroMotionSpeed < CAM_PASSIVE_MAX_SPEED &&
               g_nCameraCollisionRetries == 0 &&
               g_nCameraSpringSuppressFlag == 0) {
      delta = 0;
      g_anCameraSpringDeltaState[CAM_AXIS_YAW] = 0;
    } else if (g_nCameraCurrentMode == CAM_MODE_HAZARD) {
      delta = 0;
    } else {
      ANGLE_DELTA(g_nCameraTargetYaw, g_nCameraSpringYaw);
    }
  }
  SPRING(CAM_AXIS_YAW);
  if (g_nSpyroState == SPYRO_ST_GLIDE &&
      g_nSpyroSubstate == SPYRO_SUB_GLIDE_ENTER) {
    int *p = g_anCameraSpringDeltaStateBlock;

    if (*p < -CAM_SPRING_CLAMP) {
      *p = -CAM_SPRING_CLAMP;
    }
    if (*p > CAM_SPRING_CLAMP) {
      *p = CAM_SPRING_CLAMP;
    }
  }
  if ((g_dwActiveCameraOptions & CAM_OPT_SMOOTH_SWING) &&
      g_nCameraCollisionRetries == 0 &&
      (g_nCameraCurrentMode == (CAM_HELD | CAM_MODE_FREE_LOOK) ||
       g_nCameraCurrentMode == CAM_MODE_CHARGE)) {
    CLAMP_STATE(g_nCameraSpringYawDelta, CAM_SPRING_CLAMP_SWING);
  }
  if (((g_dwActiveCameraOptions & CAM_OPT_PASSIVE_FOLLOW) ||
       g_nCameraForcePassiveFollow) &&
      g_nCameraSpringReengageCountdown == 0 && g_nCameraLosClear != 0 &&
      g_nCameraSpringSuppressFlag == 0 &&
      (g_nCameraCurrentMode == CAM_MODE_STAND ||
       g_nCameraCurrentMode == CAM_HELD)) {
    CLAMP_STATE(g_nCameraSpringYawDelta, CAM_SPRING_CLAMP);
  }
  APPLY(CAM_AXIS_YAW);

  if (PASSIVE_HOLD()) {
    goto hold;
  }
  if ((g_dwActiveCameraOptions & CAM_OPT_PASSIVE_FOLLOW) &&
      g_nSpyroState == SPYRO_ST_FALL && g_nCameraCollisionRetries == 0) {
    delta = 0;
    g_anCameraSpringDeltaState[CAM_AXIS_PITCH] = 0;
  } else if (g_nCameraCurrentMode == CAM_MODE_HAZARD) {
  hold:
    delta = 0;
  } else {
    ANGLE_DELTA(g_nCameraTargetPitch, g_nCameraSpringPitch);
  }
  drive = g_anCameraSpringDriveGains[CAM_SPRING_ROW(CAM_AXIS_PITCH) + column];
  SPRING(CAM_AXIS_PITCH);
  if ((g_dwActiveCameraOptions & CAM_OPT_SMOOTH_SWING) &&
      g_nCameraCurrentMode == (CAM_HELD | CAM_MODE_FREE_LOOK) &&
      g_nCameraCollisionRetries == 0) {
    CLAMP_STATE(g_nCameraSpringPitchDelta, CAM_SPRING_CLAMP_SWING);
  }
  APPLY(CAM_AXIS_PITCH);

  if (PASSIVE_HOLD() || g_nCameraCurrentMode == CAM_MODE_HAZARD) {
    delta = 0;
  } else {
    delta = g_nCameraTargetDistance - g_nCameraSpringDistance;
  }
  drive =
      g_anCameraSpringDriveGains[CAM_SPRING_ROW(CAM_AXIS_DISTANCE) + column];
  SPRING(CAM_AXIS_DISTANCE);
  APPLY(CAM_AXIS_DISTANCE);

  if (PASSIVE_HOLD() || g_nCameraCurrentMode == CAM_MODE_HAZARD) {
    delta = 0;
  } else {
    ANGLE_DELTA(g_nCameraTargetRoll, g_nCameraSpringRoll);
  }
  drive = g_anCameraSpringDriveGains[CAM_SPRING_ROW(CAM_AXIS_ROLL) + column];
  SPRING(CAM_AXIS_ROLL);
  APPLY(CAM_AXIS_ROLL);

  if (PASSIVE_HOLD() || g_nCameraCurrentMode == CAM_MODE_HAZARD) {
    delta = 0;
  } else {
    ANGLE_DELTA(g_nCameraTargetPitchBias, g_nCameraSpringPitchBias);
  }
  drive =
      g_anCameraSpringDriveGains[CAM_SPRING_ROW(CAM_AXIS_PITCH_BIAS) + column];
  SPRING(CAM_AXIS_PITCH_BIAS);
  APPLY(CAM_AXIS_PITCH_BIAS);

  if (PASSIVE_HOLD() || g_nCameraCurrentMode == CAM_MODE_HAZARD) {
    delta = 0;
  } else {
    ANGLE_DELTA(g_nCameraTargetYawBias, g_nCameraSpringYawBias);
  }
  drive =
      g_anCameraSpringDriveGains[CAM_SPRING_ROW(CAM_AXIS_YAW_BIAS) + column];
  SPRING(CAM_AXIS_YAW_BIAS);
  APPLY(CAM_AXIS_YAW_BIAS);
}
