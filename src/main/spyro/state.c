#include "actor_states.h"
#include "angle.h"
#include "camera.h"
#include "emit_record.h"
#include "funcs.h"
#include "globals.h"
#include "level_id.h"
#include "level_sfx.h"
#include "main_shared.h"
#include "match_asm.h"
#include "model.h"
#include "pad.h"
#include "pad_buttons.h"
#include "sample_bank.h"
#include "sdata.h"
#include "spyro_block.h"
#include "spyro_state.h"
#include "vecmath.h"

/* SPYRO_ST_LIFT (scripted lift toward g_anSpyroRequestTargetPos): the start Z,
   the rise to cover, the starting yaw and the yaw turn to the scripted camera
   actor's heading, spread over the rise by IntegrateSpyroMotionForSubstep.
   g_pSpyroHomingTargetActor: nearest live actor in a +-0x200 cone ahead,
   steered toward for 0x78 frames.  g_bSpyroJumpHoldForced: SPYRO_ST_JUMP
   entered from SPYRO_ST_HAZARD, treated as a held jump button. */
SDATA(g_nSpyroScriptedLiftBaseZ, 4);
SDATA(g_bSpyroJumpHoldForced, 4);
SDATA(g_nSpyroScriptedLiftRise, 4);
SDATA(g_nSpyroScriptedLiftYaw, 4);
SDATA(g_pSpyroHomingTargetActor, 4);
SDATA(g_nSpyroScriptedLiftTurn, 4);

/* Charge-speed latch: the first charge frame takes the carried speed, at
   least SPYRO_CHARGE_SPEED, as the speed target. */
#define LATCH_CHARGE_SPEED(vec)                                                \
  {                                                                            \
    int v;                                                                     \
                                                                               \
    g_nSpyroFloorReferenceZ = g_nSpyroWorldPosZ;                               \
    v = VectorLength(vec, VEC_LEN_XYZ);                                        \
    if (v < SPYRO_CHARGE_SPEED) {                                              \
      v = SPYRO_CHARGE_SPEED;                                                  \
    }                                                                          \
    g_nSpyroSpeedTarget = v;                                                   \
  }

/* Spyro state transition (0x8003ea68, 4960 bytes). Runs the exit side of the
   old state (stop the charge/glide/fly loops, play the hurt states' exit
   sounds),
   then the entry setup of the new one: motion carry-over, gravity step and
   enable, speed targets and sounds, the charge homing-target search
   (SPYRO_ST_CHARGE), the dizzy stars (SPYRO_ST_CHARGE_BONK) and the
   scripted-move / ledge setups. Finally swaps the state, restarts the state
   timer and loads the state's animation play rate. */
void ChangeSpyroState(int state) {
  int d[3];
  int v[3];

  switch (g_nSpyroState) {
  case SPYRO_ST_CHARGE:
  case SPYRO_ST_GLIDE:
  case SPYRO_ST_FLY:
  case SPYRO_ST_SUPERCHARGE:
    StopSoundVoicesByOwner((Actor *)g_anSpyroWorldPos, VOICE_STOP_LOOPING);
    break;
  case SPYRO_ST_HURT_TINT:
    g_abSpyroTintRgba[3] = 0;
    /* FALLTHROUGH */
  case SPYRO_ST_HURT:
  case SPYRO_ST_KNOCKBACK:
  case SPYRO_ST_HURT_LONG:
  case SPYRO_ST_KNOCKBACK_2:
    PlaySoundEffect(LEVEL_SFX(SFX_SPYRO_HURT_EXIT), (Actor *)g_anSpyroWorldPos,
                    SFX_MODE_FLAT,
                    (unsigned char *)g_anSpyroWorldPos + SPYB_VOICE_MARKER);
    break;
  case SPYRO_ST_HURT_PINNED:
    PlaySoundEffect(LEVEL_SFX(SFX_SPYRO_PINNED_EXIT),
                    (Actor *)g_anSpyroWorldPos, SFX_MODE_FLAT,
                    (unsigned char *)g_anSpyroWorldPos + SPYB_VOICE_MARKER);
    break;
  }
  switch (state) {
  case SPYRO_ST_IDLE: {
    int t;

    g_nSpyroSubstate = 0;
    t = g_abSpyroIdleDelayRandTable[GetRandomU32() & 7] *
        SPYRO_ANIM_DESC(SPYRO_ANIMID_IDLE, SPYRO_ANIM_DESC_FRAMES);
    g_nSpyroGravityStep = -SPYRO_LIGHT_FALL_ACCEL;
    g_nSpyroLateJumpAllowed = 1;
    g_nSpyroIdleAnimTimeout = t - 2;
    break;
  }
  case SPYRO_ST_WALK:
  case SPYRO_ST_RUN:
  case SPYRO_ST_STOP:
  case SPYRO_ST_JOG: {
    int a;

    SPY[SPY_SUBSTATE] = 0;
    a = (ArcTan2(SPY[SPY_MOTION], g_anSpyroMotionVec[1], ATAN_ANGLE12) -
         g_nSpyroBodyYaw) &
        ANGLE12_MASK;
    if (ANGLE12_BEHIND(a)) {
      g_nSpyroMotionSpeed = 0;
    } else {
      g_nSpyroMotionSpeed = VectorLength(&SPY[SPY_MOTION], VEC_LEN_XY);
    }
    if (state == SPYRO_ST_STOP && SPY[SPY_SPEED] > SPYRO_STOP_MAX_SPEED) {
      SPY[SPY_SPEED] = SPYRO_STOP_MAX_SPEED;
    }
    g_nSpyroGravityStep = -SPYRO_GROUND_PRESS;
    ApplyGravityAlongSpyroSlopeNormal();
    if (g_nSpyroGroundSlope != 0) {
      int k;
      int base;
      int side;

      a = (g_nSpyroBodyYaw - ArcTan2(g_anSpyroGroundNormal[0],
                                     g_anSpyroGroundNormal[1], ATAN_ANGLE12)) &
          ANGLE12_MASK;
      if (a > ANGLE12_HALF) {
        a -= ANGLE12_TURN;
      }
      k = (g_nSpyroGroundSlope << 12) / (SLOPE_STEEP - 1);
      a = abs(a);
      if (a > ANGLE12_QUARTER) { /* facing uphill */
        a = ANGLE12_HALF - a;
        base = g_anSpyroSlopeSpeedTargets[SLOPE_SPEED_UP] +
               ((k * (g_anSpyroSlopeSpeedTargets[SLOPE_SPEED_UP +
                                                 SLOPE_SPEED_STEEPEST] -
                      g_anSpyroSlopeSpeedTargets[SLOPE_SPEED_UP])) >>
                12);
      } else {
        base = g_anSpyroSlopeSpeedTargets[SLOPE_SPEED_DOWN] +
               ((k * (g_anSpyroSlopeSpeedTargets[SLOPE_SPEED_DOWN +
                                                 SLOPE_SPEED_STEEPEST] -
                      g_anSpyroSlopeSpeedTargets[SLOPE_SPEED_DOWN])) >>
                12);
      }
      side = g_anSpyroSlopeSpeedTargets[SLOPE_SPEED_ACROSS] +
             ((k * (g_anSpyroSlopeSpeedTargets[SLOPE_SPEED_ACROSS +
                                               SLOPE_SPEED_STEEPEST] -
                    g_anSpyroSlopeSpeedTargets[SLOPE_SPEED_ACROSS])) >>
              12);
      base = (base * LookupCosine(a)) >> 12;
      side = (side * LookupSine(a)) >> 12;
      g_nSpyroMoveTargetMag = IntegerSqrt(base * base + side * side);
    } else {
      g_nSpyroMoveTargetMag = g_anSpyroSlopeSpeedTargets[SLOPE_SPEED_DOWN];
    }
    g_nSpyroLateJumpAllowed = 1;
    if (g_nSpyroScriptedMoveMode != 0) {
      g_nSpyroSpeedTarget = 0;
      g_nSpyroMoveTargetYaw = g_abSpyroRequestEuler[2] << 4;
    }
    break;
  }
  case SPYRO_ST_TURN_AROUND:
    g_nSpyroSubstate = 0;
    g_nSpyroGravityStep = -SPYRO_GROUND_PRESS;
    goto slope_gravity;
  case SPYRO_ST_JUMP:
    CopyVector(g_anSpyroVelocity,
               SPY_VIA(g_anSpyroVelocity, SPY_VELOCITY, SPY_MOTION));
    g_anSpyroCarryMotionVec[2] = SPYRO_JUMP_SPEED;
    g_nSpyroGravityStep = -SPYRO_FALL_ACCEL;
    g_nSpyroSubstate = 0;
    g_nSpyroGlideQueued = 0;
    g_nSpyroLateJumpAllowed = 0;
    g_nSpyroFloorReferenceZ = g_nSpyroWorldPosZ + (g_anSpyroVelocity[2] >> 6);
    if (g_nSpyroState == SPYRO_ST_HAZARD) {
      g_bSpyroJumpHoldForced = 1;
    } else {
      g_bSpyroJumpHoldForced = 0;
    }
    break;
  case SPYRO_ST_FALL:
    SPY[SPY_SUBSTATE] = 0;
    CopyVector(&SPY[SPY_VELOCITY], &SPY[SPY_MOTION]);
    if (g_nSpyroGroundProbeSuspendFlag == 0 &&
        g_nSpyroGroundSlope < SLOPE_NUDGE_MAX) {
      NudgeSpyroFromWallProbes();
    }
    g_nSpyroGravityStep = -SPYRO_FALL_ACCEL;
    g_nSpyroGlideQueued = 0;
    g_nSpyroLateJumpAllowed = 0;
    break;
  case SPYRO_ST_GLIDE_DROP:
    g_nSpyroSubstate = 0;
    g_anSpyroVelocity[0] = 0;
    g_anSpyroVelocity[1] = 0;
    g_anSpyroVelocity[2] = g_anSpyroMotionVec[2];
    if (g_nSpyroGroundProbeSuspendFlag == 0 &&
        g_nSpyroGroundSlope < SLOPE_NUDGE_MAX) {
      NudgeSpyroFromWallProbes();
    }
    g_nSpyroGravityStep = -SPYRO_FALL_ACCEL;
    g_nSpyroGlideQueued = 0;
    g_nSpyroLateJumpAllowed = 0;
    break;
  case SPYRO_ST_SIDESTEP_L1:
  case SPYRO_ST_SIDESTEP_R1:
    SPY[SPY_SUBSTATE] = 0;
    CopyVector(&SPY[SPY_CARRY], &SPY[SPY_MOTION]);
    g_nSpyroMotionSpeed = 0;
    g_nSpyroGravityStep = -SPYRO_GROUND_PRESS;
    goto slope_gravity;
  case SPYRO_ST_CHARGE: {
    int best;
    Actor *actor;
    int *me;

    CopyVector(g_anSpyroCarryMotionVec,
               SPY_VIA(g_anSpyroCarryMotionVec, SPY_CARRY, SPY_MOTION));
    if (g_nSpyroChargeHoldFlag != 0) {
      if (!(g_nSpyroSubstate & SPYRO_SUB_CHARGE_LATCHED)) {
        LATCH_CHARGE_SPEED(g_anSpyroCarryMotionVec);
      }
      g_nSpyroSubstate = SPYRO_SUB_CHARGE_LATCHED | SPYRO_SUB_CHARGE_STEER;
    } else {
      g_nSpyroSubstate = SPYRO_SUB_CHARGE_STEER;
    }
    SPY[SPY_BODY_YAW_STEP] = 0;
    g_nSpyroGravityStep = -SPYRO_CHARGE_FALL_ACCEL;
    me = SPY;
    ApplyGravityAlongSpyroSlopeNormal();
    ApplyGravityAlongSpyroSlopeTangent();
    g_nSpyroLateJumpAllowed = 1;
    PlaySoundEffect(LEVEL_SFX(SFX_CHARGE), (Actor *)me, SFX_MODE_FLAT,
                    (unsigned char *)SPY + SPYB_VOICE_MARKER + 1);
    g_pSpyroHomingTargetActor = 0;
    {
      int t = (g_nSpyroBodyYaw - g_nCameraEulerYaw) & ANGLE12_MASK;

      if (t > ANGLE12_HALF) {
        t -= ANGLE12_TURN;
      }
      if (abs(t) <= SPYRO_HOMING_CONE) {
        break;
      }
    }
    actor = g_pActorListBase;
    best = SPYRO_HOMING_NO_SCORE;
    if (actor < g_pActorPoolDynBase) {
      int *from = me;

      do {
        if (actor->mode < 0x7F && actor->collision != 0) {
          int a;
          int da;

          SubtractVector(d, &actor->posX, from);
          a = (ArcTan2(d[0], d[1], ATAN_ANGLE12) - g_nSpyroBodyYaw) &
              ANGLE12_MASK;
          if (a > ANGLE12_HALF) {
            a -= ANGLE12_TURN;
          }
          da = abs(a);
          if (da < SPYRO_HOMING_CONE) {
            int x = d[0];
            int y = d[1];
            int z = d[2];

            x = abs(x) + abs(y) + abs(z);
            if (x < SPYRO_HOMING_QUICK_RANGE) {
              x = VectorLength(d, VEC_LEN_XYZ);
              if (x < SPYRO_HOMING_RANGE) {
                x += da * SPYRO_HOMING_ANGLE_WEIGHT;
                if (x < best) {
                  best = x;
                  g_pSpyroHomingTargetActor = actor;
                }
              }
            }
          }
        }
      next:
        actor++;
      } while (actor < g_pActorPoolDynBase);
    }
    break;
  }
  case SPYRO_ST_CHARGE_BONK: {
    int i;
    int ang;

    ang = GetRandomU32() & 0x3F;
    for (i = 0; i < 4; i++) {
      v[0] = 0;
      v[1] = (short)(unsigned short)g_anCosineLut[ang] >> 7;
      v[2] = g_anSineLut[ang] >> 7;
      RotateVectorByMatrix(g_anSpyroBodyMtx, v, v);
      g_pfnLevelEmitParticles(1, EMIT_DUST_PUFF, v, 1);
      ang = (ang + ANGLE8_QUARTER) & ANGLE8_MASK;
    }
    SPY[SPY_SUBSTATE] = 0;
    ZeroVector(&SPY[SPY_CARRY]);
    SubtractVector(&SPY[SPY_VELOCITY], &SPY[SPY_CARRY], &SPY[SPY_VELOCITY]);
    RShiftVector3(&SPY[SPY_VELOCITY], 2);
    g_nSpyroGravityStep = -SPYRO_HURT_FALL_ACCEL;
    g_nSpyroLateJumpAllowed = 1;
    if (g_nPulseRumbleAmount < BONK_RUMBLE_AMOUNT) {
      g_nPulseRumbleAmount = BONK_RUMBLE_AMOUNT;
    }
    if (g_nPulseRumbleTimer < BONK_RUMBLE_FRAMES) {
      g_nPulseRumbleTimer = BONK_RUMBLE_FRAMES;
    }
    break;
  }
  case SPYRO_ST_TEETER:
    g_nSpyroGravityStep = -SPYRO_GROUND_PRESS;
    goto stand;
  case SPYRO_ST_HURT_TINT: {
    int len;

    SPY[SPY_SUBSTATE] = 0;
    ZeroVector(&SPY[SPY_VELOCITY]);
    SubtractVector(&SPY[SPY_VELOCITY], &SPY[SPY_VELOCITY], &SPY[SPY_MOTION]);
    len = VectorLength(&SPY[SPY_VELOCITY], VEC_LEN_XYZ);
    if (len > SPYRO_HURT_KNOCK_MAX_SPEED) {
      ScaleVector3ByRatio(&SPY[SPY_VELOCITY], len, SPYRO_HURT_KNOCK_MAX_SPEED);
    }
    g_nSpyroGravityStep = -SPYRO_HURT_FALL_ACCEL;
    if (g_nSpyroAirborneFrames == 0) {
      ApplyGravityAlongSpyroSlopeNormal();
    }
    g_nSpyroLateJumpAllowed = 1;
    g_pfnLevelEmitParticles(5, EMIT_SPYRO_HURT_SPARKLE, 0, 0);
    if (g_nVibrationLevel < HURT_TINT_VIBRATION) {
      g_nVibrationLevel = HURT_TINT_VIBRATION;
    }
    break;
  }
  case SPYRO_ST_HURT: {
    int len;

    SPY[SPY_SUBSTATE] = 0;
    ZeroVector(&SPY[SPY_VELOCITY]);
    SubtractVector(&SPY[SPY_VELOCITY], &SPY[SPY_VELOCITY], &SPY[SPY_MOTION]);
    len = VectorLength(&SPY[SPY_VELOCITY], VEC_LEN_XYZ);
    if (len > SPYRO_HURT_KNOCK_MAX_SPEED) {
      ScaleVector3ByRatio(&SPY[SPY_VELOCITY], len, SPYRO_HURT_KNOCK_MAX_SPEED);
    }
    g_nSpyroGravityStep = -SPYRO_HURT_FALL_ACCEL;
    if (g_nSpyroAirborneFrames == 0) {
      ApplyGravityAlongSpyroSlopeNormal();
    }
    g_nSpyroLateJumpAllowed = 1;
    g_pfnLevelEmitParticles(5, EMIT_SPYRO_HURT_SPARKLE, 0, 0);
    if (g_nHitRumbleTimer < HIT_RUMBLE_FRAMES) {
      g_nHitRumbleTimer = HIT_RUMBLE_FRAMES;
    }
    PlaySoundEffect(LEVEL_SFX(SFX_SPYRO_HURT), (Actor *)SPY, SFX_MODE_FLAT,
                    (unsigned char *)SPY + SPYB_VOICE_MARKER);
    break;
  }
  case SPYRO_ST_HURT_PINNED:
    SPY[SPY_SUBSTATE] = 0;
    ZeroVector(&SPY[SPY_VELOCITY]);
    g_anSpyroVelocity[2] = -SPYRO_PINNED_PRESS;
    g_nSpyroLateJumpAllowed = 1;
    g_pfnLevelEmitParticles(5, EMIT_SPYRO_HURT_SPARKLE, 0, 0);
    if (g_nHitRumbleTimer < HIT_RUMBLE_FRAMES) {
      g_nHitRumbleTimer = HIT_RUMBLE_FRAMES;
    }
    PlaySoundEffect(LEVEL_SFX(SFX_SPYRO_HURT_PINNED), (Actor *)SPY,
                    SFX_MODE_FLAT, (unsigned char *)SPY + SPYB_VOICE_MARKER);
    break;
  case SPYRO_ST_HURT_LONG:
    SPY[SPY_SUBSTATE] = 0;
    ZeroVector(&SPY[SPY_VELOCITY]);
    g_nSpyroGravityStep = -SPYRO_HURT_FALL_ACCEL;
    g_nSpyroLateJumpAllowed = 1;
    g_pfnLevelEmitParticles(5, EMIT_SPYRO_HURT_SPARKLE, 0, 0);
    if (g_nHitRumbleTimer < HIT_RUMBLE_FRAMES) {
      g_nHitRumbleTimer = HIT_RUMBLE_FRAMES;
    }
    break;
  case SPYRO_ST_GLIDE:
  case SPYRO_ST_GLIDE_SLOW:
  case SPYRO_ST_FLY:
  case SPYRO_ST_FLY_CLIMB:
  case SPYRO_ST_FLY_DIVE: {
    int *st = &SPY[SPY_STATE];

    if (*st == SPYRO_ST_GLIDE || *st == SPYRO_ST_GLIDE_SLOW ||
        (unsigned)(*st - SPYRO_ST_FLY) < 2 || *st == SPYRO_ST_FLY_DIVE) {
      g_nSpyroLateJumpAllowed = 0;
      break;
    }
    g_nSpyroSubstate = 0;
    g_nSpyroGlideSpentFlag = 1;
    g_nSpyroBodyYawStep = 0;
    g_nSpyroPitchRateAccum = 0;
    g_nSpyroTurnRateAccum = 0;
    g_nSpyroMotionAux = 0;
    if (g_nSpyroMotionZ > 0) {
      g_nSpyroMotionZ = 0;
    }
    g_nSpyroMotionSpeed =
        VectorLength(&st[SPY_VELOCITY - SPY_STATE], VEC_LEN_XY);
    if (g_nSpyroMotionSpeed > SPYRO_GLIDE_SPEED) {
      g_nSpyroMotionSpeed = SPYRO_GLIDE_SPEED;
    }
    if (g_nSpyroMotionSpeed < SPYRO_GLIDE_MIN_SPEED) {
      g_nSpyroMotionSpeed = SPYRO_GLIDE_MIN_SPEED;
    }
    if (*st != SPYRO_ST_FALL || g_nFlightLevelActive ||
        g_nSpyroFloorReferenceZ < g_nSpyroWorldPosZ) {
      g_nSpyroFloorReferenceZ = g_nSpyroWorldPosZ;
    }
    if (g_nSpyroBodyRoll > 0x400) {
      g_nSpyroGlideBankingFlag = 1;
    } else {
      g_nSpyroGlideBankingFlag = 0;
    }
    g_nSpyroGravityStep = -SPYRO_LIGHT_FALL_ACCEL;
    g_nSpyroLateJumpAllowed = 0;
    break;
  }
  case SPYRO_ST_LIFT: {
    int t;

    if (SPY[SPY_SCRIPTED_ACTOR] != 0) {
      ZeroVector(&SPY[SPY_VELOCITY]);
      g_nSpyroScriptedLiftYaw = g_nSpyroBodyYaw;
      g_nSpyroFloorReferenceZ = g_anSpyroRequestTargetPos[2];
      g_nSpyroScriptedLiftRise =
          g_anSpyroRequestTargetPos[2] - g_nSpyroWorldPosZ - 0x800;
      g_nSpyroScriptedLiftBaseZ = g_nSpyroWorldPosZ;
      g_nSpyroScriptedLiftTurn =
          ((g_nSpyroScriptedLiftRise << 10) >> 10) + 0x400;
      g_nSpyroScriptedLiftTurn = ((ANGLE8_TO_12(g_pScriptedCameraActor->rotZ)) -
                                  g_nSpyroBodyYaw - g_nSpyroScriptedLiftTurn) &
                                 ANGLE12_MASK;
      if (g_nSpyroScriptedLiftTurn > ANGLE12_HALF) {
        g_nSpyroScriptedLiftTurn -= ANGLE12_TURN;
      }
    } else {
      CopyVector(&SPY[SPY_VELOCITY], &SPY[SPY_MOTION]);
    }
    g_nSpyroGravityStep = SPYRO_LIGHT_FALL_ACCEL; /* rising */
    g_nSpyroLateJumpAllowed = 0;
    break;
  }
  case SPYRO_ST_FIDGET_0:
  case SPYRO_ST_FIDGET_1:
  case SPYRO_ST_FIDGET_2:
  case SPYRO_ST_FIDGET_3:
  case SPYRO_ST_FIDGET_4:
  case SPYRO_ST_FIDGET_5:
  case SPYRO_ST_FIDGET_6:
  case SPYRO_ST_FIDGET_7:
  case SPYRO_ST_FIDGET_8: {
    int t;

    g_nSpyroSubstate = 0;
    t = SPYRO_ANIM_DESC(state, SPYRO_ANIM_DESC_FRAMES);
    g_nSpyroGravityStep = -SPYRO_GROUND_PRESS;
    g_nSpyroIdleAnimTimeout = t << 4;
    break;
  }
  case SPYRO_ST_SLIDE:
    SPY[SPY_SUBSTATE] = 0;
    CopyVector(&SPY[SPY_CARRY], &SPY[SPY_MOTION]);
    if (g_nSpyroCarryMotionZ > 0) {
      g_nSpyroCarryMotionZ = 0;
    }
    g_nSpyroGravityStep = -SPYRO_SLIDE_FALL_ACCEL;
    ApplyGravityAlongSpyroSlopeNormal();
    ApplyGravityAlongSpyroSlopeTangent();
    g_nSpyroLateJumpAllowed = 0;
    break;
  case SPYRO_ST_CHARGE_AIR:
    CopyVector(g_anSpyroVelocity,
               SPY_VIA(g_anSpyroVelocity, SPY_VELOCITY, SPY_MOTION));
    CopyVector(SPY_VIA(g_anSpyroVelocity, SPY_VELOCITY, SPY_CARRY),
               SPY_VIA(g_anSpyroVelocity, SPY_VELOCITY, SPY_MOTION));
    g_nSpyroBodyYawStep = 0;
    if (g_nSpyroChargeHoldFlag != 0) {
      if (!(g_nSpyroSubstate & SPYRO_SUB_CHARGE_LATCHED)) {
        LATCH_CHARGE_SPEED(SPY_VIA(g_anSpyroVelocity, SPY_VELOCITY, SPY_CARRY));
      }
      SPY[SPY_SUBSTATE] |= SPYRO_SUB_CHARGE_LATCHED;
    } else {
      g_nSpyroSubstate = 0;
      g_anSpyroVelocity[2] = 0;
      g_anSpyroCarryMotionVec[2] = 0;
    }
    g_nSpyroGravityStep = -SPYRO_CHARGE_FALL_ACCEL;
    g_nSpyroGlideQueued = 0;
    g_nSpyroLateJumpAllowed = 0;
    break;
  case SPYRO_ST_SUPERCHARGE_AIR:
    CopyVector(g_anSpyroVelocity,
               SPY_VIA(g_anSpyroVelocity, SPY_VELOCITY, SPY_MOTION));
    CopyVector(SPY_VIA(g_anSpyroVelocity, SPY_VELOCITY, SPY_CARRY),
               SPY_VIA(g_anSpyroVelocity, SPY_VELOCITY, SPY_MOTION));
    g_nSpyroBodyYawStep = 0;
    g_nSpyroSubstate = SPYRO_SUB_CHARGE_LATCHED | SPYRO_SUB_SUPERCHARGE;
    g_nSpyroGravityStep = 0;
    g_nSpyroGlideQueued = 0;
    break;
  case SPYRO_ST_SCRIPTED_WALK:
    SPY[SPY_SUBSTATE] = 0;
    if (ANGLE12_BEHIND(
            (ArcTan2(SPY[SPY_MOTION], g_anSpyroMotionVec[1], ATAN_ANGLE12) -
             g_nSpyroBodyYaw) &
            ANGLE12_MASK)) {
      g_nSpyroMotionSpeed = 0;
    } else {
      g_nSpyroMotionSpeed = VectorLength(&SPY[SPY_MOTION], VEC_LEN_XY);
    }
    g_nSpyroGravityStep = -SPYRO_GROUND_PRESS;
    ApplyGravityAlongSpyroSlopeNormal();
    g_nSpyroMoveTargetMag = SPYRO_SCRIPTED_WALK_SPEED;
    g_nSpyroTurnRateAccum = 0;
    g_nSpyroLateJumpAllowed = 1;
    break;
  case SPYRO_ST_KNOCKBACK:
  case SPYRO_ST_KNOCKBACK_2:
    g_anSpyroVelocity[0] = g_anSpyroRequestVelocity[0] << 6;
    g_nSpyroSubstate = 0;
    g_anSpyroVelocity[1] = g_anSpyroRequestVelocity[1] << 6;
    if (g_anSpyroRequestVelocity[2] != 0) {
      g_anSpyroVelocity[2] = g_anSpyroRequestVelocity[2] << 6;
    } else {
      g_anSpyroVelocity[2] = g_anSpyroMotionVec[2];
    }
    ArcTan2(SPY[SPY_VELOCITY], g_anSpyroVelocity[1], ATAN_ANGLE12);
    g_nSpyroGravityStep = -SPYRO_HURT_FALL_ACCEL;
    if (g_nSpyroAirborneFrames == 0) {
      ApplyGravityAlongSpyroSlopeNormal();
    }
    g_nSpyroLateJumpAllowed = 1;
    g_pfnLevelEmitParticles(5, EMIT_SPYRO_HURT_SPARKLE, 0, 0);
    PlaySoundEffect(LEVEL_SFX(SFX_SPYRO_HURT), (Actor *)SPY, SFX_MODE_FLAT,
                    (unsigned char *)SPY + SPYB_VOICE_MARKER);
    if (g_nHitRumbleTimer < HIT_RUMBLE_FRAMES) {
      g_nHitRumbleTimer = HIT_RUMBLE_FRAMES;
    }
    break;
  case SPYRO_ST_HAZARD: {
    int *p = &SPY[SPY_SUBSTATE];
    int *me;

    *p = 0;
    g_nSpyroGravityStep = -SPYRO_LIGHT_FALL_ACCEL;
    g_nSpyroLateJumpAllowed = 1;
    if (g_nSpyroInvulnFrames == 0) {
      g_pfnLevelEmitParticles(5, EMIT_SPYRO_HURT_SPARKLE, 0, 0);
    }
    g_pfnLevelSpawnActor(g_anHazardEnterSpawnType[g_nSpyroHazardKind], 0);
    me = SPY;
    PlaySoundEffect(LEVEL_SFX(SFX_SPLASH), (Actor *)me, SFX_MODE_FLAT,
                    (unsigned char *)SPY + SPYB_VOICE_MARKER);
    PlaySoundEffect(LEVEL_SFX(SFX_SPYRO_HURT_EXIT), (Actor *)me, SFX_MODE_FLAT,
                    (unsigned char *)SPY + SPYB_VOICE_MARKER);
    if (g_nHitRumbleTimer < HIT_RUMBLE_FRAMES) {
      g_nHitRumbleTimer = HIT_RUMBLE_FRAMES;
    }
    break;
  }
  case SPYRO_ST_DEATH:
    SPY[SPY_SUBSTATE] = 0;
    CopyVector(&SPY[SPY_CARRY], &SPY[SPY_MOTION]);
    g_nSpyroGravityStep = -SPYRO_HURT_FALL_ACCEL;
    if (g_nSpyroAirborneFrames == 0) {
    slope_gravity:
      ApplyGravityAlongSpyroSlopeNormal();
    }
    g_nSpyroLateJumpAllowed = 1;
    break;
  case SPYRO_ST_DEATH_PINNED:
    SPY[SPY_SUBSTATE] = 0;
    CopyVector(&SPY[SPY_CARRY], &SPY[SPY_MOTION]);
    g_nSpyroGravityStep = -SPYRO_HURT_FALL_ACCEL;
    if (g_nSpyroAirborneFrames == 0) {
      ApplyGravityAlongSpyroSlopeNormal();
    }
    g_nSpyroLateJumpAllowed = 1;
    g_pfnLevelEmitParticles(5, EMIT_SPYRO_HURT_SPARKLE, 0, 0);
    break;
  case SPYRO_ST_SCRIPTED_HOLD:
    g_nSpyroGravityStep = -SPYRO_LIGHT_FALL_ACCEL;
    /* FALLTHROUGH */
  case SPYRO_ST_FACE_TARGET:
  stand:
    g_nSpyroSubstate = 0;
    g_nSpyroLateJumpAllowed = 1;
    break;
  case SPYRO_ST_SUPERCHARGE:
    CopyVector(g_anSpyroCarryMotionVec,
               SPY_VIA(g_anSpyroCarryMotionVec, SPY_CARRY, SPY_MOTION));
    if (!(g_nSpyroSubstate & SPYRO_SUB_CHARGE_LATCHED)) {
      LATCH_CHARGE_SPEED(g_anSpyroCarryMotionVec);
    }
    SPY[SPY_SUBSTATE] = SPYRO_SUB_CHARGE_LATCHED | SPYRO_SUB_SUPERCHARGE;
    g_nSpyroBodyYawStep = 0;
    g_nSpyroGravityStep = -SPYRO_CHARGE_FALL_ACCEL;
    ApplyGravityAlongSpyroSlopeNormal();
    ApplyGravityAlongSpyroSlopeTangent();
    g_nSpyroLateJumpAllowed = 1;
    g_nSpyroMotionAux = 0;
    PlaySoundEffect(LEVEL_SFX(SFX_SUPERCHARGE), (Actor *)SPY, SFX_MODE_FLAT,
                    (unsigned char *)SPY + SPYB_VOICE_MARKER + 1);
    break;
  }
  {
    int prev = g_nSpyroStateTimer;

    g_nSpyroState = state;
    g_nSpyroStateTimer = 0;
    g_nSpyroPrevStateTimer = prev;
    g_nSpyroAnimPlayRate = g_abSpyroAnimDescTable
        [g_abSpyroStateAnimIndexMap[*(volatile int *)&g_nSpyroState] * 4 + 3];
  }
}

