#include "camera.h"
#include "funcs.h"
#include "globals.h"
#include "gpu_prim.h"
#include "main_shared.h"
#include "sdata.h"
#include "spyro_state.h"
#include "vecmath.h"

#define CAM g_anCameraPosBlock
#define CAM_EYE 0x03           /* g_anCameraEyeTarget */
#define CAM_TARGET_YAW 0x0E    /* g_nCameraTargetYaw/Pitch/Distance */
#define CAM_SPRING_YAW 0x14    /* g_nCameraSpringYaw.. */
#define CAM_YAW 0x1A           /* g_nCameraYaw.. */
#define CAM_RETRIES 0x27       /* g_nCameraCollisionRetries */
#define CAM_STUCK_FRAMES 0x28  /* g_nCameraStuckFrames */
#define CAM_SNAPPED 0x29       /* g_nCameraSnapped */
#define CAM_ANCHOR 0x2A        /* g_pCameraAnchorPos */
#define CAM_TARGET_PARAMS 0x2C /* g_pCameraTargetParams */

SDATA(g_nCameraLosClear, 4);

typedef struct {
  int yaw;
  int pitch;
  int distance;
  int pad[3];
} CameraEscapeOffset;

#define ESCAPE(off)                                                            \
  (*(CameraEscapeOffset *)((char *)g_anCameraEscapeOffsets + (off)))

/* Seat the eye at the current angles plus one escape offset (scaled by 1 <<
 * shift). */
#define TRY_ESCAPE(off, shift)                                                 \
  (CAM[CAM_SPRING_YAW] =                                                       \
       (CAM[CAM_YAW] + (ESCAPE(off).yaw << (shift))) & 0xFFF,                  \
   CAM[CAM_SPRING_YAW + 1] =                                                   \
       (CAM[CAM_YAW + 1] + (ESCAPE(off).pitch << (shift))) & 0xFFF,            \
   CAM[CAM_SPRING_YAW + 2] =                                                   \
       CAM[CAM_YAW + 2] + (ESCAPE(off).distance << (shift)),                   \
   ComputeCameraOrbitOffset(&CAM[CAM_EYE]),                                    \
   AddVector(&CAM[CAM_EYE], &CAM[CAM_EYE], *(int **)&CAM[CAM_ANCHOR]))

/* A clear seat was found: hold it for 5 frames of retries, make it the
   target and restore the current angles. */
#define FOUND()                                                                \
  {                                                                            \
    CAM[CAM_RETRIES] = 5;                                                      \
    ComputeCameraToTargetAngle(&CAM[CAM_EYE]);                                 \
    CAM[CAM_TARGET_YAW] = CAM[CAM_YAW];                                        \
    CAM[CAM_TARGET_YAW + 1] = CAM[CAM_YAW + 1];                                \
    CAM[CAM_TARGET_YAW + 2] = CAM[CAM_YAW + 2];                                \
    *(CameraAngles *)&CAM[CAM_YAW] = angles;                                   \
    return;                                                                    \
  }

/* Per-frame gameplay camera (0x80034480, 2052 bytes). Marches the eye toward
   its target in 0x100 substeps, sliding off collisions (6 tries a step, else
   restores the start); smooths small residual moves, then tests two-way line
   of sight to Spyro. Occluded: after 0x1E stuck frames re-seats through the
   five reseat parameter blocks, then probes the auto-rotate offsets at 1x/2x/4x
   for a clear seat. Clear: decays the collision retries and keeps the stuck
   count while Spyro is off the screen centre or far away.
   Allocation notes: `j` is shared by the collision retries and every probe
   loop, each probe loop has its own `offN = j * 24` giv (its init must follow
   the hoisted invariants), and FOUND() is written per loop so jump.c
   cross-jumps the copies into the original's single tail. */
