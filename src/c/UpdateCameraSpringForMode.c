#include "camera.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "match_asm.h"
#include "pad.h"
#include "sdata.h"
#include "spyro_block.h"
#include "spyro_state.h"
#include "vecmath.h"

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