#undef LATCH_CHARGE_SPEED

/* Matching notes (AdvanceSpyroPhysics).
   - Frame: the original's 0x98 frame has an untouched 8-byte reload slot at
     sp+0x68 after the i (0x58) / steps (0x60) spills.  `hole` reproduces it:
     an uninitialised local whose only reference is an empty asm with a "g"
     constraint is live through the whole function, gets no hard register,
     and reload gives it a stack slot (alter_reg, in pseudo order) that the
     asm then names as a memory operand -- no instruction is emitted.
   - loop.c movable threshold (&n lifetime 5 -> 4 via a pre-computed arg),
     a user-var retry preheader (base/b/p are not hoistable out of the outer
     loop), one variable `p` for contacted+nudge (set twice, never a
     movable), a non-sdata WS block so cse keeps `b + 4`, and an array-free
     `*q` suspend RMW (MEM_IN_STRUCT_P decides sched2 aliasing). */

SDATA(g_nSpyroScriptedAlignFrames, 4);
SDATA(g_pSpyroContactActor, 4);

#define REQ g_adwSpyroRequestMaskBlock
#define WS g_anSpyroWallSlideBlock

#define ACTOR_POS(a) (&((Actor *)a)->posX)

/* Pull `pos` back to at most SPYRO_MAX_CORRECTION from `from`. */
#define LIMIT_STEP(pos, from)                                                  \
  {                                                                            \
                                                                               \
    SubtractVector(d, pos, from);                                              \
    len = VectorLength(d, VEC_LEN_XYZ);                                        \
    if (len > SPYRO_MAX_CORRECTION) {                                          \
      ScaleVector3ByRatio(d, len, SPYRO_MAX_CORRECTION);                       \
      AddVector(pos, from, d);                                                 \
    }                                                                          \
  }

/* World-collision response at `pos`: keep the ground probe only for a ledge
   or glide contact facing the motion, slide off steep walls (unless the
   surface is a trigger of kind 6), take the resolved position, latch the
   first contact and limit the correction to SPYRO_MAX_CORRECTION from `from`.
 */
#define SLIDE(from, SET_WALL)                                                  \
  {                                                                            \
    int s = g_nSpyroState;                                                     \
                                                                               \
    if (s == SPYRO_ST_GLIDE || (unsigned)(s - SPYRO_ST_FLY) < 2 ||             \
        s == SPYRO_ST_FLY_DIVE) {                                              \
      SubtractVector(d, g_anCollisionContactPoint, pos);                       \
      SetVector3Magnitude(d, 0x100);                                           \
      if (d[2] >= -0xC8) {                                                     \
        int *v = pos + SPY_VELOCITY_FP6;                                       \
        CopyVector(n, v);                                                      \
        SetVector3Magnitude(n, 0x100);                                         \
        if ((len = (d[0] * n[0] + d[1] * n[1] + d[2] * n[2]) >> 8) < 0xE1) {   \
          goto keep##from;                                                     \
        }                                                                      \
      }                                                                        \
    }                                                                          \
    g_nSpyroGroundProbeSuspendFlag = 0;                                        \
    keep##from : if ((signed char)ArcTan2(                                     \
                         g_anCollisionHitNormal[2],                            \
                         VectorLength(g_anCollisionHitNormal, VEC_LEN_XY),     \
                         ATAN_ANGLE8) >= SLOPE_STEEP) {                        \
      int t = g_nGroundSurfaceType & TRIGGER_REGION_MASK;                      \
      unsigned char kind;                                                      \
                                                                               \
      if (t != TRIGGER_REGION_NONE) {                                          \
        kind = **(unsigned char **)((char *)g_pTriggerRegionTable + t * 4);    \
      } else {                                                                 \
        kind = 0;                                                              \
      }                                                                        \
      if (kind != TRIGGER_PORTAL) {                                            \
        SET_WALL;                                                              \
      }                                                                        \
    }                                                                          \
    CopyVector(pos, g_anCollisionResolvedPos);                                 \
    if (g_nSpyroFirstContactSurfaceType == SURFACE_NO_CONTACT) {               \
      g_nSpyroFirstContactSurfaceType = g_nGroundSurfaceType;                  \
      CopyVector(pos + SPY_FIRST_POINT, g_anCollisionContactPoint);            \
      CopyVector(pos + SPY_FIRST_NORMAL, g_anCollisionHitNormal);              \
    }                                                                          \
    LIMIT_STEP(pos, from);                                                     \
  }