void UpdateGameplayCamera(void) {
  int saved[3];
  int d[3];
  CameraAngles angles;
  int hit = 0;
  int far;
  int n;
  int i;
  int j;
  int mask;
  int off1;
  int off2;
  int off3;

  SubtractVector(d, &CAM[CAM_EYE], g_anSpyroWorldPos);
  far = VectorLength(d, VEC_LEN_XYZ) > CAM_FAR_RANGE;
  CopyVector(saved, &CAM[CAM_EYE]);
  SubtractVector(d, &CAM[CAM_EYE], CAM);
  n = VectorLength(d, VEC_LEN_XYZ) / CAM_STEP_LEN + 1;
  if (n >= 2) {
    d[0] /= n;
    d[1] /= n;
    d[2] /= n;
  }
  CopyVector(&CAM[CAM_EYE], CAM);
  for (i = 0; i < n; i++) {
    AddVector(&CAM[CAM_EYE], &CAM[CAM_EYE], d);
    j = 0;
    while (CollideSphereWithWorldAndActors(&CAM[CAM_EYE], CAM_COLLIDE_RADIUS,
                                           CAM_COLLIDE_RADIUS)) {
      CopyVector(&CAM[CAM_EYE], g_anCollisionResolvedPos);
      j++;
      hit = 1;
      if (j >= CAM_COLLIDE_TRIES) {
        break;
      }
    }
    if (j == CAM_COLLIDE_TRIES) {
      CopyVector(&CAM[CAM_EYE], saved);
      i = n;
    }
  }
  CAM[CAM_SNAPPED] = 0;
  if (j < CAM_COLLIDE_TRIES) {
    if (hit) {
      SubtractVector(saved, &CAM[CAM_EYE], CAM);
      if (abs(saved[0]) < CAM_SLIDE_SLOP) {
        saved[0] = 0;
      }
      if (abs(saved[1]) < CAM_SLIDE_SLOP) {
        saved[1] = 0;
      }
      if (abs(saved[2]) < CAM_SLIDE_SLOP) {
        saved[2] = 0;
      }
      AddVector(CAM, CAM, saved);
    } else {
      CopyVector(CAM, &CAM[CAM_EYE]);
      CAM[CAM_SNAPPED] = 1;
    }
  }
  g_nCameraTargetInverted = ComputeCameraToTargetAngle(CAM);
  CopyVector(d, g_anSpyroWorldPos);
  d[2] -= CAM_LOS_DROP;
  if (!CheckLineOfSightSegment(CAM, d) || !CheckLineOfSightSegment(d, CAM)) {
    g_nCameraLosClear = 0;
    if (g_bSpyroOtBinBias < SPYRO_OT_BIAS_PINNED) {
      g_bSpyroOtBinBias = SPYRO_OT_BIAS_OCCLUDED;
    }
    g_nSpyroShadowOtBinBias = 0;
    if (++g_nCameraStuckFrames >= CAM_RESEAT_FRAMES) {
      for (j = 0; j < CAM_RESEAT_COUNT; j++) {
        *(int **)&CAM[CAM_TARGET_PARAMS] = &g_anCameraStuckReseatParams[j * 6];
        ResetCameraStateToTarget();
        if (!CollideSphereWithWorldAndActors(&CAM[CAM_EYE], CAM_COLLIDE_RADIUS,
                                             CAM_COLLIDE_RADIUS) &&
            CheckLineOfSightSegment(&CAM[CAM_EYE], d) &&
            CheckLineOfSightSegment(d, &CAM[CAM_EYE])) {
          return;
        }
      }
    }
    mask = 0;
    angles = *(CameraAngles *)&CAM[CAM_YAW];
    for (j = 0; j < CAM_ESCAPE_COUNT; j++) {
      off1 = j * sizeof(CameraEscapeOffset);
      TRY_ESCAPE(off1, 0);
      if (!CollideSphereWithWorldAndActors(&CAM[CAM_EYE], CAM_COLLIDE_RADIUS,
                                           CAM_COLLIDE_RADIUS)) {
        mask |= 1 << j;
        if (CheckLineOfSightSegment(&CAM[CAM_EYE], d) &&
            CheckLineOfSightSegment(d, &CAM[CAM_EYE])) {
          FOUND();
        }
      }
    }
    if (mask != 0) {
      for (j = 0; j < CAM_ESCAPE_COUNT; j++) {
        off2 = j * sizeof(CameraEscapeOffset);
        if ((mask >> j) & 1) {
          TRY_ESCAPE(off2, 1);
          if (!CollideSphereWithWorldAndActors(
                  &CAM[CAM_EYE], CAM_COLLIDE_RADIUS, CAM_COLLIDE_RADIUS) &&
              CheckLineOfSightSegment(&CAM[CAM_EYE], d) &&
              CheckLineOfSightSegment(d, &CAM[CAM_EYE])) {
            FOUND();
          }
        }
      }
    }
    if (mask != 0) {
      for (j = 0; j < CAM_ESCAPE_COUNT; j++) {
        off3 = j * sizeof(CameraEscapeOffset);
        if ((mask >> j) & 1) {
          TRY_ESCAPE(off3, 2);
          if (!CollideSphereWithWorldAndActors(
                  &CAM[CAM_EYE], CAM_COLLIDE_RADIUS, CAM_COLLIDE_RADIUS) &&
              CheckLineOfSightSegment(&CAM[CAM_EYE], d) &&
              CheckLineOfSightSegment(d, &CAM[CAM_EYE])) {
            FOUND();
          }
        }
      }
    }
    return;
  }
  g_nCameraLosClear = 1;
  if (g_nCameraCollisionRetries != 0) {
    if (g_nCameraCurrentMode != CAM_MODE_STAND &&
        g_nCameraCurrentMode != (CAM_HELD | CAM_MODE_FREE_LOOK)) {
      g_nCameraCollisionRetries--;
    } else if (g_nCameraTargetInverted != 0) {
      g_nCameraCollisionRetries--;
    }
  }
  if (g_nCameraCurrentMode < 0 ||
      (ProjectWorldPointGTEUnscaled(d, g_anSpyroWorldPos),
       (unsigned)(d[0] - (CAM_ONSCREEN_MARGIN_X + 1)) <
               SCREEN_W - 2 * CAM_ONSCREEN_MARGIN_X - 1 &&
           (unsigned)(d[1] - (CAM_ONSCREEN_MARGIN_Y + 1)) <
               SCREEN_H - 2 * CAM_ONSCREEN_MARGIN_Y - 1 &&
           !far)) {
    g_nCameraStuckFrames = 0;
    return;
  }
  CAM[CAM_STUCK_FRAMES]++;
}
