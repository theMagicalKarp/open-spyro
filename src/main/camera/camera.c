#include "camera.h"
#include "angle.h"
#include "cd_stream.h"
#include "funcs.h"
#include "globals.h"
#include "gpu_prim.h"
#include "main_shared.h"
#include "match_asm.h"
#include "pad.h"
#include "pad_buttons.h"
#include "portal.h"
#include "sdata.h"
#include "spyro_block.h"
#include "spyro_state.h"
#include "vecmath.h"

/* 0x80033c50: build the 3x3 camera view matrix from
   g_nCameraEulerPitch/Yaw/Roll (X*Y*Z order). Writes the full matrix to
   g_anCameraViewMtx (0x80076de4) and a Y-row-scaled (*0x140>>9) copy to
   g_anWorldToCameraRotMtx (0x80076dd0). */
void BuildCameraViewMatrix(void) {
  int v;
  MATRIX view;
  MATRIX axis;

  FillWord(&view, 0, 0x20);
  SetTransMatrix(&view);
  view.m[0][0] = 0x1000;
  v = LookupCosine(g_anCameraEulerPitchBlock[0]);
  view.m[1][1] = v;
  v = LookupSine(g_anCameraEulerPitchBlock[0]);
  view.m[2][1] = v;
  v = LookupSine(g_anCameraEulerPitchBlock[0]);
  view.m[1][2] = -v;
  v = LookupCosine(g_anCameraEulerPitchBlock[0]);
  view.m[2][2] = v;

  FillWord(&axis, 0, 0x20);
  v = LookupCosine(g_nCameraEulerYaw);
  axis.m[0][0] = v;
  v = LookupSine(g_nCameraEulerYaw);
  axis.m[2][0] = -v;
  axis.m[1][1] = 0x1000;
  v = LookupSine(g_nCameraEulerYaw);
  axis.m[0][2] = v;
  v = LookupCosine(g_nCameraEulerYaw);
  axis.m[2][2] = v;
  MulMatrix(&view, &axis);

  FillWord(&axis, 0, 0x20);
  v = LookupCosine(g_nCameraEulerRoll);
  axis.m[0][0] = v;
  v = LookupSine(g_nCameraEulerRoll);
  axis.m[1][0] = -v;
  v = LookupSine(g_nCameraEulerRoll);
  axis.m[0][1] = v;
  v = LookupCosine(g_nCameraEulerRoll);
  axis.m[1][1] = v;
  axis.m[2][2] = 0x1000;
  MulMatrix(&view, &axis);

  CopyWords(CAM_VIEW_MTX_VIA_EULER, &view, 0x14);
  v = view.m[1][0] * 0x140;
  if (v < 0) {
    v += 0x1ff;
  }
  view.m[1][0] = v >> 9;
  v = view.m[1][1] * 0x140;
  if (v < 0) {
    v += 0x1ff;
  }
  view.m[1][1] = v >> 9;
  v = view.m[1][2] * 0x140;
  if (v < 0) {
    v += 0x1ff;
  }
  view.m[1][2] = v >> 9;
  CopyWords(CAM_W2C_MTX_VIA_EULER, &view, 0x14);
}

/* March a ray along the segment to - from in 0x400-unit steps, testing
   world/actor occlusion at each step; returns 1 if the whole segment is clear,
   0 on the first hit (0x80033e40). */
int CheckLineOfSightSegment(int *from, int *to) {
  int seg[4];
  int cur[4];
  int next[4];
  int len;
  int steps;
  int i;

  SubtractVector(seg, to, from);
  len = VectorLength(seg, VEC_LEN_XYZ);
  steps = len >> 10;
  ScaleVector3ByRatio(seg, len, 0x400);
  CopyVector(cur, from);
  for (i = 0; i < steps; i++) {
    AddVector(next, cur, seg);
    if (CastRayWorldAndActors(cur, next) != 0) {
      return 0;
    }
    CopyVector(cur, next);
  }
  return 1;
}

/* Camera-to-target angle solver (0x80033f08, 0x290). Builds the anchor→
   target delta vector, refines the 2D/3D distances, and derives the
   camera pitch/yaw from it. When the wrapped yaw delta is closer to the
   inverted heading (and the mode is CAM_HELD | CAM_MODE_FREE_LOOK or the target
   is flagged inverted), flips the framing: yaw += half-turn (or snaps to the
   spring yaw when the target is nearly overhead), pitch mirrored around 0x800,
   and returns 1; otherwise commits the plain yaw (or spring yaw) and
   returns 0. Tail publishes the draft roll and the wrapped pitch/yaw
   biases against the euler camera state. */

int ComputeCameraToTargetAngle(int *target) {
  int vec[3];
  int yaw;
  int dist;
  int oldYaw;
  int flipYaw;
  int yawDelta;
  int flipDelta;
  int pitchBias;
  int yawBias;
  int *pitch;

  SubtractVector(vec, target, g_pCameraAnchorPos);
  dist = VectorLength(vec, VEC_LEN_XYZ);
  g_nCameraDistance = dist;
  g_nCameraDistance = RefineSqrtEstimate(vec, dist, 1);
  dist = VectorLength(vec, VEC_LEN_XY);
  dist = RefineSqrtEstimate(vec, dist, 0);
  g_nCameraPitch = ArcTan2(dist, vec[2], ATAN_ANGLE12);
  yaw = ArcTan2(vec[0], -vec[1], ATAN_ANGLE12);
  oldYaw = g_nCameraYaw;
  yawDelta = (yaw - oldYaw) & ANGLE12_MASK;
  if (yawDelta >= 0x801) {
    yawDelta -= 0x1000;
  }
  flipYaw = oldYaw - ANGLE12_HALF;
  flipDelta = (yaw - flipYaw) & ANGLE12_MASK;
  if (flipDelta >= 0x801) {
    flipDelta -= 0x1000;
  }
  if (abs(flipDelta) < abs(yawDelta) &&
      (g_nCameraCurrentMode == (CAM_HELD | CAM_MODE_FREE_LOOK) ||
       g_nCameraTargetInverted != 0)) {
    if (abs(vec[0]) < 0x81) {
      if (abs(vec[1]) < 0x81) {
        goto spring;
      }
    }
    g_nCameraYaw = (yaw + ANGLE12_HALF) & ANGLE12_MASK;
    goto flip;
  spring:
    g_nCameraYaw = g_nCameraSpringYaw;
  flip:
    yaw = 1;
    pitch = g_anCameraPitchBlock;
    pitch[0] = (ANGLE12_HALF - pitch[0]) & ANGLE12_MASK;
  } else {
    if (abs(vec[0]) >= 0x81 || abs(vec[1]) >= 0x81) {
      g_nCameraYaw = yaw;
    } else {
      g_nCameraYaw = g_nCameraSpringYaw;
    }
    yaw = 0;
  }
  pitchBias = (g_nCameraEulerPitch - g_nCameraPitch) & ANGLE12_MASK;
  g_nCameraDraftRoll = g_nCameraEulerRoll;
  g_nCameraDraftPitchBias = pitchBias;
  if (pitchBias >= 0x801) {
    g_nCameraDraftPitchBias = pitchBias - 0x1000;
  }
  yawBias = (ANGLE12_HALF - g_nCameraEulerYaw - g_nCameraYaw) & ANGLE12_MASK;
  g_nCameraDraftYawBias = yawBias;
  if (yawBias >= 0x801) {
    g_nCameraDraftYawBias = yawBias - 0x1000;
  }
  return yaw;
}

/* The camera's target pose: six consecutive ints from g_nCameraTargetYaw. */
#define CAMERA_TARGET (*(CameraAngles *)&g_nCameraTargetYaw)

/* 0x80034198 (0x6c) — load the camera target pose from the current mode's
   parameter record: yaw relative to the mode's yaw bias (wrapped to 12 bits),
   then pitch, distance, roll and the two biases verbatim.

   The target pose is written as record members: an in-struct store depends
   on the next in-struct `params[k]` read (sched.c true_dependence), which
   keeps the original's load/store interleave instead of batching the six
   loads. */
