#include "camera.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "match_asm.h"
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

#define PARTICLE_SPAWN(a, b, c, d)                                             \
  ((void (*)(int, int, int *, int))g_pfnLevelEmitParticles)(a, b, c, d)

/* Landing: pick the ground state from the speed target and current speed. */
#define LAND_ON_GROUND()                                                       \
  if (g_nSpyroSpeedTarget > 0) {                                               \
    if (g_nSpyroMotionSpeed > 0xF00) {                                         \
      ChangeSpyroState(SPYRO_ST_RUN);                                          \
    } else if (g_nSpyroMotionSpeed > 0x780) {                                  \
      ChangeSpyroState(SPYRO_ST_JOG);                                          \
    } else {                                                                   \
      ChangeSpyroState(SPYRO_ST_WALK);                                         \
    }                                                                          \
  } else if (g_nSpyroMotionSpeed >= 0x780) {                                   \
    ChangeSpyroState(SPYRO_ST_STOP);                                           \
  } else {                                                                     \
    ResetSpyroLinearMotion();                                                  \
    ChangeSpyroState(SPYRO_ST_IDLE);                                           \
  }

/* Jump: glide-jump in flight levels and level 0x40, else the plain jump. */
#define JUMP_STATE()                                                           \
  if (g_nFlightLevelActive != 0 || g_nActiveLevelId == 0x40) {                 \
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
      PARTICLE_SPAWN(1, 0x21, v, 0);                                           \
      a = (a + 0x40) & 0xFF;                                                   \
    }                                                                          \
  }

/* Per-frame Spyro state update (0x80041670, 10612 bytes). Each state first
   lets the pad dispatcher take a transition, then runs its own decision tree
   and hands the chosen next state to ChangeSpyroState. */
