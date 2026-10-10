#include "globals.h"
#include "sdata.h"

extern int ComputeCameraToTargetAngle(int *pos);
extern void LoadCameraTargetAnglesFromMode(void);
extern int AbsAngleDelta12(int a, int b);
extern void SubtractVector(int *dst, int *a, int *b);
extern void AddVector(int *dst, int *a, int *b);
extern void CopyVector(int *dst, int *src);
extern void ScaleVector3Sat(int *dst, int *src, int scale);
extern void RShiftVector3(int *vec, int shift);
extern int ArcTan2(int y, int x, int high_precision);
extern void AdvanceCameraSpringStep(int column);
extern void ComputeCameraOrbitOffset(int *out);
extern void SnapCameraToTarget(void);
extern void UpdateCameraEulerAngles(void);
extern void UpdateCameraManualYawInput(void);
extern void UpdateGameplayCamera(void);
extern int abs(int x);

extern int g_nSpyroWorldPosY; /* g_anSpyroWorldPos[1] */
extern int D_80078A60;        /* g_anSpyroWorldPos[2] */

/* Held-base views of the camera record (from g_anCameraPos) and the Spyro
   record (from g_anSpyroWorldPos). */
extern int g_anCameraPosBlock[];
extern int g_anSpyroWorldPosBlock[];
/* Register-base view of g_anCameraScriptStickIntegrator. */
extern int g_anCameraScriptStickIntegratorBlock[];

#define CAM g_anCameraPosBlock
#define CAM_CURRENT_MODE 0x0C  /* g_nCameraCurrentMode */
#define CAM_STUCK_FRAMES 0x28  /* g_nCameraStuckFrames */
#define CAM_ANCHOR 0x2A        /* g_pCameraAnchorPos */
#define CAM_TARGET_PARAMS 0x2C /* g_pCameraTargetParams */
#define CAM_SUPPRESS 0x32      /* g_nCameraSpringSuppressFlag */
#define STICK g_anCameraScriptStickIntegratorBlock
#define SPY g_anSpyroWorldPosBlock
#define SPY_MOTION 0x43   /* g_anSpyroMotionVec */
#define SPY_BODY_YAW 0x47 /* g_nSpyroBodyYaw */

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

typedef struct {
  int v[6];
} CameraAngles;