void LoadCameraTargetAnglesFromMode(void) {
  int *params = g_pCameraTargetParams;

  CAMERA_TARGET.yaw = (params[0] - g_nCameraParamsReferenceYaw) & 0xfff;
  CAMERA_TARGET.pitch = params[1];
  CAMERA_TARGET.distance = params[2];
  CAMERA_TARGET.roll = params[3];
  CAMERA_TARGET.pitchBias = params[4];
  CAMERA_TARGET.yawBias = params[5];
}

#undef CAMERA_TARGET

/* Compute the camera's orbit offset vector from the spring parameters
   (0x80034204, 0xF4). out = spherical-to-cartesian of (pitch, yaw, distance)
   in 1.19.12 fixed point: x = dist*cos(pitch)*cos(yaw), y =
   -dist*cos(pitch)*sin(yaw), z = dist*sin(pitch). */

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

/* Publish the spring camera's orientation as the 12-bit euler angles the render
   path consumes (0x800342f8, 0x60 bytes). Pitch picks up the spring bias, roll
   passes straight through, and yaw is measured backwards from 0x800. The single
   `angle` scratch carries roll and then yaw, so both live in one register. */
void UpdateCameraEulerAngles(void) {
  int angle = g_nCameraSpringRoll;

  g_nCameraEulerPitch =
      (g_nCameraPitch + g_nCameraSpringPitchBias) & ANGLE12_MASK;
  g_nCameraEulerRoll = angle;
  angle = g_nCameraYaw;
  g_nCameraEulerYaw =
      (ANGLE12_HALF - angle - g_nCameraSpringYawBias) & ANGLE12_MASK;
}

/* Snap the spring camera to its per-mode target (0x80034358, 0x128).
   Loads the target angles for the current mode, copies all six target
   angles into the spring set, zeroes the spring delta state and the
   collision/stuck counters, recomputes the eye position (orbit offset +
   anchor), seats the smoothed position on it, refreshes the euler angles,
   and clears any camera shake. */

/* The spring-block base is taken twice (`springBase = (springAlias = ...)`)
   on purpose: one pseudo alone is copy-propagated into the prologue and the
   `lui/addiu` pair then lands ahead of the register saves. */
void ResetCameraStateToTarget(void) {
  int *yaw;
  int tgtYaw;
  int tgtPitch;
  int tgtDist;
  int tgtRoll;
  int tgtPitchBias;
  int *springBase;
  int *eye;
  int tgtYawBias;
  volatile int *pSpringPitch;
  int *springAlias;
  springBase = (springAlias = g_anCameraSpringYawBlock);
  LoadCameraTargetAnglesFromMode();
  yaw = springBase;
  eye = &yaw[-17];
  tgtYaw = *((volatile int *)(&g_nCameraTargetYaw));
  tgtPitch = *((volatile int *)(&g_nCameraTargetPitch));
  tgtDist = *((volatile int *)(&g_nCameraTargetDistance));
  tgtRoll = *((volatile int *)(&g_nCameraTargetRoll));
  tgtPitchBias = *((volatile int *)(&g_nCameraTargetPitchBias));
  tgtYawBias = *((volatile int *)(&g_nCameraTargetYawBias));
  g_anCameraSpringDeltaState[0] = 0;
  g_anCameraSpringDeltaState[1] = 0;
  g_anCameraSpringDeltaState[2] = 0;
  g_anCameraSpringDeltaState[3] = 0;
  g_anCameraSpringDeltaState[4] = 0;
  g_anCameraSpringDeltaState[5] = 0;
  g_nCameraCollisionRetries = 0;
  g_nCameraStuckFrames = 0;
  pSpringPitch = (volatile int *)(&g_nCameraSpringPitch);
  *((volatile int *)yaw) = tgtYaw;
  *pSpringPitch = tgtPitch;
  *((volatile int *)(&g_nCameraSpringDistance)) = tgtDist;
  *((volatile int *)(&g_nCameraSpringRoll)) = tgtRoll;
  *((volatile int *)(&g_nCameraSpringPitchBias)) = tgtPitchBias;
  *((volatile int *)(&g_nCameraSpringYawBias)) = tgtYawBias;
  ComputeCameraOrbitOffset(eye);
  AddVector(eye, eye, g_pCameraAnchorPos);
  yaw = yaw - 0x14;
  CopyVector(yaw, eye);
  g_nCameraTargetInverted = ComputeCameraToTargetAngle(yaw);
  UpdateCameraEulerAngles();
  g_nCameraShakeMagnitude = 0;
  g_nCameraShakeDuration = 0;
}

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
       (CAM[CAM_YAW] + (ESCAPE(off).yaw << (shift))) & ANGLE12_MASK,           \
   CAM[CAM_SPRING_YAW + 1] =                                                   \
       (CAM[CAM_YAW + 1] + (ESCAPE(off).pitch << (shift))) & ANGLE12_MASK,     \
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

#undef CAM_EYE
#undef CAM_TARGET_YAW
#undef CAM_SPRING_YAW
#undef CAM_YAW
#undef CAM_RETRIES
#undef CAM_STUCK_FRAMES
#undef CAM_SNAPPED
#undef CAM_ANCHOR
#undef CAM_TARGET_PARAMS
#undef ESCAPE
#undef TRY_ESCAPE
#undef FOUND

SDATA(g_nCameraLosClear, 4);

/* Camera cut (0x80034c84, 100 bytes): snaps the smoothed camera position to
   the current eye (g_anCameraPos + 0xC), zeros the spring tracking state,
   raises g_nCameraSnapped + g_nCameraLosClear and rebases the camera-to-target
   angles. */
void SnapCameraToTarget(void) {
  CopyVector(g_anCameraPos, g_anCameraPos + 3); /* g_anCameraEyeTarget */
  g_nCameraCollisionRetries = 0;
  g_nCameraStuckFrames = 0;
  g_nCameraSnapped = 1;
  g_nCameraLosClear = 1;
  g_nCameraTargetInverted = ComputeCameraToTargetAngle(g_anCameraPos);
}

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

#undef ANGLE_DELTA
#undef SPRING
#undef CLAMP_STATE
#undef APPLY
#undef PASSIVE_HOLD

#define CAM g_anCameraPosBlock
#define CAM_CURRENT_MODE 0x0C /* g_nCameraCurrentMode */
#define CAM_NEXT_MODE 0x26    /* g_nCameraNextMode */
#define CAM_ANCHOR 0x2A       /* g_pCameraAnchorPos */

#define STATE_MODE() ((&g_abSpyroStateCameraMode)[g_nSpyroState])

/* Re-sync the spring set to the angles recomputed from the eye position. */
#define REBASE()                                                               \
  {                                                                            \
    g_nCameraTargetInverted = ComputeCameraToTargetAngle(CAM);                 \
    SCHED_FENCE();                                                             \
    *(CameraAngles *)&g_nCameraSpringYaw = *(CameraAngles *)&g_nCameraYaw;     \
  }

SDATA(g_nCameraLosClear, 4);
SDATA(g_nCameraPassiveModeHoldTimer, 4);
SDATA(g_nCameraSpringReengageCountdown, 4);

/* Per-frame camera-mode arbiter (0x800357a4, 1972 bytes). Holds or releases
   the sentinel modes against the request bits (SPYRO_RQ_CAMERA ->
   CAM_MODE_REQUEST, SPYRO_RQ_PORTAL_PATH_CAMERA -> CAM_MODE_PORTAL_PATH,
   SPYRO_RQ_TARGET_CAMERA -> CAM_MODE_SCRIPTED_TARGET, SPYRO_RQ_FOLLOW_CAMERA
   -> CAM_MODE_SCRIPTED_FOLLOW) and otherwise falls back to the per-state mode
   table. Free-look exits to the passive hold CAM_HELD | CAM_MODE_PASSIVE when
   Triangle releases; the passive modes persist while the hold timer runs; an
   idle Spyro near the screen anchor re-enters passive mode. On a mode change,
   re-seats the spring angles where needed and, on entering free-look, clears
   the script-stick integrator. */