void UpdateSpyroStateBehavior(void) {
  switch (g_nSpyroState) {
  case SPYRO_ST_IDLE:
    if (DispatchSpyroPadStateChanges(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_dwPadPressed & PAD_CROSS) {
      ChangeSpyroState(SPYRO_ST_JUMP);
      break;
    }
    if (g_nSpyroAirborneFrames >= 4 || g_nSpyroGroundSlope >= 0x17) {
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
    if (g_nSpyroFrameMotionLength > 0x400) {
      ChangeSpyroState(SPYRO_ST_STOP);
      g_nSpyroStateFlags = 7;
      break;
    }
    if (g_nCameraNextMode == (CAM_HELD | CAM_MODE_FREE_LOOK)) {
      break;
    }
    if (g_nPadDirectionalIdleFlag == 0) {
      ChangeSpyroState(SPYRO_ST_WALK);
      break;
    }
    if (g_nSpyroPadSnapshotCountdown != 0) {
      break;
    }
    if (g_nSpyroCliffEdgeHint != 0) {
      ChangeSpyroState(SPYRO_ST_TEETER);
      break;
    }
    if (g_nSpyroStateTimer >= g_nSpyroIdleAnimTimeout &&
        g_nSpyroHornStrikeState != 1) {
      CycleSpyroIdleAnim();
    }
    break;
  case SPYRO_ST_WALK:
    if (DispatchSpyroPadStateChanges(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_dwPadPressed & PAD_CROSS) {
      ChangeSpyroState(SPYRO_ST_JUMP);
      break;
    }
    if (g_nSpyroAirborneFrames >= 4 || g_nSpyroGroundSlope >= 0x17) {
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
        g_nSpyroStateTimer >= 0x10) {
      ChangeSpyroState(SPYRO_ST_IDLE);
      break;
    }
    if (g_nSpyroMotionSpeed > 0xF00) {
      ChangeSpyroState(SPYRO_ST_RUN);
    } else if (g_nSpyroMotionSpeed > 0x780) {
      ChangeSpyroState(SPYRO_ST_JOG);
    }
    break;
  case SPYRO_ST_RUN:
    if (DispatchSpyroPadStateChanges(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_dwPadPressed & PAD_CROSS) {
      ChangeSpyroState(SPYRO_ST_JUMP);
      break;
    }
    if (g_nSpyroAirborneFrames >= 4 || g_nSpyroGroundSlope >= 0x17) {
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
    if (AbsAngleDelta12(g_nSpyroMoveTargetYaw, g_nSpyroBodyYaw) > 0x500) {
      register int a0 asm("$4");

      REG_RESERVE(a0);
      if (g_nPadStickActiveFlag != 0) {
        int d;

        KEEP_IN_REG(a0);
        d = ((unsigned char *)g_pPadSubstepState)[0x17] - 0x7F;

        if (d < 0) {
          d = -d;
        }
        if (d > 0x30) {
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
    if (g_nSpyroMotionSpeed < 0x640) {
      ChangeSpyroState(SPYRO_ST_WALK);
      break;
    }
    if (g_nSpyroMotionSpeed < 0xC80) {
      ChangeSpyroState(SPYRO_ST_JOG);
    }
    break;
  case SPYRO_ST_STOP:
    if (DispatchSpyroPadStateChanges(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_dwPadPressed & PAD_CROSS) {
      ChangeSpyroState(SPYRO_ST_JUMP);
      break;
    }
    if (g_nSpyroAirborneFrames >= 4 || g_nSpyroGroundSlope >= 0x17) {
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
        g_nSpyroFrameMotionLength < 0x100) {
      ChangeSpyroState(SPYRO_ST_IDLE);
      break;
    }
    if (g_nSpyroSpeedTarget > 0 && g_nSpyroStateTimer < 0x12 &&
        AbsAngleDelta12(g_nSpyroMoveTargetYaw, g_nSpyroBodyYaw) > 0x500) {
      ChangeSpyroState(SPYRO_ST_TURN_AROUND);
      break;
    }
    if (g_nSpyroMotionSpeed > 0xF00 && g_nSpyroSpeedTarget > 0) {
      ChangeSpyroState(SPYRO_ST_RUN);
      break;
    }
    if (g_nSpyroMotionSpeed > 0x780 && g_nSpyroSpeedTarget > 0) {
      ChangeSpyroState(SPYRO_ST_JOG);
      break;
    }
    if (g_nSpyroMotionSpeed > 0 && g_nSpyroSpeedTarget > 0) {
      ChangeSpyroState(SPYRO_ST_WALK);
    }
    break;
  case SPYRO_ST_TURN_AROUND:
    if (DispatchSpyroPadStateChanges(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_nSpyroAirborneFrames >= 4 || g_nSpyroGroundSlope >= 0x17) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (g_nSpyroStateTimer < 0x25) {
      break;
    }
    if (g_nPadDirectionalIdleFlag != 0) {
      ChangeSpyroState(SPYRO_ST_IDLE);
    } else {
      g_nSpyroMotionSpeed = 0xA00;
      ChangeSpyroState(SPYRO_ST_WALK);
    }
    break;
  case SPYRO_ST_JUMP:
    if (DispatchSpyroPadStateChanges(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_nSpyroGroundRunFlag == 0 && (g_dwPadPressed & PAD_CROSS)) {
      g_nSpyroJumpHoldLatch = 1;
    }
    if (g_anSpyroMotionVec[2] < 0) {
      g_nSpyroStateFlags = 1;
    }
    if (g_nSpyroStateFlags == 0 && !(g_dwPadHeld & PAD_CROSS) &&
        g_bSpyroJumpHoldForced == 0) {
      g_nSpyroStateFlags = 2;
    }
    if (g_nSpyroAirborneFrames == 0 && g_nSpyroStateTimer >= 0x10) {
      if (g_nSpyroGroundSlope >= 0x17) {
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
      g_nSpyroMotionSpeed = 0x1F80;
      break;
    }
    if (g_nSpyroGroundRunFlag == 0 && g_nSpyroStateFlags == 1 &&
        g_nSpyroJumpHoldLatch != 0) {
      JUMP_STATE();
      break;
    }
    if (g_anSpyroMotionVec[2] < -0x1900 || g_nSpyroStateTimer >= 0x79) {
      ChangeSpyroState(SPYRO_ST_FALL);
    }
    break;
  case SPYRO_ST_FALL:
    if (DispatchSpyroPadStateChanges(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_dwPadPressed & PAD_CROSS) {
      if (g_nSpyroGravityEnableFlag != 0 && g_nSpyroStateTimer < 4) {
        ChangeSpyroState(SPYRO_ST_JUMP);
        break;
      }
      if ((g_dwPadPressed & PAD_CROSS) && g_nSpyroStateFlags == 0) {
        if (g_nFlightLevelActive != 0 || g_nActiveLevelId == 0x40) {
          if (g_nSpyroGroundRunFlag == 0 || g_nSpyroStateTimer >= 0x10) {
            ChangeSpyroState(SPYRO_ST_FLY);
          }
          break;
        }
        if (g_nSpyroGroundRunFlag == 0 || g_nSpyroStateTimer >= 0x1F) {
          ChangeSpyroState(SPYRO_ST_GLIDE);
        }
        break;
      }
    }
    if (g_nSpyroWorldPosZ >= g_anLevelDeathPlaneZTable[g_nLevelIntroIndex] &&
        g_nSpyroAirborneFrames == 0) {
      int next;
      int fwd[3];
      int back[3];

      if (g_nSpyroGroundSlope >= 0x17) {
        if (g_nSpyroStateTimer >= 9) {
          ChangeSpyroState(SPYRO_ST_SLIDE);
        }
        break;
      }
      if (g_nSpyroSpeedTarget > 0) {
        if (g_nSpyroMotionSpeed > 0xF00) {
          next = SPYRO_ST_RUN;
        } else {
          next = SPYRO_ST_WALK;
          if (g_nSpyroMotionSpeed > 0x780) {
            next = SPYRO_ST_JOG;
          }
        }
        ChangeSpyroState(next);
        LANDING_DUST(fwd);
      } else {
        next = SPYRO_ST_STOP;
        if (g_nSpyroMotionSpeed < 0x780) {
          ResetSpyroLinearMotion();
          next = SPYRO_ST_IDLE;
        }
        ChangeSpyroState(next);
        LANDING_DUST(back);
      }
      break;
    }
  fall_timeout:
    if (g_nSpyroStateTimer > 0x12C) {
      if (g_nFlightLevelActive != 0) {
        ((void (*)(void))g_pfnLevelFallImpactCallback)();
        ((void (*)(void))g_pfnLevelUpdateActors)();
      } else {
        g_nExtraLives++;
        TriggerRespawnOrGameOver();
      }
    }
    break;
  case SPYRO_ST_08:
    if (DispatchSpyroPadStateChanges(SPYRO_EV_MASK_NO_HAZARD)) {
      if (g_nSpyroStateTimer >= 0xD) {
        g_nSpyroBodyYaw = g_nSpyroMoveTargetYaw;
        g_nSpyroBodyPitch = -g_nSpyroBodyPitch & 0xFFF;
        g_nSpyroBodyRoll = -g_nSpyroBodyRoll & 0xFFF;
      }
      break;
    }
    if (g_nSpyroStateTimer < 0x14) {
      break;
    }
    g_nSpyroBodyYaw = g_nSpyroMoveTargetYaw;
    g_nSpyroBodyPitch = -g_nSpyroBodyPitch & 0xFFF;
    g_nSpyroBodyRoll = -g_nSpyroBodyRoll & 0xFFF;
    if (g_nPadDirectionalIdleFlag != 0) {
      ChangeSpyroState(SPYRO_ST_IDLE);
    } else {
      ChangeSpyroState(SPYRO_ST_WALK);
      g_nSpyroMotionSpeed = 0x15;
    }
    break;
  case SPYRO_ST_SIDESTEP_L1:
    if (DispatchSpyroPadStateChanges(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (SPY[SPY_AIRBORNE] >= 4 || g_nSpyroGroundSlope >= 0x17) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (g_dwPadHeld & PAD_L1) {
      break;
    }
    if (g_nSpyroStateTimer < 0x15) {
      break;
    }
    CopyVector(&SPY[SPY_VELOCITY], &SPY[SPY_CARRY]);
    g_nSpyroMotionSpeed = 0;
    ChangeSpyroState(SPYRO_ST_IDLE);
    break;
  case SPYRO_ST_SIDESTEP_R1:
    if (DispatchSpyroPadStateChanges(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (SPY[SPY_AIRBORNE] >= 4 || g_nSpyroGroundSlope >= 0x17) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (g_dwPadHeld & PAD_R1) {
      break;
    }
    if (g_nSpyroStateTimer < 0x15) {
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
    if ((g_dwPadHeld & (PAD_CROSS | PAD_SQUARE)) == 0xC0 &&
        g_nSpyroGroundSlope < 0xC) {
      ChangeSpyroState(SPYRO_ST_CHARGE_AIR);
      g_nSpyroVelocityZ += 0xDC0;
      g_nSpyroAirborneFrames++;
      break;
    }
    if (g_nSpyroAirborneFrames >= 4) {
      ChangeSpyroState(SPYRO_ST_CHARGE_AIR);
      break;
    }
    if (g_nSpyroGroundSlope >= 0x17) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (!(g_dwPadHeld & PAD_SQUARE)) {
      ChangeSpyroState(SPYRO_ST_RUN);
      g_nSpyroStateTimer = g_nSpyroPrevStateTimer * 2;
      break;
    }
    if (g_nSpyroMotionSpeed > 0xC80 &&
        g_nSpyroFrameMotionLength * 2 < g_nSpyroMotionSpeed &&
        g_nScriptedRespawnFlag == 0) {
      ChangeSpyroState(SPYRO_ST_CHARGE_BONK);
      break;
    }
    if (SPY[SPY_CHARGE_HOLD] != 0) {
      if (g_nSpyroMotionSpeed > 0x3000) {
        ChangeSpyroState(SPYRO_ST_SUPERCHARGE);
        break;
      }
      if (!(g_nSpyroStateFlags & 0x80)) {
        int v;

        g_nSpyroFloorReferenceZ = g_nSpyroWorldPosZ;
        v = VectorLength(&SPY[SPY_CARRY], 1);
        if (v < 0x1F80) {
          v = 0x1F80;
        }
        g_nSpyroSpeedTarget = v;
        g_nSpyroStateFlags |= 0x80;
      }
    } else {
      g_nSpyroStateFlags &= ~0x80;
    }
    break;
  case SPYRO_ST_CHARGE_BONK:
    if (DispatchSpyroPadStateChanges(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_nSpyroStateTimer > 0x10 &&
        (g_nSpyroAirborneFrames != 0 || g_nSpyroGroundSlope >= 0x17)) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (g_nSpyroStateTimer >= 0x19) {
      ChangeSpyroState(SPYRO_ST_IDLE);
    }
    break;
  case SPYRO_ST_TEETER:
    if (DispatchSpyroPadStateChanges(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_dwPadPressed & PAD_CROSS) {
      ChangeSpyroState(SPYRO_ST_JUMP);
      break;
    }
    if (g_nSpyroAirborneFrames >= 4 || g_nSpyroGroundSlope >= 0x17) {
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
    if (g_nSpyroPadSnapshotCountdown != 0) {
      ChangeSpyroState(SPYRO_ST_IDLE);
      break;
    }
    if (g_nSpyroStateTimer < 0x1F) {
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
    if (g_nSpyroStateTimer < 0x18) {
      break;
    }
    if (g_nSparxHealth < 0) {
      ChangeSpyroState(SPYRO_ST_DEATH);
      break;
    }
    goto land;
  case SPYRO_ST_HURT:
  case SPYRO_ST_KNOCKBACK:
  case SPYRO_ST_KNOCKBACK_2:
    if (g_nSpyroStateTimer < 0x18) {
      break;
    }
    if (g_nSparxHealth < 0) {
      ChangeSpyroState(SPYRO_ST_DEATH);
      break;
    }
    goto land;
  case SPYRO_ST_HURT_LONG:
    if (g_nSpyroStateTimer < 0x30) {
      break;
    }
    if (g_nSparxHealth < 0) {
      ChangeSpyroState(SPYRO_ST_DEATH);
      break;
    }
    goto land;
  case SPYRO_ST_GLIDE:
    if (DispatchSpyroPadStateChanges(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_nSpyroAirborneFrames == 0) {
      if (g_nSpyroGroundSlope >= 0x17) {
        ChangeSpyroState(SPYRO_ST_FALL);
      } else {
        LAND_ON_GROUND();
      }
      break;
    }
    if (g_nSpyroGroundProbeSuspendFlag == 0 ||
        g_nSpyroFrameMotionLength < 0x400) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (g_dwPadHeld & PAD_SQUARE) {
      ChangeSpyroState(SPYRO_ST_CHARGE_AIR);
      break;
    }
    if (g_nSpyroStateTimer > 0x10 && (g_dwPadPressed & PAD_TRIANGLE)) {
      ChangeSpyroState(SPYRO_ST_GLIDE_DROP);
      break;
    }
    if (g_nSpyroFallTrackingFlag != 0) {
      int d;
      int pad;

      if (g_nSpyroStateFlags != 0) {
        break;
      }
      d = g_nSpyroWorldPosZ;
      if (d - g_nSpyroFloorReferenceZ > 0x200) {
        ChangeSpyroState(SPYRO_ST_FALL);
        break;
      }
      pad = ((int *)g_pPadSubstepState)[1];
      if (pad & 8) {
        g_nSpyroStateFlags = 1;
      } else if (pad & 4) {
        g_nSpyroStateFlags = 2;
        g_nSpyroMotionAux = d;
      }
      break;
    }
    if (g_nSpyroStateFlags == 0 &&
        g_nSpyroFloorReferenceZ + 0x18 < g_nSpyroWorldPosZ) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (g_nSpyroMotionSpeed < 0xC80 && g_nSpyroStateTimer > 0x10) {
      ChangeSpyroState(SPYRO_ST_GLIDE_SLOW);
    }
    break;
  case SPYRO_ST_LIFT:
    DispatchSpyroPadStateChanges(SPYRO_EV_MASK_DEFAULT);
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

    if (DispatchSpyroPadStateChanges(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_dwPadPressed & PAD_CROSS) {
      ChangeSpyroState(SPYRO_ST_JUMP);
      break;
    }
    air = Opaque(&SPY[SPY_AIRBORNE]);
    if (*air >= 4 || g_nSpyroGroundSlope >= 0x17) {
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
    if (g_nSpyroFrameMotionLength > 0x400) {
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
      if (g_nSpyroPadSnapshotCountdown == 0) {
        unsigned char *b = (unsigned char *)air;

        if (b[SPYB_ANIM_CURRENT - 4 * SPY_AIRBORNE] ==
                g_abSpyroStateAnimIndexMap[g_nSpyroState] &&
            b[SPYB_ANIM_FRAME - 4 * SPY_AIRBORNE] >=
                g_abSpyroAnimDescTable[b[SPYB_ANIM_CURRENT - 4 * SPY_AIRBORNE] *
                                           4 +
                                       1] -
                    1) {
          ChangeSpyroState(SPYRO_ST_IDLE);
          break;
        }
        if (g_nSpyroHornStrikeState != 1) {
          break;
        }
      }
    }
    RestartSpyroAnimWithState(SPYRO_ST_IDLE);
    break;
  }
  case SPYRO_ST_GLIDE_DROP:
    if (DispatchSpyroPadStateChanges(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_nSpyroWorldPosZ >= g_anLevelDeathPlaneZTable[g_nLevelIntroIndex] &&
        g_nSpyroAirborneFrames == 0) {
      if (g_nSpyroStateTimer >= 0x18 || g_nSpyroGroundSlope >= 0x17) {
        ChangeSpyroState(SPYRO_ST_FALL);
        g_nSpyroStateTimer = g_nSpyroPrevStateTimer;
      } else {
        ChangeSpyroState(SPYRO_ST_IDLE);
      }
      if (SPY[SPY_COUNTDOWN] < 8) {
        SPY[SPY_COUNTDOWN] = 8;
      }
      ResetSpyroLinearMotion();
    }
    if (g_dwPadPressed & PAD_CROSS) {
      if (g_nSpyroGroundRunFlag == 0 || g_nSpyroStateTimer >= 0x10) {
        ChangeSpyroState(SPYRO_ST_GLIDE);
      }
      break;
    }
    goto fall_timeout;
  case SPYRO_ST_SLIDE:
    if (DispatchSpyroPadStateChanges(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if ((g_dwPadHeld & PAD_CROSS) && g_nSpyroStateTimer < 4) {
      ChangeSpyroState(SPYRO_ST_JUMP);
      break;
    }
    if ((g_dwPadHeld & PAD_SQUARE) && g_nSpyroMotionSpeed > 0x1F00) {
      ChangeSpyroState(SPYRO_ST_CHARGE_AIR);
      break;
    }
    if (g_nSpyroAirborneFrames >= 8) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (g_nSpyroGroundSlope < 0x17 &&
        g_bSpyroAnimFrame == g_abSpyroAnimDescTable[0x51] - 1 &&
        g_nSpyroFrameMotionLength < 0x900) {
      LAND_ON_GROUND();
      break;
    }
    if (g_nSpyroStateTimer > 0x12C) {
      if (g_nFlightLevelActive != 0) {
        ((void (*)(void))g_pfnLevelFallImpactCallback)();
        ((void (*)(void))g_pfnLevelUpdateActors)();
      } else {
        g_nExtraLives++;
        TriggerRespawnOrGameOver();
      }
    }
    break;
  case SPYRO_ST_CHARGE_AIR:
    if (g_nSpyroGroundRunFlag == 0 && (g_dwPadPressed & PAD_CROSS)) {
      g_nSpyroJumpHoldLatch = 1;
    }
    if (!(g_nSpyroStateFlags & 0x40) && DispatchSpyroChargeStateChanges()) {
      break;
    }
    if (g_nSpyroGroundSlope >= 0x17 && g_nScriptedRespawnFlag == 0 &&
        g_anSpyroVelocity[0] * g_anSpyroGroundNormal[0] +
                g_anSpyroVelocity[1] * g_anSpyroGroundNormal[1] <
            0) {
      ChangeSpyroState(SPYRO_ST_CHARGE_BONK);
      break;
    }
    if (g_nSpyroContactedActorFlag == 0 && g_nSpyroMotionSpeed > 0xC80 &&
        g_nSpyroFrameMotionLength * 2 < g_nSpyroMotionSpeed &&
        g_nScriptedRespawnFlag == 0) {
      BuildSpyroVelocityFromBodyEuler();
      ChangeSpyroState(SPYRO_ST_CHARGE_BONK);
      break;
    }
    if (g_nSpyroAirborneFrames == 0) {
      if (g_nSpyroStateFlags & 0x40) {
        ChangeSpyroState(SPYRO_ST_SUPERCHARGE);
      } else {
        ChangeSpyroState(SPYRO_ST_CHARGE);
      }
      break;
    }
    if (g_anSpyroVelocity[2] < 0 && g_nSpyroJumpHoldLatch != 0) {
      JUMP_STATE();
    }
    break;
  case SPYRO_ST_JOG:
    if (DispatchSpyroPadStateChanges(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_dwPadPressed & PAD_CROSS) {
      ChangeSpyroState(SPYRO_ST_JUMP);
      break;
    }
    if (g_nSpyroAirborneFrames >= 4 || g_nSpyroGroundSlope >= 0x17) {
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
    if (g_nSpyroMotionSpeed > 0xF00) {
      ChangeSpyroState(SPYRO_ST_RUN);
    } else if (g_nSpyroMotionSpeed < 0x640) {
      ChangeSpyroState(SPYRO_ST_WALK);
    }
    break;
  case SPYRO_ST_GLIDE_SLOW:
    if (DispatchSpyroPadStateChanges(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_nSpyroAirborneFrames == 0) {
      if (g_nSpyroGroundSlope >= 0x17) {
        ChangeSpyroState(SPYRO_ST_FALL);
      } else {
        LAND_ON_GROUND();
      }
      break;
    }
    if (g_nSpyroGroundProbeSuspendFlag == 0 ||
        g_nSpyroFrameMotionLength < 0x400) {
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
    if (g_nSpyroFloorReferenceZ + 0x18 < g_nSpyroWorldPosZ) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (g_nSpyroMotionSpeed > 0xC80) {
      JUMP_STATE();
    }
    break;
  case SPYRO_ST_SUPERCHARGE_AIR: {
    int t;

    if (g_nSpyroStateTimer >= 4) {
      g_nSpyroGravityEnableFlag = 0;
    }
    if ((g_dwPadHeld & PAD_CROSS) && g_nSpyroGravityEnableFlag != 0) {
      g_nSpyroGravityEnableFlag = 0;
      g_nSpyroVelocityZ += 0xDC0;
    } else if ((g_dwPadPressed & PAD_CROSS) && g_nSpyroGroundRunFlag == 0) {
      g_nSpyroJumpHoldLatch = 1;
    }
    if (g_nSpyroGroundSlope >= 0x17 && g_nScriptedRespawnFlag == 0) {
      t = g_anSpyroVelocity[0] * g_anSpyroGroundNormal[0] +
          g_anSpyroVelocity[1] * g_anSpyroGroundNormal[1];
      if (t < 0) {
        ChangeSpyroState(SPYRO_ST_CHARGE_BONK);
        break;
      }
    }
    t = VectorLength(g_anSpyroVelocity, 1);
    if (t == 0) {
      t = 1;
    }
    t = (g_nSpyroFrameMotionLength << 12) / t;
    if (g_nSpyroContactedActorFlag == 0 && t < 0x800 &&
        g_nScriptedRespawnFlag == 0) {
      ChangeSpyroState(SPYRO_ST_CHARGE_BONK);
      break;
    }
    if (g_nSpyroAirborneFrames == 0) {
      ChangeSpyroState(SPYRO_ST_CHARGE);
      break;
    }
    if (g_anSpyroVelocity[2] < 0) {
      if (g_nSpyroJumpHoldLatch == 0) {
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
    if (g_nSpyroStateTimer < 0x18) {
      break;
    }
    if (g_nSparxHealth < 0) {
      ChangeSpyroState(SPYRO_ST_DEATH);
      break;
    }
  land: {
    int next = SPYRO_ST_FALL;

    if (g_nSpyroAirborneFrames == 0) {
      next = OpaqueInt(SPYRO_ST_IDLE);
    }
    ChangeSpyroState(next);
    if (SPY[SPY_COUNTDOWN] < 0xC) {
      SPY[SPY_COUNTDOWN] = 0xC;
    }
    break;
  }
  case SPYRO_ST_SCRIPTED_WALK:
    if (DispatchSpyroPadStateChanges(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_dwPadPressed & PAD_CROSS) {
      ChangeSpyroState(SPYRO_ST_JUMP);
      break;
    }
    if (g_nSpyroAirborneFrames >= 4 || g_nSpyroGroundSlope >= 0x17) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (g_nSpyroMotionSpeed != 0 || g_nSpyroSpeedTarget != 0 ||
        g_nSpyroStateTimer < 0x10) {
      ChangeSpyroState(SPYRO_ST_WALK);
      break;
    }
    ChangeSpyroState(SPYRO_ST_IDLE);
    break;
  case SPYRO_ST_HAZARD:
    if (DispatchSpyroPadStateChanges(SPYRO_EV_MASK_NO_HAZARD)) {
      break;
    }
    if (SPY[SPY_SPARX_HEALTH] < 0) {
      break;
    }
    if (g_nSpyroStateTimer > 0x3C) {
      SPY[SPY_SPARX_HEALTH] = -1;
      ((void (*)(int, int))g_pfnLevelSpawnActor)(
          g_anOrbDropTimeoutSpawnType[g_nSpyroOrbDropSpawnIndex], 0);
      break;
    }
    if (g_nSpyroStateTimer >= 0x10 && (g_dwPadHeld & PAD_CROSS)) {
      ChangeSpyroState(SPYRO_ST_JUMP);
    }
    break;
  case SPYRO_ST_FLY:
    if (DispatchSpyroPadStateChanges(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_nSpyroAirborneFrames == 0) {
      if (g_nSpyroGroundSlope < 0x17) {
        LAND_ON_GROUND();
      } else {
        ChangeSpyroState(SPYRO_ST_FALL);
      }
    } else if (g_nSpyroGroundProbeSuspendFlag == 0 ||
               g_nSpyroFrameMotionLength < 0x400) {
      ChangeSpyroState(SPYRO_ST_FALL);
    } else if (g_anSpyroMotionVec[2] > 0x800 || (g_dwPadPressed & PAD_CROSS)) {
      ChangeSpyroState(SPYRO_ST_FLY_CLIMB);
    } else if (g_anSpyroMotionVec[2] < -0x1000) {
      ChangeSpyroState(SPYRO_ST_FLY_DIVE);
    } else if (g_nSpyroStateFlags == 0) {
      int d = g_nSpyroWorldPosZ;

      if (d - g_nSpyroFloorReferenceZ > 0x200) {
        ChangeSpyroState(SPYRO_ST_FALL);
      } else {
        int pad = ((int *)g_pPadSubstepState)[1];

        if (pad & 8) {
          g_nSpyroStateFlags = 1;
        } else if (pad & 4) {
          g_nSpyroStateFlags = 2;
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
    if (g_nSpyroStateTimer > 0x10 && (g_dwPadPressed & PAD_TRIANGLE)) {
      ChangeSpyroState(SPYRO_ST_GLIDE_DROP);
    }
    break;
  case SPYRO_ST_FLY_CLIMB:
    if (DispatchSpyroPadStateChanges(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_nSpyroAirborneFrames == 0) {
      if (g_nSpyroGroundSlope < 0x17) {
        LAND_ON_GROUND();
      } else {
        ChangeSpyroState(SPYRO_ST_FALL);
      }
    } else if (g_nSpyroGroundProbeSuspendFlag == 0 ||
               g_nSpyroFrameMotionLength < 0x400) {
      ChangeSpyroState(SPYRO_ST_FALL);
    } else if (g_nSpyroStateTimer >= 0x1F && g_anSpyroMotionVec[2] < 0x600) {
      ChangeSpyroState(SPYRO_ST_FLY);
    } else if (SPY[SPY_STATE_FLAGS] == 0) {
      int d = g_nSpyroWorldPosZ;

      if (d - g_nSpyroFloorReferenceZ > 0x200) {
        ChangeSpyroState(SPYRO_ST_FALL);
      } else {
        int pad = ((int *)g_pPadSubstepState)[1];

        if (pad & 8) {
          SPY[SPY_STATE_FLAGS] = 1;
        } else if (pad & 4) {
          SPY[SPY_STATE_FLAGS] = 2;
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
    if (g_nSpyroStateTimer > 0x10 && (g_dwPadPressed & PAD_TRIANGLE)) {
      ChangeSpyroState(SPYRO_ST_GLIDE_DROP);
    }
    break;
  case SPYRO_ST_FLY_DIVE:
    if (DispatchSpyroPadStateChanges(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_nSpyroAirborneFrames == 0) {
      if (g_nSpyroGroundSlope < 0x17) {
        LAND_ON_GROUND();
      } else {
        ChangeSpyroState(SPYRO_ST_FALL);
      }
    } else if (g_nSpyroGroundProbeSuspendFlag == 0 ||
               g_nSpyroFrameMotionLength < 0x400) {
      ChangeSpyroState(SPYRO_ST_FALL);
    } else if (g_dwPadPressed & PAD_CROSS) {
      ChangeSpyroState(SPYRO_ST_FLY_CLIMB);
    } else if (g_nSpyroStateTimer >= 0x1F && g_anSpyroMotionVec[2] >= -0xBFF) {
      ChangeSpyroState(SPYRO_ST_FLY);
    } else if (SPY[SPY_STATE_FLAGS] == 0) {
      int d = g_nSpyroWorldPosZ;

      if (d - g_nSpyroFloorReferenceZ > 0x200) {
        ChangeSpyroState(SPYRO_ST_FALL);
      } else {
        int pad = ((int *)g_pPadSubstepState)[1];

        if (pad & 8) {
          SPY[SPY_STATE_FLAGS] = 1;
        } else if (pad & 4) {
          SPY[SPY_STATE_FLAGS] = 2;
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
    if (g_nSpyroStateTimer > 0x10 && (g_dwPadPressed & PAD_TRIANGLE)) {
      ChangeSpyroState(SPYRO_ST_GLIDE_DROP);
    }
    break;
  case SPYRO_ST_SCRIPTED_HOLD:
    if (DispatchSpyroPadStateChanges(SPYRO_EV_MASK_DEFAULT)) {
      break;
    }
    if (g_dwSpyroRequestMask == 0) {
      ChangeSpyroState(SPYRO_ST_IDLE);
    }
    break;
  case SPYRO_ST_SUPERCHARGE:
    if (DispatchSpyroPadStateChanges(SPYRO_EV_MASK_SUPERCHARGE)) {
      break;
    }
    if (g_dwPadHeld & PAD_CROSS) {
      if (g_nSpyroMotionAux >= 10 || g_nSpyroAirborneFrames != 0) {
        goto jump;
      }
      g_nSpyroMotionAux += g_nFrameStep;
    }
    if (g_dwPadReleased & PAD_CROSS) {
    jump: {
      int *vz;

      ChangeSpyroState(SPYRO_ST_SUPERCHARGE_AIR);
      vz = &SPY[SPY_VELOCITY + 2];
      g_nSpyroGravityEnableFlag = 0;
      *vz += 0xDC0;
      g_nSpyroAirborneFrames++;
      break;
    }
    }
    if (g_nSpyroAirborneFrames >= 4) {
      ChangeSpyroState(SPYRO_ST_SUPERCHARGE_AIR);
      g_nSpyroGravityEnableFlag = 1;
      break;
    }
    if (g_nSpyroGroundSlope >= 0x17) {
      ChangeSpyroState(SPYRO_ST_FALL);
      break;
    }
    if (g_nSpyroStateTimer >= 0xF &&
        !(*(volatile unsigned int *)&g_dwPadHeld & PAD_SQUARE)) {
      ChangeSpyroState(SPYRO_ST_CHARGE);
      break;
    }
    if (g_nSpyroContactedActorFlag == 0 && g_nSpyroMotionSpeed > 0xC80 &&
        g_nSpyroFrameMotionLength * 2 < g_nSpyroMotionSpeed &&
        g_nScriptedRespawnFlag == 0) {
      ChangeSpyroState(SPYRO_ST_CHARGE_BONK);
      break;
    }
    if (g_nSpyroMotionSpeed < 0x1E00) {
      ChangeSpyroState(SPYRO_ST_CHARGE);
    }
    break;
  }
}
