#include "actor_states.h"
#include "camera.h"
#include "emit_record.h"
#include "funcs.h"
#include "globals.h"
#include "level_id.h"
#include "main_shared.h"
#include "match_asm.h"
#include "pad.h"
#include "pad_buttons.h"
#include "sdata.h"
#include "spyro_block.h"
#include "spyro_state.h"
#include "vecmath.h"

SDATA(g_nSpyroScriptedLiftBaseZ, 4);
SDATA(g_bSpyroJumpHoldForced, 4);
SDATA(g_nSpyroScriptedLiftRise, 4);
SDATA(g_nSpyroScriptedLiftYaw, 4);
SDATA(g_pSpyroHomingTargetActor, 4);
SDATA(g_nSpyroScriptedLiftTurn, 4);
SDATA(g_pSpyroContactActor, 4);

/* One view of the Spyro record from its world position (0x80078a58). */

#define PADB ((unsigned char *)g_pPadSubstepState)
#define RAMP ((signed char *)g_abSpyroEulerRateRampSigned)

/* Opaque copy: the result is a fresh value to cse, so a block address stays
   in its register and later fields are addressed off it. */
static inline int OpaqueInt(int p) {
  int q;

  OPAQUE_COPY(q, p);
  return q;
}

static inline int *Opaque(int *p) {
  int *q;

  OPAQUE_COPY(q, p);
  return q;
}

/* Invulnerability frames count down to zero. */
#define INVULN_TICK(p)                                                         \
  {                                                                            \
    int n = (p)[0] - 1;                                                        \
                                                                               \
    (p)[0] = n;                                                                \
    if (n < 0) {                                                               \
      (p)[0] = 0;                                                              \
    }                                                                          \
  }

/* Analog stick (or d-pad) as signed x/y in -PAD_STICK_MAX..PAD_STICK_MAX. */
#define READ_STICK(x, y)                                                       \
  if (g_pPadSubstepState->stickActive != 0) {                                  \
    x = PADB[PAD_LX_OFFSET] - PAD_STICK_CENTER;                                \
    y = PAD_STICK_CENTER - PADB[PAD_LY_OFFSET];                                \
  } else {                                                                     \
    if (g_pPadSubstepState->held & PAD_UP) {                                   \
      y = PAD_STICK_MAX;                                                       \
    } else {                                                                   \
      y = (g_pPadSubstepState->held & PAD_DOWN) ? -PAD_STICK_MAX : 0;          \
    }                                                                          \
    if (g_pPadSubstepState->held & PAD_RIGHT) {                                \
      x = PAD_STICK_MAX;                                                       \
    } else {                                                                   \
      x = (g_pPadSubstepState->held & PAD_LEFT) ? -PAD_STICK_MAX : 0;          \
    }                                                                          \
  }

/* Slide along the wall that pushed Spyro: add the into-wall share of the
   velocity back along the nudge direction. */
#define WALL_SLIDE(w, setspeed)                                                \
  if ((w)[0] != 0) {                                                           \
    int *nudge = (w) + 1;                                                      \
    int k;                                                                     \
                                                                               \
    g_anSpyroWallSlideNudgeAccum[2] = 0;                                       \
    SetVector3Magnitude(nudge, FIXED_ONE);                                     \
    k = (-(w)[0x10] * (w)[1] -                                                 \
         g_nSpyroVelocityY * g_anSpyroWallSlideNudgeAccum[1]) >>               \
        12;                                                                    \
    (w) += 0x10;                                                               \
    if (k > 0) {                                                               \
      ScaleVector3ByRatio(nudge, FIXED_ONE, k);                                \
      AddVector((w), (w), nudge);                                              \
      setspeed;                                                                \
    }                                                                          \
  }

/* On the ground the platform carry is added; airborne or at a cliff edge
   gravity takes over. */
#define FINISH_VELOCITY()                                                      \
  if (g_nSpyroCliffEdgeHint != 0 || g_nSpyroAirborneFrames != 0) {             \
    g_nSpyroVelocityZ = g_anSpyroMotionVec[2] - SPYRO_FALL_ACCEL;              \
  } else {                                                                     \
    AddVector(g_anSpyroVelocity, g_anSpyroVelocity, g_anSpyroSlopeGravityVec); \
  }

#define CLAMP_FALL_SPEED()                                                     \
  if (g_nSpyroVelocityZ < -SPYRO_TERMINAL_SPEED) {                             \
    g_nSpyroVelocityZ = -SPYRO_TERMINAL_SPEED;                                 \
  }

/* Glide/charge speed from the height fallen below the floor reference. */
#define SPEED_FROM_DROP(base, shift)                                           \
  {                                                                            \
    int f = g_nSpyroFloorReferenceZ - g_nSpyroWorldPosZ;                       \
                                                                               \
    if (f < 0) {                                                               \
      g_nSpyroMotionSpeed = (base) - IntegerSqrt(-f << (shift));               \
    } else {                                                                   \
      g_nSpyroMotionSpeed = IntegerSqrt(f << (shift)) + (base);                \
    }                                                                          \
  }

#define SPEED_FROM_DROP_T(base)                                                \
  {                                                                            \
    int f = g_nSpyroFloorReferenceZ - g_nSpyroWorldPosZ;                       \
    int v;                                                                     \
                                                                               \
    if (f < 0) {                                                               \
      v = (base) - IntegerSqrt(-f << 13);                                      \
    } else {                                                                   \
      v = (base) + IntegerSqrt(f << 13);                                       \
    }                                                                          \
    g_nSpyroMotionSpeed = v;                                                   \
    SCHED_FENCE();                                                             \
  }

/* Ease the pitch toward a yaw-rate-dependent bank. */
#define PITCH_TOWARD_TURN()                                                    \
  {                                                                            \
    p = (-g_nSpyroBodyYawStep * 8 - g_nSpyroBodyPitch) & ANGLE12_MASK;         \
                                                                               \
    if (p > ANGLE12_HALF) {                                                    \
      p -= ANGLE12_TURN;                                                       \
    }                                                                          \
    if (p > 0x10) {                                                            \
      p = 0x10;                                                                \
    } else if (p < -0x10) {                                                    \
      p = -0x10;                                                               \
    }                                                                          \
    SPY[SPY_PITCH] = (SPY[SPY_PITCH] + p) & ANGLE12_MASK;                      \
  }

#define LEVEL_GLIDE_HANDOFF()                                                  \
  {                                                                            \
    g_nSpyroSubstate = 0;                                                      \
    g_nCameraNextMode = (&g_abSpyroStateCameraMode)[g_nSpyroState];            \
    g_bSpyroOtBinBias = 4;                                                     \
  }

#define APPROACH_GLIDE_SPEED(t)                                                \
  if ((t)[SPY_SPEED - SPY_SPEED_TARGET] < (t)[0]) {                            \
    (t)[SPY_SPEED - SPY_SPEED_TARGET] += 0x80;                                 \
    if ((t)[SPY_SPEED - SPY_SPEED_TARGET] > (t)[0]) {                          \
      (t)[SPY_SPEED - SPY_SPEED_TARGET] = (t)[0];                              \
    }                                                                          \
  } else if ((t)[SPY_SPEED - SPY_SPEED_TARGET] > (t)[0]) {                     \
    (t)[SPY_SPEED - SPY_SPEED_TARGET] -= 0x80;                                 \
    if ((t)[SPY_SPEED - SPY_SPEED_TARGET] < (t)[0]) {                          \
      (t)[SPY_SPEED - SPY_SPEED_TARGET] = (t)[0];                              \
    }                                                                          \
  }

/* Source-shape notes (each one is load-bearing for the byte match):
   - case-15 subcase-0 pins (w $20, nx $17, sp $21), `$5`/`$6`-pinned held
     bases, and the `$9`/`$18` dead register references at the end steer
     global.c and reload toward the original's registers;
   - `p` is one case-scope local shared by subcases 1/2/10 and both
     PITCH_TOWARD_TURN sites, and `tip` doubles as the stick block's forward
     component (both keep their original call-crossing allocnos);
   - `k` is bumped after the spawn call so sched1 hoists it past the call;
   - case 26's actor base is a fresh single-set `ob` so sched1 boosts its la;
   - the 0x1B spawn writes wakePos[2] last so the fn pointer lands in t1. */
/* Spyro motion for one pad substep (0x80043fe4, 15228 bytes): steering,
   speed and velocity per state, the glide flight model, then the 1/64
   position accumulator and the persistent byte euler angles. */
