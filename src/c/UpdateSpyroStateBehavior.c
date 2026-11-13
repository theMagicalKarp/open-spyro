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