void SelectCameraMode(void) {
  int pt[3];
  int *nextp = &CAM[CAM_NEXT_MODE];
  int mode = *nextp;
  int table;
  unsigned int opts;

  if (mode == (CAM_HELD | CAM_MODE_FREE_LOOK)) {
    if (g_dwActiveCameraOptions & CAM_OPT_PASSIVE_FOLLOW) {
      if (g_nSpyroState == SPYRO_ST_IDLE || g_nSpyroState == SPYRO_ST_TEETER) {
        if (!(g_dwPadHeld & PAD_TRIANGLE)) {
          g_nCameraNextMode = (CAM_HELD | CAM_MODE_PASSIVE);
          g_nCameraPassiveModeHoldTimer = 0x2D;
          g_nSpyroSwingEulerTargetX = 0;
          g_nSpyroSwingEulerTargetY = 0;
          g_nSpyroSwingEulerTargetZ = 0;
        }
        goto done;
      }
    } else if ((g_dwPadHeld & PAD_TRIANGLE) && g_nSpyroState == SPYRO_ST_IDLE) {
      goto done;
    }
    {
      int t = STATE_MODE();

      g_nSpyroSwingEulerTargetX = 0;
      g_nSpyroSwingEulerTargetY = 0;
      g_nSpyroSwingEulerTargetZ = 0;
      *nextp = t;
    }
  } else if (mode == (CAM_HELD | CAM_MODE_REQUEST)) {
    if (!(SPY[SPY_REQUEST] & SPYRO_RQ_CAMERA)) {
      CAM[CAM_NEXT_MODE] = STATE_MODE();
      *(int **)&CAM[CAM_ANCHOR] = SPY;
      g_nCameraTargetInverted = ComputeCameraToTargetAngle(CAM);
      SCHED_FENCE();
      opts = g_dwActiveCameraOptions;
      *(CameraAngles *)&g_nCameraSpringYaw = *(CameraAngles *)&g_nCameraYaw;
      if (((opts & CAM_OPT_PASSIVE_FOLLOW) || g_nCameraForcePassiveFollow) &&
          CAM[CAM_NEXT_MODE] == CAM_MODE_STAND &&
          g_nCameraSpringSuppressFlag == 0) {
        CAM[CAM_NEXT_MODE] = (CAM_HELD | CAM_MODE_PASSIVE);
        goto hold;
      }
    } else if (CAM[CAM_CURRENT_MODE] != mode) {
      REBASE();
    }
  } else if (mode == (CAM_HELD | CAM_MODE_SCRIPTED_TARGET)) {
    if (!(SPY[SPY_REQUEST] & SPYRO_RQ_TARGET_CAMERA)) {
      CAM[CAM_NEXT_MODE] = STATE_MODE();
      *(int **)&CAM[CAM_ANCHOR] = SPY;
      REBASE();
    } else if (CAM[CAM_CURRENT_MODE] != mode) {
      REBASE();
    }
  } else if (mode == (CAM_HELD | CAM_MODE_SCRIPTED_FOLLOW)) {
    if (!(SPY[SPY_REQUEST] & SPYRO_RQ_FOLLOW_CAMERA)) {
      CAM[CAM_NEXT_MODE] = STATE_MODE();
      *(int **)&CAM[CAM_ANCHOR] = SPY;
      REBASE();
    } else if (CAM[CAM_CURRENT_MODE] != mode) {
      REBASE();
    }
  } else if (mode == (CAM_HELD | CAM_MODE_PORTAL_PATH)) {
    if (!(SPY[SPY_REQUEST] & SPYRO_RQ_PORTAL_PATH_CAMERA)) {
      table = STATE_MODE();
      *(int **)&CAM[CAM_ANCHOR] = SPY;
      *nextp = table;
    } else if (CAM[CAM_CURRENT_MODE] != mode) {
      if (g_nPortalCamPathReverseFlag != 0) {
        g_pPortalCamPathData->index = g_pPortalCamPathData->count - 1;
      } else {
        g_pPortalCamPathData->index = 0;
      }
    }
  } else if (mode == (CAM_HELD | CAM_MODE_PASSIVE)) {
  passive:
    table = STATE_MODE();
    if (table == 0 || table == 4) {
      if (g_nCameraPassiveModeHoldTimer <= 0) {
        goto fallback;
      }
    } else {
      *nextp = table;
    }
  } else if (mode == (CAM_HELD | CAM_MODE_PASSIVE_SETTLE)) {
    goto passive;
  } else if (mode == CAM_HELD) {
    if (SPY[SPY_SPEED] > 0x400) {
      CAM[CAM_NEXT_MODE] = STATE_MODE();
    } else {
      ProjectWorldPointGTEUnscaled(pt, SPY);
      pt[0] -= 0x100;
      pt[1] -= 0x78;
      if (abs(pt[0]) > 0x40 || abs(pt[1]) > 0x28 || (unsigned)pt[2] > 0x1400) {
      fallback:
        g_nCameraNextMode = STATE_MODE();
      }
    }
  } else if (mode == 0 && SPY[SPY_SCRIPTED_MOVE] == 0 &&
             ((g_dwActiveCameraOptions & CAM_OPT_PASSIVE_FOLLOW) ||
              g_nCameraForcePassiveFollow) &&
             g_nCameraSpringSuppressFlag == 0) {
    ProjectWorldPointGTEUnscaled(pt, SPY);
    pt[0] -= 0x100;
    pt[1] -= 0x78;
    if (abs(pt[0]) > 0x40 || abs(pt[1]) > 0x28 || (unsigned)pt[2] > 0x1400) {
      g_nCameraNextMode = (CAM_HELD | CAM_MODE_PASSIVE);
    hold:
      g_nCameraPassiveModeHoldTimer = 0x2D;
    }
  } else {
    if (*(int **)&CAM[CAM_ANCHOR] != g_anSpyroWorldPos &&
        CAM[CAM_CURRENT_MODE] != CAM_MODE_HAZARD) {
      *(int **)&CAM[CAM_ANCHOR] = g_anSpyroWorldPos;
      REBASE();
    }
  }
done:
  if (CAM[CAM_NEXT_MODE] != g_nCameraCurrentMode) {
    if (((g_dwActiveCameraOptions & CAM_OPT_PASSIVE_FOLLOW) ||
         g_nCameraForcePassiveFollow) &&
        SPY[SPY_SCRIPTED_MOVE] == 0 && g_nCameraSpringSuppressFlag == 0 &&
        (CAM[CAM_NEXT_MODE] == CAM_MODE_STAND ||
         CAM[CAM_NEXT_MODE] == (CAM_HELD | CAM_MODE_STAND)) &&
        g_nCameraCurrentMode != (CAM_HELD | CAM_MODE_PASSIVE_SETTLE) &&
        *(int **)&CAM[CAM_ANCHOR] == SPY) {
      g_nCameraPassiveModeHoldTimer = 8;
      CAM[CAM_NEXT_MODE] = (CAM_HELD | CAM_MODE_PASSIVE_SETTLE);
      g_nCameraSpringReengageCountdown = 0x5A;
    } else if (CAM[CAM_CURRENT_MODE] == CAM_MODE_SIDESTEP ||
               CAM[CAM_CURRENT_MODE] ==
                   (CAM_HELD | CAM_MODE_SCRIPTED_VANTAGE)) {
      REBASE();
    }
    g_nCameraCurrentMode = g_nCameraNextMode;
    if (g_nCameraCurrentMode == (CAM_HELD | CAM_MODE_FREE_LOOK)) {
      if (g_nCameraLosClear != 0) {
        g_nCameraCollisionRetries = 0;
      }
      g_anCameraScriptStickIntegrator[0] = 0;
      g_anCameraScriptStickIntegrator[1] = 0;
    }
  }
}

#undef CAM_CURRENT_MODE
#undef CAM_NEXT_MODE
#undef CAM_ANCHOR
#undef STATE_MODE
#undef REBASE

SDATA(g_nCameraManualYawInput, 4);

/* R1/L1 camera rotate (0x80035f58, 92 bytes): writes +-0x400 to
   g_nCameraManualYawInput, the yaw-drive override AdvanceCameraSpringStep
   consumes. Suppressed by request bit 0x1000 or while the camera is stuck. */
