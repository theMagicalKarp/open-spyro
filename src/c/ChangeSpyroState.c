#include "emit_record.h"
#include "funcs.h"
#include "globals.h"
#include "level_sfx.h"
#include "main_shared.h"
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

#define PARTICLE_SPAWN(a, b, c, d)                                             \
  ((void (*)(int, int, int *, int))g_pfnLevelEmitParticles)(a, b, c, d)

/* Charge-speed latch: the first charge frame takes the carried speed, at
   least 0x1F80, as the speed target. */
#define LATCH_CHARGE_SPEED(vec)                                                \
  {                                                                            \
    int v;                                                                     \
                                                                               \
    g_nSpyroFloorReferenceZ = g_nSpyroWorldPosZ;                               \
    v = VectorLength(vec, VEC_LEN_XYZ);                                        \
    if (v < 0x1F80) {                                                          \
      v = 0x1F80;                                                              \
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
    StopSoundVoicesByOwner(g_anSpyroWorldPos, VOICE_STOP_LOOPING);
    break;
  case SPYRO_ST_HURT_TINT:
    g_abSpyroTintRgba[3] = 0;
    /* FALLTHROUGH */
  case SPYRO_ST_HURT:
  case SPYRO_ST_KNOCKBACK:
  case SPYRO_ST_HURT_LONG:
  case SPYRO_ST_KNOCKBACK_2:
    PlaySoundEffect(LEVEL_SFX(SFX_SPYRO_HURT_EXIT), g_anSpyroWorldPos,
                    SFX_MODE_FLAT,
                    (unsigned char *)g_anSpyroWorldPos + SPYB_VOICE_MARKER);
    break;
  case SPYRO_ST_HURT_PINNED:
    PlaySoundEffect(LEVEL_SFX(SFX_SPYRO_PINNED_EXIT), g_anSpyroWorldPos,
                    SFX_MODE_FLAT,
                    (unsigned char *)g_anSpyroWorldPos + SPYB_VOICE_MARKER);
    break;
  }
  switch (state) {
  case SPYRO_ST_IDLE: {
    int t;

    g_nSpyroSubstate = 0;
    t = g_abSpyroIdleDelayRandTable[GetRandomU32() & 7] *
        g_abSpyroAnimDescTable[2];
    g_nSpyroGravityStep = -0x80;
    g_nSpyroGravityEnableFlag = 1;
    g_nSpyroIdleAnimTimeout = t - 2;
    break;
  }
  case SPYRO_ST_WALK:
  case SPYRO_ST_RUN:
  case SPYRO_ST_STOP:
  case SPYRO_ST_JOG: {
    int a;

    SPY[SPY_SUBSTATE] = 0;
    a = (ArcTan2(SPY[SPY_MOTION], g_anSpyroMotionVec[1], 1) - g_nSpyroBodyYaw) &
        0xFFF;
    if ((unsigned)(a - 0x401) < 0x7FF) {
      g_nSpyroMotionSpeed = 0;
    } else {
      g_nSpyroMotionSpeed = VectorLength(&SPY[SPY_MOTION], VEC_LEN_XY);
    }
    if (state == SPYRO_ST_STOP && SPY[SPY_SPEED] > 0x1400) {
      SPY[SPY_SPEED] = 0x1400;
    }
    g_nSpyroGravityStep = -0xC00;
    ApplyGravityAlongSpyroSlopeNormal();
    if (g_nSpyroGroundSlope != 0) {
      int k;
      int base;
      int side;

      a = (g_nSpyroBodyYaw -
           ArcTan2(g_anSpyroGroundNormal[0], g_anSpyroGroundNormal[1], 1)) &
          0xFFF;
      if (a > 0x800) {
        a -= 0x1000;
      }
      k = (g_nSpyroGroundSlope << 12) / 22;
      a = abs(a);
      if (a > 0x400) {
        a = 0x800 - a;
        base = g_anSpyroSlopeSpeedTargets[2] +
               ((k * (g_anSpyroSlopeSpeedTargets[5] -
                      g_anSpyroSlopeSpeedTargets[2])) >>
                12);
      } else {
        base = g_anSpyroSlopeSpeedTargets[0] +
               ((k * (g_anSpyroSlopeSpeedTargets[3] -
                      g_anSpyroSlopeSpeedTargets[0])) >>
                12);
      }
      side = g_anSpyroSlopeSpeedTargets[1] +
             ((k * (g_anSpyroSlopeSpeedTargets[4] -
                    g_anSpyroSlopeSpeedTargets[1])) >>
              12);
      base = (base * LookupCosine(a)) >> 12;
      side = (side * LookupSine(a)) >> 12;
      g_nSpyroMoveTargetMag = IntegerSqrt(base * base + side * side);
    } else {
      g_nSpyroMoveTargetMag = g_anSpyroSlopeSpeedTargets[0];
    }
    g_nSpyroGravityEnableFlag = 1;
    if (g_nSpyroScriptedMoveMode != 0) {
      g_nSpyroSpeedTarget = 0;
      g_nSpyroMoveTargetYaw = g_abSpyroRequestEuler[2] << 4;
    }
    break;
  }
  case SPYRO_ST_TURN_AROUND:
    g_nSpyroSubstate = 0;
    g_nSpyroGravityStep = -0xC00;
    goto slope_gravity;
  case SPYRO_ST_JUMP:
    CopyVector(g_anSpyroVelocity, g_anSpyroVelocity + 6);
    g_anSpyroCarryMotionVec[2] = 0xDC0;
    g_nSpyroGravityStep = -0xC0;
    g_nSpyroSubstate = 0;
    g_nSpyroJumpHoldLatch = 0;
    g_nSpyroGravityEnableFlag = 0;
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
    if (g_nSpyroGroundProbeSuspendFlag == 0 && g_nSpyroGroundSlope < 0x20) {
      NudgeSpyroFromWallProbes();
    }
    g_nSpyroGravityStep = -0xC0;
    g_nSpyroJumpHoldLatch = 0;
    g_nSpyroGravityEnableFlag = 0;
    break;
  case SPYRO_ST_GLIDE_DROP:
    g_nSpyroSubstate = 0;
    g_anSpyroVelocity[0] = 0;
    g_anSpyroVelocity[1] = 0;
    g_anSpyroVelocity[2] = g_anSpyroMotionVec[2];
    if (g_nSpyroGroundProbeSuspendFlag == 0 && g_nSpyroGroundSlope < 0x20) {
      NudgeSpyroFromWallProbes();
    }
    g_nSpyroGravityStep = -0xC0;
    g_nSpyroJumpHoldLatch = 0;
    g_nSpyroGravityEnableFlag = 0;
    break;
  case SPYRO_ST_SIDESTEP_L1:
  case SPYRO_ST_SIDESTEP_R1:
    SPY[SPY_SUBSTATE] = 0;
    CopyVector(&SPY[SPY_CARRY], &SPY[SPY_MOTION]);
    g_nSpyroMotionSpeed = 0;
    g_nSpyroGravityStep = -0xC00;
    goto slope_gravity;
  case SPYRO_ST_CHARGE: {
    int best;
    unsigned char *actor;
    int *me;

    CopyVector(g_anSpyroCarryMotionVec, g_anSpyroCarryMotionVec + 9);
    if (g_nSpyroChargeHoldFlag != 0) {
      if (!(g_nSpyroSubstate & SPYRO_SUB_CHARGE_LATCHED)) {
        LATCH_CHARGE_SPEED(g_anSpyroCarryMotionVec);
      }
      g_nSpyroSubstate = SPYRO_SUB_CHARGE_LATCHED | 1;
    } else {
      g_nSpyroSubstate = 1;
    }
    SPY[SPY_BODY_YAW_STEP] = 0;
    g_nSpyroGravityStep = -0x240;
    me = SPY;
    ApplyGravityAlongSpyroSlopeNormal();
    ApplyGravityAlongSpyroSlopeTangent();
    g_nSpyroGravityEnableFlag = 1;
    PlaySoundEffect(LEVEL_SFX(SFX_CHARGE), me, SFX_MODE_FLAT,
                    (unsigned char *)SPY + SPYB_VOICE_MARKER + 1);
    g_pSpyroHomingTargetActor = 0;
    {
      int t = (g_nSpyroBodyYaw - g_nCameraEulerYaw) & 0xFFF;

      if (t > 0x800) {
        t -= 0x1000;
      }
      if (abs(t) <= 0x200) {
        break;
      }
    }
    actor = g_pActorListBase;
    best = 0x7530;
    if (actor < (unsigned char *)g_pActorPoolDynBase) {
      int *from = me;

      do {
        if (((Actor *)actor)->mode < 0x7F && ((Actor *)actor)->collision != 0) {
          int a;
          int da;

          SubtractVector(d, &((Actor *)actor)->posX, from);
          a = (ArcTan2(d[0], d[1], 1) - g_nSpyroBodyYaw) & 0xFFF;
          if (a > 0x800) {
            a -= 0x1000;
          }
          da = abs(a);
          if (da < 0x200) {
            int x = d[0];
            int y = d[1];
            int z = d[2];

            x = abs(x) + abs(y) + abs(z);
            if (x < 0x4000) {
              x = VectorLength(d, VEC_LEN_XYZ);
              if (x < 0x1800) {
                x += da * 4;
                if (x < best) {
                  best = x;
                  g_pSpyroHomingTargetActor = actor;
                }
              }
            }
          }
        }
      next:
        actor += ACTOR_SIZE;
      } while (actor < (unsigned char *)g_pActorPoolDynBase);
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
      v[2] = (short)g_anSineLut[ang] >> 7;
      RotateVectorByMatrix(g_anSpyroBodyMtx, v, v);
      PARTICLE_SPAWN(1, EMIT_DUST_PUFF, v, 1);
      ang = (ang + 0x40) & 0xFF;
    }
    SPY[SPY_SUBSTATE] = 0;
    ZeroVector(&SPY[SPY_CARRY]);
    SubtractVector(&SPY[SPY_VELOCITY], &SPY[SPY_CARRY], &SPY[SPY_VELOCITY]);
    RShiftVector3(&SPY[SPY_VELOCITY], 2);
    g_nSpyroGravityStep = -0x300;
    g_nSpyroGravityEnableFlag = 1;
    if (g_nPulseRumbleAmount < 0x78) {
      g_nPulseRumbleAmount = 0x78;
    }
    if (g_nPulseRumbleTimer < 0xF) {
      g_nPulseRumbleTimer = 0xF;
    }
    break;
  }
  case SPYRO_ST_TEETER:
    g_nSpyroGravityStep = -0xC00;
    goto stand;
  case SPYRO_ST_HURT_TINT: {
    int len;

    SPY[SPY_SUBSTATE] = 0;
    ZeroVector(&SPY[SPY_VELOCITY]);
    SubtractVector(&SPY[SPY_VELOCITY], &SPY[SPY_VELOCITY], &SPY[SPY_MOTION]);
    len = VectorLength(&SPY[SPY_VELOCITY], VEC_LEN_XYZ);
    if (len > 0x1E00) {
      ScaleVector3ByRatio(&SPY[SPY_VELOCITY], len, 0x1E00);
    }
    g_nSpyroGravityStep = -0x300;
    if (g_nSpyroAirborneFrames == 0) {
      ApplyGravityAlongSpyroSlopeNormal();
    }
    g_nSpyroGravityEnableFlag = 1;
    PARTICLE_SPAWN(5, EMIT_SPYRO_ORBIT_SPARKLE, 0, 0);
    if (g_nVibrationLevel < 0x2D) {
      g_nVibrationLevel = 0x2D;
    }
    break;
  }
  case SPYRO_ST_HURT: {
    int len;

    SPY[SPY_SUBSTATE] = 0;
    ZeroVector(&SPY[SPY_VELOCITY]);
    SubtractVector(&SPY[SPY_VELOCITY], &SPY[SPY_VELOCITY], &SPY[SPY_MOTION]);
    len = VectorLength(&SPY[SPY_VELOCITY], VEC_LEN_XYZ);
    if (len > 0x1E00) {
      ScaleVector3ByRatio(&SPY[SPY_VELOCITY], len, 0x1E00);
    }
    g_nSpyroGravityStep = -0x300;
    if (g_nSpyroAirborneFrames == 0) {
      ApplyGravityAlongSpyroSlopeNormal();
    }
    g_nSpyroGravityEnableFlag = 1;
    PARTICLE_SPAWN(5, EMIT_SPYRO_ORBIT_SPARKLE, 0, 0);
    if (g_nHitRumbleTimer < 0xF) {
      g_nHitRumbleTimer = 0xF;
    }
    PlaySoundEffect(LEVEL_SFX(SFX_SPYRO_HURT), SPY, SFX_MODE_FLAT,
                    (unsigned char *)SPY + SPYB_VOICE_MARKER);
    break;
  }
  case SPYRO_ST_HURT_PINNED:
    SPY[SPY_SUBSTATE] = 0;
    ZeroVector(&SPY[SPY_VELOCITY]);
    g_anSpyroVelocity[2] = -0xC00;
    g_nSpyroGravityEnableFlag = 1;
    PARTICLE_SPAWN(5, EMIT_SPYRO_ORBIT_SPARKLE, 0, 0);
    if (g_nHitRumbleTimer < 0xF) {
      g_nHitRumbleTimer = 0xF;
    }
    PlaySoundEffect(LEVEL_SFX(SFX_SPYRO_HURT_PINNED), SPY, SFX_MODE_FLAT,
                    (unsigned char *)SPY + SPYB_VOICE_MARKER);
    break;
  case SPYRO_ST_HURT_LONG:
    SPY[SPY_SUBSTATE] = 0;
    ZeroVector(&SPY[SPY_VELOCITY]);
    g_nSpyroGravityStep = -0x300;
    g_nSpyroGravityEnableFlag = 1;
    PARTICLE_SPAWN(5, EMIT_SPYRO_ORBIT_SPARKLE, 0, 0);
    if (g_nHitRumbleTimer < 0xF) {
      g_nHitRumbleTimer = 0xF;
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
      g_nSpyroGravityEnableFlag = 0;
      break;
    }
    g_nSpyroSubstate = 0;
    g_nSpyroGlideEnteredLatch = 1;
    g_nSpyroBodyYawStep = 0;
    g_nSpyroPitchRateAccum = 0;
    g_nSpyroTurnRateAccum = 0;
    g_nSpyroMotionAux = 0;
    if (g_nSpyroMotionZ > 0) {
      g_nSpyroMotionZ = 0;
    }
    g_nSpyroMotionSpeed =
        VectorLength(&st[SPY_VELOCITY - SPY_STATE], VEC_LEN_XY);
    if (g_nSpyroMotionSpeed > 0x1900) {
      g_nSpyroMotionSpeed = 0x1900;
    }
    if (g_nSpyroMotionSpeed < 0x780) {
      g_nSpyroMotionSpeed = 0x780;
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
    g_nSpyroGravityStep = -0x80;
    g_nSpyroGravityEnableFlag = 0;
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
      g_nSpyroScriptedLiftTurn =
          ((((Actor *)g_pScriptedCameraActor)->rotZ << 4) - g_nSpyroBodyYaw -
           g_nSpyroScriptedLiftTurn) &
          0xFFF;
      if (g_nSpyroScriptedLiftTurn > 0x800) {
        g_nSpyroScriptedLiftTurn -= 0x1000;
      }
    } else {
      CopyVector(&SPY[SPY_VELOCITY], &SPY[SPY_MOTION]);
    }
    g_nSpyroGravityStep = 0x80;
    g_nSpyroGravityEnableFlag = 0;
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
    t = g_abSpyroAnimDescTable[state * 4 + 2];
    g_nSpyroGravityStep = -0xC00;
    g_nSpyroIdleAnimTimeout = t << 4;
    break;
  }
  case SPYRO_ST_SLIDE:
    SPY[SPY_SUBSTATE] = 0;
    CopyVector(&SPY[SPY_CARRY], &SPY[SPY_MOTION]);
    if (g_nSpyroCarryMotionZ > 0) {
      g_nSpyroCarryMotionZ = 0;
    }
    g_nSpyroGravityStep = -0x200;
    ApplyGravityAlongSpyroSlopeNormal();
    ApplyGravityAlongSpyroSlopeTangent();
    g_nSpyroGravityEnableFlag = 0;
    break;
  case SPYRO_ST_CHARGE_AIR:
    CopyVector(g_anSpyroVelocity, g_anSpyroVelocity + 6);
    CopyVector(g_anSpyroVelocity - 3, g_anSpyroVelocity + 6);
    g_nSpyroBodyYawStep = 0;
    if (g_nSpyroChargeHoldFlag != 0) {
      if (!(g_nSpyroSubstate & SPYRO_SUB_CHARGE_LATCHED)) {
        LATCH_CHARGE_SPEED(g_anSpyroVelocity - 3);
      }
      SPY[SPY_SUBSTATE] |= SPYRO_SUB_CHARGE_LATCHED;
    } else {
      g_nSpyroSubstate = 0;
      g_anSpyroVelocity[2] = 0;
      g_anSpyroCarryMotionVec[2] = 0;
    }
    g_nSpyroGravityStep = -0x240;
    g_nSpyroJumpHoldLatch = 0;
    g_nSpyroGravityEnableFlag = 0;
    break;
  case SPYRO_ST_SUPERCHARGE_AIR:
    CopyVector(g_anSpyroVelocity, g_anSpyroVelocity + 6);
    CopyVector(g_anSpyroVelocity - 3, g_anSpyroVelocity + 6);
    g_nSpyroBodyYawStep = 0;
    g_nSpyroSubstate = SPYRO_SUB_CHARGE_LATCHED | SPYRO_SUB_SUPERCHARGE;
    g_nSpyroGravityStep = 0;
    g_nSpyroJumpHoldLatch = 0;
    break;
  case SPYRO_ST_SCRIPTED_WALK:
    SPY[SPY_SUBSTATE] = 0;
    if ((unsigned)(((ArcTan2(SPY[SPY_MOTION], g_anSpyroMotionVec[1], 1) -
                     g_nSpyroBodyYaw) &
                    0xFFF) -
                   0x401) < 0x7FF) {
      g_nSpyroMotionSpeed = 0;
    } else {
      g_nSpyroMotionSpeed = VectorLength(&SPY[SPY_MOTION], VEC_LEN_XY);
    }
    g_nSpyroGravityStep = -0xC00;
    ApplyGravityAlongSpyroSlopeNormal();
    g_nSpyroMoveTargetMag = 0x280;
    g_nSpyroTurnRateAccum = 0;
    g_nSpyroGravityEnableFlag = 1;
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
    ArcTan2(SPY[SPY_VELOCITY], g_anSpyroVelocity[1], 1);
    g_nSpyroGravityStep = -0x300;
    if (g_nSpyroAirborneFrames == 0) {
      ApplyGravityAlongSpyroSlopeNormal();
    }
    g_nSpyroGravityEnableFlag = 1;
    PARTICLE_SPAWN(5, EMIT_SPYRO_ORBIT_SPARKLE, 0, 0);
    PlaySoundEffect(LEVEL_SFX(SFX_SPYRO_HURT), SPY, SFX_MODE_FLAT,
                    (unsigned char *)SPY + SPYB_VOICE_MARKER);
    if (g_nHitRumbleTimer < 0xF) {
      g_nHitRumbleTimer = 0xF;
    }
    break;
  case SPYRO_ST_HAZARD: {
    int *p = &SPY[SPY_SUBSTATE];
    int *me;

    *p = 0;
    g_nSpyroGravityStep = -0x80;
    g_nSpyroGravityEnableFlag = 1;
    if (g_nSpyroInputLockoutCountdown == 0) {
      PARTICLE_SPAWN(5, EMIT_SPYRO_ORBIT_SPARKLE, 0, 0);
    }
    ((void (*)(int, int))g_pfnLevelSpawnActor)(
        g_anHazardEnterSpawnType[g_nSpyroHazardKind], 0);
    me = SPY;
    PlaySoundEffect(LEVEL_SFX(SFX_SPLASH), me, SFX_MODE_FLAT,
                    (unsigned char *)SPY + SPYB_VOICE_MARKER);
    PlaySoundEffect(LEVEL_SFX(SFX_SPYRO_HURT_EXIT), me, SFX_MODE_FLAT,
                    (unsigned char *)SPY + SPYB_VOICE_MARKER);
    if (g_nHitRumbleTimer < 0xF) {
      g_nHitRumbleTimer = 0xF;
    }
    break;
  }
  case SPYRO_ST_DEATH:
    SPY[SPY_SUBSTATE] = 0;
    CopyVector(&SPY[SPY_CARRY], &SPY[SPY_MOTION]);
    g_nSpyroGravityStep = -0x300;
    if (g_nSpyroAirborneFrames == 0) {
    slope_gravity:
      ApplyGravityAlongSpyroSlopeNormal();
    }
    g_nSpyroGravityEnableFlag = 1;
    break;
  case SPYRO_ST_DEATH_PINNED:
    SPY[SPY_SUBSTATE] = 0;
    CopyVector(&SPY[SPY_CARRY], &SPY[SPY_MOTION]);
    g_nSpyroGravityStep = -0x300;
    if (g_nSpyroAirborneFrames == 0) {
      ApplyGravityAlongSpyroSlopeNormal();
    }
    g_nSpyroGravityEnableFlag = 1;
    PARTICLE_SPAWN(5, EMIT_SPYRO_ORBIT_SPARKLE, 0, 0);
    break;
  case SPYRO_ST_SCRIPTED_HOLD:
    g_nSpyroGravityStep = -0x80;
    /* FALLTHROUGH */
  case SPYRO_ST_08:
  stand:
    g_nSpyroSubstate = 0;
    g_nSpyroGravityEnableFlag = 1;
    break;
  case SPYRO_ST_SUPERCHARGE:
    CopyVector(g_anSpyroCarryMotionVec, g_anSpyroCarryMotionVec + 9);
    if (!(g_nSpyroSubstate & SPYRO_SUB_CHARGE_LATCHED)) {
      LATCH_CHARGE_SPEED(g_anSpyroCarryMotionVec);
    }
    SPY[SPY_SUBSTATE] = SPYRO_SUB_CHARGE_LATCHED | SPYRO_SUB_SUPERCHARGE;
    g_nSpyroBodyYawStep = 0;
    g_nSpyroGravityStep = -0x240;
    ApplyGravityAlongSpyroSlopeNormal();
    ApplyGravityAlongSpyroSlopeTangent();
    g_nSpyroGravityEnableFlag = 1;
    g_nSpyroMotionAux = 0;
    PlaySoundEffect(LEVEL_SFX(SFX_SUPERCHARGE), SPY, SFX_MODE_FLAT,
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