/* Wall-slide contributions: the first pass takes a fresh flag pointer, the
   retry pass a hoisted nudge pointer. */
#define WALL_FIRST                                                             \
  {                                                                            \
    int *w = WS;                                                               \
    *w = one;                                                                  \
    SetVector3Magnitude(g_anCollisionHitNormal, SPYRO_WALL_NUDGE);             \
    w++;                                                                       \
    AddVector(w, w, g_anCollisionHitNormal);                                   \
  }
#define WALL_RETRY                                                             \
  {                                                                            \
    *b = one;                                                                  \
    SetVector3Magnitude(g_anCollisionHitNormal, SPYRO_WALL_NUDGE);             \
    AddVector(p, p, g_anCollisionHitNormal);                                   \
  }

/* Spyro's world collision step (0x8003fe40, 4392 bytes). Applies the pending
   impulse, then the scripted requests (state, velocity toward a target,
   euler, camera modes, the enemy-lock/ledge approach with its alignment test).
   Moves Spyro along the frame velocity in substeps of at most
   SPYRO_COLLIDE_STEP, resolving actor contacts and world collisions (wall-slide
   nudges, the first contact, at most SPYRO_MAX_CORRECTION per try, 6 tries then
   6 more from the substep start) and finally derives the frame's motion vector.
 */
int AdvanceSpyroPhysics(void) {
  int d[3];
  int n[3];
  int start[3];
  int e[3];
  int i;
  int steps;
  int len;
  int hole;

  g_nSpyroContactedActorFlag = 0;
  g_nSpyroFirstContactSurfaceType = SURFACE_NO_CONTACT;
  g_nSpyroWallSlideContributedFlag = 0;
  {
    int *q = &SPY[SPY_PROBE_SUSPEND];

    (*q)++;
  }
  ZeroVector(&SPY[SPY_NUDGE]);
  AddVector(SPY, SPY, &SPY[SPY_IMPULSE]);
  ZeroVector(&SPY[SPY_IMPULSE]);
  CopyVector(&SPY[SPY_PREV_POS], SPY);
  if ((int)g_dwSpyroRequestMask < 0) {
    unsigned int m;
    register unsigned int t asm("$4");
    unsigned int *rq;
    unsigned int *rq2;

    if ((g_dwSpyroRequestMask & SPYRO_RQ_FORCE_STATE) &&
        SPY[SPY_SPARX_HEALTH] >= 0 &&
        SPY[SPY_STATE] != SPY[SPY_REQUEST_STATE]) {
      ChangeSpyroState(SPY[SPY_REQUEST_STATE]);
      SPY[SPY_AIRBORNE] = 1;
    }
    rq = REQ;
    m = *rq;
    if (m & (SPYRO_RQ_VEL_X | SPYRO_RQ_VEL_Y | SPYRO_RQ_VEL_Z)) {
      if (m & SPYRO_RQ_RAW_VELOCITY) {
        if (m & SPYRO_RQ_VEL_X) {
          g_anSpyroVelocityFp6[0] = g_anSpyroRequestVelocity[0] << 6;
          g_anSpyroVelocity[0] = g_anSpyroVelocityFp6[0] / g_nFrameStep;
        }
        if (m & SPYRO_RQ_VEL_Y) {
          g_anSpyroVelocityFp6[1] = g_anSpyroRequestVelocity[1] << 6;
          g_anSpyroVelocity[1] = g_anSpyroVelocityFp6[1] / g_nFrameStep;
        }
        if (m & SPYRO_RQ_VEL_Z) {
          g_anSpyroVelocityFp6[2] = g_anSpyroRequestVelocity[2] << 6;
          g_anSpyroVelocity[2] = g_anSpyroVelocityFp6[2] / g_nFrameStep;
        }
      } else {
        if (m & SPYRO_RQ_VEL_X) {
          g_anSpyroVelocityFp6[0] =
              (g_anSpyroRequestTargetPos[0] - g_anSpyroWorldPos[0]) << 6;
          g_anSpyroVelocity[0] = g_anSpyroVelocityFp6[0] / g_nFrameStep;
        }
        if (m & SPYRO_RQ_VEL_Y) {
          g_anSpyroVelocityFp6[1] =
              (g_anSpyroRequestTargetPos[1] - g_nSpyroWorldPosY) << 6;
          g_anSpyroVelocity[1] = g_anSpyroVelocityFp6[1] / g_nFrameStep;
        }
        if (m & SPYRO_RQ_VEL_Z) {
          g_anSpyroVelocityFp6[2] =
              (g_anSpyroRequestTargetPos[2] - g_nSpyroWorldPosZ) << 6;
          g_anSpyroVelocity[2] = g_anSpyroVelocityFp6[2] / g_nFrameStep;
        }
      }
      g_nSpyroMotionSpeed = VectorLength((int *)(rq - 0x40), VEC_LEN_XY);
    }
    rq2 = REQ;
    t = *rq2;
    if (t & SPYRO_RQ_PITCH) {
      g_abSpyroPersistentEuler[0] = g_abSpyroRequestEuler[0];
      g_nSpyroBodyPitch = g_abSpyroRequestEuler[0] << 4;
    }
    m = t;
    if (m & SPYRO_RQ_ROLL) {
      g_abSpyroPersistentEuler[1] = g_abSpyroRequestEuler[1];
      g_nSpyroBodyRoll = g_abSpyroRequestEuler[1] << 4;
    }
    if (m & SPYRO_RQ_YAW) {
      g_abSpyroPersistentEuler[2] = g_abSpyroRequestEuler[2];
      g_nSpyroBodyYaw = g_abSpyroRequestEuler[2] << 4;
    }
    if (g_nCameraNextMode_blk[0] != (CAM_HELD | CAM_MODE_FREE_LOOK)) {
      if (m & SPYRO_RQ_CAMERA) {
        g_nCameraParamsReferenceYaw = 0;
        g_nCameraNextMode_blk[0] = (CAM_HELD | CAM_MODE_REQUEST);
        g_pCameraAnchorPos = g_pSpyroRequestCameraAnchor;
        g_pCameraTargetParams = g_pSpyroRequestCameraTargetParams;
      } else if (m & SPYRO_RQ_PORTAL_PATH_CAMERA) {
        g_nCameraNextMode_blk[0] = (CAM_HELD | CAM_MODE_PORTAL_PATH);
      } else if (m & SPYRO_RQ_LOCK_ON) {
        int keepYaw = g_nSpyroMoveTargetYaw;
        int a;
        unsigned int lock;

        a = (g_pScriptedCameraActor->rotZ - (ANGLE12_TO_8(g_nCameraEulerYaw))) &
            ANGLE8_MASK;
        if (a > ANGLE8_HALF) {
          a -= ANGLE8_TURN;
        }
        lock = m | SPYRO_RQ_FREEZE_INPUT;
        if (abs(a) < LOCK_ON_ALIGN_ANGLE) {
          if (g_nSpyroScriptedAlignFrames >= LOCK_ON_SETTLE_FRAMES) {
            ComputeSpyroMoveTargetFromPad(g_nSpyroMoveTargetMag);
            if (g_nSpyroSpeedTarget != 0) {
              int side;

              SubtractVector(e, (int *)&rq2[-SPY_REQUEST],
                             ACTOR_POS(g_pScriptedCameraActor));
              a = ((ArcTan2(e[0], e[1], ATAN_ANGLE8) -
                    g_pScriptedCameraActor->rotZ) +
                   0x80) &
                  ANGLE8_MASK;
              if (a > ANGLE8_HALF) {
                a -= ANGLE8_TURN;
              }
              if (g_nActiveLevelId == LEVEL_PEACE_KEEPERS) {
                side = LOCK_ON_CONE_PEACE_KEEPERS;
                if ((int)rq2[SPY_STATE - SPY_REQUEST] ==
                    SPYRO_ST_SCRIPTED_WALK) {
                  side = LOCK_ON_CONE_WALK_PEACE_KEEPERS;
                }
              } else {
                side = LOCK_ON_CONE;
                if ((int)rq2[SPY_STATE - SPY_REQUEST] ==
                    SPYRO_ST_SCRIPTED_WALK) {
                  side = LOCK_ON_CONE_WALK;
                }
              }
              if (abs(a) < side) {
                int h;
                int t;

                SubtractVector(e, ACTOR_POS(SPY[SPY_SCRIPTED_ACTOR]), SPY);
                a = ArcTan2(e[0], e[1], ATAN_ANGLE12);
                h = g_pScriptedCameraActor->rotZ;
                a = (a - (h << 4)) & ANGLE12_MASK;
                if (a < 0x800) {
                  side = 1;
                  t = h + LOCK_ON_ORBIT_TURN;
                } else {
                  side = 0;
                  t = h - LOCK_ON_ORBIT_TURN;
                }
                a = t << 4;
                SCHED_FENCE();
                a = (a - g_nSpyroMoveTargetYaw) & ANGLE12_MASK;
                if (a > ANGLE12_HALF) {
                  a -= ANGLE12_TURN;
                }
                if (abs(a) < LOCK_ON_ORBIT_MATCH) {
                  if (g_nSpyroState != SPYRO_ST_SCRIPTED_WALK) {
                    ChangeSpyroState(SPYRO_ST_SCRIPTED_WALK);
                  }
                  g_dwSpyroRequestMask |= SPYRO_RQ_FORCE_STATE;
                  if (side) {
                    g_nSpyroSubstate = SPYRO_SUB_ORBIT_NEG;
                  } else {
                    g_nSpyroSubstate = SPYRO_SUB_ORBIT_POS;
                  }
                }
              } else {
                g_nSpyroTurnRateAccum = 0;
              }
            } else {
              g_nSpyroTurnRateAccum = 0;
            }
          } else {
            g_nSpyroTurnRateAccum = 0;
            *rq2 = lock;
            if (g_nSpyroInputBlankCountdown < 2) {
              g_nSpyroInputBlankCountdown = 2;
            }
          }
          g_nSpyroScriptedAlignFrames += g_nFrameStep;
        } else {
          g_nSpyroScriptedAlignFrames = 0;
          *rq2 = lock;
          if (g_nSpyroInputBlankCountdown < 2) {
            g_nSpyroInputBlankCountdown = 2;
          }
        }
        g_pCameraAnchorPos = g_anCameraLatchedAnchorPos;
        CopyVector(g_anCameraLatchedAnchorPos,
                   ACTOR_POS(g_pScriptedCameraActor));
        g_anCameraLatchedAnchorPos[2] += LOCK_ON_ANCHOR_LIFT;
        g_nCameraParamsReferenceYaw =
            ANGLE8_TO_12(g_pScriptedCameraActor->rotZ);
        g_pCameraTargetParams = g_anCameraParamEnemyLock;
        g_nCameraNextMode = (CAM_HELD | CAM_MODE_REQUEST);
        g_nSpyroMoveTargetYaw = keepYaw;
        g_dwSpyroRequestMask |= SPYRO_RQ_CAMERA;
      } else if (m & SPYRO_RQ_TARGET_CAMERA) {
        g_nCameraNextMode_blk[0] = (CAM_HELD | CAM_MODE_SCRIPTED_TARGET);
      } else {
        register unsigned int r asm("$2") = t & SPYRO_RQ_FOLLOW_CAMERA;

        if (r) {
          g_nCameraNextMode_blk[0] = (CAM_HELD | CAM_MODE_SCRIPTED_FOLLOW);
        }
      }
    }
    if (g_dwSpyroRequestMask & SPYRO_RQ_SCRIPTED_WALK) {
      g_nSpyroScriptedMoveMode = 1;
      ChangeSpyroState(SPYRO_ST_WALK);
    }
  } else {
    SPY[SPY_REQUEST] = 0;
  }
  RShiftVector3(g_anSpyroVelocityFp6, 6);
  if (g_dwSpyroRequestMask & SPYRO_RQ_NO_CLIP) {
    AddVector(g_anSpyroVelocityFp6 - SPY_VELOCITY_FP6,
              g_anSpyroVelocityFp6 - SPY_VELOCITY_FP6, g_anSpyroVelocityFp6);
  } else {
    len = VectorLength(g_anSpyroVelocityFp6, VEC_LEN_XYZ);
    steps = len / SPYRO_COLLIDE_STEP + 1;
    if (steps >= 2) {
      g_anSpyroVelocityFp6[0] /= steps;
      g_anSpyroVelocityFp6[1] /= steps;
      g_anSpyroVelocityFp6[2] /= steps;
    }
    i = 0;
    if (steps > 0) {
      register int *fp6 asm("$16") = g_anSpyroVelocityFp6;
      int *from = fp6 + 3;
      int *pos = fp6 - SPY_VELOCITY_FP6;
      register int *st asm("$21") = fp6 - (SPY_VELOCITY_FP6 - SPY_STATE);
      register int one asm("$20") = 1;

      do {
        int tries;
        int *p;

        CopyVector(from, pos);
        CopyVector(start, pos);
        AddVector(pos, pos, from - 3);
        tries = 0;
        p = pos + SPY_CONTACTED;
        do {
          int mask = 0;
          void *hit;

          switch (*st) {
          case SPYRO_ST_CHARGE_AIR:
            if (st[1] & SPYRO_SUB_SUPERCHARGE) {
              hit = DispatchActorContactAtSphere(
                  pos, SPYRO_RADIUS, &g_dwSpyroTriggerEventFlags,
                  ACTOR_HIT_SUPER, 0, CONTACT_STOP_AT_SOLID);
              if (hit == 0) {
                goto collide;
              }
              if (st[SPY_TRIGGER_FLAGS - SPY_STATE] & SPYRO_EV_BLOCKS_SUPER) {
                hit = DispatchActorContactAtSphere(pos, SPYRO_RADIUS,
                                                   &g_dwSpyroTriggerEventFlags,
                                                   ACTOR_HIT_SUPER, 0, one);
              }
              break;
            }
            /* FALLTHROUGH */
          case SPYRO_ST_CHARGE:
            hit = DispatchActorContactAtSphere(
                st - SPY_STATE, SPYRO_RADIUS,
                (unsigned int *)(st - (SPY_STATE - SPY_TRIGGER_FLAGS)),
                ACTOR_HIT_CHARGE, 0, one);
            if (hit == 0) {
              goto collide;
            }
            if (g_nPulseRumbleAmount < CHARGE_HIT_RUMBLE_AMOUNT) {
              g_nPulseRumbleAmount = CHARGE_HIT_RUMBLE_AMOUNT;
            }
            if (g_nPulseRumbleTimer < CHARGE_HIT_RUMBLE_FRAMES) {
              g_nPulseRumbleTimer = CHARGE_HIT_RUMBLE_FRAMES;
            }
            break;
          case SPYRO_ST_SUPERCHARGE_AIR:
          case SPYRO_ST_SUPERCHARGE:
            mask |= ACTOR_HIT_SUPER;
            hit = DispatchActorContactAtSphere(pos, SPYRO_RADIUS,
                                               &g_dwSpyroTriggerEventFlags,
                                               mask, 0, CONTACT_STOP_AT_SOLID);
            if (hit == 0) {
              goto collide;
            }
            if (!(g_dwSpyroTriggerEventFlags & SPYRO_EV_BLOCKS_SUPER)) {
              break;
            }
            /* FALLTHROUGH */
          default:
            hit = DispatchActorContactAtSphere(
                pos, SPYRO_RADIUS, &g_dwSpyroTriggerEventFlags, mask, 0, one);
            break;
          }
        contact:
          if (hit != 0) {
            g_pSpyroContactActor = hit;
            *p = one;
            LIMIT_STEP(pos, from);
          }
        collide:
          if (!CollideSphereWithWorldAndActors(pos, SPYRO_RADIUS,
                                               SPYRO_RADIUS)) {
            if (hit == 0) {
              break;
            }
          } else {
            SLIDE(from, WALL_FIRST);
          }
          tries++;
        } while (tries < 6);
        if (tries == 6) {
          int *b;
          int *base;

          tries = 0;
          b = WS;
          p = b + 1;
          base = start;
          do {
            if (!CollideSphereWithWorldAndActors(pos, SPYRO_RADIUS,
                                                 SPYRO_RADIUS)) {
              break;
            }
            SLIDE(base, WALL_RETRY);
            tries++;
          } while (tries < 6);
        }
      } while (++i < steps);
    }
  }
  SubtractVector(g_anSpyroMotionVec, g_anSpyroMotionVec - SPY_MOTION,
                 g_anSpyroMotionVec - (SPY_MOTION - SPY_PREV_POS));
  LShiftVector3(g_anSpyroMotionVec, 6);
  g_anSpyroMotionVec[0] /= g_nFrameStep;
  g_anSpyroMotionVec[1] /= g_nFrameStep;
  g_anSpyroMotionVec[2] /= g_nFrameStep;
  g_nSpyroFrameMotionLength = VectorLength(g_anSpyroMotionVec, VEC_LEN_XYZ);
  __asm__ volatile("" : : "g"(hole));
  return 1;
}

#undef REQ
#undef WS
#undef ACTOR_POS
#undef LIMIT_STEP
#undef SLIDE
#undef WALL_FIRST
#undef WALL_RETRY

SDATA(g_bSpyroJumpHoldForced, 4);

/* Opaque copy: the result is a fresh value to cse, so a block address stays
   in its register and later fields are addressed off it. */
static inline int *Opaque(int *p) {
  int *q;

  OPAQUE_COPY(q, p);
  return q;
}

static inline int OpaqueInt(int p) {
  int q;

  OPAQUE_COPY(q, p);
  return q;
}