void UpdateCameraManualYawInput(void) {
  g_nCameraManualYawInput = 0;
  if (g_dwSpyroRequestMask & SPYRO_RQ_LOCK_ON) {
    return;
  }
  if (g_nCameraStuckFrames != 0) {
    return;
  }
  if (g_dwPadHeld & PAD_R2) {
    g_nCameraManualYawInput = -0x400;
  } else if (g_dwPadHeld & PAD_L2) {
    g_nCameraManualYawInput = 0x400;
  }
}

/* Register-base view of g_anCameraScriptStickIntegrator. */

#define CAM g_anCameraPosBlock
#define CAM_CURRENT_MODE 0x0C  /* g_nCameraCurrentMode */
#define CAM_STUCK_FRAMES 0x28  /* g_nCameraStuckFrames */
#define CAM_ANCHOR 0x2A        /* g_pCameraAnchorPos */
#define CAM_TARGET_PARAMS 0x2C /* g_pCameraTargetParams */
#define CAM_SUPPRESS 0x32      /* g_nCameraSpringSuppressFlag */
#define STICK g_anCameraScriptStickIntegratorBlock

/* Offsets from the stuck-frames field (the function's held base `s`). */
#define S_POS (-0x28) /* g_anCameraPos */
#define S_TARGET_YAW_BIAS (-0x15)
#define S_SPRING_YAW (-0x14)
#define S_SPRING_PITCH (-0x13)
#define S_YAW (-0x0E)
#define S_PITCH (-0x0D)
#define S_CURRENT_MODE (-0x1C)
#define S_TARGET_PARAMS 0x04
#define S_INVERTED 0x08

/* A scripted camera actor's world x, read as a raw int field. */
#define ACTOR_X(a) ACTOR_RAW(a, int, posX)

SDATA(g_nCameraLosClear, 4);
SDATA(g_nCameraManualYawInput, 4);
SDATA(g_nCameraPassiveModeHoldTimer, 4);
SDATA(g_nCameraSpringReengageCountdown, 4);

/* Re-seat all six spring angles on the angles recomputed from the eye. */
#define REBASE_ALL(pos)                                                        \
  {                                                                            \
    g_nCameraTargetInverted = ComputeCameraToTargetAngle(pos);                 \
    *(CameraAngles *)&g_nCameraSpringYaw = *(CameraAngles *)&g_nCameraYaw;     \
  }

/* Look-ahead yaw bias: the angle between Spyro and Spyro plus a scaled
   motion step, as seen from the camera. */
#define LOOKAHEAD_BIAS(rel, ahead, pos, shift, out)                            \
  {                                                                            \
    int a;                                                                     \
                                                                               \
    SubtractVector(rel, g_anSpyroWorldPos, pos);                               \
    ScaleVector3Sat(ahead, &g_anSpyroWorldPos[SPY_MOTION],                     \
                    g_bCameraLookaheadMotionScale);                            \
    RShiftVector3(ahead, shift);                                               \
    AddVector(ahead, rel, ahead);                                              \
    a = ArcTan2(rel[0], rel[1], ATAN_ANGLE12);                                 \
    out = (a - ArcTan2(ahead[0], ahead[1], ATAN_ANGLE12)) & ANGLE12_MASK;      \
  }

/* Per-frame camera spring driver (0x80035fb4, 5208 bytes). Picks the target
   parameter block and anchor for the current mode (passive look-around,
   behind-Spyro modes 1/2/4 front or reverse, mode 3, strafe-biased mode 5,
   latched gem-grab mode 6, script-stick free look 9, scripted target 7 and
   follow 0xC, request mode 0xA), then springs toward it g_nFrameStep times
   with the stuck-escalated tuning column and re-syncs the spring angles. */