/* World x/y of a scripted camera actor. */
#define ACTOR_X(a) (*(int *)((char *)(a) + 0xC))
#define ACTOR_Y(a) (*(int *)((char *)(a) + 0x10))

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
    a = ArcTan2(rel[0], rel[1], 1);                                            \
    out = (a - ArcTan2(ahead[0], ahead[1], 1)) & 0xFFF;                        \
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

  if (((g_dwActiveCameraOptions & 0x10) || g_nCameraLookAroundActive) &&
      CAM[CAM_CURRENT_MODE] == 0xF) {
    CAM[CAM_CURRENT_MODE] = 0;
  }
  __asm__("" : "=r"(s) : "0"(&CAM[CAM_STUCK_FRAMES]));
  column = (*s + 1) >> 1;
  if (column >= 5) {
    column = 4;
  }
  switch (s[S_CURRENT_MODE]) {
  case 0:
  case 0x80000000:
    if (!((g_dwActiveCameraOptions & 0x10) &&
          g_nCameraSpringSuppressFlag == 0 && g_nCameraManualYawInput == 0 &&
          g_nCameraCollisionRetries == 0 &&
          (g_nCameraSpringReengageCountdown != 0 || g_nSpyroState == 0x1B))) {
      int yaw;

      *(int **)&CAM[CAM_TARGET_PARAMS] = g_anCameraParamDefault;
      yaw = SPY[SPY_BODY_YAW];
      g_pCameraAnchorPos = SPY;
      g_nCameraTargetYawBiasFromMode = yaw;
      g_nCameraTargetInverted = ComputeCameraToTargetAngle(CAM);
      g_nCameraSpringYaw = g_nCameraYaw;
      if (g_nCameraCollisionRetries == 0 || (int)g_dwGamestateFrames < 0x78) {
        LoadCameraTargetAnglesFromMode();
      }
    }
    if (((g_dwActiveCameraOptions & 0x10) || g_nCameraLookAroundActive) &&
        g_nPadAllInputIdleFlag && g_nCameraLosClear &&
        g_nCameraSpringSuppressFlag == 0) {
      int left = g_nCameraSpringReengageCountdown;
      int d;
      int p;

      if (left == 0) {
        break;
      }
      d = (g_nCameraYaw - 0x800 + g_nSpyroBodyYaw) & 0xFFF;
      if (d > 0x800) {
        d -= 0x1000;
      }
      p = (g_nCameraPitch - g_anCameraParamDefault[1]) & 0xFFF;
      if (p > 0x800) {
        p -= 0x1000;
      }
      if (abs(d) < 0x191 && abs(p) < 0x101) {
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
      g_nCameraSpringReengageCountdown = 0x5A;
    }
    break;
  case 0x80000010: {
    int yaw;

    g_pCameraTargetParams = g_anCameraParamDefault;
    yaw = SPY[SPY_BODY_YAW];
    g_pCameraAnchorPos = SPY;
    g_nCameraTargetYawBiasFromMode = yaw;
    g_nCameraTargetInverted = ComputeCameraToTargetAngle(&s[S_POS]);
    g_nCameraSpringYaw = g_nCameraYaw;
    if (g_nCameraCollisionRetries == 0) {
      LoadCameraTargetAnglesFromMode();
    }
  }
    /* FALLTHROUGH */
  case 0x80000011:
    g_nCameraPassiveModeHoldTimer--;
    break;
  case 1: {
    int yaw = g_nSpyroBodyYaw;

    *(int **)&CAM[CAM_ANCHOR] = g_anSpyroWorldPos;
    g_nCameraTargetYawBiasFromMode = yaw;
    if (g_nCameraCollisionRetries != 0) {
      break;
    }
    if (AbsAngleDelta12(yaw, g_nCameraEulerYaw) > 0x578) {
      g_pCameraTargetParams = g_anCameraParamMode2Reverse;
    } else {
      g_pCameraTargetParams = g_anCameraParamMode2Front;
      if (!(g_dwActiveCameraOptions & 0x40)) {
        g_nCameraTargetInverted = ComputeCameraToTargetAngle(CAM);
        g_nCameraSpringYaw = g_nCameraYaw;
      }
    }
    LoadCameraTargetAnglesFromMode();
    LOOKAHEAD_BIAS(rel, ahead, g_anCameraPos, 6, g_nCameraTargetYawBias);
    break;
  }
  case 3: {
    int yaw = g_nSpyroBodyYaw;

    g_pCameraTargetParams = g_anCameraParamMode3;
    g_pCameraAnchorPos = g_anSpyroWorldPos;
    g_nCameraTargetYawBiasFromMode = yaw;
    g_nCameraTargetInverted = ComputeCameraToTargetAngle(&s[S_POS]);
    g_nCameraSpringYaw = g_nCameraYaw;
    if (g_nCameraCollisionRetries != 0) {
      REBASE_ALL(&s[S_POS]);
      break;
    }
    LoadCameraTargetAnglesFromMode();
    g_nCameraTargetYawBias = (-g_nSpyroBodyYawStep << 3) & 0xFFF;
    break;
  }
  case 5: {
    int yaw = g_nSpyroBodyYaw;

    g_pCameraAnchorPos = g_anSpyroWorldPos;
    g_nCameraTargetYawBiasFromMode = yaw;
    if (g_nCameraCollisionRetries == 0) {
      if (g_nSpyroFallTrackingFlag != 0) {
        if (g_nSpyroStateFlags == 2) {
          yaw = (yaw - g_nCameraEulerYaw) & 0xFFF;
          if (yaw > 0x800) {
            yaw -= 0x1000;
          }
          if (yaw > 0) {
            *(int **)&s[S_TARGET_PARAMS] = g_anCameraParamMode5RightBias;
          } else {
            *(int **)&s[S_TARGET_PARAMS] = g_anCameraParamMode5LeftBias;
          }
          CAM[0x2B] = (g_nSpyroBodyYaw + 0x800) & 0xFFF;
          g_nCameraTargetInverted = ComputeCameraToTargetAngle(CAM);
          g_nCameraSpringYaw = g_nCameraYaw;
          LoadCameraTargetAnglesFromMode();
        } else if (g_nSpyroStateFlags == 1) {
          yaw = (yaw - g_nCameraEulerYaw) & 0xFFF;
          if (yaw > 0x800) {
            yaw -= 0x1000;
          }
          if (yaw > 0x100) {
            *(int **)&s[S_TARGET_PARAMS] = g_anCameraParamMode5RightBias;
          } else if (yaw < -0x100) {
            *(int **)&s[S_TARGET_PARAMS] = g_anCameraParamMode5LeftBias;
          } else {
            *(int **)&s[S_TARGET_PARAMS] = g_anCameraParamMode2Front;
          }
          g_nCameraTargetInverted = ComputeCameraToTargetAngle(g_anCameraPos);
          g_nCameraSpringYaw = g_nCameraYaw;
        } else {
          *(int **)&s[S_TARGET_PARAMS] = g_anCameraParamMode2Front;
          s[S_INVERTED] = ComputeCameraToTargetAngle(&s[S_POS]);
          s[S_SPRING_YAW] = s[S_YAW];
          s[S_SPRING_PITCH] = s[S_PITCH];
          LoadCameraTargetAnglesFromMode();
          LOOKAHEAD_BIAS(rel2, ahead2, &s[S_POS], 5, s[S_TARGET_YAW_BIAS]);
        }
      } else {
        int r = AbsAngleDelta12(yaw, g_nCameraEulerYaw);

        if (g_nSpyroStateFlags != 0xB && r > 0x578) {
          g_pCameraTargetParams = g_anCameraParamMode2Reverse;
        } else {
          *(int **)&CAM[CAM_TARGET_PARAMS] = g_anCameraParamMode2Front;
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
  case 6:
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
        0x20) {
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
  case 4: {
    int yaw = g_nSpyroBodyYaw;

    g_pCameraAnchorPos = g_anSpyroWorldPos;
    g_nCameraTargetYawBiasFromMode = yaw;
    if (g_nCameraCollisionRetries == 0) {
      if (AbsAngleDelta12(yaw, g_nCameraEulerYaw) > 0x578) {
        g_pCameraTargetParams = g_anCameraParamMode4Reverse;
      } else {
        g_pCameraTargetParams = g_anCameraParamMode4Front;
        if (!(g_dwActiveCameraOptions & 0x40)) {
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
  case 0x80000009: {
    int yaw = g_nSpyroBodyYaw;
    int *ig;
    int t;
    int r;

    g_pCameraTargetParams = g_anCameraParamMode9ScriptStick;
    g_pCameraAnchorPos = g_anSpyroWorldPos;
    g_nCameraTargetYawBiasFromMode = yaw;
    g_nCameraTargetInverted = ComputeCameraToTargetAngle(&s[S_POS]);
    g_nCameraSpringYaw = g_nCameraYaw;
    g_nCameraSpringPitch = g_nCameraPitch;
    g_nCameraSpringDistance = g_nCameraDistance;
    if (g_nCameraCollisionRetries == 0) {
      unsigned char *pad;

      LoadCameraTargetAnglesFromMode();
      pad = g_pPadSubstepState;
      if (*(int *)pad == 3 &&
          (*(unsigned int *)(pad + 0x14) & 0xFFFF0000) != 0x7F7F0000) {
        g_anCameraScriptStickIntegrator[0] += (pad[0x16] - 0x7F) >> 1;
        g_anCameraScriptStickIntegrator[1] += (0x7F - pad[0x17]) >> 1;
      } else {
        if (g_dwPadHeld & 0x8000) {
          STICK[0] -= 0x30;
        } else if (g_dwPadHeld & 0x2000) {
          STICK[0] += 0x30;
        }
        if (g_dwPadHeld & 0x1000) {
          STICK[1] += 0x30;
        } else if (g_dwPadHeld & 0x4000) {
          STICK[1] -= 0x30;
        }
      }
      __asm__("" : "=r"(ig) : "0"(STICK));
      if (ig[0] < -0x400) {
        ig[0] = -0x400;
      }
      if (ig[0] > 0x400) {
        ig[0] = 0x400;
      }
      if (ig[1] < -0x3D0) {
        ig[1] = -0x3D0;
      }
      if (ig[1] > 0x500) {
        ig[1] = 0x500;
      }
      g_nCameraTargetYaw += ig[0];
      t = ig[1];
      if (t < -0x140) {
        g_nCameraTargetPitch -= 0x140;
        g_nCameraTargetPitchBias += 0x140 + t;
      } else {
        g_nCameraTargetPitch += t;
      }
      r = -g_anCameraScriptStickIntegrator[0];
      if (r < -0x300) {
        r = -0x300;
      }
      if (r > 0x300) {
        r = 0x300;
      }
      g_nCameraScriptStickRequestX = r;
      r = -g_anCameraScriptStickIntegrator[1];
      if (r < -0x200) {
        r = -0x200;
      }
      if (r > 0x200) {
        r = 0x200;
      }
      g_nCameraScriptStickRequestY = r;
    }
    column = 5;
    break;
  }
  case 2: {
    int yaw = g_nSpyroBodyYaw;

    g_pCameraAnchorPos = g_anSpyroWorldPos;
    g_nCameraTargetYawBiasFromMode = yaw;
    if (g_nCameraCollisionRetries == 0) {
      if (AbsAngleDelta12(yaw, g_nCameraEulerYaw) > 0x578) {
        g_pCameraTargetParams = g_anCameraParamMode2Reverse;
      } else {
        g_pCameraTargetParams = g_anCameraParamMode2Front;
      }
      LoadCameraTargetAnglesFromMode();
    } else {
      REBASE_ALL(&s[S_POS]);
    }
    break;
  }
  case 0x80000007: {
    unsigned char *actor;
    int y;
    int z;

    g_pCameraTargetParams = g_anCameraParamScriptedTarget;
    actor = g_pScriptedCameraActor;
    g_pCameraAnchorPos = g_anCameraScriptedTargetPos;
    g_pCameraAnchorPos[0] = ACTOR_X(actor);
    z = D_80078A60;
    y = ACTOR_Y(actor);
    g_anCameraScriptedTargetPos[2] = z;
    g_anCameraScriptedTargetPos[1] = y;
    g_nCameraTargetYawBiasFromMode = actor[0x46] << 4;
    g_nCameraTargetInverted = ComputeCameraToTargetAngle(&s[S_POS]);
    g_nCameraSpringYaw = g_nCameraYaw;
    g_nCameraSpringPitch = g_nCameraPitch;
    g_nCameraSpringDistance = g_nCameraDistance;
    LoadCameraTargetAnglesFromMode();
    break;
  }
  case 0x8000000C: {
    unsigned char *actor = g_pScriptedCameraActor;

    g_pCameraAnchorPos = g_anSpyroWorldPos;
    g_nCameraTargetYawBiasFromMode =
        ArcTan2(ACTOR_X(actor) - g_pCameraAnchorPos[0],
                ACTOR_Y(actor) - g_nSpyroWorldPosY, 1);
    if (g_nCameraCollisionRetries != 0) {
      break;
    }
    g_pCameraTargetParams = g_anCameraParamModeC_ScriptedFollow;
    g_nCameraTargetInverted = ComputeCameraToTargetAngle(&s[S_POS]);
    LoadCameraTargetAnglesFromMode();
    break;
  }
  case 0x8000000A:
    g_nCameraTargetInverted = ComputeCameraToTargetAngle(&s[S_POS]);
    g_nCameraSpringYaw = g_nCameraYaw;
    g_nCameraSpringPitch = g_nCameraPitch;
    g_nCameraSpringDistance = g_nCameraDistance;
    if (g_nCameraCollisionRetries == 0) {
      LoadCameraTargetAnglesFromMode();
    } else {
      REBASE_ALL(&s[S_POS]);
    }
    if (g_dwSpyroRequestMask & 0x1000) {
      column = 5;
    }
    break;
  case 0xF: {
    int yaw = g_nSpyroBodyYaw;

    g_pCameraTargetParams = g_anCameraParamDefault;
    g_pCameraAnchorPos = g_anSpyroWorldPos;
    g_nCameraTargetYawBiasFromMode = yaw;
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
  if (g_nCameraCurrentMode != 0x80000009 &&
      g_nCameraCurrentMode != 0x80000007 &&
      g_nCameraCurrentMode != 0x80000012 &&
      g_nCameraCurrentMode != 0x8000000B &&
      g_nCameraCurrentMode != 0x8000000E && g_nCameraCurrentMode != 3) {
    UpdateCameraManualYawInput();
  } else {
    g_nCameraManualYawInput = 0;
  }
  if (((g_dwActiveCameraOptions & 0x10) || g_nCameraLookAroundActive) &&
      g_nCameraLosClear && g_nCameraSpringReengageCountdown &&
      CAM[CAM_SUPPRESS] == 0 && g_nCameraManualYawInput == 0 &&
      (g_nCameraCurrentMode == 0 || g_nCameraCurrentMode == 0x80000000)) {
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
  if (!((g_dwActiveCameraOptions & 0x10) && g_nSpyroState == 5 &&
        g_nCameraSpringSuppressFlag == 0 && g_nSpyroMotionSpeed < 0x400)) {
    g_nCameraSpringYaw = g_nCameraYaw;
  }
  g_nCameraSpringPitch = g_nCameraPitch;
  g_nCameraSpringDistance = g_nCameraDistance;
  g_nCameraSpringRoll = g_nCameraDraftRoll;
  g_nCameraSpringPitchBias = g_nCameraDraftPitchBias;
  g_nCameraSpringYawBias = g_nCameraDraftYawBias;
}