/* Landing: pick the ground state from the speed target and current speed. */
#define LAND_ON_GROUND()                                                       \
  if (g_nSpyroSpeedTarget > 0) {                                               \
    if (g_nSpyroMotionSpeed > SPYRO_RUN_SPEED) {                               \
      ChangeSpyroState(SPYRO_ST_RUN);                                          \
    } else if (g_nSpyroMotionSpeed > SPYRO_JOG_SPEED) {                        \
      ChangeSpyroState(SPYRO_ST_JOG);                                          \
    } else {                                                                   \
      ChangeSpyroState(SPYRO_ST_WALK);                                         \
    }                                                                          \
  } else if (g_nSpyroMotionSpeed >= SPYRO_JOG_SPEED) {                         \
    ChangeSpyroState(SPYRO_ST_STOP);                                           \
  } else {                                                                     \
    ResetSpyroLinearMotion();                                                  \
    ChangeSpyroState(SPYRO_ST_IDLE);                                           \
  }

/* Jump: glide-jump in flight levels and Gnasty's Loot, else the plain jump. */
#define JUMP_STATE()                                                           \
  if (g_nFlightLevelActive != 0 || g_nActiveLevelId == LEVEL_GNASTYS_LOOT) {   \
    ChangeSpyroState(SPYRO_ST_FLY);                                            \
  } else {                                                                     \
    ChangeSpyroState(SPYRO_ST_GLIDE);                                          \
  }

/* Ring of four dust puffs around Spyro's facing. */
#define LANDING_DUST(v)                                                        \
  {                                                                            \
    int i;                                                                     \
    int a;                                                                     \
                                                                               \
    a = g_abSpyroPersistentEuler[2];                                           \
    v[2] = 0;                                                                  \
    for (i = 0; i < 4; i++) {                                                  \
      v[0] = (short)(unsigned short)g_anCosineLut[a] >> 7;                     \
      v[1] = (short)((unsigned short *)g_anSineLut)[a] >> 7;                   \
      g_pfnLevelEmitParticles(1, EMIT_DUST_PUFF, v, 0);                        \
      a = (a + 0x40) & 0xFF;                                                   \
    }                                                                          \
  }

/* Per-frame Spyro state update (0x80041670, 10612 bytes). Each state first
   lets the pad dispatcher take a transition, then runs its own decision tree
   and hands the chosen next state to ChangeSpyroState. */