void UpdateCameraSpringForMode(void) {
  /* The original frame reserves seven 16-byte vectors; only four are used
     (sp+0x10/0x20 by mode 1, sp+0x30/0x50 by mode 5). */
  int rel[4];
  int ahead[4];
  int rel2[4];
  int unused0[4];
  int ahead2[4];
  int unused1[4];
  int unused2[4];
  int column;
  int *s;
  int i;

  if (((g_dwActiveCameraOptions & CAM_OPT_PASSIVE_FOLLOW) ||
       g_nCameraForcePassiveFollow) &&
      CAM[CAM_CURRENT_MODE] == CAM_MODE_BONK) {
    CAM[CAM_CURRENT_MODE] = CAM_MODE_STAND;
  }
  OPAQUE_COPY(s, &CAM[CAM_STUCK_FRAMES]);
  column = (*s + 1) >> 1;
  if (column >= 5) {
    column = 4;
  }
  switch (s[S_CURRENT_MODE]) {
  case CAM_MODE_STAND:
  case (CAM_HELD | CAM_MODE_STAND):
    if (!((g_dwActiveCameraOptions & CAM_OPT_PASSIVE_FOLLOW) &&
          g_nCameraSpringSuppressFlag == 0 && g_nCameraManualYawInput == 0 &&
          g_nCameraCollisionRetries == 0 &&
          (g_nCameraSpringReengageCountdown != 0 ||
           g_nSpyroState == SPYRO_ST_HURT_LONG))) {
      int yaw;

      *(int **)&CAM[CAM_TARGET_PARAMS] = g_anCameraParamDefault;
      yaw = SPY[SPY_BODY_YAW];
      g_pCameraAnchorPos = SPY;
      g_nCameraParamsReferenceYaw = yaw;
      g_nCameraTargetInverted = ComputeCameraToTargetAngle(CAM);
      g_nCameraSpringYaw = g_nCameraYaw;
      if (g_nCameraCollisionRetries == 0 ||
          (int)g_dwGamestateFrames < CAM_SETTLE_FRAMES) {
        LoadCameraTargetAnglesFromMode();
      }
    }
    if (((g_dwActiveCameraOptions & CAM_OPT_PASSIVE_FOLLOW) ||
         g_nCameraForcePassiveFollow) &&
        g_nPadAllInputIdleFlag && g_nCameraLosClear &&
        g_nCameraSpringSuppressFlag == 0) {
      int left = g_nCameraSpringReengageCountdown;
      int d;
      int p;

      if (left == 0) {
        break;
      }
      d = (g_nCameraYaw - ANGLE12_HALF + g_nSpyroBodyYaw) & ANGLE12_MASK;
      if (d > ANGLE12_HALF) {
        d -= ANGLE12_TURN;
      }
      p = (g_nCameraPitch - g_anCameraParamDefault[1]) & ANGLE12_MASK;
      if (p > ANGLE12_HALF) {
        p -= ANGLE12_TURN;
      }
      if (abs(d) < CAM_REENGAGE_YAW && abs(p) < CAM_REENGAGE_PITCH) {
        break;
      }
      g_nCameraSpringReengageCountdown = left - g_nFrameStep;
      if (g_nCameraSpringReengageCountdown > 0) {
        break;
      }
      g_nCameraSpringReengageCountdown = 0;
      g_anCameraSpringDeltaState[0] = 0;
      g_anCameraSpringDeltaState[1] = 0;
      g_anCameraSpringDeltaState[2] = 0;
      g_anCameraSpringDeltaState[3] = 0;
      g_anCameraSpringDeltaState[4] = 0;
      g_anCameraSpringDeltaState[5] = 0;
    } else {
      g_nCameraSpringReengageCountdown = CAM_REENGAGE_FRAMES;
    }
    break;
  case (CAM_HELD | CAM_MODE_PASSIVE): {
    int yaw;

    g_pCameraTargetParams = g_anCameraParamDefault;
    yaw = SPY[SPY_BODY_YAW];
    g_pCameraAnchorPos = SPY;
    g_nCameraParamsReferenceYaw = yaw;
    g_nCameraTargetInverted = ComputeCameraToTargetAngle(&s[S_POS]);
    g_nCameraSpringYaw = g_nCameraYaw;
    if (g_nCameraCollisionRetries == 0) {
      LoadCameraTargetAnglesFromMode();
    }
  }
    /* FALLTHROUGH */
  case (CAM_HELD | CAM_MODE_PASSIVE_SETTLE):
    g_nCameraPassiveModeHoldTimer--;
    break;
  case CAM_MODE_MOVE: {
    int yaw = g_nSpyroBodyYaw;

    *(int **)&CAM[CAM_ANCHOR] = g_anSpyroWorldPos;
    g_nCameraParamsReferenceYaw = yaw;
    if (g_nCameraCollisionRetries != 0) {
      break;
    }
    if (AbsAngleDelta12(yaw, g_nCameraEulerYaw) > CAM_FACING_ANGLE) {
      g_pCameraTargetParams = g_anCameraParamFollowFacing;
    } else {
      g_pCameraTargetParams = g_anCameraParamFollowBehind;
      if (!(g_dwActiveCameraOptions & CAM_OPT_SMOOTH_SWING)) {
        g_nCameraTargetInverted = ComputeCameraToTargetAngle(CAM);
        g_nCameraSpringYaw = g_nCameraYaw;
      }
    }
    LoadCameraTargetAnglesFromMode();
    LOOKAHEAD_BIAS(rel, ahead, g_anCameraPos, 6, g_nCameraTargetYawBias);
    break;
  }
  case CAM_MODE_CHARGE: {
    int yaw = g_nSpyroBodyYaw;

    g_pCameraTargetParams = g_anCameraParamCharge;
    g_pCameraAnchorPos = g_anSpyroWorldPos;
    g_nCameraParamsReferenceYaw = yaw;
    g_nCameraTargetInverted = ComputeCameraToTargetAngle(&s[S_POS]);
    g_nCameraSpringYaw = g_nCameraYaw;
    if (g_nCameraCollisionRetries != 0) {
      REBASE_ALL(&s[S_POS]);
      break;
    }
    LoadCameraTargetAnglesFromMode();
    g_nCameraTargetYawBias = (-g_nSpyroBodyYawStep << 3) & ANGLE12_MASK;
    break;
  }
  case CAM_MODE_GLIDE: {
    int yaw = g_nSpyroBodyYaw;

    g_pCameraAnchorPos = g_anSpyroWorldPos;
    g_nCameraParamsReferenceYaw = yaw;
    if (g_nCameraCollisionRetries == 0) {
      if (g_nSpyroFlightMode != 0) {
        if (g_nSpyroSubstate == SPYRO_SUB_GLIDE_L1) {
          yaw = (yaw - g_nCameraEulerYaw) & ANGLE12_MASK;
          if (yaw > ANGLE12_HALF) {
            yaw -= ANGLE12_TURN;
          }
          if (yaw > 0) {
            *(int **)&s[S_TARGET_PARAMS] = g_anCameraParamGlideRightBias;
          } else {
            *(int **)&s[S_TARGET_PARAMS] = g_anCameraParamGlideLeftBias;
          }
          CAM[0x2B] = (g_nSpyroBodyYaw + ANGLE12_HALF) & ANGLE12_MASK;
          g_nCameraTargetInverted = ComputeCameraToTargetAngle(CAM);
          g_nCameraSpringYaw = g_nCameraYaw;
          LoadCameraTargetAnglesFromMode();
        } else if (g_nSpyroSubstate == SPYRO_SUB_GLIDE_R1) {
          yaw = (yaw - g_nCameraEulerYaw) & ANGLE12_MASK;
          if (yaw > ANGLE12_HALF) {
            yaw -= ANGLE12_TURN;
          }
          if (yaw > CAM_GLIDE_BIAS_ANGLE) {
            *(int **)&s[S_TARGET_PARAMS] = g_anCameraParamGlideRightBias;
          } else if (yaw < -CAM_GLIDE_BIAS_ANGLE) {
            *(int **)&s[S_TARGET_PARAMS] = g_anCameraParamGlideLeftBias;
          } else {
            *(int **)&s[S_TARGET_PARAMS] = g_anCameraParamFollowBehind;
          }
          g_nCameraTargetInverted = ComputeCameraToTargetAngle(g_anCameraPos);
          g_nCameraSpringYaw = g_nCameraYaw;
        } else {
          *(int **)&s[S_TARGET_PARAMS] = g_anCameraParamFollowBehind;
          s[S_INVERTED] = ComputeCameraToTargetAngle(&s[S_POS]);
          s[S_SPRING_YAW] = s[S_YAW];
          s[S_SPRING_PITCH] = s[S_PITCH];
          LoadCameraTargetAnglesFromMode();
          LOOKAHEAD_BIAS(rel2, ahead2, &s[S_POS], 5, s[S_TARGET_YAW_BIAS]);
        }
      } else {
        int r = AbsAngleDelta12(yaw, g_nCameraEulerYaw);

        if (g_nSpyroSubstate != SPYRO_SUB_GLIDE_ENTER && r > CAM_FACING_ANGLE) {
          g_pCameraTargetParams = g_anCameraParamFollowFacing;
        } else {
          *(int **)&CAM[CAM_TARGET_PARAMS] = g_anCameraParamFollowBehind;
          g_nCameraTargetInverted = ComputeCameraToTargetAngle(CAM);
          g_nCameraSpringYaw = g_nCameraYaw;
          g_nCameraSpringPitch = g_nCameraPitch;
        }
        LoadCameraTargetAnglesFromMode();
        LOOKAHEAD_BIAS(rel2, ahead2, g_anCameraPos, 5, g_nCameraTargetYawBias);
      }
    } else {
      REBASE_ALL(&s[S_POS]);
    }
    break;
  }
  case CAM_MODE_HAZARD:
    if (g_pCameraAnchorPos != g_anCameraLatchedAnchorPos) {
      CopyVector(g_anCameraLatchedAnchorPos, g_pCameraAnchorPos);
      g_pCameraAnchorPos = g_anCameraLatchedAnchorPos;
    }
    g_nCameraManualYawInput = 0;
    if (abs(g_anCameraSpringDeltaState[0]) +
            abs(g_anCameraSpringDeltaState[1]) +
            abs(g_anCameraSpringDeltaState[2]) +
            abs(g_anCameraSpringDeltaState[3]) +
            abs(g_anCameraSpringDeltaState[4]) +
            abs(g_anCameraSpringDeltaState[5]) >
        CAM_SPRING_REST) {
      for (i = 0; i < g_nFrameStep; i++) {
        AdvanceCameraSpringStep(0);
      }
      ComputeCameraOrbitOffset(g_anCameraEyeTarget);
      AddVector(g_anCameraEyeTarget, g_anCameraEyeTarget, g_pCameraAnchorPos);
      SnapCameraToTarget();
      UpdateCameraEulerAngles();
    }
    g_nCameraTargetInverted = ComputeCameraToTargetAngle(g_anCameraPos);
    g_nCameraSpringYaw = g_nCameraYaw;
    g_nCameraSpringPitch = g_nCameraPitch;
    g_nCameraSpringDistance = g_nCameraDistance;
    g_nCameraSpringRoll = g_nCameraDraftRoll;
    g_nCameraSpringPitchBias = g_nCameraDraftPitchBias;
    g_nCameraSpringYawBias = g_nCameraDraftYawBias;
    return;
  case CAM_MODE_FALL: {
    int yaw = g_nSpyroBodyYaw;

    g_pCameraAnchorPos = g_anSpyroWorldPos;
    g_nCameraParamsReferenceYaw = yaw;
    if (g_nCameraCollisionRetries == 0) {
      if (AbsAngleDelta12(yaw, g_nCameraEulerYaw) > CAM_FACING_ANGLE) {
        g_pCameraTargetParams = g_anCameraParamFallFacing;
      } else {
        g_pCameraTargetParams = g_anCameraParamFallBehind;
        if (!(g_dwActiveCameraOptions & CAM_OPT_SMOOTH_SWING)) {
          g_nCameraTargetInverted = ComputeCameraToTargetAngle(&s[S_POS]);
          g_nCameraSpringYaw = g_nCameraYaw;
        }
      }
      LoadCameraTargetAnglesFromMode();
    } else {
      REBASE_ALL(&s[S_POS]);
    }
    break;
  }
  case (CAM_HELD | CAM_MODE_FREE_LOOK): {
    int yaw = g_nSpyroBodyYaw;
    int *ig;
    int t;
    int r;

    g_pCameraTargetParams = g_anCameraParamFreeLook;
    g_pCameraAnchorPos = g_anSpyroWorldPos;
    g_nCameraParamsReferenceYaw = yaw;
    g_nCameraTargetInverted = ComputeCameraToTargetAngle(&s[S_POS]);
    g_nCameraSpringYaw = g_nCameraYaw;
    g_nCameraSpringPitch = g_nCameraPitch;
    g_nCameraSpringDistance = g_nCameraDistance;
    if (g_nCameraCollisionRetries == 0) {
      unsigned char *pad;

      LoadCameraTargetAnglesFromMode();
      pad = (unsigned char *)g_pPadSubstepState;
      if (((PadSubstep *)pad)->padType == PAD_TYPE_ANALOG &&
          (((PadSubstep *)pad)->analog.word & PAD_LEFT_STICK_MASK) !=
              PAD_LEFT_STICK_CENTERED) {
        g_anCameraScriptStickIntegrator[0] +=
            (((PadSubstep *)pad)->analog.axis[PAD_AXIS_LX] -
             PAD_STICK_CENTER) >>
            1;
        g_anCameraScriptStickIntegrator[1] +=
            (PAD_STICK_CENTER -
             ((PadSubstep *)pad)->analog.axis[PAD_AXIS_LY]) >>
            1;
      } else {
        if (g_dwPadHeld & PAD_LEFT) {
          STICK[0] -= CAM_LOOK_DPAD_STEP;
        } else if (g_dwPadHeld & PAD_RIGHT) {
          STICK[0] += CAM_LOOK_DPAD_STEP;
        }
        if (g_dwPadHeld & PAD_UP) {
          STICK[1] += CAM_LOOK_DPAD_STEP;
        } else if (g_dwPadHeld & PAD_DOWN) {
          STICK[1] -= CAM_LOOK_DPAD_STEP;
        }
      }
      OPAQUE_COPY(ig, STICK);
      if (ig[0] < -CAM_LOOK_MAX_YAW) {
        ig[0] = -CAM_LOOK_MAX_YAW;
      }
      if (ig[0] > CAM_LOOK_MAX_YAW) {
        ig[0] = CAM_LOOK_MAX_YAW;
      }
      if (ig[1] < -CAM_LOOK_MAX_DOWN) {
        ig[1] = -CAM_LOOK_MAX_DOWN;
      }
      if (ig[1] > CAM_LOOK_MAX_UP) {
        ig[1] = CAM_LOOK_MAX_UP;
      }
      g_nCameraTargetYaw += ig[0];
      t = ig[1];
      if (t < -CAM_LOOK_PITCH_SPLIT) {
        g_nCameraTargetPitch -= CAM_LOOK_PITCH_SPLIT;
        g_nCameraTargetPitchBias += CAM_LOOK_PITCH_SPLIT + t;
      } else {
        g_nCameraTargetPitch += t;
      }
      r = -g_anCameraScriptStickIntegrator[0];
      if (r < -CAM_LOOK_HEAD_YAW) {
        r = -CAM_LOOK_HEAD_YAW;
      }
      if (r > CAM_LOOK_HEAD_YAW) {
        r = CAM_LOOK_HEAD_YAW;
      }
      g_nSpyroSwingEulerTargetZ = r;
      r = -g_anCameraScriptStickIntegrator[1];
      if (r < -CAM_LOOK_HEAD_PITCH) {
        r = -CAM_LOOK_HEAD_PITCH;
      }
      if (r > CAM_LOOK_HEAD_PITCH) {
        r = CAM_LOOK_HEAD_PITCH;
      }
      g_nSpyroSwingEulerTargetY = r;
    }
    column = 5;
    break;
  }
  case CAM_MODE_SIDESTEP: {
    int yaw = g_nSpyroBodyYaw;

    g_pCameraAnchorPos = g_anSpyroWorldPos;
    g_nCameraParamsReferenceYaw = yaw;
    if (g_nCameraCollisionRetries == 0) {
      if (AbsAngleDelta12(yaw, g_nCameraEulerYaw) > CAM_FACING_ANGLE) {
        g_pCameraTargetParams = g_anCameraParamFollowFacing;
      } else {
        g_pCameraTargetParams = g_anCameraParamFollowBehind;
      }
      LoadCameraTargetAnglesFromMode();
    } else {
      REBASE_ALL(&s[S_POS]);
    }
    break;
  }
  case (CAM_HELD | CAM_MODE_SCRIPTED_TARGET): {
    Actor *actor;
    int y;
    int z;

    g_pCameraTargetParams = g_anCameraParamScriptedTarget;
    actor = g_pScriptedCameraActor;
    g_pCameraAnchorPos = g_anCameraScriptedTargetPos;
    g_pCameraAnchorPos[0] = ACTOR_X(actor);
    z = g_nSpyroWorldPosZ;
    y = actor->posY;
    g_anCameraScriptedTargetPos[2] = z;
    g_anCameraScriptedTargetPos[1] = y;
    g_nCameraParamsReferenceYaw = ANGLE8_TO_12(actor->rotZ);
    g_nCameraTargetInverted = ComputeCameraToTargetAngle(&s[S_POS]);
    g_nCameraSpringYaw = g_nCameraYaw;
    g_nCameraSpringPitch = g_nCameraPitch;
    g_nCameraSpringDistance = g_nCameraDistance;
    LoadCameraTargetAnglesFromMode();
    break;
  }
  case (CAM_HELD | CAM_MODE_SCRIPTED_FOLLOW): {
    Actor *actor = g_pScriptedCameraActor;

    g_pCameraAnchorPos = g_anSpyroWorldPos;
    g_nCameraParamsReferenceYaw =
        ArcTan2(ACTOR_X(actor) - g_pCameraAnchorPos[0],
                actor->posY - g_nSpyroWorldPosY, ATAN_ANGLE12);
    if (g_nCameraCollisionRetries != 0) {
      break;
    }
    g_pCameraTargetParams = g_anCameraParamScriptedFollow;
    g_nCameraTargetInverted = ComputeCameraToTargetAngle(&s[S_POS]);
    LoadCameraTargetAnglesFromMode();
    break;
  }
  case (CAM_HELD | CAM_MODE_REQUEST):
    g_nCameraTargetInverted = ComputeCameraToTargetAngle(&s[S_POS]);
    g_nCameraSpringYaw = g_nCameraYaw;
    g_nCameraSpringPitch = g_nCameraPitch;
    g_nCameraSpringDistance = g_nCameraDistance;
    if (g_nCameraCollisionRetries == 0) {
      LoadCameraTargetAnglesFromMode();
    } else {
      REBASE_ALL(&s[S_POS]);
    }
    if (g_dwSpyroRequestMask & SPYRO_RQ_LOCK_ON) {
      column = 5;
    }
    break;
  case CAM_MODE_BONK: {
    int yaw = g_nSpyroBodyYaw;

    g_pCameraTargetParams = g_anCameraParamDefault;
    g_pCameraAnchorPos = g_anSpyroWorldPos;
    g_nCameraParamsReferenceYaw = yaw;
    g_nCameraTargetInverted = ComputeCameraToTargetAngle(&s[S_POS]);
    g_nCameraSpringYaw = g_nCameraYaw;
    g_nCameraSpringDistance = g_nCameraDistance;
    if (g_nCameraCollisionRetries == 0) {
      LoadCameraTargetAnglesFromMode();
      break;
    }
    REBASE_ALL(&s[S_POS]);
    break;
  }
  }
  if (g_nCameraCurrentMode != (CAM_HELD | CAM_MODE_FREE_LOOK) &&
      g_nCameraCurrentMode != (CAM_HELD | CAM_MODE_SCRIPTED_TARGET) &&
      g_nCameraCurrentMode != (CAM_HELD | CAM_MODE_LEVEL_TRANSITION) &&
      g_nCameraCurrentMode != (CAM_HELD | CAM_MODE_PORTAL_PATH) &&
      g_nCameraCurrentMode != (CAM_HELD | CAM_MODE_SCRIPTED_VANTAGE) &&
      g_nCameraCurrentMode != CAM_MODE_CHARGE) {
    UpdateCameraManualYawInput();
  } else {
    g_nCameraManualYawInput = 0;
  }
  if (((g_dwActiveCameraOptions & CAM_OPT_PASSIVE_FOLLOW) ||
       g_nCameraForcePassiveFollow) &&
      g_nCameraLosClear && g_nCameraSpringReengageCountdown &&
      CAM[CAM_SUPPRESS] == 0 && g_nCameraManualYawInput == 0 &&
      (g_nCameraCurrentMode == CAM_MODE_STAND ||
       g_nCameraCurrentMode == CAM_HELD)) {
    CopyVector(&CAM[3], CAM);
    g_anCameraSpringDeltaState[0] = 0;
    g_anCameraSpringDeltaState[1] = 0;
    g_anCameraSpringDeltaState[2] = 0;
    g_anCameraSpringDeltaState[3] = 0;
    g_anCameraSpringDeltaState[4] = 0;
    g_anCameraSpringDeltaState[5] = 0;
  } else {
    for (i = 0; i < g_nFrameStep; i++) {
      AdvanceCameraSpringStep(column);
    }
    ComputeCameraOrbitOffset(g_anCameraEyeTarget);
    AddVector(g_anCameraEyeTarget, g_anCameraEyeTarget, g_pCameraAnchorPos);
    UpdateGameplayCamera();
    UpdateCameraEulerAngles();
  }
  g_nCameraTargetInverted = ComputeCameraToTargetAngle(g_anCameraPos);
  if (!((g_dwActiveCameraOptions & CAM_OPT_PASSIVE_FOLLOW) &&
        g_nSpyroState == SPYRO_ST_JUMP && g_nCameraSpringSuppressFlag == 0 &&
        g_nSpyroMotionSpeed < SPYRO_PAD_TURN_SPEED)) {
    g_nCameraSpringYaw = g_nCameraYaw;
  }
  g_nCameraSpringPitch = g_nCameraPitch;
  g_nCameraSpringDistance = g_nCameraDistance;
  g_nCameraSpringRoll = g_nCameraDraftRoll;
  g_nCameraSpringPitchBias = g_nCameraDraftPitchBias;
  g_nCameraSpringYawBias = g_nCameraDraftYawBias;
}