void IntegrateSpyroMotionForSubstep(int substep) {
  int jumpSteer[3];
  int fallDrag[3];
  int toHomingTarget[3];
  int scratch[3];
  int wakePos[3];
  int tipPos[3];
  int tipNext[3];
  int liftStep[3];
  int slideDrag[3];
  int orbitStep[3];
  int knockDrag[3];
  int t;
  int *sp;

  g_pPadSubstepState = (PadSubstep *)&g_anPadSubstepRing[substep * 6];
  t = g_nSpyroStateTimer + substep;
  if (g_nGamestate == GS_GAMEPLAY && g_nActiveLevelId == LEVEL_GNASTYS_LOOT &&
      g_nSpyroAirborneFrames == 0) {
    g_nSpyroFlightMode = 1;
  }
  switch (g_nSpyroState) {
  case SPYRO_ST_IDLE:
  case SPYRO_ST_TEETER:
  case SPYRO_ST_FIDGET_0:
  case SPYRO_ST_FIDGET_1:
  case SPYRO_ST_FIDGET_2:
  case SPYRO_ST_FIDGET_3:
  case SPYRO_ST_FIDGET_4:
  case SPYRO_ST_FIDGET_5:
  case SPYRO_ST_FIDGET_6:
  case SPYRO_ST_FIDGET_7:
  case SPYRO_ST_FIDGET_8:
    INVULN_TICK(&SPY[SPY_INVULN]);
    if (g_nSpyroScriptedMoveMode == 0) {
      ComputeSpyroMoveTargetFromPad(0);
    } else if (g_nSpyroStateTimer > SPYRO_SCRIPTED_FLAME_DELAY &&
               g_bSpyroFlameActive == 0) {
      if (g_nSpyroSubstate != 0) {
        g_nSpyroScriptedMoveMode = 0;
      } else {
        g_nSpyroSubstate = SPYRO_SUB_IDLE_FLAMED;
        g_adwPadPressedBlock[0] |= PAD_CIRCLE; /* scripted flame breath */
      }
    }
    ZeroVector(g_anSpyroVelocity);
    break;
  case SPYRO_ST_WALK:
  case SPYRO_ST_RUN:
  case SPYRO_ST_JOG: {
    int r;
    int *w;

    INVULN_TICK(&SPY[SPY_INVULN]);
    if (g_nSpyroScriptedMoveMode == 0) {
      ComputeSpyroMoveTargetFromPad(g_nSpyroMoveTargetMag);
    } else {
      int d = (g_nSpyroMoveTargetYaw - g_nSpyroBodyYaw) & ANGLE12_MASK;

      if (d > ANGLE12_HALF) {
        d -= ANGLE12_TURN;
      }
      if (abs(d) < 0x10) {
        ChangeSpyroState(SPYRO_ST_IDLE);
      }
    }
    g_nSpyroSubstate = 0;
    if (g_nSpyroMotionSpeed < SPYRO_PAD_TURN_SPEED) {
      r = ApplyPadInputToSpyroBodyYaw();
    } else {
      int lean = 2;

      if (g_nSpyroMotionSpeed < SPYRO_RUN_DROP_SPEED) {
        lean = 1;
      }
      r = TurnSpyroTowardMoveTargetWithLean(lean);
    }
    if (g_nSpyroSubstate != 0) { /* SPYRO_SUB_WALK_PIVOT */
      int s = g_nSpyroMotionSpeed - SPYRO_GROUND_DECEL;

      g_nSpyroMotionSpeed = s;
      if (s < 0) {
        g_nSpyroMotionSpeed = 0;
      }
    } else {
      int a = SPYRO_GROUND_ACCEL;

      if (r > SPYRO_SHARP_TURN) {
        a = OpaqueInt(0);
      }
      AdvanceSpyroSpeedTowardTarget(a, SPYRO_GROUND_DECEL);
    }
    BuildSpyroVelocityFromBodyEuler();
    w = &SPY[SPY_WALL_SLIDE];
    OPAQUE(w);
    WALL_SLIDE(w, g_nSpyroMotionSpeed = VectorLength(w, VEC_LEN_XYZ));
    {
      register int *cb asm("$6") = &SPY[SPY_CLIFF];

      OPAQUE(cb);
      if (cb[0] != 0 || g_nSpyroAirborneFrames != 0) {
        g_nSpyroVelocityZ = g_anSpyroMotionVec[2] - SPYRO_FALL_ACCEL;
      } else {
        AddVector(g_anSpyroVelocity, g_anSpyroVelocity,
                  g_anSpyroSlopeGravityVec);
      }
    }
    break;
  }
  case SPYRO_ST_TURN_AROUND: {
    int d;

    INVULN_TICK(&SPY[SPY_INVULN]);
    d = AbsAngleDelta12(g_nSpyroMoveTargetYaw, g_nSpyroBodyYaw);
    if (d < 0x80) {
      d >>= 3;
    } else {
      d = t;
      if (d > 0xB) {
        d = 0xB;
      }
      d = g_abSpyroEulerRateRampUp[d];
    }
    IntegrateSpyroBodyEuler(d);
    if (g_nSpyroCliffEdgeHint != 0) {
      g_nSpyroMotionSpeed = 0;
    } else {
      int s = g_nSpyroMotionSpeed - SPYRO_TURN_AROUND_BRAKE;

      g_nSpyroMotionSpeed = s;
      if (s < 0) {
        g_nSpyroMotionSpeed = 0;
      }
    }
    BuildSpyroVelocityFromBodyEuler();
    AddVector(g_anSpyroVelocity, g_anSpyroVelocity, g_anSpyroSlopeGravityVec);
    break;
  }
  case SPYRO_ST_STOP: {
    int r;
    int *w;

    INVULN_TICK(&SPY[SPY_INVULN]);
    if (g_nPadDirectionalIdleFlag != 0 &&
        g_nSpyroSubstate == SPYRO_SUB_STOP_FROM_SPEED) {
      g_nSpyroSpeedTarget = 0;
      if (g_pSpyroContactActor != 0) {
        Actor *a = g_pSpyroContactActor;

        g_nSpyroMoveTargetYaw =
            ArcTan2(a->posX - g_anSpyroWorldPos[0], a->posY - g_nSpyroWorldPosY,
                    ATAN_ANGLE12);
      }
    } else {
      ComputeSpyroMoveTargetFromPad(g_nSpyroMoveTargetMag);
    }
    if (SPY[SPY_SUBSTATE] != 7) {
      SPY[SPY_SUBSTATE] = 0;
    }
    if (g_nSpyroMotionSpeed < SPYRO_PAD_TURN_SPEED) {
      r = ApplyPadInputToSpyroBodyYaw();
    } else {
      int lean = 2;

      if (g_nSpyroMotionSpeed < SPYRO_RUN_DROP_SPEED) {
        lean = 1;
      }
      r = TurnSpyroTowardMoveTargetWithLean(lean);
    }
    if (g_nSpyroCliffEdgeHint != 0) {
      g_nSpyroMotionSpeed = 0;
    } else if (g_nSpyroSubstate != 0) {
      int s = g_nSpyroMotionSpeed - SPYRO_GROUND_DECEL;

      g_nSpyroMotionSpeed = s;
      if (s < 0) {
        g_nSpyroMotionSpeed = 0;
      }
    } else {
      int a = SPYRO_GROUND_ACCEL;

      if (r > SPYRO_SHARP_TURN) {
        a = OpaqueInt(0);
      }
      AdvanceSpyroSpeedTowardTarget(a, SPYRO_GROUND_DECEL);
    }
    BuildSpyroVelocityFromBodyEuler();
    w = &SPY[SPY_WALL_SLIDE];
    OPAQUE(w);
    WALL_SLIDE(w, g_nSpyroMotionSpeed = VectorLength(w, VEC_LEN_XYZ));
    {
      register int *ab asm("$6") = &SPY[SPY_AIRBORNE];

      OPAQUE(ab);
      if (ab[0] != 0) {
        g_nSpyroVelocityZ = g_anSpyroMotionVec[2] - SPYRO_FALL_ACCEL;
      } else {
        AddVector(ab + (SPY_VELOCITY - SPY_AIRBORNE),
                  ab + (SPY_VELOCITY - SPY_AIRBORNE), g_anSpyroSlopeGravityVec);
      }
    }
    break;
  }
  case SPYRO_ST_JUMP: {
    int *cd = &SPY[SPY_INVULN];
    int *vel;
    int l;

    OPAQUE(cd);
    INVULN_TICK(cd);
    ComputeSpyroMoveTargetFromPad(SPYRO_JUMP_MOVE_SPEED);
    TurnSpyroTowardMoveTargetWithLean(2);
    AdvanceSpyroSpeedTowardTarget(SPYRO_GROUND_ACCEL, SPYRO_GROUND_DECEL);
    vel = cd + (SPY_VELOCITY - SPY_INVULN);
    cd[SPY_CARRY - SPY_INVULN] =
        (g_nSpyroMotionSpeed * LookupCosine(g_nSpyroMoveTargetYaw)) >> 12;
    g_nSpyroCarryMotionY =
        (g_nSpyroMotionSpeed * LookupSine(g_nSpyroMoveTargetYaw)) >> 12;
    SubtractVector(jumpSteer, cd + (SPY_CARRY - SPY_INVULN), vel);
    jumpSteer[2] = 0;
    l = VectorLength(jumpSteer, VEC_LEN_XY);
    if (l > SPYRO_JUMP_STEER_MAX) {
      ScaleVector3ByRatio(jumpSteer, l, SPYRO_JUMP_STEER_MAX);
    }
    AddVector(vel, vel, jumpSteer);
    if (g_nSpyroWallSlideContributedFlag != 0) {
      int *nudge = cd + (SPY_NUDGE - SPY_INVULN);

      g_anSpyroWallSlideNudgeAccum[2] = 0;
      SetVector3Magnitude(nudge, FIXED_ONE);
      l = (-cd[SPY_VELOCITY - SPY_INVULN] * cd[SPY_NUDGE - SPY_INVULN] -
           g_nSpyroVelocityY * g_anSpyroWallSlideNudgeAccum[1]) >>
          12;
      if (l > 0) {
        ScaleVector3ByRatio(nudge, FIXED_ONE, l);
        AddVector(vel, vel, nudge);
      }
    }
    if (t < SPYRO_JUMP_HOLD_TICKS &&
        g_nSpyroSubstate == SPYRO_SUB_JUMP_RISING &&
        ((g_pPadSubstepState->held & PAD_CROSS) ||
         g_bSpyroJumpHoldForced != 0)) {
      int c;
      int v;

      c = g_nSpyroCarryMotionZ;
      c += SPYRO_JUMP_HOLD_LIFT;
      c += g_nSpyroGravityStep;
      g_nSpyroCarryMotionZ = c;
      v = ((g_nSpyroFloorReferenceZ - g_nSpyroWorldPosZ) << 6) -
          g_anSpyroVelocityFp6[2] + c;
      g_nSpyroVelocityZ = v;
      g_nSpyroFloorReferenceZ =
          ((g_nSpyroWorldPosZ << 6) + v + g_anSpyroVelocityFp6[2]) >> 6;
    } else {
      SPY[SPY_CARRY + 2] += g_nSpyroGravityStep;
      g_nSpyroVelocityZ = SPY[SPY_CARRY + 2];
    }
    break;
  }
  case SPYRO_ST_FALL: {
    int *vel;
    int *vz;
    int l;

    INVULN_TICK(&SPY[SPY_INVULN]);
    if (g_nSpyroFlightMode != 0) {
      int d;

      ComputeSpyroMoveTargetFromPad(g_nSpyroMoveTargetMag);
      d = (g_nSpyroMoveTargetYaw - g_nSpyroBodyYaw) & ANGLE12_MASK;
      if (d > ANGLE12_HALF) {
        d -= ANGLE12_TURN;
      }
      if (d > 0x20) {
        d = 0x20;
      }
      if (d < -0x20) {
        d = -0x20;
      }
      IntegrateSpyroBodyEuler(d);
    }
    if (SPY[SPY_FRAME_MOTION] < 0x80) {
      CopyVector(fallDrag, &SPY[SPY_NUDGE]);
      RShiftVector3(fallDrag, 3);
      AddVector(&SPY[SPY_VELOCITY], &SPY[SPY_VELOCITY], fallDrag);
    }
    ZeroVector(fallDrag);
    vel = &SPY[SPY_VELOCITY];
    SubtractVector(fallDrag, fallDrag, vel);
    fallDrag[2] = 0;
    l = VectorLength(fallDrag, VEC_LEN_XY);
    if (l > SPYRO_FALL_STEER_MAX) {
      ScaleVector3ByRatio(fallDrag, l, SPYRO_FALL_STEER_MAX);
    }
    fallDrag[2] = g_nSpyroGravityStep;
    AddVector(vel, vel, fallDrag);
    if (g_nSpyroGroundProbeSuspendFlag == 0 &&
        g_nSpyroGroundSlope < SLOPE_NUDGE_MAX) {
      NudgeSpyroFromWallProbes();
    }
    vz = &SPY[SPY_VELOCITY + 2];
    OPAQUE(vz);
    if (vz[0] < -SPYRO_TERMINAL_SPEED) {
      vz[0] = -SPYRO_TERMINAL_SPEED;
    }
    if (g_nSpyroWallSlideContributedFlag != 0) {
      int *nudge = vz + (SPY_NUDGE - SPY_VELOCITY - 2);
      int k;

      g_anSpyroWallSlideNudgeAccum[2] = 0;
      SetVector3Magnitude(nudge, FIXED_ONE);
      k = (-vz[-2] * vz[SPY_NUDGE - SPY_VELOCITY - 2] -
           g_nSpyroVelocityY * g_anSpyroWallSlideNudgeAccum[1]) >>
          12;
      if (k > 0) {
        vz -= 2;
        ScaleVector3ByRatio(nudge, FIXED_ONE, k);
        AddVector(vz, vz, nudge);
      }
    }
    break;
  }
  case SPYRO_ST_SIDESTEP_L1:
  case SPYRO_ST_SIDESTEP_R1: {
    int *cd = &SPY[SPY_INVULN];
    int *vel;

    OPAQUE(cd);
    INVULN_TICK(cd);
    ComputeSpyroMoveTargetFromPad(SPYRO_SIDESTEP_SPEED);
    TurnSpyroTowardMoveTargetWithLean(3);
    g_nSpyroSpeedTarget = SPYRO_SIDESTEP_SPEED;
    AdvanceSpyroSpeedTowardTarget(SPYRO_GROUND_ACCEL, SPYRO_GROUND_DECEL);
    {
      int m = g_nSpyroMotionSpeed;

      vel = cd + (SPY_VELOCITY - SPY_INVULN);
      *vel = 0;
      g_nSpyroVelocityZ = 0;
      g_nSpyroVelocityY = m;
      if (g_nSpyroState == SPYRO_ST_SIDESTEP_R1) {
        g_nSpyroVelocityY = -m;
      }
    }
    RotateVectorByMatrix(cd - (SPY_INVULN - 0xD), vel, vel);
    cd[SPY_CARRY - SPY_INVULN] -= cd[SPY_CARRY - SPY_INVULN] >> 4;
    g_nSpyroCarryMotionY -= g_nSpyroCarryMotionY >> 4;
    g_nSpyroCarryMotionZ -= g_nSpyroCarryMotionZ >> 4;
    AddVector(Opaque(vel), vel, cd + (SPY_CARRY - SPY_INVULN));
    FINISH_VELOCITY();
    break;
  }
  case SPYRO_ST_CHARGE:
    INVULN_TICK(&SPY[SPY_INVULN]);
    if (g_nSpyroSubstate & SPYRO_SUB_CHARGE_STEER) {
      int stickX;
      int stickY;
      int relYaw;
      int cosYaw;
      int d;
      int rateMin;
      int *chargeHold;
      signed char *ramp;

      READ_STICK(stickX, stickY);
      relYaw = (SPY[SPY_BODY_YAW] - g_anCameraEulerYawBlock[0]) & ANGLE12_MASK;
      cosYaw = LookupCosine(relYaw);
      ramp = (signed char *)g_abSpyroEulerRateRampSigned;
      OPAQUE(ramp);
      d = LookupSine(relYaw);
      rateMin = ramp[0];
      g_nSpyroBodyYawStep = (-stickX * cosYaw - stickY * d) >> 13;
      if (g_nSpyroBodyYawStep < rateMin) {
        g_nSpyroBodyYawStep = rateMin;
      }
      if (g_nSpyroBodyYawStep > ramp[12]) {
        g_nSpyroBodyYawStep = ramp[12];
      }
      IntegrateSpyroBodyEuler(g_nSpyroBodyYawStep);
      if (g_pSpyroHomingTargetActor != 0 &&
          g_nSpyroStateTimer < SPYRO_HOMING_TICKS) {
        d = (SPY[SPY_BODY_YAW] - g_anCameraEulerYawBlock[0]) & ANGLE12_MASK;

        if (d > ANGLE12_HALF) {
          d -= ANGLE12_TURN;
        }
        if (abs(d) >= SPYRO_HOMING_CONE) {
          int l;

          SubtractVector(toHomingTarget, &g_pSpyroHomingTargetActor->posX,
                         &SPY[SPY_POS]);
          l = VectorLength(toHomingTarget, VEC_LEN_XYZ);
          d = (ArcTan2(toHomingTarget[0], toHomingTarget[1], ATAN_ANGLE12) -
               SPY[SPY_BODY_YAW]) &
              ANGLE12_MASK;
          if (d > ANGLE12_HALF) {
            d -= ANGLE12_TURN;
          }
          if (abs(d) < SPYRO_HOMING_CONE && l < SPYRO_HOMING_RANGE) {
            d >>= 2;
            if (d < ramp[0]) {
              d = ramp[0];
            }
            if (d > ramp[12]) {
              d = ramp[12];
            }
            IntegrateSpyroBodyEuler(d);
            goto steered;
          }
        }
        g_pSpyroHomingTargetActor = 0;
      }
    steered:
      chargeHold = &SPY[SPY_CHARGE_HOLD];
      OPAQUE(chargeHold);
      if (chargeHold[0] != 0) {
        int *vel;

        SPEED_FROM_DROP(SPYRO_CHARGE_SPEED, 13);
        if (SPY[SPY_SPEED] > SPYRO_MAX_SPEED) {
          SPY[SPY_SPEED] = SPYRO_MAX_SPEED;
        }
        if (SPY[SPY_SPEED] < SPYRO_CHARGE_MIN_SPEED) {
          g_nSpyroSpeedTarget = SPYRO_CHARGE_MIN_SPEED;
          g_nSpyroFloorReferenceZ = g_nSpyroWorldPosZ;
        }
        vel = &SPY[SPY_VELOCITY];
        BuildSpyroVelocityFromBodyEuler();
        CopyVector(&SPY[SPY_CARRY], vel);
        if (g_nSpyroCliffEdgeHint != 0) {
          g_nSpyroVelocityZ -= SPYRO_FALL_ACCEL;
        } else {
          AddVector(vel, vel, &SPY[SPY_SLOPE_GRAVITY]);
        }
      } else {
        int *carry = chargeHold + (SPY_CARRY - SPY_CHARGE_HOLD);
        int *vel;

        scratch[0] =
            (-chargeHold[SPY_CARRY - SPY_CHARGE_HOLD] * SPYRO_CHARGE_DRAG) >>
            12;
        scratch[1] = (-g_nSpyroCarryMotionY * SPYRO_CHARGE_DRAG) >> 12;
        scratch[2] = (-g_nSpyroCarryMotionZ * SPYRO_CHARGE_DRAG) >> 12;
        AddVector(carry, carry, scratch);
        g_nSpyroMotionSpeed = SPYRO_CHARGE_THRUST;
        vel = chargeHold + (SPY_VELOCITY - SPY_CHARGE_HOLD);
        BuildSpyroVelocityFromBodyEuler();
        AddVector(carry, carry, vel);
        AddVector(carry, carry,
                  chargeHold + (SPY_SLOPE_SLIDE - SPY_CHARGE_HOLD));
        g_nSpyroMotionSpeed = VectorLength(carry, VEC_LEN_XYZ);
        AddVector(vel, carry,
                  chargeHold + (SPY_SLOPE_GRAVITY - SPY_CHARGE_HOLD));
      }
    } else {
      int *vel;
      int l;

      g_nSpyroSpeedTarget = 0;
      AdvanceSpyroSpeedTowardTarget(0, SPYRO_CHARGE_STOP_DECEL);
      vel = &SPY[SPY_VELOCITY];
      BuildSpyroVelocityFromBodyEuler();
      AddVector(vel, vel, &SPY[SPY_SLOPE_SLIDE]);
      l = VectorLength(vel, VEC_LEN_XYZ);
      g_nSpyroMotionSpeed = l;
      if (l > SPYRO_TERMINAL_SPEED) {
        ScaleVector3ByRatio(vel, l, SPYRO_TERMINAL_SPEED);
        g_nSpyroMotionSpeed = SPYRO_TERMINAL_SPEED;
      }
      if (g_nSpyroCliffEdgeHint != 0) {
        g_nSpyroVelocityZ -= SPYRO_FALL_ACCEL;
      } else {
        AddVector(g_anSpyroVelocity, g_anSpyroVelocity,
                  g_anSpyroSlopeGravityVec);
      }
    }
    break;
  case SPYRO_ST_CHARGE_BONK:
    INVULN_TICK(&SPY[SPY_INVULN]);
    if (g_nSpyroVelocityZ < 0) {
      g_nSpyroVelocityZ = 0;
    }
    if (t > 0x12) {
      ScaleVector3Shifted(&SPY[SPY_VELOCITY], 7, 3);
    }
    g_nSpyroVelocityZ += g_nSpyroGravityStep;
    break;
  case SPYRO_ST_HURT_TINT:
  case SPYRO_ST_HURT:
    g_nSpyroSpeedTarget = 0;
    AdvanceSpyroSpeedTowardTarget(0, SPYRO_GROUND_DECEL);
    BuildSpyroVelocityFromBodyEuler();
    g_nSpyroVelocityZ = g_anSpyroMotionVec[2] - SPYRO_FALL_ACCEL;
    CLAMP_FALL_SPEED();
    break;
  case SPYRO_ST_HURT_PINNED:
    if (SPY[SPY_AIRBORNE] != 0) {
      g_nSpyroVelocityZ -= SPYRO_PINNED_FALL_ACCEL;
      CLAMP_FALL_SPEED();
    } else {
      ZeroVector(&SPY[SPY_VELOCITY]);
    }
    break;
  case SPYRO_ST_HURT_LONG:
    if (SPY[SPY_AIRBORNE] != 0) {
      g_nSpyroVelocityZ -= SPYRO_PINNED_FALL_ACCEL;
      CLAMP_FALL_SPEED();
    } else {
      ZeroVector(&SPY[SPY_VELOCITY]);
    }
    SPY[SPY_BODY_YAW] += SPYRO_HURT_LONG_SPIN;
    break;
  case SPYRO_ST_GLIDE:
  case SPYRO_ST_GLIDE_SLOW:
  case SPYRO_ST_FLY:
  case SPYRO_ST_FLY_CLIMB:
  case SPYRO_ST_FLY_DIVE: {
    int x;
    int y;
    int p;
    int *tip;

    INVULN_TICK(&SPY[SPY_INVULN]);
    if (g_nSpyroFlightMode != 0) {
      int moving;
      int n;
      register int mix asm("$20");
      int i;
      int k;
      register unsigned char *ap asm("$19");
      int n0;
      int *next;

      switch (g_nSpyroSubstate) {
      case 0: {
        int w;
        int z;
        int c;
        register int nx asm("$17");
        register int *sp asm("$21");

        if (g_pPadSubstepState->stickActive != 0) {
          x = PADB[PAD_LX_OFFSET] - PAD_STICK_CENTER;
          y = PAD_STICK_CENTER - PADB[PAD_LY_OFFSET];
        } else {
          if (g_pPadSubstepState->held & PAD_UP) {
            g_nSpyroMotionAux += SPYRO_FLY_DPAD_STEP;
            if (g_nSpyroMotionAux > PAD_STICK_MAX) {
              g_nSpyroMotionAux = PAD_STICK_MAX;
            }
          } else if (g_pPadSubstepState->held & PAD_DOWN) {
            g_nSpyroMotionAux -= SPYRO_FLY_DPAD_STEP;
            if (g_nSpyroMotionAux < -PAD_STICK_MAX) {
              g_nSpyroMotionAux = -PAD_STICK_MAX;
            }
          } else {
            x = -g_nSpyroMotionAux;
            if (x > SPYRO_FLY_DPAD_RECENTRE) {
              x = SPYRO_FLY_DPAD_RECENTRE;
            }
            if (x < -SPYRO_FLY_DPAD_RECENTRE) {
              x = -SPYRO_FLY_DPAD_RECENTRE;
            }
            g_nSpyroMotionAux += x;
          }
          y = g_nSpyroMotionAux;
          if (g_pPadSubstepState->held & PAD_RIGHT) {
            g_nSpyroTurnRateAccum += SPYRO_FLY_DPAD_STEP;
            if (g_nSpyroTurnRateAccum > PAD_STICK_MAX) {
              g_nSpyroTurnRateAccum = PAD_STICK_MAX;
            }
          } else if (g_pPadSubstepState->held & PAD_LEFT) {
            g_nSpyroTurnRateAccum -= SPYRO_FLY_DPAD_STEP;
            if (g_nSpyroTurnRateAccum < -PAD_STICK_MAX) {
              g_nSpyroTurnRateAccum = -PAD_STICK_MAX;
            }
          } else {
            w = -g_nSpyroTurnRateAccum;
            if (w > SPYRO_FLY_DPAD_RECENTRE) {
              w = SPYRO_FLY_DPAD_RECENTRE;
            }
            if (w < -SPYRO_FLY_DPAD_RECENTRE) {
              w = -SPYRO_FLY_DPAD_RECENTRE;
            }
            g_nSpyroTurnRateAccum += w;
          }
          x = g_nSpyroTurnRateAccum;
        }
        nx = -x;
        if (y != 0) {
          z = y;
        } else {
          z = g_nSpyroBodyRoll >> 3;
          if (g_nSpyroMotionSpeed < SPYRO_FLIGHT_CRUISE_SPEED) {
            z += (SPYRO_FLIGHT_CRUISE_SPEED - g_nSpyroMotionSpeed) >> 6;
          }
          if (z > PAD_STICK_MAX) {
            z = PAD_STICK_MAX;
          }
          if (z < -PAD_STICK_MAX) {
            z = -PAD_STICK_MAX;
          }
        }
        c = LookupCosine(SPY[SPY_ROLL]);
        x = (z * -15) >> 7;
        w = (nx * 20) >> 7;
        if (g_nSpyroWorldPosZ < g_nSpyroFloorReferenceZ) {
          scratch[2] = (g_nSpyroVelocityZ >> 3) - 0x400;
          if (scratch[2] < 0) {
            int *pos = &SPY[SPY_POS];

            scratch[0] = g_anSpyroVelocity[0] >> 3;
            scratch[1] = g_nSpyroVelocityY >> 3;
            AddVector(scratch, scratch, pos);
            if (CheckLineOfSightSegment(pos, scratch) == 0) {
              y = (g_nSpyroWorldPosZ - g_nCollisionResolvedZ) >> 3;
              if (y > 7) {
                y = 7;
              }
              x += y;
            }
          }
        }
        SPY[SPY_BODY_YAW] = (SPY[SPY_BODY_YAW] + w) & ANGLE12_MASK;
        g_nSpyroBodyRoll = (g_nSpyroBodyRoll + x) & ANGLE12_MASK;
        if (g_nSpyroBodyRoll > ANGLE12_HALF) {
          g_nSpyroBodyRoll -= ANGLE12_TURN;
        }
        if (g_nSpyroBodyRoll > SPYRO_FLY_MAX_ROLL) {
          g_nSpyroBodyRoll = SPYRO_FLY_MAX_ROLL;
        }
        x = g_nSpyroFloorReferenceZ - g_nSpyroWorldPosZ;
        if (x < 0) {
          g_nSpyroMotionSpeed =
              SPYRO_FLIGHT_CRUISE_SPEED - (IntegerSqrt(-x) << 7);
        } else {
          g_nSpyroMotionSpeed =
              (IntegerSqrt(x) << 6) + SPYRO_FLIGHT_CRUISE_SPEED;
        }
        if (g_nFlightLevelActive == 0) {
          SPY[SPY_SPEED] += SPYRO_FREE_FLIGHT_BOOST;
        }
        sp = &SPY[SPY_SPEED];
        OPAQUE(sp);
        if (sp[0] > SPYRO_MAX_SPEED) {
          sp[0] = SPYRO_MAX_SPEED;
        }
        x += 0x132;
        y = x >> 2;
        if (y < 0) {
          y = 0;
        }
        if (y < g_nSpyroBodyRoll) {
          g_nSpyroBodyRoll = y;
        }
        if (g_nSpyroBodyRoll < -SPYRO_FLY_MAX_ROLL) {
          g_nSpyroBodyRoll = -SPYRO_FLY_MAX_ROLL;
        }
        if (sp[0] < SPYRO_GLIDE_MIN_SPEED) {
          sp[0] = SPYRO_GLIDE_MIN_SPEED;
        }
        g_nSpyroPitchRateAccum += ((w * -(c << 5)) >> 12) -
                                  ((g_nSpyroPitchRateAccum << 4) >> 6) -
                                  ((g_nSpyroBodyPitch << 6) >> 6);
        g_nSpyroBodyPitch =
            (g_nSpyroBodyPitch + (g_nSpyroPitchRateAccum >> 6)) & ANGLE12_MASK;
        if (g_nSpyroBodyPitch > ANGLE12_HALF) {
          g_nSpyroBodyPitch -= ANGLE12_TURN;
        }
        if (g_nSpyroBodyPitch > SPYRO_FLY_MAX_PITCH) {
          g_nSpyroBodyPitch = SPYRO_FLY_MAX_PITCH;
        }
        if (g_nSpyroBodyPitch < -SPYRO_FLY_MAX_PITCH) {
          g_nSpyroBodyPitch = -SPYRO_FLY_MAX_PITCH;
        }
        BuildSpyroVelocityFromBodyEuler();
        if (g_nFlightLevelActive != 0) {
          int a;
          int q;

          x = g_nActiveLevelId / 10 - 1;
          q = (int)scratch;
          SubtractVector((int *)q, g_anFlightBoundsCenter + x * 3,
                         sp + (0 - SPY_SPEED));
          RShiftVector3((int *)q, 4);
          y = VectorLength((int *)q, VEC_LEN_XY);
          a = g_anFlightBoundsRadius[x * 3] >> 4;
          if (a < y) {
            z = y - a;
            ScaleVector3ByRatio((int *)q, y, z);
            LShiftVector3((int *)q, 5);
            scratch[2] = 0;
            AddVector(sp + (SPY_VELOCITY - SPY_SPEED),
                      sp + (SPY_VELOCITY - SPY_SPEED), (int *)q);
            q = (ArcTan2(scratch[0], scratch[1], ATAN_ANGLE12) -
                 g_nSpyroBodyYaw) &
                ANGLE12_MASK;
            if (q > ANGLE12_HALF) {
              q -= ANGLE12_TURN;
            }
            q = (q * z) >> 15;
            if (q < -0x20) {
              q = -0x20;
            }
            if (q > 0x20) {
              q = 0x20;
            }
            sp[SPY_BODY_YAW - SPY_SPEED] = g_nSpyroBodyYaw + q;
          }
        }
        break;
      }
      case SPYRO_SUB_GLIDE_R1: {
        SPY[SPY_BODY_YAW] &= ANGLE12_MASK;
        g_nSpyroBodyRoll = (g_nSpyroBodyRoll + SPYRO_ROLL_RATE) & ANGLE12_MASK;
        if (g_nSpyroBodyRoll > ANGLE12_HALF) {
          g_nSpyroBodyRoll -= ANGLE12_TURN;
        }
        if (g_nSpyroBodyRoll > SPYRO_ROLL_QUARTER) {
          g_nSpyroGlideBankingFlag = 1;
        } else if (g_nSpyroBodyRoll > 0) {
          if (g_nSpyroGlideBankingFlag != 0) {
            g_nSpyroSubstate = 0;
          }
          g_nSpyroGlideBankingFlag = 0;
        }
        p = -SPY[SPY_PITCH] & ANGLE12_MASK;
        if (p > ANGLE12_HALF) {
          p -= ANGLE12_TURN;
        }
        if (p > 0x10) {
          p = 0x10;
        }
        if (p < -0x10) {
          p = -0x10;
        }
        x = g_nSpyroFloorReferenceZ - g_nSpyroWorldPosZ;
        SPY[SPY_PITCH] = (SPY[SPY_PITCH] + p) & ANGLE12_MASK;
        if (x < 0) {
          KEEP_IN_REG(p);
          g_nSpyroMotionSpeed =
              SPYRO_FLIGHT_CRUISE_SPEED - (IntegerSqrt(-x) << 7);
        } else {
          g_nSpyroMotionSpeed =
              (IntegerSqrt(x) << 6) + SPYRO_FLIGHT_CRUISE_SPEED;
        }
        if (SPY[SPY_SPEED] > SPYRO_MAX_SPEED) {
          SPY[SPY_SPEED] = SPYRO_MAX_SPEED;
        }
        if (SPY[SPY_SPEED] < SPYRO_BANK_MIN_SPEED &&
            g_nSpyroGlideBankingFlag == 0) {
          g_nSpyroSubstate = 0;
          g_nSpyroTurnRateAccum = 0;
          g_nSpyroMotionAux = 0;
        }
        BuildSpyroVelocityFromBodyEuler();
        break;
      }
      case SPYRO_SUB_GLIDE_L1: {
        register int *gb asm("$6") = &SPY[SPY_GLIDE_BANK];

        OPAQUE(gb);
        if (gb[0] != 0) {
          int dz;

          x = -g_nSpyroBodyRoll & ANGLE12_MASK;

          if (x > ANGLE12_HALF) {
            x -= ANGLE12_TURN;
          }
          dz = g_nSpyroMotionAux - g_nSpyroWorldPosZ;
          x += (dz - (g_anSpyroMotionVec[2] >> 5)) >> 2;
          if (x > SPYRO_ROLL_RATE) {
            x = SPYRO_ROLL_RATE;
          }
          if (x < -SPYRO_ROLL_RATE) {
            x = -SPYRO_ROLL_RATE;
          }
          g_nSpyroBodyRoll = (g_nSpyroBodyRoll + x) & ANGLE12_MASK;
          if (g_nSpyroBodyRoll > ANGLE12_HALF) {
            g_nSpyroBodyRoll -= ANGLE12_TURN;
          }
          g_nSpyroBodyYaw &= ANGLE12_MASK;
          if (abs(g_nSpyroBodyPitch) < 0x40) {
            if (abs(dz) < 0x100 && abs(g_nSpyroBodyRoll) < 0x80) {
              g_nSpyroSubstate = 0;
              gb[0] = 0;
              g_nSpyroTurnRateAccum = 0;
              g_nSpyroMotionAux = 0;
            }
            g_nSpyroBodyPitch = 0;
          } else {
            p = -g_nSpyroBodyPitch & ANGLE12_MASK;

            if (p > ANGLE12_HALF) {
              p -= ANGLE12_TURN;
            }
            if (p > 0x10) {
              p = 0x10;
            }
            if (p < -0x10) {
              p = -0x10;
            }
            g_nSpyroTurnRateAccum += p;
            if (g_nSpyroTurnRateAccum > 0x40) {
              g_nSpyroTurnRateAccum = 0x40;
            }
            if (g_nSpyroTurnRateAccum < -0x40) {
              g_nSpyroTurnRateAccum = -0x40;
            }
            gb[SPY_PITCH - SPY_GLIDE_BANK] =
                (gb[SPY_PITCH - SPY_GLIDE_BANK] + g_nSpyroTurnRateAccum) &
                0xFFF;
          }
          if (SPY[SPY_PITCH] > ANGLE12_HALF) {
            SPY[SPY_PITCH] -= ANGLE12_TURN;
          }
        } else {
          int yy = gb[SPY_BODY_YAW - SPY_GLIDE_BANK] & ANGLE12_MASK;
          int r;

          gb[SPY_BODY_YAW - SPY_GLIDE_BANK] = yy;
          gb[SPY_ROLL - SPY_GLIDE_BANK] =
              (gb[SPY_ROLL - SPY_GLIDE_BANK] + SPYRO_ROLL_RATE) & ANGLE12_MASK;
          if (gb[SPY_ROLL - SPY_GLIDE_BANK] > ANGLE12_HALF) {
            gb[SPY_ROLL - SPY_GLIDE_BANK] -= ANGLE12_TURN;
          }
          r = gb[SPY_ROLL - SPY_GLIDE_BANK];
          if (r < -SPYRO_ROLL_QUARTER) {
            gb[0] = 1;
            gb[SPY_BODY_YAW - SPY_GLIDE_BANK] =
                (yy + ANGLE12_HALF) & ANGLE12_MASK;
            gb[SPY_ROLL - SPY_GLIDE_BANK] = (0x800 - r) & ANGLE12_MASK;
            g_nSpyroTurnRateAccum = 0;
            g_nSpyroBodyPitch =
                (g_nSpyroBodyPitch + ANGLE12_HALF) & ANGLE12_MASK;
          }
          p = -g_nSpyroBodyPitch & ANGLE12_MASK;
          if (p > ANGLE12_HALF) {
            p -= ANGLE12_TURN;
          }
          if (p > 0x10) {
            p = 0x10;
          }
          if (p < -0x10) {
            p = -0x10;
          }
          g_nSpyroBodyPitch = (g_nSpyroBodyPitch + p) & ANGLE12_MASK;
        }
        x = g_nSpyroFloorReferenceZ - g_nSpyroWorldPosZ;
        if (x < 0) {
          g_nSpyroMotionSpeed =
              SPYRO_FLIGHT_CRUISE_SPEED - (IntegerSqrt(-x) << 7);
        } else {
          g_nSpyroMotionSpeed =
              (IntegerSqrt(x) << 6) + SPYRO_FLIGHT_CRUISE_SPEED;
        }
        if (SPY[SPY_SPEED] > SPYRO_MAX_SPEED) {
          SPY[SPY_SPEED] = SPYRO_MAX_SPEED;
        }
        if (SPY[SPY_SPEED] < SPYRO_BANK_MIN_SPEED &&
            g_nSpyroGlideBankingFlag == 0 && g_nSpyroBodyRoll < 0x400) {
          g_nSpyroSubstate = 0;
          g_nSpyroTurnRateAccum = 0;
          g_nSpyroMotionAux = 0;
        }
        BuildSpyroVelocityFromBodyEuler();
        break;
      }
      case 3: {
        int p;

        if (g_anCameraPosBlock[12] /* g_nCameraCurrentMode */ !=
            (CAM_HELD | CAM_MODE_SCRIPTED_VANTAGE)) {
          CopyVector(g_anSpyroDeathCamRecordBlock, g_anCameraPosBlock);
          ActivateScriptedCameraView(
              SCRIPTED_CAM_VIEW_AT_POS(g_anSpyroDeathCamRecordBlock));
        }
        x = (SPYRO_ROLL_QUARTER - SPY[SPY_ROLL]) & ANGLE12_MASK;
        if (x > ANGLE12_HALF) {
          x -= ANGLE12_TURN;
        }
        if (x > SPYRO_ROLL_RATE) {
          x = SPYRO_ROLL_RATE;
        }
        if (x < -SPYRO_ROLL_RATE) {
          x = -SPYRO_ROLL_RATE;
        }
        SPY[SPY_ROLL] = (SPY[SPY_ROLL] + x) & ANGLE12_MASK;
        g_nSpyroBodyYaw &= ANGLE12_MASK;
        if (SPY[SPY_ROLL] > ANGLE12_HALF) {
          SPY[SPY_ROLL] -= ANGLE12_TURN;
        }
        p = 0x40;
        if (abs(SPY[SPY_ROLL] - SPYRO_ROLL_QUARTER) >= 0x80) {
          p = -g_nSpyroBodyPitch & ANGLE12_MASK;
          if (p > ANGLE12_HALF) {
            p -= ANGLE12_TURN;
          }
          if (p > 0x10) {
            p = 0x10;
          }
          if (p < -0x10) {
            p = -0x10;
          }
        }
        SPY[SPY_PITCH] = (SPY[SPY_PITCH] + p) & ANGLE12_MASK;
        x = SPYRO_FLIGHT_CRUISE_SPEED - g_nSpyroMotionSpeed;
        if (x < -0x80) {
          x = -0x80;
        }
        if (x > 0x80) {
          x = 0x80;
        }
        g_nSpyroMotionSpeed += x;
        BuildSpyroVelocityFromBodyEuler();
        break;
      }
      case SPYRO_SUB_GLIDE_ARRIVE: {
        SPY[SPY_SPEED_TARGET] = SPYRO_GLIDE_SPEED;
        APPROACH_GLIDE_SPEED(&SPY[SPY_SPEED_TARGET]);
        if (SPY[SPY_SPEED] < SPYRO_GLIDE_MIN_SPEED) {
          SPY[SPY_SPEED] = SPYRO_GLIDE_MIN_SPEED;
        }
        g_abSpyroPersistentEuler[2] = g_nSpyroBodyYaw >> 4;
        y = 0x1E;
        if (g_nSpyroGlideBankingFlag != 0) {
          if (g_nSpyroBodyRoll >= -0xFF) {
            LEVEL_GLIDE_HANDOFF();
          }
        } else if (g_nSpyroBodyRoll < 0) {
          g_nSpyroGlideBankingFlag = 1;
        }
        SPY[SPY_ROLL] = (SPY[SPY_ROLL] + y) & ANGLE12_MASK;
        g_abSpyroPersistentEuler[1] = SPY[SPY_ROLL] >> 4;
        if (SPY[SPY_ROLL] > ANGLE12_HALF) {
          SPY[SPY_ROLL] -= ANGLE12_TURN;
        }
        BuildSpyroVelocityFromBodyEuler();
        if (g_nSpyroInputBlankCountdown <= 0) {
          g_nSpyroInputBlankCountdown = 1;
        }
        p = (-g_nSpyroBodyYawStep * 8 - g_nSpyroBodyPitch) & ANGLE12_MASK;
        if (p > ANGLE12_HALF) {
          p -= ANGLE12_TURN;
        }
        if (p > 0x10) {
          p = 0x10;
        } else if (p < -0x10) {
          p = -0x10;
        }
        SPY[SPY_PITCH] = (SPY[SPY_PITCH] + p) & ANGLE12_MASK;
        break;
      }
      }
      if (g_nSpyroTriggerCurrentEventId != 0 &&
          g_nSpyroWorldPosZ - g_nSpyroGroundHeightZ <= 0xC00) {
        n0 = (g_nSpyroMotionSpeed >> 11) + 2;
        if (g_anSpyroVelocityFp6[0] == 0 && g_anSpyroVelocityFp6[1] == 0) {
          n0 += 0x10000;
        }
        CopyVector(wakePos, g_anSpyroVelocityFp6);
        RShiftVector3(wakePos, 6);
        wakePos[0] += g_anSpyroWorldPos[0];
        wakePos[1] += g_nSpyroWorldPosY;
        wakePos[2] =
            g_nSpyroGroundHeightZ; /* last: keeps t0 live past the fn load */
        g_pfnLevelEmitParticles(1, EMIT_FLIGHT_GROUND_WAKE, wakePos, n0);
      }
      moving = 0;
      if (g_anSpyroVelocityFp6[0] != 0 || g_anSpyroVelocityFp6[1] != 0) {
        moving = 1;
      }
      if (g_nSpyroState == SPYRO_ST_FLY) {
        int b = moving - 3;

        n = (g_nSpyroMotionSpeed >> 11) - b;
      } else {
        int b = moving - 1;

        n = (g_nSpyroMotionSpeed >> 11) - b;
      }
      if (n <= 0) {
        n = 1;
      }
      mix = g_bSpyroSubFrameTimer;
      if (moving && mix < 0xD) {
        mix += 4;
      }
      i = 0;
      tip = tipPos;
      ap = g_abSpyroAnimPrevBlock;
      OPAQUE(ap);
      next = tipNext;
      k = 9;
      for (; i < 2; i++) {
        AccumulateAnimRootMotionDelta(k, tip, ap[0], ap[6]);
        if (mix != 0) {
          AccumulateAnimRootMotionDelta(k, next, ap[1], ap[7]);
          LerpVectorGTE(tip, tip, next, mix << 8);
        }
        RotateVectorByMatrix((int *)SPY_FROM_ANIM_PREV(ap, SPYB_BODY_MTX), tip,
                             tip);
        AddVector(tip, tip, (int *)SPY_FROM_ANIM_PREV(ap, 0));
        CopyVector(next, (int *)SPY_FROM_ANIM_PREV(ap, SPY_VELOCITY_FP6 * 4));
        RShiftVector3(next, 6);
        AddVector(tip, tip, next);
        g_pfnLevelEmitParticles(1, EMIT_FLIGHT_TRAIL, tip, n);
        k += 0x5B;
      }
    } else {
      int a;
      int side;
      int c;

      READ_STICK(x, y);
      a = (g_nSpyroBodyYaw - g_nCameraEulerYaw) & ANGLE12_MASK;
      side = (-x * LookupCosine(a) - y * LookupSine(a)) >> 12;
      /* Forward stick component; reuses tip's register-sized slot. */
      tip = (int *)((y * LookupCosine(a) - x * LookupSine(a)) >> 12);
      if (((int)tip) < 0) {
        g_nSpyroSpeedTarget = ((((int)tip) * 0x1180) >> 7) + SPYRO_GLIDE_SPEED;
      } else {
        g_nSpyroSpeedTarget = SPYRO_GLIDE_SPEED;
      }
      {
        register int *t asm("$5") = &SPY[SPY_SPEED_TARGET];
        int tg;

        OPAQUE(t);
        tg = t[0];
        if (t[SPY_SPEED - SPY_SPEED_TARGET] < tg) {
          t[SPY_SPEED - SPY_SPEED_TARGET] += 0x80;
          if (t[SPY_SPEED - SPY_SPEED_TARGET] > tg) {
            t[SPY_SPEED - SPY_SPEED_TARGET] = tg;
          }
        } else if (t[SPY_SPEED - SPY_SPEED_TARGET] > tg) {
          t[SPY_SPEED - SPY_SPEED_TARGET] -= 0x80;
          if (t[SPY_SPEED - SPY_SPEED_TARGET] < tg) {
            t[SPY_SPEED - SPY_SPEED_TARGET] = tg;
          }
        }
      }
      if (SPY[SPY_SPEED] < SPYRO_GLIDE_MIN_SPEED) {
        SPY[SPY_SPEED] = SPYRO_GLIDE_MIN_SPEED;
      }
      g_nSpyroBodyYawStep = OpaqueInt(side * 20) >> 7;
      g_abSpyroPersistentEuler[2] =
          (g_nSpyroBodyYaw + g_nSpyroBodyYawStep) >> 4;
      g_nSpyroBodyYaw += g_nSpyroBodyYawStep;
      if (g_nSpyroSubstate == SPYRO_SUB_GLIDE_HOLD) {
        if (g_nReturnPortalLevelId == 0) {
          g_nSpyroBodyYawStep =
              ((g_abSpyroRequestEuler[2] << 4) - g_nSpyroBodyYaw) &
              ANGLE12_MASK;
          if (g_nSpyroBodyYawStep > ANGLE12_HALF) {
            g_nSpyroBodyYawStep -= ANGLE12_TURN;
          }
          g_nSpyroBodyYawStep <<= 4;
          if (g_nSpyroBodyYawStep < -0x10) {
            g_nSpyroBodyYawStep = -0x10;
          }
          if (g_nSpyroBodyYawStep > 0x10) {
            g_nSpyroBodyYawStep = 0x10;
          }
          g_nSpyroBodyYaw += g_nSpyroBodyYawStep;
        }
        ZeroVector(g_anSpyroVelocity);
        g_nSpyroBodyRoll = 0;
      } else if (g_nSpyroSubstate == SPYRO_SUB_GLIDE_TO_TARGET) {
        BuildSpyroVelocityFromBodyEuler();
        g_nSpyroVelocityZ = 0;
        g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_NO_CLIP);
        if (g_nSpyroInputBlankCountdown <= 0) {
          g_nSpyroInputBlankCountdown = 1;
        }
        if (ApproxDist2D(&SPY[SPY_POS], &SPY[SPY_REQUEST_TARGET]) < 0x300) {
          LEVEL_GLIDE_HANDOFF();
        }
        g_nSpyroBodyRoll = 0;
      } else if (g_nSpyroSubstate == SPYRO_SUB_GLIDE_ENTER) {
        BuildSpyroVelocityFromBodyEuler();
        g_nSpyroVelocityZ = g_anSpyroMotionVec[2];
        g_nSpyroVelocityZ = g_anSpyroMotionVec[2] + g_nSpyroGravityStep;
        if (g_nSpyroVelocityZ < -0x780) {
          g_nSpyroVelocityZ = -0x780;
        }
        g_nSpyroBodyRoll = 0;
        if (g_nSpyroInputBlankCountdown <= 0) {
          g_nSpyroInputBlankCountdown = 1;
        }
      } else if (g_nSpyroSubstate == SPYRO_SUB_GLIDE_ARRIVE) {

        if (g_nSpyroStateTimer < 0x10) {
          y = 0;
        } else {
          y = FindGroundHeightBelow(&SPY[SPY_POS], 0x10000);
          if (g_nSpyroGlideBankingFlag != 0) {
            int d = g_nSpyroWorldPosZ - y;

            if (d < 0x200) {
              LEVEL_GLIDE_HANDOFF();
            } else if (d < 0x800) {
              y = (-0x100 - g_nSpyroBodyRoll) & ANGLE12_MASK;
              if (y > ANGLE12_HALF) {
                y -= ANGLE12_TURN;
              }
              y >>= 2;
              if (y < -0x3C) {
                y = -0x3C;
              }
              if (y > 0x3C) {
                y = 0x3C;
              }
            } else {
              goto bank;
            }
          } else {
          bank:
            y = 0x1E;
            if ((unsigned int)(g_nSpyroBodyRoll + 0x100) > 0x500) {
              g_nSpyroGlideBankingFlag = 1;
              y = (-0x400 - g_nSpyroBodyRoll) & ANGLE12_MASK;
              if (y > ANGLE12_HALF) {
                y -= ANGLE12_TURN;
              }
              if (g_nActiveLevelId == LEVEL_CLIFF_TOWN && abs(y) < 0x100) {
                g_nCameraNextMode = (&g_abSpyroStateCameraMode)[g_nSpyroState];
              }
              y >>= 4;
              if (y < -0x1E) {
                y = -0x1E;
              }
              if (y > 0x1E) {
                y = 0x1E;
              }
            } else {
              g_nSpyroGlideBankingFlag = 0;
            }
          }
        }
        SPY[SPY_ROLL] = (SPY[SPY_ROLL] + y) & ANGLE12_MASK;
        g_abSpyroPersistentEuler[1] = SPY[SPY_ROLL] >> 4;
        if (SPY[SPY_ROLL] > ANGLE12_HALF) {
          SPY[SPY_ROLL] -= ANGLE12_TURN;
        }
        BuildSpyroVelocityFromBodyEuler();
        if (g_nSpyroInputBlankCountdown <= 0) {
          g_nSpyroInputBlankCountdown = 1;
        }
      } else {
        BuildSpyroVelocityFromBodyEuler();
        g_nSpyroVelocityZ = g_anSpyroMotionVec[2];
        g_nSpyroVelocityZ = g_anSpyroMotionVec[2] + g_nSpyroGravityStep;
        if (g_nSpyroVelocityZ < -0x780) {
          g_nSpyroVelocityZ = -0x780;
        }
        g_nSpyroBodyRoll = 0;
      }
      PITCH_TOWARD_TURN();
    }
    break;
  }
  case SPYRO_ST_GLIDE_DROP:
    INVULN_TICK(&SPY[SPY_INVULN]);
    g_nSpyroVelocityZ += g_nSpyroGravityStep;
    CLAMP_FALL_SPEED();
    break;
  case SPYRO_ST_LIFT: {
    Actor *actor;
    int reach;
    int *zb;
    int lift;
    int a;
    int base;
    int z;
    int m;

    INVULN_TICK(&SPY[SPY_INVULN]);
    actor = (Actor *)SPY[SPY_SCRIPTED_ACTOR];
    if (actor == 0) {
      break;
    }
    reach = 0x500;
    if (actor->type == ACTOR_TYPE_WHIRLWIND) {
      reach = ((WhirlwindState *)actor->state)->radius;
    }
    zb = &g_nSpyroWorldPosZ;
    OPAQUE(zb);
    base = g_nSpyroScriptedLiftBaseZ;
    z = zb[0];
    if (z < base + 0x400) {
      lift = g_nSpyroCarryMotionZ + 0x80;
      if (lift > 0x12C0) {
        lift = 0x12C0;
      }
      CopyVector(liftStep, zb + 0x5D);
      a = zb[0] - g_nSpyroScriptedLiftBaseZ;
      g_nSpyroSubstate = 0;
      g_nSpyroMoveTargetMag = 0x400;
      a = ((a * a) >> 11) + g_nSpyroScriptedLiftYaw;
    } else if (z < zb[0x54] - 0x400) {
      if (g_nSpyroSubstate <= 0) {
        g_nSpyroSubstate = 1;
        g_nSpyroScriptedLiftYaw += 0x200;
      }
      m = z - base - 0x400;
      a = g_nSpyroScriptedLiftYaw +
          (g_nSpyroScriptedLiftTurn * m) / g_nSpyroScriptedLiftRise;
      g_nSpyroMoveTargetMag = 0xC80;
      a = (a + ((m << 10) >> 10)) & 0xFFF;
      lift = 0x12C0;
      if (reach < m) {
        m = reach;
      }
      liftStep[0] = (m * LookupCosine(g_nSpyroBodyYaw)) >> 12;
      liftStep[1] = (m * LookupSine(g_nSpyroBodyYaw)) >> 12;
      AddVector(liftStep, liftStep, zb + 0x5D);
    } else {
      a = (g_nSpyroBodyYaw - (g_pScriptedCameraActor->rotZ << 4)) &
          ANGLE12_MASK;
      m = reach;
      if (a > ANGLE12_HALF) {
        a -= ANGLE12_TURN;
      }
      if (abs(a) < 0x100) {
        m += 0x800;
      }
      liftStep[0] = (m * LookupCosine(g_nSpyroBodyYaw)) >> 12;
      liftStep[1] = (m * LookupSine(g_nSpyroBodyYaw)) >> 12;
      AddVector(liftStep, liftStep, zb + 0x5D);
      a = zb[0x54] - zb[0];
      lift = (a * 0x12C0) >> 10;
      a = (g_pScriptedCameraActor->rotZ << 4) - ((a * a) >> 11);
      g_nSpyroMoveTargetMag = 0xC80;
    }
    liftStep[2] = 0;
    SubtractVector(liftStep, liftStep, g_anSpyroWorldPos);
    m = VectorLength(liftStep, VEC_LEN_XY);
    if (g_nSpyroMoveTargetMag < m) {
      ScaleVector3ByRatio(liftStep, m, g_nSpyroMoveTargetMag);
    }
    CopyVector(g_anSpyroWorldPos + SPY_CARRY, liftStep);
    CopyVector(g_anSpyroWorldPos + SPY_VELOCITY, g_anSpyroWorldPos + SPY_CARRY);
    g_nSpyroCarryMotionZ = lift;
    g_nSpyroVelocityZ = lift;
    g_nSpyroBodyYaw = a & ANGLE12_MASK;
    break;
  }
  case SPYRO_ST_SLIDE: {
    int *carry;
    int l;

    INVULN_TICK(&SPY[SPY_INVULN]);
    TurnSpyroYawTowardMotion(8, 4);
    if (g_nSpyroGroundSlope < SLOPE_STEEP) {
      int *c2 = &SPY[SPY_CARRY];

      ZeroVector(slideDrag);
      SubtractVector(slideDrag, slideDrag, c2);
      l = VectorLength(slideDrag, VEC_LEN_XYZ);
      if (l > 0x100) {
        ScaleVector3ByRatio(slideDrag, l, 0x100);
      }
      AddVector(c2, c2, slideDrag);
    }
    carry = g_anSpyroCarryMotionVec;
    AddVector(carry, carry, carry - 6);
    l = VectorLength(carry, VEC_LEN_XYZ);
    g_nSpyroMotionSpeed = l;
    if (l > SPYRO_TERMINAL_SPEED) {
      ScaleVector3ByRatio(carry, l, SPYRO_TERMINAL_SPEED);
      g_nSpyroMotionSpeed = SPYRO_TERMINAL_SPEED;
    }
    AddVector(carry + 3, carry, g_anSpyroSlopeGravityVec);
    break;
  }
  case SPYRO_ST_CHARGE_AIR: {
    int x;
    int y;
    int a;
    int c;
    int *w;

    INVULN_TICK(&SPY[SPY_INVULN]);
    READ_STICK(x, y);
    a = (SPY[SPY_BODY_YAW] - g_nCameraEulerYaw) & ANGLE12_MASK;
    c = LookupCosine(a);
    g_nSpyroBodyYawStep = (-x * c - y * LookupSine(a)) >> 13;
    if (g_nSpyroBodyYawStep < RAMP[0]) {
      g_nSpyroBodyYawStep = RAMP[0];
    }
    if (g_nSpyroBodyYawStep > RAMP[12]) {
      g_nSpyroBodyYawStep = RAMP[12];
    }
    IntegrateSpyroBodyEuler(g_nSpyroBodyYawStep);
    if (g_nSpyroSubstate & SPYRO_SUB_SUPERCHARGE) {
      SPEED_FROM_DROP_T(g_nSpyroSpeedTarget);
      sp = &SPY[SPY_SPEED];
      OPAQUE(sp);
      if (sp[0] > SPYRO_MAX_SPEED) {
        sp[0] = SPYRO_MAX_SPEED;
      }
      sp[SPY_VELOCITY - SPY_SPEED] =
          (sp[0] * LookupCosine(g_nSpyroBodyYaw)) >> 12;
      g_nSpyroVelocityY = (sp[0] * LookupSine(g_nSpyroBodyYaw)) >> 12;
      CopyVector(sp + (SPY_CARRY - SPY_SPEED), sp + (SPY_VELOCITY - SPY_SPEED));
    } else {
      g_nSpyroSpeedTarget = SPYRO_CHARGE_AIR_SPEED;
      AdvanceSpyroSpeedTowardTarget(0x240, 0x180);
      g_anSpyroVelocity[0] =
          (g_nSpyroMotionSpeed * LookupCosine(SPY[SPY_BODY_YAW])) >> 12;
      g_nSpyroVelocityY =
          (g_nSpyroMotionSpeed * LookupSine(SPY[SPY_BODY_YAW])) >> 12;
    }
    w = &SPY[SPY_WALL_SLIDE];
    OPAQUE(w);
    WALL_SLIDE(w, ;);
    SPY[SPY_VELOCITY + 2] -= SPYRO_FALL_ACCEL;
    if (SPY[SPY_VELOCITY + 2] < -SPYRO_TERMINAL_SPEED) {
      SPY[SPY_VELOCITY + 2] = -SPYRO_TERMINAL_SPEED;
    }
    break;
  }
  case SPYRO_ST_SUPERCHARGE_AIR: {
    int x;
    int y;
    int a;
    int c;
    register int *yp asm("$19");

    INVULN_TICK(&SPY[SPY_INVULN]);
    READ_STICK(x, y);
    yp = &SPY[SPY_BODY_YAW];
    OPAQUE(yp);
    a = (yp[0] - g_nCameraEulerYaw) & ANGLE12_MASK;
    c = LookupCosine(a);
    g_nSpyroBodyYawStep = (-x * c - y * LookupSine(a)) >> 13;
    if (g_nSpyroBodyYawStep < RAMP[0]) {
      g_nSpyroBodyYawStep = RAMP[0];
    }
    if (g_nSpyroBodyYawStep > RAMP[12]) {
      g_nSpyroBodyYawStep = RAMP[12];
    }
    IntegrateSpyroBodyEuler(g_nSpyroBodyYawStep);
    yp[SPY_VELOCITY - SPY_BODY_YAW] =
        (g_nSpyroMotionSpeed * LookupCosine(yp[0])) >> 12;
    g_nSpyroVelocityY = (g_nSpyroMotionSpeed * LookupSine(yp[0])) >> 12;
    CopyVector(yp + (SPY_CARRY - SPY_BODY_YAW),
               yp + (SPY_VELOCITY - SPY_BODY_YAW));
    if (g_nSpyroStateTimer >= 5) {
      g_nSpyroVelocityZ -= g_nSpyroGravityStep;
    }
    CLAMP_FALL_SPEED();
    break;
  }
  case SPYRO_ST_SCRIPTED_WALK: {
    int *obj;
    int *ob;
    register int *obj2 asm("$19");
    int dist;
    int ang;
    int dt;
    int a;
    int v;

    obj = ((int **)g_pScriptedCameraActor)[0];
    obj2 = obj;
    INVULN_TICK(&SPY[SPY_INVULN]);
    SubtractVector(orbitStep, &g_pScriptedCameraActor->posX, &SPY[SPY_POS]);
    dist = VectorLength(orbitStep, VEC_LEN_XY);
    ang = ArcTan2(orbitStep[0], orbitStep[1], ATAN_ANGLE12);
    dt = (g_nSpyroSubstate == SPYRO_SUB_ORBIT_NEG ? -g_nSpyroTurnRateAccum
                                                  : g_nSpyroTurnRateAccum) >>
         6;
    ComputeSpyroMoveTargetFromPad(0x280);
    TurnSpyroTowardMoveTargetWithLean(0);
    g_nSpyroTurnRateAccum += 0x20;
    if (g_nSpyroSpeedTarget < g_nSpyroTurnRateAccum) {
      g_nSpyroTurnRateAccum = g_nSpyroSpeedTarget;
    }
    a = (ang + dt + ANGLE12_HALF) & ANGLE12_MASK;
    /* The original tests type 0xFF here, though no scripted-camera actor
     * (exit whirlwind 9, fireball cannon 225, Blowhard 495) has it and both
     * arms bump the same word (obj2 == obj). */
    if (g_pScriptedCameraActor->type == 0xFF) {
      v = obj2[5] = (obj2[5] + dt) & 0xFFF;
    } else {
      v = obj[5] = (obj[5] + dt) & 0xFFF;
    }
    g_pScriptedCameraActor->rotZ = v >> 4;
    SCHED_FENCE();
    orbitStep[0] = (dist * LookupCosine(a)) >> 12;
    {
      int t = LookupSine(a);
      int *q;

      ob = Opaque(&SPY[SPY_SCRIPTED_ACTOR]);
      q = ((int **)ob)[0] + 3;
      orbitStep[1] = (dist * t) >> 12;
      AddVector(orbitStep, orbitStep, q);
    }
    SubtractVector(orbitStep, orbitStep, ob - SPY_SCRIPTED_ACTOR);
    orbitStep[2] = 0;
    LShiftVector3(orbitStep, 6);
    SCHED_FENCE();
    obj2 = ob + (SPY_VELOCITY - SPY_SCRIPTED_ACTOR);
    CopyVector(obj2, orbitStep);
    KEEP_IN_REG(obj2);
    g_nSpyroBodyYaw = (g_nSpyroBodyYaw + dt) & ANGLE12_MASK;
    if (g_nSpyroCliffEdgeHint != 0) {
      ob[SPY_VELOCITY + 2 - SPY_SCRIPTED_ACTOR] -= SPYRO_FALL_ACCEL;
    } else {
      AddVector(g_anSpyroVelocity, g_anSpyroVelocity, g_anSpyroSlopeGravityVec);
    }
    break;
  }
  case SPYRO_ST_KNOCKBACK:
  case SPYRO_ST_KNOCKBACK_2: {
    int *vel;
    int l;

    ZeroVector(knockDrag);
    vel = &SPY[SPY_VELOCITY];
    SubtractVector(knockDrag, knockDrag, vel);
    if (g_nSpyroAirborneFrames != 0) {
      knockDrag[2] = 0;
      l = VectorLength(knockDrag, VEC_LEN_XY);
      if (l > 0x40) {
        ScaleVector3ByRatio(knockDrag, l, 0x40);
      }
      knockDrag[2] = -SPYRO_FALL_ACCEL;
      AddVector(vel, vel, knockDrag);
    } else {
      l = VectorLength(knockDrag, VEC_LEN_XYZ);
      if (l > 0x100) {
        ScaleVector3ByRatio(knockDrag, l, 0x100);
      }
      AddVector(vel, vel, knockDrag);
      AddVector(vel, vel, &SPY[SPY_SLOPE_GRAVITY]);
    }
    if (SPY[SPY_VELOCITY + 2] < -SPYRO_TERMINAL_SPEED) {
      SPY[SPY_VELOCITY + 2] = -SPYRO_TERMINAL_SPEED;
    }
    break;
  }
  case SPYRO_ST_HAZARD:
    ZeroVector(g_anSpyroVelocity);
    break;
  case SPYRO_ST_DEATH:
  case SPYRO_ST_DEATH_PINNED: {
    int *carry = &SPY[SPY_CARRY];
    int *vel = carry + 3;

    carry[0] -= carry[0] >> 4;
    g_nSpyroCarryMotionY -= g_nSpyroCarryMotionY >> 4;
    g_nSpyroCarryMotionZ -= g_nSpyroCarryMotionZ >> 4;
    CopyVector(vel, carry);
    __asm__ volatile("" : : "r"(carry), "r"(vel));
    FINISH_VELOCITY();
    break;
  }
  case SPYRO_ST_FACE_TARGET:
  case SPYRO_ST_SCRIPTED_HOLD:
    INVULN_TICK(&SPY[SPY_INVULN]);
    break;
  case SPYRO_ST_SUPERCHARGE: {
    int a;
    int *vel;

    INVULN_TICK(&SPY[SPY_INVULN]);
    if (g_pPadSubstepState->stickActive != 0) {
      g_nSpyroBodyYawStep = (PAD_STICK_CENTER - PADB[PAD_LX_OFFSET]) >> 1;
      if (g_nSpyroBodyYawStep < RAMP[0]) {
        g_nSpyroBodyYawStep = RAMP[0];
      } else if (g_nSpyroBodyYawStep > RAMP[12]) {
        g_nSpyroBodyYawStep = RAMP[12];
      }
      a = g_nSpyroBodyYawStep;
    } else {
      if (g_pPadSubstepState->held & PAD_LEFT) {
        if (g_nSpyroBodyYawStep < 0) {
          g_nSpyroBodyYawStep = 0;
        }
        if (g_nSpyroBodyYawStep < 6) {
          g_nSpyroBodyYawStep++;
        }
      } else if (g_pPadSubstepState->held & PAD_RIGHT) {
        if (g_nSpyroBodyYawStep > 0) {
          g_nSpyroBodyYawStep = 0;
        }
        if (g_nSpyroBodyYawStep > -6) {
          g_nSpyroBodyYawStep--;
        }
      } else {
        g_nSpyroBodyYawStep = 0;
      }
      a = RAMP[6 + g_nSpyroBodyYawStep];
    }
    IntegrateSpyroBodyEuler(a);
    SPEED_FROM_DROP_T(g_nSpyroSpeedTarget);
    sp = &SPY[SPY_SPEED];
    OPAQUE(sp);
    if (sp[0] > SPYRO_MAX_SPEED) {
      sp[0] = SPYRO_MAX_SPEED;
    }
    if (sp[0] < SPYRO_CHARGE_MIN_SPEED) {
      g_nSpyroSpeedTarget = SPYRO_CHARGE_MIN_SPEED;
      g_nSpyroFloorReferenceZ = g_nSpyroWorldPosZ;
    }
    vel = sp + (SPY_VELOCITY - SPY_SPEED);
    BuildSpyroVelocityFromBodyEuler();
    CopyVector(sp + (SPY_CARRY - SPY_SPEED), vel);
    if (g_nSpyroAirborneFrames == 0) {
      AddVector(vel, vel, sp + (SPY_SLOPE_GRAVITY - SPY_SPEED));
    }
    g_nPulseRumbleAmount =
        ((sp[0] - SPYRO_CHARGE_MIN_SPEED) >> 7) + SUPERCHARGE_RUMBLE_BASE;
    if (g_nPulseRumbleAmount < SUPERCHARGE_RUMBLE_MIN) {
      g_nPulseRumbleAmount = SUPERCHARGE_RUMBLE_MIN;
    }
    if (g_nPulseRumbleAmount > SUPERCHARGE_RUMBLE_MAX) {
      g_nPulseRumbleAmount = SUPERCHARGE_RUMBLE_MAX;
    }
    if (g_nPulseRumbleTimer < SUPERCHARGE_RUMBLE_FRAMES) {
      g_nPulseRumbleTimer = SUPERCHARGE_RUMBLE_FRAMES;
    }
    break;
  }
  }
  AddVector(&SPY[SPY_VELOCITY_FP6], &SPY[SPY_VELOCITY_FP6], &SPY[SPY_VELOCITY]);
  g_abSpyroPersistentEuler[0] = g_nSpyroBodyPitch >> 4;
  g_abSpyroPersistentEuler[1] = g_nSpyroBodyRoll >> 4;
  g_abSpyroPersistentEuler[2] = g_nSpyroBodyYaw >> 4;
  {
    register int hr asm("$9");

    __asm__ volatile("" : "=r"(hr));
  }
  {
    register int hs asm("$18");

    __asm__ volatile("" : "=r"(hs));
  }
}