void UpdateSpyroStateBehavior(void) {
  switch (g_nSpyroState) {
  case SPYRO_ST_IDLE:
    if (ApplySpyroTriggerEvents(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_dwPadPressed & PAD_CROSS) {
      ChangeSpyroState(SPYRO_ST_JUMP);
      break;
    }
    if (g_nSpyroAirborneFrames >= SPYRO_AIRBORNE_GRACE ||
        g_nSpyroGroundSlope >= SLOPE_STEEP) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (g_dwPadHeld & PAD_SQUARE) {
      ChangeSpyroState(SPYRO_ST_CHARGE);
      break;
    }
    if (g_dwPadPressed & PAD_L1) {
      ChangeSpyroState(SPYRO_ST_SIDESTEP_L1);
      break;
    }
    if (g_dwPadPressed & PAD_R1) {
      ChangeSpyroState(SPYRO_ST_SIDESTEP_R1);
      break;
    }
    if (g_nSpyroFrameMotionLength > SPYRO_SHOVED_MOTION) {
      ChangeSpyroState(SPYRO_ST_STOP);
      g_nSpyroSubstate = SPYRO_SUB_STOP_FROM_SPEED;
      break;
    }
    if (g_nCameraNextMode == (CAM_HELD | CAM_MODE_FREE_LOOK)) {
      break;
    }
    if (g_nPadDirectionalIdleFlag == 0) {
      ChangeSpyroState(SPYRO_ST_WALK);
      break;
    }
    if (g_nSpyroInputBlankCountdown != 0) {
      break;
    }
    if (g_nSpyroCliffEdgeHint != 0) {
      ChangeSpyroState(SPYRO_ST_TEETER);
      break;
    }
    if (g_nSpyroStateTimer >= g_nSpyroIdleAnimTimeout &&
        g_nSpyroFlameState != SPYRO_FLAME_BREATHING) {
      CycleSpyroIdleAnim();
    }
    break;
  case SPYRO_ST_WALK:
    if (ApplySpyroTriggerEvents(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_dwPadPressed & PAD_CROSS) {
      ChangeSpyroState(SPYRO_ST_JUMP);
      break;
    }
    if (g_nSpyroAirborneFrames >= SPYRO_AIRBORNE_GRACE ||
        g_nSpyroGroundSlope >= SLOPE_STEEP) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (g_dwPadHeld & PAD_SQUARE) {
      ChangeSpyroState(SPYRO_ST_CHARGE);
      break;
    }
    if (g_dwPadPressed & PAD_L1) {
      ChangeSpyroState(SPYRO_ST_SIDESTEP_L1);
      break;
    }
    if (g_dwPadPressed & PAD_R1) {
      ChangeSpyroState(SPYRO_ST_SIDESTEP_R1);
      break;
    }
    if (g_nSpyroMotionSpeed == 0 && g_nSpyroSpeedTarget == 0 &&
        g_nSpyroStateTimer >= SPYRO_WALK_MIN_TICKS) {
      ChangeSpyroState(SPYRO_ST_IDLE);
      break;
    }
    if (g_nSpyroMotionSpeed > SPYRO_RUN_SPEED) {
      ChangeSpyroState(SPYRO_ST_RUN);
    } else if (g_nSpyroMotionSpeed > SPYRO_JOG_SPEED) {
      ChangeSpyroState(SPYRO_ST_JOG);
    }
    break;
  case SPYRO_ST_RUN:
    if (ApplySpyroTriggerEvents(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_dwPadPressed & PAD_CROSS) {
      ChangeSpyroState(SPYRO_ST_JUMP);
      break;
    }
    if (g_nSpyroAirborneFrames >= SPYRO_AIRBORNE_GRACE ||
        g_nSpyroGroundSlope >= SLOPE_STEEP) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (g_dwPadHeld & PAD_SQUARE) {
      ChangeSpyroState(SPYRO_ST_CHARGE);
      break;
    }
    if (g_dwPadPressed & PAD_L1) {
      ChangeSpyroState(SPYRO_ST_SIDESTEP_L1);
      break;
    }
    if (g_dwPadPressed & PAD_R1) {
      ChangeSpyroState(SPYRO_ST_SIDESTEP_R1);
      break;
    }
    if (g_nSpyroSpeedTarget == 0) {
      ChangeSpyroState(SPYRO_ST_STOP);
      break;
    }
    if (AbsAngleDelta12(g_nSpyroMoveTargetYaw, g_nSpyroBodyYaw) >
        SPYRO_REVERSE_ANGLE) {
      register int a0 asm("$4");

      REG_RESERVE(a0);
      if (g_nPadStickActiveFlag != 0) {
        int d;

        KEEP_IN_REG(a0);
        d = ((unsigned char *)g_pPadSubstepState)[PAD_LY_OFFSET] -
            PAD_STICK_CENTER;

        if (d < 0) {
          d = -d;
        }
        if (d > SPYRO_TURN_AROUND_STICK) {
          ChangeSpyroState(SPYRO_ST_TURN_AROUND);
        } else {
          ChangeSpyroState(SPYRO_ST_STOP);
        }
      } else {
        ChangeSpyroState(SPYRO_ST_TURN_AROUND);
      }
      break;
    }
    if (g_nSpyroSpeedTarget == 0) {
      ChangeSpyroState(SPYRO_ST_IDLE);
      break;
    }
    if (g_nSpyroMotionSpeed < SPYRO_WALK_DROP_SPEED) {
      ChangeSpyroState(SPYRO_ST_WALK);
      break;
    }
    if (g_nSpyroMotionSpeed < SPYRO_RUN_DROP_SPEED) {
      ChangeSpyroState(SPYRO_ST_JOG);
    }
    break;
  case SPYRO_ST_STOP:
    if (ApplySpyroTriggerEvents(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_dwPadPressed & PAD_CROSS) {
      ChangeSpyroState(SPYRO_ST_JUMP);
      break;
    }
    if (g_nSpyroAirborneFrames >= SPYRO_AIRBORNE_GRACE ||
        g_nSpyroGroundSlope >= SLOPE_STEEP) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (g_dwPadHeld & PAD_SQUARE) {
      ChangeSpyroState(SPYRO_ST_CHARGE);
      break;
    }
    if (g_dwPadPressed & PAD_L1) {
      ChangeSpyroState(SPYRO_ST_SIDESTEP_L1);
      break;
    }
    if (g_dwPadPressed & PAD_R1) {
      ChangeSpyroState(SPYRO_ST_SIDESTEP_R1);
      break;
    }
    if (g_nSpyroMotionSpeed == 0 && g_nSpyroSpeedTarget == 0 &&
        g_nSpyroFrameMotionLength < SPYRO_STOP_SETTLE_MOTION) {
      ChangeSpyroState(SPYRO_ST_IDLE);
      break;
    }
    if (g_nSpyroSpeedTarget > 0 && g_nSpyroStateTimer < SPYRO_STOP_TURN_TICKS &&
        AbsAngleDelta12(g_nSpyroMoveTargetYaw, g_nSpyroBodyYaw) >
            SPYRO_REVERSE_ANGLE) {
      ChangeSpyroState(SPYRO_ST_TURN_AROUND);
      break;
    }
    if (g_nSpyroMotionSpeed > SPYRO_RUN_SPEED && g_nSpyroSpeedTarget > 0) {
      ChangeSpyroState(SPYRO_ST_RUN);
      break;
    }
    if (g_nSpyroMotionSpeed > SPYRO_JOG_SPEED && g_nSpyroSpeedTarget > 0) {
      ChangeSpyroState(SPYRO_ST_JOG);
      break;
    }
    if (g_nSpyroMotionSpeed > 0 && g_nSpyroSpeedTarget > 0) {
      ChangeSpyroState(SPYRO_ST_WALK);
    }
    break;
  case SPYRO_ST_TURN_AROUND:
    if (ApplySpyroTriggerEvents(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_nSpyroAirborneFrames >= SPYRO_AIRBORNE_GRACE ||
        g_nSpyroGroundSlope >= SLOPE_STEEP) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (g_nSpyroStateTimer < SPYRO_TURN_AROUND_TICKS) {
      break;
    }
    if (g_nPadDirectionalIdleFlag != 0) {
      ChangeSpyroState(SPYRO_ST_IDLE);
    } else {
      g_nSpyroMotionSpeed = SPYRO_TURN_AROUND_EXIT_SPEED;
      ChangeSpyroState(SPYRO_ST_WALK);
    }
    break;
  case SPYRO_ST_JUMP:
    if (ApplySpyroTriggerEvents(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_nSpyroGlideSpentFlag == 0 && (g_dwPadPressed & PAD_CROSS)) {
      g_nSpyroGlideQueued = 1;
    }
    if (g_anSpyroMotionVec[2] < 0) {
      g_nSpyroSubstate = SPYRO_SUB_JUMP_FALLING;
    }
    if (g_nSpyroSubstate == SPYRO_SUB_JUMP_RISING &&
        !(g_dwPadHeld & PAD_CROSS) && g_bSpyroJumpHoldForced == 0) {
      g_nSpyroSubstate = SPYRO_SUB_JUMP_RELEASED;
    }
    if (g_nSpyroAirborneFrames == 0 &&
        g_nSpyroStateTimer >= SPYRO_JUMP_LAND_TICKS) {
      if (g_nSpyroGroundSlope >= SLOPE_STEEP) {
        ChangeSpyroState(SPYRO_ST_SLIDE);
        break;
      }
      if (g_anSpyroGroundNormal[0] * g_anSpyroVelocity[0] +
              g_anSpyroGroundNormal[1] * g_anSpyroVelocity[1] +
              g_anSpyroGroundNormal[2] * g_anSpyroVelocity[2] <
          0) {
        LAND_ON_GROUND();
      }
      break;
    }
    if (g_dwPadHeld & PAD_SQUARE) {
      ChangeSpyroState(SPYRO_ST_CHARGE_AIR);
      g_nSpyroMotionSpeed = SPYRO_CHARGE_SPEED;
      break;
    }
    if (g_nSpyroGlideSpentFlag == 0 &&
        g_nSpyroSubstate == SPYRO_SUB_JUMP_FALLING &&
        g_nSpyroGlideQueued != 0) {
      JUMP_STATE();
      break;
    }
    if (g_anSpyroMotionVec[2] < -SPYRO_JUMP_FALL_SPEED ||
        g_nSpyroStateTimer > SPYRO_JUMP_FALL_TICKS) {
      ChangeSpyroState(SPYRO_ST_FALL);
    }
    break;
  case SPYRO_ST_FALL:
    if (ApplySpyroTriggerEvents(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_dwPadPressed & PAD_CROSS) {
      if (g_nSpyroLateJumpAllowed != 0 &&
          g_nSpyroStateTimer < SPYRO_LATE_JUMP_TICKS) {
        ChangeSpyroState(SPYRO_ST_JUMP);
        break;
      }
      if ((g_dwPadPressed & PAD_CROSS) && g_nSpyroSubstate == 0) {
        if (g_nFlightLevelActive != 0 ||
            g_nActiveLevelId == LEVEL_GNASTYS_LOOT) {
          if (g_nSpyroGlideSpentFlag == 0 ||
              g_nSpyroStateTimer >= SPYRO_REGLIDE_TICKS) {
            ChangeSpyroState(SPYRO_ST_FLY);
          }
          break;
        }
        if (g_nSpyroGlideSpentFlag == 0 ||
            g_nSpyroStateTimer >= SPYRO_REGLIDE_FALL_TICKS) {
          ChangeSpyroState(SPYRO_ST_GLIDE);
        }
        break;
      }
    }
    if (g_nSpyroWorldPosZ >= g_anLevelDeathPlaneZTable[g_nLevelIndex] &&
        g_nSpyroAirborneFrames == 0) {
      int next;
      int fwd[3];
      int back[3];

      if (g_nSpyroGroundSlope >= SLOPE_STEEP) {
        if (g_nSpyroStateTimer >= 9) {
          ChangeSpyroState(SPYRO_ST_SLIDE);
        }
        break;
      }
      if (g_nSpyroSpeedTarget > 0) {
        if (g_nSpyroMotionSpeed > SPYRO_RUN_SPEED) {
          next = SPYRO_ST_RUN;
        } else {
          next = SPYRO_ST_WALK;
          if (g_nSpyroMotionSpeed > SPYRO_JOG_SPEED) {
            next = SPYRO_ST_JOG;
          }
        }
        ChangeSpyroState(next);
        LANDING_DUST(fwd);
      } else {
        next = SPYRO_ST_STOP;
        if (g_nSpyroMotionSpeed < SPYRO_JOG_SPEED) {
          ResetSpyroLinearMotion();
          next = SPYRO_ST_IDLE;
        }
        ChangeSpyroState(next);
        LANDING_DUST(back);
      }
      break;
    }
  fall_timeout:
    if (g_nSpyroStateTimer > SPYRO_STUCK_RESPAWN_TICKS) {
      if (g_nFlightLevelActive != 0) {
        g_pfnLevelEndFlightRun();
        g_pfnLevelUpdateActors();
      } else {
        g_nExtraLives++;
        TriggerRespawnOrGameOver();
      }
    }
    break;
  case SPYRO_ST_FACE_TARGET:
    if (ApplySpyroTriggerEvents(SPYRO_EV_MASK_NO_HAZARD)) {
      if (g_nSpyroStateTimer >= SPYRO_FACE_TARGET_SNAP_TICKS) {
        g_nSpyroBodyYaw = g_nSpyroMoveTargetYaw;
        g_nSpyroBodyPitch = -g_nSpyroBodyPitch & ANGLE12_MASK;
        g_nSpyroBodyRoll = -g_nSpyroBodyRoll & ANGLE12_MASK;
      }
      break;
    }
    if (g_nSpyroStateTimer < SPYRO_FACE_TARGET_TICKS) {
      break;
    }
    g_nSpyroBodyYaw = g_nSpyroMoveTargetYaw;
    g_nSpyroBodyPitch = -g_nSpyroBodyPitch & ANGLE12_MASK;
    g_nSpyroBodyRoll = -g_nSpyroBodyRoll & ANGLE12_MASK;
    if (g_nPadDirectionalIdleFlag != 0) {
      ChangeSpyroState(SPYRO_ST_IDLE);
    } else {
      ChangeSpyroState(SPYRO_ST_WALK);
      g_nSpyroMotionSpeed = SPYRO_FACE_TARGET_EXIT_SPEED;
    }
    break;
  case SPYRO_ST_SIDESTEP_L1:
    if (ApplySpyroTriggerEvents(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (SPY[SPY_AIRBORNE] >= SPYRO_AIRBORNE_GRACE ||
        g_nSpyroGroundSlope >= SLOPE_STEEP) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (g_dwPadHeld & PAD_L1) {
      break;
    }
    if (g_nSpyroStateTimer < SPYRO_SIDESTEP_MIN_TICKS) {
      break;
    }
    CopyVector(&SPY[SPY_VELOCITY], &SPY[SPY_CARRY]);
    g_nSpyroMotionSpeed = 0;
    ChangeSpyroState(SPYRO_ST_IDLE);
    break;
  case SPYRO_ST_SIDESTEP_R1:
    if (ApplySpyroTriggerEvents(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (SPY[SPY_AIRBORNE] >= SPYRO_AIRBORNE_GRACE ||
        g_nSpyroGroundSlope >= SLOPE_STEEP) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (g_dwPadHeld & PAD_R1) {
      break;
    }
    if (g_nSpyroStateTimer < SPYRO_SIDESTEP_MIN_TICKS) {
      break;
    }
    CopyVector(&SPY[SPY_VELOCITY], &SPY[SPY_CARRY]);
    g_nSpyroMotionSpeed = 0;
    ChangeSpyroState(SPYRO_ST_IDLE);
    break;
  case SPYRO_ST_CHARGE:
    if (DispatchSpyroChargeStateChanges()) {
      break;
    }
    if ((g_dwPadHeld & (PAD_CROSS | PAD_SQUARE)) == (PAD_CROSS | PAD_SQUARE) &&
        g_nSpyroGroundSlope < SLOPE_FLAT) {
      ChangeSpyroState(SPYRO_ST_CHARGE_AIR);
      g_nSpyroVelocityZ += SPYRO_JUMP_SPEED;
      g_nSpyroAirborneFrames++;
      break;
    }
    if (g_nSpyroAirborneFrames >= SPYRO_AIRBORNE_GRACE) {
      ChangeSpyroState(SPYRO_ST_CHARGE_AIR);
      break;
    }
    if (g_nSpyroGroundSlope >= SLOPE_STEEP) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (!(g_dwPadHeld & PAD_SQUARE)) {
      ChangeSpyroState(SPYRO_ST_RUN);
      g_nSpyroStateTimer = g_nSpyroPrevStateTimer * 2;
      break;
    }
    if (g_nSpyroMotionSpeed > SPYRO_RUN_DROP_SPEED &&
        g_nSpyroFrameMotionLength * 2 < g_nSpyroMotionSpeed &&
        g_nPortalTransitionActive == 0) {
      ChangeSpyroState(SPYRO_ST_CHARGE_BONK);
      break;
    }
    if (SPY[SPY_CHARGE_HOLD] != 0) {
      if (g_nSpyroMotionSpeed > SPYRO_SUPERCHARGE_SPEED) {
        ChangeSpyroState(SPYRO_ST_SUPERCHARGE);
        break;
      }
      if (!(g_nSpyroSubstate & SPYRO_SUB_CHARGE_LATCHED)) {
        int v;

        g_nSpyroFloorReferenceZ = g_nSpyroWorldPosZ;
        v = VectorLength(&SPY[SPY_CARRY], VEC_LEN_XYZ);
        if (v < SPYRO_CHARGE_SPEED) {
          v = SPYRO_CHARGE_SPEED;
        }
        g_nSpyroSpeedTarget = v;
        g_nSpyroSubstate |= SPYRO_SUB_CHARGE_LATCHED;
      }
    } else {
      g_nSpyroSubstate &= ~SPYRO_SUB_CHARGE_LATCHED;
    }
    break;
  case SPYRO_ST_CHARGE_BONK:
    if (ApplySpyroTriggerEvents(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_nSpyroStateTimer > SPYRO_BONK_FALL_TICKS &&
        (g_nSpyroAirborneFrames != 0 || g_nSpyroGroundSlope >= SLOPE_STEEP)) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (g_nSpyroStateTimer >= SPYRO_BONK_TICKS) {
      ChangeSpyroState(SPYRO_ST_IDLE);
    }
    break;
  case SPYRO_ST_TEETER:
    if (ApplySpyroTriggerEvents(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_dwPadPressed & PAD_CROSS) {
      ChangeSpyroState(SPYRO_ST_JUMP);
      break;
    }
    if (g_nSpyroAirborneFrames >= SPYRO_AIRBORNE_GRACE ||
        g_nSpyroGroundSlope >= SLOPE_STEEP) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (g_dwPadHeld & PAD_SQUARE) {
      ChangeSpyroState(SPYRO_ST_CHARGE);
      break;
    }
    if (g_dwPadPressed & PAD_L1) {
      ChangeSpyroState(SPYRO_ST_SIDESTEP_L1);
      break;
    }
    if (g_dwPadPressed & PAD_R1) {
      ChangeSpyroState(SPYRO_ST_SIDESTEP_R1);
      break;
    }
    if (g_nCameraNextMode == (CAM_HELD | CAM_MODE_FREE_LOOK)) {
      ChangeSpyroState(SPYRO_ST_IDLE);
      break;
    }
    if (g_nPadDirectionalIdleFlag == 0) {
      ChangeSpyroState(SPYRO_ST_WALK);
      break;
    }
    if (g_nSpyroInputBlankCountdown != 0) {
      ChangeSpyroState(SPYRO_ST_IDLE);
      break;
    }
    if (g_nSpyroStateTimer < SPYRO_TEETER_MIN_TICKS) {
      break;
    }
    if (g_nSpyroCliffEdgeHint == 0) {
      ChangeSpyroState(SPYRO_ST_IDLE);
    }
    break;
  case SPYRO_ST_HURT_TINT:
    if (g_dwSpyroTriggerEventFlags & SPYRO_EV_HIT_TINT) {
      break;
    }
    if (g_nSpyroStateTimer < SPYRO_HURT_TICKS) {
      break;
    }
    if (g_nSparxHealth < SPARX_GONE) {
      ChangeSpyroState(SPYRO_ST_DEATH);
      break;
    }
    goto land;
  case SPYRO_ST_HURT:
  case SPYRO_ST_KNOCKBACK:
  case SPYRO_ST_KNOCKBACK_2:
    if (g_nSpyroStateTimer < SPYRO_HURT_TICKS) {
      break;
    }
    if (g_nSparxHealth < SPARX_GONE) {
      ChangeSpyroState(SPYRO_ST_DEATH);
      break;
    }
    goto land;
  case SPYRO_ST_HURT_LONG:
    if (g_nSpyroStateTimer < SPYRO_HURT_LONG_TICKS) {
      break;
    }
    if (g_nSparxHealth < SPARX_GONE) {
      ChangeSpyroState(SPYRO_ST_DEATH);
      break;
    }
    goto land;
  case SPYRO_ST_GLIDE:
    if (ApplySpyroTriggerEvents(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_nSpyroAirborneFrames == 0) {
      if (g_nSpyroGroundSlope >= SLOPE_STEEP) {
        ChangeSpyroState(SPYRO_ST_FALL);
      } else {
        LAND_ON_GROUND();
      }
      break;
    }
    if (g_nSpyroGroundProbeSuspendFlag == 0 ||
        g_nSpyroFrameMotionLength < SPYRO_GLIDE_STALL_MOTION) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (g_dwPadHeld & PAD_SQUARE) {
      ChangeSpyroState(SPYRO_ST_CHARGE_AIR);
      break;
    }
    if (g_nSpyroStateTimer > SPYRO_GLIDE_DROP_DELAY &&
        (g_dwPadPressed & PAD_TRIANGLE)) {
      ChangeSpyroState(SPYRO_ST_GLIDE_DROP);
      break;
    }
    if (g_nSpyroFlightMode != 0) {
      int d;
      int pad;

      if (g_nSpyroSubstate != 0) {
        break;
      }
      d = g_nSpyroWorldPosZ;
      if (d - g_nSpyroFloorReferenceZ > SPYRO_FLY_CEILING) {
        ChangeSpyroState(SPYRO_ST_FALL);
        break;
      }
      pad = g_pPadSubstepState->held;
      if (pad & PAD_R1) {
        g_nSpyroSubstate = SPYRO_SUB_GLIDE_R1;
      } else if (pad & PAD_L1) {
        g_nSpyroSubstate = SPYRO_SUB_GLIDE_L1;
        g_nSpyroMotionAux = d;
      }
      break;
    }
    if (g_nSpyroSubstate == 0 &&
        g_nSpyroFloorReferenceZ + SPYRO_GLIDE_RISE_MAX < g_nSpyroWorldPosZ) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (g_nSpyroMotionSpeed < SPYRO_RUN_DROP_SPEED &&
        g_nSpyroStateTimer > SPYRO_GLIDE_SLOW_TICKS) {
      ChangeSpyroState(SPYRO_ST_GLIDE_SLOW);
    }
    break;
  case SPYRO_ST_LIFT:
    ApplySpyroTriggerEvents(SPYRO_EV_MASK_DEFAULT);
    break;
  case SPYRO_ST_FIDGET_0:
  case SPYRO_ST_FIDGET_1:
  case SPYRO_ST_FIDGET_2:
  case SPYRO_ST_FIDGET_3:
  case SPYRO_ST_FIDGET_4:
  case SPYRO_ST_FIDGET_5:
  case SPYRO_ST_FIDGET_6:
  case SPYRO_ST_FIDGET_7:
  case SPYRO_ST_FIDGET_8: {
    int *air;

    if (ApplySpyroTriggerEvents(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_dwPadPressed & PAD_CROSS) {
      ChangeSpyroState(SPYRO_ST_JUMP);
      break;
    }
    air = Opaque(&SPY[SPY_AIRBORNE]);
    if (*air >= SPYRO_AIRBORNE_GRACE || g_nSpyroGroundSlope >= SLOPE_STEEP) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (g_dwPadHeld & PAD_SQUARE) {
      ChangeSpyroState(SPYRO_ST_CHARGE);
      break;
    }
    if (g_dwPadPressed & PAD_L1) {
      ChangeSpyroState(SPYRO_ST_SIDESTEP_L1);
      break;
    }
    if (g_dwPadPressed & PAD_R1) {
      ChangeSpyroState(SPYRO_ST_SIDESTEP_R1);
      break;
    }
    if (g_nSpyroFrameMotionLength > SPYRO_SHOVED_MOTION) {
      ChangeSpyroState(SPYRO_ST_STOP);
      break;
    }
    if (g_nCameraNextMode != (CAM_HELD | CAM_MODE_FREE_LOOK)) {
      if (g_nPadDirectionalIdleFlag == 0) {
        ChangeSpyroState(SPYRO_ST_WALK);
        break;
      }
      if (g_nSpyroCliffEdgeHint != 0) {
        RestartSpyroAnimWithState(SPYRO_ST_TEETER);
        break;
      }
      if (g_nSpyroInputBlankCountdown == 0) {
        unsigned char *b = (unsigned char *)air;

        if (b[SPYB_ANIM_CURRENT - 4 * SPY_AIRBORNE] ==
                g_abSpyroStateAnimIndexMap[g_nSpyroState] &&
            b[SPYB_ANIM_FRAME - 4 * SPY_AIRBORNE] >=
                SPYRO_ANIM_DESC(b[SPYB_ANIM_CURRENT - 4 * SPY_AIRBORNE],
                                SPYRO_ANIM_DESC_LOOP_END) -
                    1) {
          ChangeSpyroState(SPYRO_ST_IDLE);
          break;
        }
        if (g_nSpyroFlameState != SPYRO_FLAME_BREATHING) {
          break;
        }
      }
    }
    RestartSpyroAnimWithState(SPYRO_ST_IDLE);
    break;
  }
  case SPYRO_ST_GLIDE_DROP:
    if (ApplySpyroTriggerEvents(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_nSpyroWorldPosZ >= g_anLevelDeathPlaneZTable[g_nLevelIndex] &&
        g_nSpyroAirborneFrames == 0) {
      if (g_nSpyroStateTimer >= SPYRO_GLIDE_DROP_FALL_TICKS ||
          g_nSpyroGroundSlope >= SLOPE_STEEP) {
        ChangeSpyroState(SPYRO_ST_FALL);
        g_nSpyroStateTimer = g_nSpyroPrevStateTimer;
      } else {
        ChangeSpyroState(SPYRO_ST_IDLE);
      }
      if (SPY[SPY_INPUT_BLANK] < SPYRO_DROP_LAND_BLANK) {
        SPY[SPY_INPUT_BLANK] = SPYRO_DROP_LAND_BLANK;
      }
      ResetSpyroLinearMotion();
    }
    if (g_dwPadPressed & PAD_CROSS) {
      if (g_nSpyroGlideSpentFlag == 0 ||
          g_nSpyroStateTimer >= SPYRO_REGLIDE_TICKS) {
        ChangeSpyroState(SPYRO_ST_GLIDE);
      }
      break;
    }
    goto fall_timeout;
  case SPYRO_ST_SLIDE:
    if (ApplySpyroTriggerEvents(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if ((g_dwPadHeld & PAD_CROSS) &&
        g_nSpyroStateTimer < SPYRO_LATE_JUMP_TICKS) {
      ChangeSpyroState(SPYRO_ST_JUMP);
      break;
    }
    if ((g_dwPadHeld & PAD_SQUARE) &&
        g_nSpyroMotionSpeed > SPYRO_SLIDE_CHARGE_SPEED) {
      ChangeSpyroState(SPYRO_ST_CHARGE_AIR);
      break;
    }
    if (g_nSpyroAirborneFrames >= SPYRO_SLIDE_AIRBORNE_GRACE) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (g_nSpyroGroundSlope < SLOPE_STEEP &&
        g_bSpyroAnimFrame ==
            SPYRO_ANIM_DESC(SPYRO_ANIMID_SLIDE, SPYRO_ANIM_DESC_LOOP_END) - 1 &&
        g_nSpyroFrameMotionLength < SPYRO_SLIDE_LAND_MOTION) {
      LAND_ON_GROUND();
      break;
    }
    if (g_nSpyroStateTimer > SPYRO_STUCK_RESPAWN_TICKS) {
      if (g_nFlightLevelActive != 0) {
        g_pfnLevelEndFlightRun();
        g_pfnLevelUpdateActors();
      } else {
        g_nExtraLives++;
        TriggerRespawnOrGameOver();
      }
    }
    break;
  case SPYRO_ST_CHARGE_AIR:
    if (g_nSpyroGlideSpentFlag == 0 && (g_dwPadPressed & PAD_CROSS)) {
      g_nSpyroGlideQueued = 1;
    }
    if (!(g_nSpyroSubstate & SPYRO_SUB_SUPERCHARGE) &&
        DispatchSpyroChargeStateChanges()) {
      break;
    }
    if (g_nSpyroGroundSlope >= SLOPE_STEEP && g_nPortalTransitionActive == 0 &&
        g_anSpyroVelocity[0] * g_anSpyroGroundNormal[0] +
                g_anSpyroVelocity[1] * g_anSpyroGroundNormal[1] <
            0) {
      ChangeSpyroState(SPYRO_ST_CHARGE_BONK);
      break;
    }
    if (g_nSpyroContactedActorFlag == 0 &&
        g_nSpyroMotionSpeed > SPYRO_RUN_DROP_SPEED &&
        g_nSpyroFrameMotionLength * 2 < g_nSpyroMotionSpeed &&
        g_nPortalTransitionActive == 0) {
      BuildSpyroVelocityFromBodyEuler();
      ChangeSpyroState(SPYRO_ST_CHARGE_BONK);
      break;
    }
    if (g_nSpyroAirborneFrames == 0) {
      if (g_nSpyroSubstate & SPYRO_SUB_SUPERCHARGE) {
        ChangeSpyroState(SPYRO_ST_SUPERCHARGE);
      } else {
        ChangeSpyroState(SPYRO_ST_CHARGE);
      }
      break;
    }
    if (g_anSpyroVelocity[2] < 0 && g_nSpyroGlideQueued != 0) {
      JUMP_STATE();
    }
    break;
  case SPYRO_ST_JOG:
    if (ApplySpyroTriggerEvents(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_dwPadPressed & PAD_CROSS) {
      ChangeSpyroState(SPYRO_ST_JUMP);
      break;
    }
    if (g_nSpyroAirborneFrames >= SPYRO_AIRBORNE_GRACE ||
        g_nSpyroGroundSlope >= SLOPE_STEEP) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (g_dwPadHeld & PAD_SQUARE) {
      ChangeSpyroState(SPYRO_ST_CHARGE);
      break;
    }
    if (g_dwPadPressed & PAD_L1) {
      ChangeSpyroState(SPYRO_ST_SIDESTEP_L1);
      break;
    }
    if (g_dwPadPressed & PAD_R1) {
      ChangeSpyroState(SPYRO_ST_SIDESTEP_R1);
      break;
    }
    if (g_nSpyroMotionSpeed > SPYRO_RUN_SPEED) {
      ChangeSpyroState(SPYRO_ST_RUN);
    } else if (g_nSpyroMotionSpeed < SPYRO_WALK_DROP_SPEED) {
      ChangeSpyroState(SPYRO_ST_WALK);
    }
    break;
  case SPYRO_ST_GLIDE_SLOW:
    if (ApplySpyroTriggerEvents(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_nSpyroAirborneFrames == 0) {
      if (g_nSpyroGroundSlope >= SLOPE_STEEP) {
        ChangeSpyroState(SPYRO_ST_FALL);
      } else {
        LAND_ON_GROUND();
      }
      break;
    }
    if (g_nSpyroGroundProbeSuspendFlag == 0 ||
        g_nSpyroFrameMotionLength < SPYRO_GLIDE_STALL_MOTION) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (g_dwPadHeld & PAD_SQUARE) {
      ChangeSpyroState(SPYRO_ST_CHARGE_AIR);
      break;
    }
    if (g_dwPadPressed & PAD_TRIANGLE) {
      ChangeSpyroState(SPYRO_ST_GLIDE_DROP);
      break;
    }
    if (g_nSpyroFloorReferenceZ + SPYRO_GLIDE_RISE_MAX < g_nSpyroWorldPosZ) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (g_nSpyroMotionSpeed > SPYRO_RUN_DROP_SPEED) {
      JUMP_STATE();
    }
    break;
  case SPYRO_ST_SUPERCHARGE_AIR: {
    int t;

    if (g_nSpyroStateTimer >= SPYRO_LATE_JUMP_TICKS) {
      g_nSpyroLateJumpAllowed = 0;
    }
    if ((g_dwPadHeld & PAD_CROSS) && g_nSpyroLateJumpAllowed != 0) {
      g_nSpyroLateJumpAllowed = 0;
      g_nSpyroVelocityZ += SPYRO_JUMP_SPEED;
    } else if ((g_dwPadPressed & PAD_CROSS) && g_nSpyroGlideSpentFlag == 0) {
      g_nSpyroGlideQueued = 1;
    }
    if (g_nSpyroGroundSlope >= SLOPE_STEEP && g_nPortalTransitionActive == 0) {
      t = g_anSpyroVelocity[0] * g_anSpyroGroundNormal[0] +
          g_anSpyroVelocity[1] * g_anSpyroGroundNormal[1];
      if (t < 0) {
        ChangeSpyroState(SPYRO_ST_CHARGE_BONK);
        break;
      }
    }
    t = VectorLength(g_anSpyroVelocity, VEC_LEN_XYZ);
    if (t == 0) {
      t = 1;
    }
    t = (g_nSpyroFrameMotionLength << 12) / t;
    if (g_nSpyroContactedActorFlag == 0 && t < SPYRO_SUPERCHARGE_BONK_RATIO &&
        g_nPortalTransitionActive == 0) {
      ChangeSpyroState(SPYRO_ST_CHARGE_BONK);
      break;
    }
    if (g_nSpyroAirborneFrames == 0) {
      ChangeSpyroState(SPYRO_ST_CHARGE);
      break;
    }
    if (g_anSpyroVelocity[2] < 0) {
      if (g_nSpyroGlideQueued == 0) {
        ChangeSpyroState(SPYRO_ST_CHARGE_AIR);
      } else {
        JUMP_STATE();
      }
    }
    break;
  }
  case SPYRO_ST_HURT_PINNED:
    if (g_dwSpyroTriggerEventFlags & SPYRO_EV_HIT_PINNED) {
      break;
    }
    if (g_nSpyroStateTimer < SPYRO_HURT_TICKS) {
      break;
    }
    if (g_nSparxHealth < SPARX_GONE) {
      ChangeSpyroState(SPYRO_ST_DEATH);
      break;
    }
  land: {
    int next = SPYRO_ST_FALL;

    if (g_nSpyroAirborneFrames == 0) {
      next = OpaqueInt(SPYRO_ST_IDLE);
    }
    ChangeSpyroState(next);
    if (SPY[SPY_INPUT_BLANK] < SPYRO_HURT_LAND_BLANK) {
      SPY[SPY_INPUT_BLANK] = SPYRO_HURT_LAND_BLANK;
    }
    break;
  }
  case SPYRO_ST_SCRIPTED_WALK:
    if (ApplySpyroTriggerEvents(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_dwPadPressed & PAD_CROSS) {
      ChangeSpyroState(SPYRO_ST_JUMP);
      break;
    }
    if (g_nSpyroAirborneFrames >= SPYRO_AIRBORNE_GRACE ||
        g_nSpyroGroundSlope >= SLOPE_STEEP) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (g_nSpyroMotionSpeed != 0 || g_nSpyroSpeedTarget != 0 ||
        g_nSpyroStateTimer < SPYRO_WALK_MIN_TICKS) {
      ChangeSpyroState(SPYRO_ST_WALK);
      break;
    }
    ChangeSpyroState(SPYRO_ST_IDLE);
    break;
  case SPYRO_ST_HAZARD:
    if (ApplySpyroTriggerEvents(SPYRO_EV_MASK_NO_HAZARD)) {
      break;
    }
    if (SPY[SPY_SPARX_HEALTH] < 0) {
      break;
    }
    if (g_nSpyroStateTimer > SPYRO_HAZARD_TICKS) {
      SPY[SPY_SPARX_HEALTH] = -1;
      g_pfnLevelSpawnActor(g_anHazardTimeoutSpawnType[g_nSpyroHazardKind], 0);
      break;
    }
    if (g_nSpyroStateTimer >= SPYRO_HAZARD_JUMP_TICKS &&
        (g_dwPadHeld & PAD_CROSS)) {
      ChangeSpyroState(SPYRO_ST_JUMP);
    }
    break;
  case SPYRO_ST_FLY:
    if (ApplySpyroTriggerEvents(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_nSpyroAirborneFrames == 0) {
      if (g_nSpyroGroundSlope < SLOPE_STEEP) {
        LAND_ON_GROUND();
      } else {
        ChangeSpyroState(SPYRO_ST_FALL);
      }
    } else if (g_nSpyroGroundProbeSuspendFlag == 0 ||
               g_nSpyroFrameMotionLength < SPYRO_GLIDE_STALL_MOTION) {
      ChangeSpyroState(SPYRO_ST_FALL);
    } else if (g_anSpyroMotionVec[2] > SPYRO_FLY_CLIMB_VZ ||
               (g_dwPadPressed & PAD_CROSS)) {
      ChangeSpyroState(SPYRO_ST_FLY_CLIMB);
    } else if (g_anSpyroMotionVec[2] < -SPYRO_FLY_DIVE_VZ) {
      ChangeSpyroState(SPYRO_ST_FLY_DIVE);
    } else if (g_nSpyroSubstate == 0) {
      int d = g_nSpyroWorldPosZ;

      if (d - g_nSpyroFloorReferenceZ > SPYRO_FLY_CEILING) {
        ChangeSpyroState(SPYRO_ST_FALL);
      } else {
        int pad = g_pPadSubstepState->held;

        if (pad & PAD_R1) {
          g_nSpyroSubstate = SPYRO_SUB_GLIDE_R1;
        } else if (pad & PAD_L1) {
          g_nSpyroSubstate = SPYRO_SUB_GLIDE_L1;
          g_nSpyroMotionAux = d;
        }
      }
    }
    if (g_nFlightLevelActive != 0) {
      break;
    }
    if (g_dwPadHeld & PAD_SQUARE) {
      ChangeSpyroState(SPYRO_ST_CHARGE_AIR);
      break;
    }
    if (g_nSpyroStateTimer > SPYRO_GLIDE_DROP_DELAY &&
        (g_dwPadPressed & PAD_TRIANGLE)) {
      ChangeSpyroState(SPYRO_ST_GLIDE_DROP);
    }
    break;
  case SPYRO_ST_FLY_CLIMB:
    if (ApplySpyroTriggerEvents(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_nSpyroAirborneFrames == 0) {
      if (g_nSpyroGroundSlope < SLOPE_STEEP) {
        LAND_ON_GROUND();
      } else {
        ChangeSpyroState(SPYRO_ST_FALL);
      }
    } else if (g_nSpyroGroundProbeSuspendFlag == 0 ||
               g_nSpyroFrameMotionLength < SPYRO_GLIDE_STALL_MOTION) {
      ChangeSpyroState(SPYRO_ST_FALL);
    } else if (g_nSpyroStateTimer >= SPYRO_FLY_PITCH_MIN_TICKS &&
               g_anSpyroMotionVec[2] < SPYRO_FLY_CLIMB_LEVEL_VZ) {
      ChangeSpyroState(SPYRO_ST_FLY);
    } else if (SPY[SPY_SUBSTATE] == 0) {
      int d = g_nSpyroWorldPosZ;

      if (d - g_nSpyroFloorReferenceZ > SPYRO_FLY_CEILING) {
        ChangeSpyroState(SPYRO_ST_FALL);
      } else {
        int pad = g_pPadSubstepState->held;

        if (pad & PAD_R1) {
          SPY[SPY_SUBSTATE] = SPYRO_SUB_GLIDE_R1;
        } else if (pad & PAD_L1) {
          SPY[SPY_SUBSTATE] = SPYRO_SUB_GLIDE_L1;
          g_nSpyroMotionAux = d;
        }
      }
    }
    if (g_nFlightLevelActive != 0) {
      break;
    }
    if (g_dwPadHeld & PAD_SQUARE) {
      ChangeSpyroState(SPYRO_ST_CHARGE_AIR);
      break;
    }
    if (g_nSpyroStateTimer > SPYRO_GLIDE_DROP_DELAY &&
        (g_dwPadPressed & PAD_TRIANGLE)) {
      ChangeSpyroState(SPYRO_ST_GLIDE_DROP);
    }
    break;
  case SPYRO_ST_FLY_DIVE:
    if (ApplySpyroTriggerEvents(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_nSpyroAirborneFrames == 0) {
      if (g_nSpyroGroundSlope < SLOPE_STEEP) {
        LAND_ON_GROUND();
      } else {
        ChangeSpyroState(SPYRO_ST_FALL);
      }
    } else if (g_nSpyroGroundProbeSuspendFlag == 0 ||
               g_nSpyroFrameMotionLength < SPYRO_GLIDE_STALL_MOTION) {
      ChangeSpyroState(SPYRO_ST_FALL);
    } else if (g_dwPadPressed & PAD_CROSS) {
      ChangeSpyroState(SPYRO_ST_FLY_CLIMB);
    } else if (g_nSpyroStateTimer >= SPYRO_FLY_PITCH_MIN_TICKS &&
               g_anSpyroMotionVec[2] > -SPYRO_FLY_DIVE_LEVEL_VZ) {
      ChangeSpyroState(SPYRO_ST_FLY);
    } else if (SPY[SPY_SUBSTATE] == 0) {
      int d = g_nSpyroWorldPosZ;

      if (d - g_nSpyroFloorReferenceZ > SPYRO_FLY_CEILING) {
        ChangeSpyroState(SPYRO_ST_FALL);
      } else {
        int pad = g_pPadSubstepState->held;

        if (pad & PAD_R1) {
          SPY[SPY_SUBSTATE] = SPYRO_SUB_GLIDE_R1;
        } else if (pad & PAD_L1) {
          SPY[SPY_SUBSTATE] = SPYRO_SUB_GLIDE_L1;
          g_nSpyroMotionAux = d;
        }
      }
    }
    if (g_nFlightLevelActive != 0) {
      break;
    }
    if (g_dwPadHeld & PAD_SQUARE) {
      ChangeSpyroState(SPYRO_ST_CHARGE_AIR);
      break;
    }
    if (g_nSpyroStateTimer > SPYRO_GLIDE_DROP_DELAY &&
        (g_dwPadPressed & PAD_TRIANGLE)) {
      ChangeSpyroState(SPYRO_ST_GLIDE_DROP);
    }
    break;
  case SPYRO_ST_SCRIPTED_HOLD:
    if (ApplySpyroTriggerEvents(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_dwSpyroRequestMask == 0) {
      ChangeSpyroState(SPYRO_ST_IDLE);
    }
    break;
  case SPYRO_ST_SUPERCHARGE:
    if (ApplySpyroTriggerEvents(SPYRO_EV_MASK_SUPERCHARGE)) {
      break;
    }
    if (g_dwPadHeld & PAD_CROSS) {
      if (g_nSpyroMotionAux >= SPYRO_SUPERCHARGE_JUMP_HOLD ||
          g_nSpyroAirborneFrames != 0) {
        goto jump;
      }
      g_nSpyroMotionAux += g_nFrameStep;
    }
    if (g_dwPadReleased & PAD_CROSS) {
    jump: {
      int *vz;

      ChangeSpyroState(SPYRO_ST_SUPERCHARGE_AIR);
      vz = &SPY[SPY_VELOCITY + 2];
      g_nSpyroLateJumpAllowed = 0;
      *vz += SPYRO_JUMP_SPEED;
      g_nSpyroAirborneFrames++;
      break;
    }
    }
    if (g_nSpyroAirborneFrames >= SPYRO_AIRBORNE_GRACE) {
      ChangeSpyroState(SPYRO_ST_SUPERCHARGE_AIR);
      g_nSpyroLateJumpAllowed = 1;
      break;
    }
    if (g_nSpyroGroundSlope >= SLOPE_STEEP) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (g_nSpyroStateTimer >= SPYRO_SUPERCHARGE_MIN_TICKS &&
        !(*(volatile unsigned int *)&g_dwPadHeld & PAD_SQUARE)) {
      ChangeSpyroState(SPYRO_ST_CHARGE);
      break;
    }
    if (g_nSpyroContactedActorFlag == 0 &&
        g_nSpyroMotionSpeed > SPYRO_RUN_DROP_SPEED &&
        g_nSpyroFrameMotionLength * 2 < g_nSpyroMotionSpeed &&
        g_nPortalTransitionActive == 0) {
      ChangeSpyroState(SPYRO_ST_CHARGE_BONK);
      break;
    }
    if (g_nSpyroMotionSpeed < SPYRO_SUPERCHARGE_EXIT_SPEED) {
      ChangeSpyroState(SPYRO_ST_CHARGE);
    }
    break;
  }
}

#undef LAND_ON_GROUND
#undef JUMP_STATE
#undef LANDING_DUST

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
                ANGLE12_MASK;
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
            gb[SPY_ROLL - SPY_GLIDE_BANK] = (ANGLE12_HALF - r) & ANGLE12_MASK;
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
        g_abSpyroPersistentEuler[2] = ANGLE12_TO_8(g_nSpyroBodyYaw);
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
              y = (-ANGLE12_QUARTER - g_nSpyroBodyRoll) & ANGLE12_MASK;
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
      a = (g_nSpyroBodyYaw - (ANGLE8_TO_12(g_pScriptedCameraActor->rotZ))) &
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
      a = (ANGLE8_TO_12(g_pScriptedCameraActor->rotZ)) - ((a * a) >> 11);
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
  g_abSpyroPersistentEuler[0] = ANGLE12_TO_8(g_nSpyroBodyPitch);
  g_abSpyroPersistentEuler[1] = g_nSpyroBodyRoll >> 4;
  g_abSpyroPersistentEuler[2] = ANGLE12_TO_8(g_nSpyroBodyYaw);
  {
    register int hr asm("$9");

    __asm__ volatile("" : "=r"(hr));
  }
  {
    register int hs asm("$18");

    __asm__ volatile("" : "=r"(hs));
  }
}

#undef PADB
#undef RAMP
#undef INVULN_TICK
#undef READ_STICK
#undef WALL_SLIDE
#undef FINISH_VELOCITY
#undef CLAMP_FALL_SPEED
#undef SPEED_FROM_DROP
#undef SPEED_FROM_DROP_T
#undef PITCH_TOWARD_TURN
#undef LEVEL_GLIDE_HANDOFF
#undef APPROACH_GLIDE_SPEED

/* Pulls the floor reference toward the fall reference, at most 64 up and
   16 down per frame. */
#define TRACK_FLOOR_REFERENCE()                                                \
  {                                                                            \
    int d = g_nSpyroFallReferenceZ - g_nSpyroFloorReferenceZ;                  \
    if (d > 0x40) {                                                            \
      d = 0x40;                                                                \
    }                                                                          \
    if (d < -0x10) {                                                           \
      d = -0x10;                                                               \
    }                                                                          \
    g_nSpyroFloorReferenceZ += d;                                              \
  }

#define CLAMP_FALL_REFERENCE()                                                 \
  {                                                                            \
    if (g_nSpyroFallReferenceZ != 0) {                                         \
      TRACK_FLOOR_REFERENCE();                                                 \
    }                                                                          \
    if (g_nSpyroFloorReferenceZ > g_nSpyroFallReferenceZ &&                    \
        g_nSpyroGroundProbeSuspendFlag != 0) {                                 \
      g_nSpyroFallReferenceZ = g_nSpyroFloorReferenceZ;                        \
    }                                                                          \
    if (g_nActiveLevelId == LEVEL_GNASTYS_LOOT &&                              \
        g_nSpyroFallReferenceZ > g_nGnastyLootFallRefZCap) {                   \
      g_nSpyroFallReferenceZ = g_nGnastyLootFallRefZCap;                       \
    }                                                                          \
  }

/* 0x80047b60 (0xd2c): per-state physics step for Spyro. Each state runs its
   own mix of platform averaging, integration, ground probe, slope gravity,
   floor snap and ledge probe; walking states also derive the slope-adjusted
   move speed, and the fall states track the floor reference used for fall
   damage. Afterwards SPYRO_ST_HAZARD records the floor under the hazard
   (g_nSpyroHazardFloorZ). */
void DispatchSpyroPhysicsByState(void) {
  int pos[3];
  int puff[3];
  int dir[3];
  int dust[3];
  int dustVel[3];
  int trail[3];
  int trailVel[3];

  g_nCameraSpringSuppressFlag = 0;
  switch (g_nSpyroState) {
  case SPYRO_ST_IDLE:
  case SPYRO_ST_CHARGE_BONK:
  case SPYRO_ST_TEETER:
  case SPYRO_ST_FIDGET_0:
  case SPYRO_ST_SCRIPTED_HOLD:
  case SPYRO_ST_FIDGET_1:
  case SPYRO_ST_FIDGET_2:
  case SPYRO_ST_FIDGET_3:
  case SPYRO_ST_FIDGET_4:
  case SPYRO_ST_FIDGET_5:
  case SPYRO_ST_FIDGET_6:
  case SPYRO_ST_FIDGET_7:
  case SPYRO_ST_FIDGET_8:
    if (g_nPadDirectionalIdleFlag != 0 && (g_dwPadHeld & PAD_TRIANGLE)) {
      g_nCameraNextMode = (CAM_HELD | CAM_MODE_FREE_LOOK);
    }
    AverageSpyroPlatformMotion();
    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    if (g_dwSpyroRequestMask == 0) {
      StepSpyroOntoLocalFloor();
    }
    break;
  case SPYRO_ST_WALK:
  case SPYRO_ST_RUN:
  case SPYRO_ST_STOP:
  case SPYRO_ST_JOG: {
    AverageSpyroPlatformMotion();
    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    ApplyGravityAlongSpyroSlopeNormal();
    if (SPY[SPY_GROUND_SLOPE] != 0) {
      int ang;
      int t;
      int a;
      int b;

      ang =
          (g_nSpyroBodyYaw - ArcTan2(g_anSpyroGroundNormal[0],
                                     g_anSpyroGroundNormal[1], ATAN_ANGLE12)) &
          ANGLE12_MASK;
      if (ang > ANGLE12_HALF) {
        ang -= ANGLE12_TURN;
      }
      t = (SPY[SPY_GROUND_SLOPE] << 12) / (SLOPE_STEEP - 1);
      ang = abs(ang);
      if (ang > ANGLE12_QUARTER) { /* facing uphill */
        a = g_anSpyroSlopeSpeedTargets[SLOPE_SPEED_UP] +
            ((t * (g_anSpyroSlopeSpeedTargets[SLOPE_SPEED_UP +
                                              SLOPE_SPEED_STEEPEST] -
                   g_anSpyroSlopeSpeedTargets[SLOPE_SPEED_UP])) >>
             12);
        ang = ANGLE12_HALF - ang;
      } else {
        a = g_anSpyroSlopeSpeedTargets[SLOPE_SPEED_DOWN] +
            ((t * (g_anSpyroSlopeSpeedTargets[SLOPE_SPEED_DOWN +
                                              SLOPE_SPEED_STEEPEST] -
                   g_anSpyroSlopeSpeedTargets[SLOPE_SPEED_DOWN])) >>
             12);
      }
      b = g_anSpyroSlopeSpeedTargets[SLOPE_SPEED_ACROSS] +
          ((t * (g_anSpyroSlopeSpeedTargets[SLOPE_SPEED_ACROSS +
                                            SLOPE_SPEED_STEEPEST] -
                 g_anSpyroSlopeSpeedTargets[SLOPE_SPEED_ACROSS])) >>
           12);
      a = (a * LookupCosine(ang)) >> 12;
      b = (b * LookupSine(ang)) >> 12;
      g_nSpyroMoveTargetMag = IntegerSqrt(a * a + b * b);
    } else {
      g_nSpyroMoveTargetMag = g_anSpyroSlopeSpeedTargets[SLOPE_SPEED_DOWN];
    }
    if (SPY[SPY_STATE] == SPYRO_ST_STOP) {
      CopyVector(puff, &SPY[SPY_MOTION]);
      RShiftVector3(puff, 6);
      g_pfnLevelEmitParticles(1, EMIT_DUST_PUFF, puff, 0);
    }
  } break;
  case SPYRO_ST_TURN_AROUND:
    AverageSpyroPlatformMotion();
    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    ApplyGravityAlongSpyroSlopeNormal();
    dir[0] = 0x20;
    dir[1] = 0;
    dir[2] = 0;
    RotateVectorByMatrix(g_anSpyroBodyMtx, dir, dir);
    g_pfnLevelEmitParticles(1, EMIT_DUST_PUFF, dir, 0);
    break;
  case SPYRO_ST_HURT_TINT: {
    unsigned char frame;

    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    ApplyGravityAlongSpyroSlopeNormal();
    frame = g_bSpyroAnimFrame;
    if (frame < HURT_FLASH_FRAMES) {
      g_abSpyroTintRgba[3] = 0xFF;
      if (frame & 1) {
        g_abSpyroTintRgba[2] = HURT_FLASH_LIGHT;
        g_abSpyroTintRgba[1] = HURT_FLASH_LIGHT;
        g_abSpyroTintRgba[0] = HURT_FLASH_LIGHT;
      } else {
        g_abSpyroTintRgba[2] = HURT_FLASH_DARK;
        g_abSpyroTintRgba[1] = HURT_FLASH_DARK;
        g_abSpyroTintRgba[0] = HURT_FLASH_DARK;
      }
    } else {
      g_abSpyroTintRgba[3] = 0;
    }
  } break;
  case SPYRO_ST_HURT_PINNED:
  case SPYRO_ST_HURT_LONG:
    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    break;
  case SPYRO_ST_JUMP:
  case SPYRO_ST_LIFT:
    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    if (g_nSpyroWallSlideContributedFlag == 0) {
      ProbeSpyroLedgeForward();
    }
    break;
  case SPYRO_ST_FALL:
  case SPYRO_ST_GLIDE_DROP:
    if (SPY[SPY_FLIGHT_MODE] == 0) {
      g_nSpyroFloorReferenceZ = g_nSpyroWorldPosZ;
    }
    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    if (SPY[SPY_FLIGHT_MODE] != 0) {
      if (g_nFlightLevelActive != 0) {
        TRACK_FLOOR_REFERENCE();
      } else {
        CLAMP_FALL_REFERENCE();
      }
    }
    if (SPY[SPY_WALL_SLIDE] == 0) {
      ProbeSpyroLedgeForward();
    }
    CopyVector(&SPY[SPY_VELOCITY], &SPY[SPY_MOTION]);
    break;
  case SPYRO_ST_GLIDE_SLOW:
    if (SPY[SPY_FLIGHT_MODE] == 0) {
      g_nSpyroFloorReferenceZ = g_nSpyroWorldPosZ;
    }
    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    if (SPY[SPY_FLIGHT_MODE] != 0) {
      if (g_nFlightLevelActive != 0) {
        TRACK_FLOOR_REFERENCE();
      } else {
        CLAMP_FALL_REFERENCE();
      }
    }
    if (g_nSpyroWallSlideContributedFlag == 0) {
      ProbeSpyroLedgeForward();
    }
    break;
  case SPYRO_ST_FLY:
  case SPYRO_ST_FLY_CLIMB:
  case SPYRO_ST_FLY_DIVE:
    if (SPY[SPY_FLIGHT_MODE] == 0) {
      g_nSpyroFloorReferenceZ = g_nSpyroWorldPosZ;
    }
    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    if (SPY[SPY_FLIGHT_MODE] != 0) {
      if (g_nFlightLevelActive != 0) {
        TRACK_FLOOR_REFERENCE();
        if (g_nSpyroSubstate == SPYRO_SUB_FLY_RUN &&
            CollideSphereWithWorldAndActors(&SPY[SPY_POS], SPYRO_RADIUS,
                                            SPYRO_RADIUS) != 0) {
          g_pfnLevelEndFlightRun();
        }
      } else {
        CLAMP_FALL_REFERENCE();
      }
    }
    if (g_nSpyroWallSlideContributedFlag == 0) {
      ProbeSpyroLedgeForward();
    }
    break;
  case SPYRO_ST_GLIDE:
    if (SPY[SPY_FLIGHT_MODE] == 0) {
      g_nSpyroFloorReferenceZ = g_nSpyroWorldPosZ;
    }
    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    if (SPY[SPY_FLIGHT_MODE] != 0) {
      CLAMP_FALL_REFERENCE();
    }
    if (SPY[SPY_WALL_SLIDE] == 0) {
      ProbeSpyroLedgeForward();
    }
    if ((g_dwSpyroRequestMask & SPYRO_RQ_NO_CLIP) &&
        g_nSpyroSubstate == SPYRO_SUB_GLIDE_TO_TARGET) {
      g_nSpyroGroundHeightZ = FindGroundHeightBelow(&SPY[SPY_POS], 0x10000);
    }
    break;
  case SPYRO_ST_FACE_TARGET:
    AverageSpyroPlatformMotion();
    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    StepSpyroOntoLocalFloor();
    break;
  case SPYRO_ST_CHARGE:
    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    ApplyGravityAlongSpyroSlopeNormal();
    ApplyGravityAlongSpyroSlopeTangent();
    StepSpyroOntoLocalFloor();
    if (g_nSpyroWallSlideContributedFlag == 0) {
      ProbeSpyroLedgeForward();
    }
    break;
  case SPYRO_ST_SLIDE:
    AdvanceSpyroPhysics();
    CopyVector(&SPY[SPY_CARRY], &SPY[SPY_MOTION]);
    ProbeSpyroGroundContact();
    ApplyGravityAlongSpyroSlopeNormal();
    ApplyGravityAlongSpyroSlopeTangent();
    break;
  case SPYRO_ST_CHARGE_AIR:
  case SPYRO_ST_SUPERCHARGE_AIR:
    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    if (SPY[SPY_SUBSTATE] & SPYRO_SUB_SUPERCHARGE) {
      CopyVector(dust, &SPY[SPY_POS]);
      dust[0] += (GetRandomU32() & 0xFE) - 0x7F;
      dust[1] += (GetRandomU32() & 0xFE) - 0x7F;
      CopyVector(dustVel, &SPY[SPY_MOTION]);
      RShiftVector3(dustVel, 6);
      g_pfnLevelEmitParticles(1, EMIT_SUPERCHARGE_TRAIL, dust, (int)dustVel);
    }
    break;
  case SPYRO_ST_SIDESTEP_L1:
  case SPYRO_ST_SIDESTEP_R1:
  case SPYRO_ST_HURT:
  case SPYRO_ST_KNOCKBACK:
  case SPYRO_ST_SCRIPTED_WALK:
  case SPYRO_ST_KNOCKBACK_2:
  case SPYRO_ST_DEATH:
  case SPYRO_ST_DEATH_PINNED:
    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    ApplyGravityAlongSpyroSlopeNormal();
    break;
  case SPYRO_ST_HAZARD: {
    int *sink;
    int n;

    sink = &SPY[SPY_HAZARD_SINK];
    if (*sink != 0) {
      n = *sink + SPYRO_HAZARD_SINK_STEP;
      *sink = n;
      g_nSpyroWorldPosZ = g_nSpyroTriggerEntryZ + SPYRO_RADIUS;
      if (g_nSparxHealth >= SPARX_GONE) {
        if (n > SPYRO_HAZARD_SINK_SURVIVE) {
          *sink = SPYRO_HAZARD_SINK_SURVIVE;
        }
      } else if (n > SPYRO_HAZARD_SINK_FATAL) {
        *sink = SPYRO_HAZARD_SINK_FATAL;
      }
    }
    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    StepSpyroOntoLocalFloor();
  } break;
  case SPYRO_ST_SUPERCHARGE:
    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    ApplyGravityAlongSpyroSlopeNormal();
    ApplyGravityAlongSpyroSlopeTangent();
    CopyVector(trail, &SPY[SPY_POS]);
    trail[0] += (GetRandomU32() & 0xFE) - 0x7F;
    trail[1] += (GetRandomU32() & 0xFE) - 0x7F;
    trail[2] -= SPYRO_RADIUS;
    CopyVector(trailVel, &SPY[SPY_MOTION]);
    RShiftVector3(trailVel, 6);
    g_pfnLevelEmitParticles(1, EMIT_SUPERCHARGE_TRAIL, trail, (int)trailVel);
    break;
  }
  if (SPY[SPY_STATE] == SPYRO_ST_HAZARD) {
    g_nSpyroWorldPosZ -= g_nSpyroHazardSinkDepth;
    if (g_nSpyroGroundHeightZ != 0) {
      g_nSpyroHazardFloorZ = g_nSpyroGroundHeightZ;
    } else {
      CopyVector(pos, &SPY[SPY_POS]);
      pos[2] += 0x800;
      g_nSpyroHazardFloorZ = FindGroundHeightBelow(pos, 0xC00);
    }
  } else {
    g_nSpyroHazardFloorZ = 0;
  }
}

#undef TRACK_FLOOR_REFERENCE
#undef CLAMP_FALL_REFERENCE

/* g_nSpyroGravityStep_blk / g_nSpyroStateTimer_blk (globals.h) are the
   register-form (held-base) views of the clamped / incremented globals. */

/* 0x8004888c (0x310): per-substep Spyro state advance. Runs the state's
   attitude controller (free-fall damping for SPYRO_ST_FALL, velocity alignment
   for CHARGE_AIR / SUPERCHARGE_AIR, the near-ground flight controller for the
   rest), derives the anim play rate from the motion speed for the moving states
   (WALK, SLIDE, JOG; frame-driven for JUMP), then bumps g_nSpyroStateTimer. */
void AdvanceSpyroSubstepState(void) {
  int pad[2];
  int *p;
  unsigned char frame;

  switch (g_nSpyroState) {
  case SPYRO_ST_FALL:
    DampenSpyroFreeFallAttitude();
    break;
  case SPYRO_ST_WALK:
    UpdateSpyroFlightAttitudeNearGround();
    if (g_nSpyroMotionSpeed != 0) {
      if (g_nSpyroWallSlideContributedFlag) {
        g_nSpyroAnimPlayRate = 4;
      } else {
        g_nSpyroAnimPlayRate = g_nSpyroMotionSpeed >> 7;
      }
      p = g_nSpyroAnimPlayRate_blk;
      if (*p <= 0) {
        *p = 1;
      }
      if (*p > 16) {
        *p = 16;
      }
    } else {
      g_nSpyroAnimPlayRate = 4;
      if (g_dwActiveCameraOptions & CAM_OPT_PASSIVE_FOLLOW) {
        int *cam = g_nCameraNextMode_blk;
        int m = (CAM_HELD | CAM_MODE_STAND);
        if (*cam >= 0) {
          *cam = m;
        }
      }
    }
    break;
  case SPYRO_ST_JUMP:
    UpdateSpyroFlightAttitudeNearGround();
    if (g_nSpyroAnimMode == SPYRO_ANIM_PLAY) {
      if (g_nSpyroSubstate == SPYRO_SUB_JUMP_RISING) {
        frame = g_bSpyroAnimFrame;
        if (frame < 3) {
          g_nSpyroAnimPlayRate = 4;
        } else if (frame < 6) {
          g_nSpyroAnimPlayRate = 2;
        } else {
          g_nSpyroAnimPlayRate = 0;
        }
      } else if (g_bSpyroAnimFrame < 8) {
        g_nSpyroAnimPlayRate = 2;
      } else {
        g_nSpyroAnimPlayRate = 0;
      }
    }
    break;
  case SPYRO_ST_CHARGE:
    UpdateSpyroFlightAttitudeNearGround();
    if ((g_nSpyroSubstate & SPYRO_SUB_CHARGE_STEER) &&
        g_nSpyroContactedActorFlag != 0) {
      g_nSpyroMotionSpeed = 0xA00;
    }
    break;
  case SPYRO_ST_SLIDE:
    UpdateSpyroFlightAttitudeNearGround();
    p = g_nSpyroAnimPlayRate_blk;
    *p = (g_nSpyroMotionSpeed >> 11) + 3;
    if (*p < 4) {
      *p = 4;
    }
    if (*p > 12) {
      *p = 12;
    }
    break;
  case SPYRO_ST_CHARGE_AIR:
    AlignSpyroBodyToVelocity();
    break;
  case SPYRO_ST_JOG:
    UpdateSpyroFlightAttitudeNearGround();
    p = g_nSpyroAnimPlayRate_blk;
    *p = ((g_nSpyroMotionSpeed - 0x640) >> 9) + 6;
    if (*p < 6) {
      *p = 6;
    }
    if (*p > 16) {
      *p = 16;
    }
    break;
  case SPYRO_ST_SUPERCHARGE:
    if (g_nSpyroAirborneFrames != 0) {
      break;
    }
  case SPYRO_ST_IDLE:
  case SPYRO_ST_RUN:
  case SPYRO_ST_STOP:
  case SPYRO_ST_TURN_AROUND:
  case SPYRO_ST_HURT_TINT:
  case SPYRO_ST_FACE_TARGET:
  case SPYRO_ST_SIDESTEP_L1:
  case SPYRO_ST_SIDESTEP_R1:
  case SPYRO_ST_CHARGE_BONK:
  case SPYRO_ST_TEETER:
  case SPYRO_ST_HURT:
  case SPYRO_ST_GLIDE_DROP:
  case SPYRO_ST_LIFT:
  case SPYRO_ST_FIDGET_0:
  case SPYRO_ST_KNOCKBACK:
  case SPYRO_ST_HURT_PINNED:
  case SPYRO_ST_SCRIPTED_WALK:
  case SPYRO_ST_HURT_LONG:
  case SPYRO_ST_KNOCKBACK_2:
  case SPYRO_ST_HAZARD:
  case SPYRO_ST_DEATH:
  case SPYRO_ST_DEATH_PINNED:
  case SPYRO_ST_SCRIPTED_HOLD:
  case SPYRO_ST_FIDGET_1:
  case SPYRO_ST_FIDGET_2:
  case SPYRO_ST_FIDGET_3:
  case SPYRO_ST_FIDGET_4:
  case SPYRO_ST_FIDGET_5:
  case SPYRO_ST_FIDGET_6:
  case SPYRO_ST_FIDGET_7:
  case SPYRO_ST_FIDGET_8:
    UpdateSpyroFlightAttitudeNearGround();
    break;
  case SPYRO_ST_SUPERCHARGE_AIR:
    AlignSpyroBodyToVelocity();
    p = g_nSpyroGravityStep_blk;
    *p += 12;
    if (*p > 0xC0) {
      *p = 0xC0;
    }
    break;
  }
  p = g_nSpyroStateTimer_blk;
  *p += 1;
}

/* shared-base view of g_dwSpyroRequestMask (0x80078c4c): the mask read and
   the FindGroundHeightBelow position arg (-0x1F4 = g_anSpyroWorldPos) are
   both reached off one held register. */

/* 0x80048b9c (372 bytes) — per-frame Spyro physics driver: zero the motion
   accumulator, run the per-substep integrate loop, the footing-state
   dispatch, the per-substep finalize loop, then probe the ground below and
   fire the trigger-region event under Spyro: the ground he stands on, else
   the first surface he touched this frame (skipped under SPYRO_RQ_NO_CLIP). */
void AdvanceSpyroFrameMotion(void) {
  int i;
  int ground;
  int surface;
  int contact;
  unsigned int *req;

  ZeroVector(g_anSpyroVelocityFp6);
  for (i = 0; i < g_nFrameStep; i++) {
    IntegrateSpyroMotionForSubstep(i);
  }
  DispatchSpyroPhysicsByState();
  for (i = 0; i < g_nFrameStep; i++) {
    AdvanceSpyroSubstepState();
  }
  req = g_adwSpyroRequestMaskBlock;
  if (!(req[0] & SPYRO_RQ_NO_CLIP)) {
    ground =
        FindGroundHeightBelow((int *)(req - (SPY_REQUEST - SPY_POS)), 0x10000);
    g_nSpyroGroundHeightZ = ground;
    g_nSpyroTriggerCurrentEventId = 0;
    if (g_nSpyroAirborneFrames == 0 &&
        (surface = g_nGroundSurfaceType & TRIGGER_REGION_MASK) !=
            TRIGGER_REGION_NONE &&
        g_anSpyroWorldPos[2] - ground <= TRIGGER_ON_HEIGHT) {
      DispatchSpyroTriggerEvent(surface, TRIGGER_EV_ON);
    } else if ((contact = g_nSpyroFirstContactSurfaceType &
                          TRIGGER_REGION_MASK) != TRIGGER_REGION_NONE) {
      DispatchSpyroTriggerEvent(contact, TRIGGER_EV_TOUCH);
    } else if (g_nSpyroGroundHeightZ > 0 &&
               (surface = g_nGroundSurfaceType & TRIGGER_REGION_MASK) !=
                   contact) {
      DispatchSpyroTriggerEvent(surface, TRIGGER_EV_OVER);
    }
  }
}

/* `mesh` is Spyro's model plus anim * 4, so ANIM_SLOT reads that anim's
   header pointer (Model.m_Animations[anim]); a frame record is the header plus
   frame * 4, whose AnimationHeader.m_FrameWords word is the frame's sound key
   and its top byte the sfx id (0xFF none).  The header pointer is re-read per
   use (volatile) — the original never keeps it live across the sfx-id test. */
/* ...and from the held previous-key base (g_adwSpyroAnimSfxPrevKeyBlock,
   0x80078cbc) back to g_anSpyroWorldPos (the sound owner) and on to
   g_bSpyroSfxVoiceMarker. */
#define SFX_KEY_TO_SPYRO_POS 0x264
#define SFX_KEY_TO_VOICE_MARKER 0x3C

#define ANIM_SLOT(mesh) (*(volatile int *)((mesh) + MODEL_OFFSET(m_Animations)))
#define FRAMEREC(mesh, frame) ((frame) + ANIM_SLOT(mesh))
#define FRAME_KEY(rec)                                                         \
  (*(unsigned int *)((rec) + ANIM_HEADER_OFFSET(m_FrameWords)))
#define FRAME_SFX(rec)                                                         \
  (*(unsigned char *)((rec) + ANIM_HEADER_OFFSET(m_FrameWords) + 3))

/* 0x8004a200 (1516 bytes) — the in-level gameplay tick: snapshot the pad,
   integrate a frame of motion, run the state behaviour + the per-substep
   anim machine, compose the body and composed-body matrices, then police the
   death plane (respawn / hard-fall state change / scripted death camera).  Ends
   by firing the two anim-frame sounds (primary anim + layer-1 overlay):
   each layer's frame sound plays when its frame word differs from last
   frame's (g_dwSpyroAnimSfxPrevKey / g_dwSpyroLayer1SfxPrevKey), so a sound
   fires once on entering its keyframe. */
void TickSpyroGameplayFrame(void) {
  unsigned int key;
  unsigned int key2;
  int *countdown;
  unsigned char *euler;
  int *bodyMtx;
  int *composedMtx;
  int *pos;
  int *fallFrames;
  int *camRecord;
  unsigned char *animBlock;
  unsigned int *prevKey;
  int mesh;
  int rec;
  int ptr;
  int step;
  int frame;
  int scratch[8]; /* frame slack ahead of sfx[] (A127) */
  int sfx[2];     /* per-slot sfx id, kept in the frame across the sfx calls */
  int pad[2];     /* frame slack */

  if (((g_dwSpyroRequestMask & SPYRO_RQ_FREEZE_INPUT) != 0) ||
      (g_nSpyroScriptedMoveMode != 0)) {
    if (g_nSpyroInputBlankCountdown < 2) {
      g_nSpyroInputBlankCountdown = 2;
    }
  }
  countdown = g_anSpyroInputBlankBlock;
  g_bPadInputStashed = 0;
  if (countdown[0] != 0) {
    MovePadInputState(&g_dwPadPressed, &g_dwPadStash);
    countdown[0] -= 1;
  }
  AdvanceSpyroFrameMotion();
  if (g_nSpyroState == SPYRO_ST_HAZARD) {
    if (g_nSpyroHazardSinkDepth <= 0) {
      g_nSpyroHazardSinkDepth = 1;
    }
  } else {
    g_nSpyroHazardSinkDepth = 0;
  }
  if (((g_dwSpyroRequestMask & SPYRO_RQ_FORCE_STATE) == 0) &&
      (g_nSpyroScriptedMoveMode == 0)) {
    UpdateSpyroStateBehavior();
  }
  step = 0;
  while (step < g_nFrameStep) {
    step += 1;
    TickSpyroAnimStateMachine();
    TickSpyroFlameAttack();
    TickSpyroAnimLayer1();
    TickSpyroAnimLayer2GateTimer();
    TickSpyroAnimLayer2();
  }
  euler = g_abSpyroPersistentEulerBlock;
  bodyMtx = (int *)(euler + EULER_TO_BODY_MTX);
  euler[0] = ANGLE12_TO_8(g_nSpyroBodyPitch);
  g_abSpyroPersistentEuler[1] = g_nSpyroBodyRoll >> 4;
  g_abSpyroPersistentEuler[2] = ANGLE12_TO_8(g_nSpyroBodyYaw);
  ApplyEulerRotation(euler, bodyMtx, 0);
  composedMtx = (int *)(euler + EULER_TO_COMPOSED_MTX);
  ApplyEulerRotation(euler + 4, composedMtx, 0);
  MulMatrix0(bodyMtx, composedMtx, composedMtx);
  SampleSpyroShadowRingHeights(1);
  TickSpyroAttackEffects(g_nFrameStep);
  g_dwSpyroTriggerEventFlags = 0;
  if (g_nSparxHealth < SPARX_GONE) {
    switch (g_nSpyroState) {
    case SPYRO_ST_HAZARD:
      if (g_nSpyroHazardSinkDepth == SPYRO_HAZARD_SINK_FATAL) {
        if (g_nFlightLevelActive != 0) {
          g_pfnLevelEndFlightRun();
          g_pfnLevelUpdateActors();
        } else {
          TriggerRespawnOrGameOver();
        }
      }
      break;
    case SPYRO_ST_DEATH:
      if (g_nSpyroStateTimer >= SPYRO_DEATH_TICKS) {
        TriggerRespawnOrGameOver();
      }
      break;
    case SPYRO_ST_DEATH_PINNED:
      if (g_nSpyroStateTimer >= SPYRO_DEATH_PINNED_TICKS) {
        TriggerRespawnOrGameOver();
      }
      break;
    }
  } else if ((g_nSpyroWorldPosZ < WORLD_MIN_COORD) ||
             ((g_nSpyroState == SPYRO_ST_FALL) &&
              (g_nSpyroSubstate >= SPYRO_DEATH_FALL_FRAMES))) {
    TriggerRespawnOrGameOver();
  } else {
    pos = g_anSpyroWorldPosZBlock;
    if (pos[0] < g_anLevelDeathPlaneZTable[g_nLevelIndex]) {
      if ((g_nSpyroState != SPYRO_ST_FALL) &&
          (g_nSpyroState != SPYRO_ST_GLIDE_DROP)) {
        ChangeSpyroState(SPYRO_ST_FALL);
      }
    } else if ((g_anSpyroWorldPos[0] < SPYRO_WORLD_EDGE_XY) ||
               (g_nSpyroWorldPosY < SPYRO_WORLD_EDGE_XY)) {
      if ((g_nSpyroState != SPYRO_ST_FALL) &&
          (g_nSpyroState != SPYRO_ST_GLIDE_DROP)) {
        ZeroVector(pos + (SPY_MOTION - SPY_POS_Z));
        ZeroVector(pos + (SPY_VELOCITY - SPY_POS_Z));
        ChangeSpyroState(SPYRO_ST_FALL);
      }
    } else {
      goto done;
    }
    fallFrames = g_anSpyroSubstateBlock;
    if (fallFrames[0] == 0) {
      camRecord = g_anSpyroDeathCamRecordBlock;
      CopyVector(camRecord, g_anCameraPos);
      ActivateScriptedCameraView(SCRIPTED_CAM_VIEW_AT_POS(camRecord));
    }
    fallFrames[0] += g_nFrameStep;
  }
done:
  animBlock = g_abSpyroAnimPrevBlock;
  mesh = animBlock[0] * 4 + (int)g_apActorMeshTable;
  frame = g_bSpyroFramePrev * 4;
  sfx[0] = FRAME_SFX(FRAMEREC(mesh, frame));
  if (sfx[0] != 0xFF) {
    ptr = ANIM_SLOT(mesh);
    rec = frame + ptr;
    if (g_dwSpyroAnimSfxPrevKey != FRAME_KEY(rec)) {
      PlaySoundEffect(sfx[0], (Actor *)SPY_FROM_ANIM_PREV(animBlock, 0),
                      SFX_MODE_FLAT,
                      SPY_FROM_ANIM_PREV(animBlock, SPYB_VOICE_MARKER));
    }
  }
  key = FRAME_KEY(FRAMEREC(g_bSpyroAnimPrev * 4 + (int)g_apActorMeshTable,
                           g_bSpyroFramePrev * 4));
  prevKey = g_adwSpyroAnimSfxPrevKeyBlock;
  prevKey[0] = key;
  sfx[1] =
      FRAME_SFX(FRAMEREC(g_bSpyroAnimLayer1Prev * 4 + (int)g_apActorMeshTable,
                         g_bSpyroAnimLayer1FramePrev * 4));
  if ((sfx[1] != 0xFF) && (sfx[0] != sfx[1])) {
    ptr = ANIM_SLOT(g_bSpyroAnimLayer1Prev * 4 + (int)g_apActorMeshTable);
    rec = g_bSpyroAnimLayer1FramePrev * 4 + ptr;
    if (g_dwSpyroLayer1SfxPrevKey != FRAME_KEY(rec)) {
      PlaySoundEffect(
          sfx[1], (Actor *)((unsigned char *)prevKey - SFX_KEY_TO_SPYRO_POS),
          SFX_MODE_FLAT, (unsigned char *)prevKey + SFX_KEY_TO_VOICE_MARKER);
    }
  }
  key2 =
      FRAME_KEY(FRAMEREC(g_bSpyroAnimLayer1Prev * 4 + (int)g_apActorMeshTable,
                         g_bSpyroAnimLayer1FramePrev * 4));
  g_dwSpyroRequestMask &= ~SPYRO_RQ_PENDING;
  g_dwSpyroLayer1SfxPrevKey = key2;
  if (g_bPadInputStashed != 0) {
    MovePadInputState(&g_dwPadStash, &g_dwPadPressed);
  }
}

#undef SFX_KEY_TO_SPYRO_POS
#undef SFX_KEY_TO_VOICE_MARKER
#undef ANIM_SLOT
#undef FRAMEREC
#undef FRAME_KEY
#undef FRAME_SFX

/* shared-base view of g_nSpyroState (0x80078ad0): the state read and the
   ZeroVector arg at +0x88 (g_anSpyroVelocityFp6) share one register. */

/* 0x8004a7ec (544 bytes) — Spyro frame tick that holds him in a frozen
   glide (formerly TickSpyroFrameForceJump): snapshot the pad,
   ChangeSpyroState(SPYRO_ST_GLIDE) unless already there, substate
   SPYRO_SUB_GLIDE_HOLD, then the same substep-integrate /
   position-step / finalize / anim-tick / body-matrix-rebuild sequence as
   TickSpyroFrameNoInput. Called from LevelLoad_Update (post-level-load
   intro). */
void TickSpyroFrameGlideHold(void) {
  int i;
  int *sblock;
  int *vel;
  int *pos;
  unsigned char *euler;
  int *bodyMtx;
  int *m;
  int r;
  int y;
  int pad[8];

  g_bPadInputStashed = 0;
  MovePadInputState(&g_dwPadPressed, &g_dwPadStash);
  sblock = g_anSpyroStateBlock;
  if (sblock[0] != SPYRO_ST_GLIDE) {
    ChangeSpyroState(SPYRO_ST_GLIDE);
  }
  g_nSpyroSubstate = SPYRO_SUB_GLIDE_HOLD;
  if (g_nSpyroInputBlankCountdown <= 0) {
    g_nSpyroInputBlankCountdown = 1;
  }
  ZeroVector(&sblock[34]);
  for (i = 0; i < g_nFrameStep; i++) {
    IntegrateSpyroMotionForSubstep(i);
  }
  vel = g_anSpyroVelocityFp6;
  RShiftVector3(vel, 6);
  pos = vel - (SPY_VELOCITY_FP6 - SPY_POS);
  CopyVector(vel - (SPY_VELOCITY_FP6 - SPY_PREV_POS), pos);
  AddVector(pos, pos, vel);
  for (i = 0; i < g_nFrameStep; i++) {
    AdvanceSpyroSubstepState();
  }
  for (i = 0; i < g_nFrameStep; i++) {
    TickSpyroAnimStateMachine();
    TickSpyroFlameAttack();
    TickSpyroAnimLayer1();
    TickSpyroAnimLayer2GateTimer();
    TickSpyroAnimLayer2();
  }
  euler = g_abSpyroPersistentEulerBlock;
  bodyMtx = (int *)(euler + EULER_TO_BODY_MTX);
  euler[0] = ANGLE12_TO_8(g_nSpyroBodyPitch);
  r = g_nSpyroBodyRoll >> 4;
  y = ANGLE12_TO_8(g_nSpyroBodyYaw);
  euler[1] = r;
  euler[2] = y;
  ApplyEulerRotation(euler, bodyMtx, 0);
  m = (int *)(euler + EULER_TO_COMPOSED_MTX);
  ApplyEulerRotation(euler + 4, m, 0);
  MulMatrix0(bodyMtx, m, m);
  g_dwSpyroRequestMask = 0;
  MovePadInputState(&g_dwPadStash, &g_dwPadPressed);
}

/* shared-base view of g_nSpyroState (0x80078ad0): the state read and the
   ZeroVector arg at +0x88 (g_anSpyroVelocityFp6) share one register. */

/* 0x8004aa0c (536 bytes) — Spyro frame tick with no player input and no
   state machine: snapshot the pad, force jump state-flags if mid-jump,
   integrate the substeps, step the position by velocity>>6, run the
   per-substep finalize and anim/attack ticks, then rebuild the body
   matrices from the persistent Euler bytes. Used while Spyro coasts
   during scripted post-state delays. */
void TickSpyroFrameNoInput(void) {
  int i;
  int *sblock;
  int *vel;
  int *pos;
  unsigned char *euler;
  int *bodyMtx;
  int *m;
  int r;
  int y;
  int pad[8];

  g_bPadInputStashed = 0;
  MovePadInputState(&g_dwPadPressed, &g_dwPadStash);
  sblock = g_anSpyroStateBlock;
  if (sblock[0] == SPYRO_ST_GLIDE) {
    g_nSpyroSubstate = SPYRO_SUB_GLIDE_HOLD;
  }
  if (g_nSpyroInputBlankCountdown <= 0) {
    g_nSpyroInputBlankCountdown = 1;
  }
  ZeroVector(&sblock[34]);
  for (i = 0; i < g_nFrameStep; i++) {
    IntegrateSpyroMotionForSubstep(i);
  }
  vel = g_anSpyroVelocityFp6;
  RShiftVector3(vel, 6);
  pos = vel - (SPY_VELOCITY_FP6 - SPY_POS);
  CopyVector(vel - (SPY_VELOCITY_FP6 - SPY_PREV_POS), pos);
  AddVector(pos, pos, vel);
  for (i = 0; i < g_nFrameStep; i++) {
    AdvanceSpyroSubstepState();
  }
  for (i = 0; i < g_nFrameStep; i++) {
    TickSpyroAnimStateMachine();
    TickSpyroFlameAttack();
    TickSpyroAnimLayer1();
    TickSpyroAnimLayer2GateTimer();
    TickSpyroAnimLayer2();
  }
  euler = g_abSpyroPersistentEulerBlock;
  bodyMtx = (int *)(euler + EULER_TO_BODY_MTX);
  euler[0] = ANGLE12_TO_8(g_nSpyroBodyPitch);
  r = g_nSpyroBodyRoll >> 4;
  y = ANGLE12_TO_8(g_nSpyroBodyYaw);
  euler[1] = r;
  euler[2] = y;
  ApplyEulerRotation(euler, bodyMtx, 0);
  m = (int *)(euler + EULER_TO_COMPOSED_MTX);
  ApplyEulerRotation(euler + 4, m, 0);
  MulMatrix0(bodyMtx, m, m);
  g_dwSpyroRequestMask = 0;
  MovePadInputState(&g_dwPadStash, &g_dwPadPressed);
}

/* 0x8004ac24 (0x214): resets Spyro's entity record. Leaving a looping-sfx
   state stops Spyro's voices (SPYRO_ST_HURT_TINT just clears the tint alpha).
   With `full` set the whole 0x2a4-byte record is wiped, keeping only the world
   position, the persistent euler and the level-health flag, then Spyro is
   put back into SPYRO_ST_IDLE; otherwise only the transient flags/timers clear.
 */
void ResetSpyroEntity(int full) {
  int pos[3];
  unsigned char euler[3];
  int health;

  switch (g_nSpyroState) {
  case SPYRO_ST_CHARGE:
  case SPYRO_ST_GLIDE:
  case SPYRO_ST_FLY:
  case SPYRO_ST_SUPERCHARGE:
    StopSoundVoicesByOwner((Actor *)g_anSpyroWorldPos, VOICE_STOP_LOOPING);
    break;
  case SPYRO_ST_HURT_TINT:
    g_abSpyroTintRgba[3] = 0;
    break;
  }

  if (full) {
    CopyVector(pos, g_anSpyroWorldPos);
    euler[0] = g_abSpyroPersistentEuler[0];
    euler[1] = g_abSpyroPersistentEuler[1];
    euler[2] = g_abSpyroPersistentEuler[2];
    health = g_nSparxHealth;
    FillWord(g_anSpyroWorldPos, 0, 0x2A4);
    CopyVector(g_anSpyroWorldPos, pos);
    g_bSpyroOtBinBias = 4;
    g_bSpyroAnimFrame = 1;
    g_nSparxHealth = health;
    g_nSpyroGroundTriIndex = -1;
    g_abSpyroPersistentEuler[0] = euler[0];
    g_abSpyroPersistentEuler[1] = euler[1];
    g_abSpyroPersistentEuler[2] = euler[2];
    g_nSpyroBodyPitch = euler[0] << 4;
    g_nSpyroBodyRoll = euler[1] << 4;
    g_nSpyroBodyYaw = euler[2] << 4;
    ChangeSpyroState(SPYRO_ST_IDLE);
    g_nSpyroFirstContactSurfaceType = 0xFF;
    g_bSpyroSfxVoiceMarker = 0x3F;
    g_nSpyroSuperflameTimer = 0;
  } else {
    g_nSpyroGroundTriIndex = -1;
    g_nSpyroFirstContactSurfaceType = 0xFF;
    *(int *)g_abSpyroTintRgba = 0;
    g_dwSpyroTriggerEventFlags = 0;
    g_nSpyroHazardFloorZ = 0;
    g_nSpyroFlightMode = 0;
    g_nSpyroChargeHoldFlag = 0;
    g_nSpyroGlideBankingFlag = 0;
    g_nSpyroHazardSinkDepth = 0;
    g_nSpyroSuperflameTimer = 0;
    g_nSpyroBodyPitch = g_abSpyroPersistentEuler[0] << 4;
    g_nSpyroBodyRoll = g_abSpyroPersistentEuler[1] << 4;
    g_nSpyroBodyYaw = g_abSpyroPersistentEuler[2] << 4;
  }
}