#undef CAM_CURRENT_MODE
#undef CAM_STUCK_FRAMES
#undef CAM_ANCHOR
#undef CAM_TARGET_PARAMS
#undef CAM_SUPPRESS
#undef STICK
#undef S_POS
#undef S_TARGET_YAW_BIAS
#undef S_SPRING_YAW
#undef S_SPRING_PITCH
#undef S_YAW
#undef S_PITCH
#undef S_CURRENT_MODE
#undef S_TARGET_PARAMS
#undef S_INVERTED
#undef ACTOR_X
#undef REBASE_ALL
#undef LOOKAHEAD_BIAS

/* The path block is re-read from the global on every use (the overlay can
   swap it out under us). */
#define PATH (*(PortalCamPath *volatile *)&g_pPortalCamPathData)

/* Camera updater for mode CAM_HELD | CAM_MODE_PORTAL_PATH (portal spline path).
   Walks g_anCameraPos along the overlay-provided path at g_pPortalCamPathData:
   count byte [0], current index byte [1], waypoint vec3s at +8 + i*0x10.
   Within 0x200 of the current waypoint the index advances (forward) or
   retreats (g_nPortalCamPathReverseFlag). Speed: while an onward waypoint
   exists, dot(camera->player excess beyond 0x400, segment tangent normalised
   to 0x1000) << 7 >> 12; at the path ends the step is capped by
   g_nPortalCamPathMaxStep. (0x8003740c, 776 bytes.) */
void UpdatePortalPathCamera(void) {
  int tangent[4];
  int toPlayer[4];
  int toWaypoint[4];
  PortalCamPath *path;
  int idx;
  int dist;
  int span;
  int step;
  int quot;

  idx = PATH->index;
  SubtractVector(toWaypoint, PATH->point[idx].pos, g_anCameraPos);
  dist = VectorLength(toWaypoint, VEC_LEN_XYZ);
  if (dist < 0x200) {
    if (g_nPortalCamPathReverseFlag != 0) {
      path = PATH;
      if (path->index != 0) {
        path->index = path->index - 1;
      }
    } else {
      path = PATH;
      if (path->index < path->count - 1) {
        path->index = path->index + 1;
      }
    }
  }

  step = 0;
  if (g_nPortalCamPathReverseFlag != 0) {
    step = idx < PATH->count - 1;
  } else if (idx != 0) {
    step = 1;
  }

  if (step != 0) {
    SubtractVector(toPlayer, g_anSpyroWorldPos, g_anCameraPos);
    span = VectorLength(toPlayer, VEC_LEN_XYZ);
    step = 0;
    if (span > 0x400) {
      step = (span - 0x400) * 4;
      if (span < step) {
        step = span;
      }
      ScaleVector3ByRatio(toPlayer, span, step);
      span = step;
      if (g_nPortalCamPathReverseFlag != 0) {
        SubtractVector(tangent, g_pPortalCamPathData->point[idx].pos,
                       g_pPortalCamPathData->point[idx + 1].pos);
      } else {
        SubtractVector(tangent, g_pPortalCamPathData->point[idx].pos,
                       g_pPortalCamPathData->point[idx - 1].pos);
      }
      SetVector3Magnitude(tangent, 0x1000);
      quot = (tangent[0] * toPlayer[0] + tangent[1] * toPlayer[1] +
              tangent[2] * toPlayer[2]) /
             span;
      step = abs(quot);
      if (step < 0) {
        step = 0;
      }
      if (step > 0x1000) {
        step = 0x1000;
      }
      step = (step << 7) >> 12;
    }
  } else {
    if (g_nPortalCamPathMaxStep < (step = dist)) {
      step = g_nPortalCamPathMaxStep;
    }
  }

  span = dist;
  ScaleVector3ByRatio(toWaypoint, span, step * g_nFrameStep);
  AddVector(g_anCameraPos, g_anCameraPos, toWaypoint);
  g_nCameraTargetInverted = ComputeCameraToTargetAngle(g_anCameraPos);
  UpdateCameraEulerAngles();
}

#undef PATH

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

#undef VIEW
#undef CAMPOS
#undef CAMERA_EULER

/* The active scripted-camera record pointer (g_pScriptedCameraView) and the
   camera position 0xc4 below it are reached through one held base register. */
#define VIEW (g_apScriptedCameraViewBlock[0])
#define CAMPOS ((int *)&g_apScriptedCameraViewBlock[-49])

/* Camera updater for mode CAM_HELD | CAM_MODE_SCRIPTED_VANTAGE (scripted
   vantage triplet). While the level-load state machine is finished
   (g_nCdStreamState < LSTREAM_PRESERVE) it just re-picks the per-state camera
   mode, unless Spyro is in one of the states that own the camera themselves.
   Otherwise it retires the active record when the player has walked past its
   range (range) or left its screen window (projected y > 0xf0), draws one of
   the three ROM records at g_aScriptedCamViews (avoiding an immediate repeat)
   and re-seats Spyro's body angles from it. Records with aimAtSpyro set also
   refresh the camera euler angles. (0x800377a8, 632 bytes.) */
void UpdateScriptedCameraView(void) {
  int delta[3];
  int dist;
  int n;

  n = 0;
  if (g_nCdStreamState < LSTREAM_PRESERVE) {
    if (g_nSpyroState != SPYRO_ST_FALL &&
        g_nSpyroState != SPYRO_ST_GLIDE_DROP && g_nSpyroState != SPYRO_ST_FLY &&
        g_nSpyroState != SPYRO_ST_FLY_CLIMB &&
        g_nSpyroState != SPYRO_ST_FLY_DIVE && g_nSpyroState != SPYRO_ST_GLIDE &&
        g_nSpyroState != SPYRO_ST_HAZARD) {
      g_nCameraNextMode =
          (&g_abSpyroStateCameraMode)[*(volatile int *)&g_nSpyroState];
    }
  } else {
    if (VIEW == 0) {
      n = 1;
    } else {
      SubtractVector(delta, CAMPOS, g_anSpyroWorldPos);
      dist = VectorLength(delta, VEC_LEN_XYZ);
      SubtractVector(delta, CAMPOS,
                     SPY_VIA(g_anSpyroWorldPos, SPY_POS, SPY_PREV_POS));
      if (VIEW->range < dist && VectorLength(delta, VEC_LEN_XYZ) < dist) {
        n = 1;
      } else {
        ProjectWorldPointGTEUnscaled(delta, g_anSpyroWorldPos);
        if (delta[1] >= 0xf1) {
          n = 1;
        }
      }
    }
    if (n != 0 ||
        g_nCameraCurrentMode != (CAM_HELD | CAM_MODE_SCRIPTED_VANTAGE)) {
      n = GetRandomU32() & 0xfff;
      n = (n * 3) >> 12;
      if (VIEW == &g_aScriptedCamViews[n]) {
        n = (n + 1) % 3;
      }
      ActivateScriptedCameraView(&g_aScriptedCamViews[n]);
      CopyVector(g_anSpyroWorldPos, VIEW->spyroPos);
      *(volatile int *)&g_nSpyroBodyPitch = *(volatile int *)&VIEW->spyroPitch;
      *(volatile int *)&g_nSpyroBodyRoll = *(volatile int *)&VIEW->spyroRoll;
      *(volatile int *)&g_nSpyroBodyYaw = *(volatile int *)&VIEW->spyroYaw;
    }
  }
  if (VIEW->aimAtSpyro == 1) {
    SubtractVector(delta, g_anSpyroWorldPos, CAMPOS);
    g_nCameraTargetInverted = ComputeCameraToTargetAngle(CAMPOS);
    UpdateCameraEulerAngles();
  }
}

#undef VIEW
#undef CAMPOS

SDATA(g_nCameraManualYawInput, 4);

/* Level-transition camera, mode CAM_HELD | CAM_MODE_LEVEL_TRANSITION
   (0x80037a20, 436 bytes), set with GS_LEVEL_LOAD by portal entry,
   BeginWorldHubReturn and level select: anchors on Spyro with the body yaw as
   the yaw bias and picks the target-params block (portal exit: by CD stream
   state; otherwise once, by which side of the camera yaw Spyro faces). Springs
   column 0 once per frame step with no manual yaw, seats the eye, then copies
   the recomputed angles into the spring set. */
void UpdateLevelTransitionCamera(void) {
  int i;
  int yaw = g_nSpyroBodyYaw;

  g_pCameraAnchorPos = g_anSpyroWorldPos;
  g_nCameraParamsReferenceYaw = yaw;
  if (g_nReturnPortalLevelId != 0) {
    if (g_nCdStreamState >= LSTREAM_RELOCATE) {
      g_pCameraTargetParams = g_anCameraParamFollowFacing;
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
  g_nCameraTargetInverted =
      ComputeCameraToTargetAngle(g_anCameraEyeTarget - 3 /* g_anCameraPos */);
  g_nCameraSpringYaw = g_nCameraYaw;
  g_nCameraSpringPitch = g_nCameraPitch;
  g_nCameraSpringDistance = g_nCameraDistance;
  g_nCameraSpringRoll = g_nCameraDraftRoll;
  g_nCameraSpringPitchBias = g_nCameraDraftPitchBias;
  g_nCameraSpringYawBias = g_nCameraDraftYawBias;
}

SDATA(g_nCameraSpringReengageCountdown, 4);

/* Per-frame camera driver (0x80037bd4, 708 bytes): forces the passive follow
   (g_nCameraForcePassiveFollow) during SPYRO_ST_HURT_LONG, picks the next mode
   from the per-state table unless a scripted move or a sentinel holds it,
   arbitrates via SelectCameraMode and runs the mode's updater. Non-scripted
   modes then apply the decaying vertical camera shake and, while no CD stream
   runs, refresh the floor lookup under the camera. Pad snapshots bracket the
   frame. */
void UpdateCameraFrame(void) {
  if (g_nSpyroState == SPYRO_ST_HURT_LONG) {
    g_nCameraForcePassiveFollow = 1;
  } else {
    g_nCameraForcePassiveFollow = 0;
  }
  g_bPadInputStashed = 0;
  if (g_nSpyroInputBlankCountdown != 0) {
    MovePadInputState(&g_dwPadPressed, &g_dwPadStash);
  }
  if (g_nSpyroScriptedMoveMode != 0) {
    g_nCameraNextMode = CAM_MODE_STAND;
    g_nCameraSpringReengageCountdown = 0;
  } else {
    int *next;

    OPAQUE_COPY(next, &g_nCameraNextMode);
    if (*next >= 0) {
      *next = (&g_abSpyroStateCameraMode)[g_nSpyroState];
    }
  }
  SelectCameraMode();
  if (g_nCameraCurrentMode == (CAM_HELD | CAM_MODE_SCRIPTED_VANTAGE)) {
    UpdateScriptedCameraView();
  } else if (g_nCameraCurrentMode == (CAM_HELD | CAM_MODE_LEVEL_TRANSITION)) {
    UpdateLevelTransitionCamera();
  } else {
    if (g_nCameraCurrentMode == (CAM_HELD | CAM_MODE_PORTAL_PATH)) {
      UpdatePortalPathCamera();
    } else {
      UpdateCameraSpringForMode();
    }
    if (g_nCameraShakeDuration != 0) {
      if (g_nCameraShakePeakAmplitude < g_nCameraShakeDuration) {
        g_nCameraShakePeakAmplitude = g_nCameraShakeDuration;
      }
      g_nCameraShakeDuration = g_nCameraShakeDuration - g_nFrameStep;
      if (g_nCameraShakeDuration < 0) {
        g_nCameraShakeDuration = 0;
      }
      g_anCameraPos[2] += g_nCameraShakeMagnitude * g_nCameraShakeDuration *
                              ((int)(g_dwGamestateFrames & 2) - 1) /
                              g_nCameraShakePeakAmplitude >>
                          6;
    } else {
      g_nCameraShakePeakAmplitude = 0;
    }
    if (g_nCdStreamState < LSTREAM_PRESERVE) {
      if (FindWorldFloorBelowPoint((uint *)g_anCameraPos) != 0) {
        g_nCameraVisZone = g_nWorldFloorVisZone;
      }
      if (g_nCameraVisZone >= g_nVisZoneCount) {
        g_nCameraVisZone = -1;
      }
      g_apScriptedCameraViewBlock[0] = 0;
    }
  }
  if (g_bPadInputStashed != 0) {
    MovePadInputState(&g_dwPadStash, &g_dwPadPressed);
  }
}

/* 0x8003c85c (0x138) — derive scripted-camera stick requests that steer the
   view toward `target`: project a probe point ahead of Spyro through his body
   matrix, take the offset to the target back into body space, then convert
   the pitch/yaw angles into clamped stick-request values (dead-banded away
   from the straight-up/straight-down poles). */
void AimCameraStickTowardPoint(int *target) {
  int inv[8];
  int v[3];

  v[0] = 0x64;
  v[1] = 0;
  v[2] = 0x40;
  RotateVectorByMatrix(g_anSpyroBodyMtx, v, v);
  AddVector(v, v, SPY_VIA(g_anSpyroBodyMtx, SPY_BODY_MTX, SPY_POS));
  SubtractVector(v, target, v);
  TransposePackedMatrix(inv, g_anSpyroBodyMtx);
  RotateVectorByMatrix(inv, v, v);

  {
    int ang = ArcTan2(VectorLength(v, VEC_LEN_XY), v[2], ATAN_ANGLE12);
    g_nSpyroSwingEulerTargetY = ang;
    if ((unsigned int)(ang - 0x181) < 0x67F) {
      g_nSpyroSwingEulerTargetY = 0x180;
    } else if ((unsigned int)(ang - 0x800) < 0x680) {
      g_nSpyroSwingEulerTargetY = 0xE80;
    }
  }

  {
    int *req;
    int ang = ArcTan2(v[0], v[1], ATAN_ANGLE12);
    req = g_anCameraScriptStickRequestXBlock;
    *req = ang;
    if ((unsigned int)(ang - 0x201) < 0x5FF) {
      *req = 0x200;
    } else if ((unsigned int)(ang - 0x800) < 0x600) {
      *req = 0xE00;
    }
  }
}
