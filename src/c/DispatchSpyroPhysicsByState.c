#include "camera.h"
#include "funcs.h"
#include "globals.h"
#include "level_id.h"
#include "main_shared.h"
#include "pad_buttons.h"
#include "spyro_block.h"
#include "spyro_state.h"

typedef void (*ParticleSpawnFn)(int kind, int type, int *pos, int *vel);

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
   damage. Afterwards the swim state settles the water floor height. */
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

      ang = (g_nSpyroBodyYaw -
             ArcTan2(g_anSpyroGroundNormal[0], g_anSpyroGroundNormal[1], 1)) &
            0xFFF;
      if (ang > 0x800) {
        ang -= 0x1000;
      }
      t = (SPY[SPY_GROUND_SLOPE] << 12) / 22;
      ang = abs(ang);
      if (ang > 0x400) {
        a = g_anSpyroSlopeSpeedTargets[2] +
            ((t * (g_anSpyroSlopeSpeedTargets[5] -
                   g_anSpyroSlopeSpeedTargets[2])) >>
             12);
        ang = 0x800 - ang;
      } else {
        a = g_anSpyroSlopeSpeedTargets[0] +
            ((t * (g_anSpyroSlopeSpeedTargets[3] -
                   g_anSpyroSlopeSpeedTargets[0])) >>
             12);
      }
      b = g_anSpyroSlopeSpeedTargets[1] +
          ((t *
            (g_anSpyroSlopeSpeedTargets[4] - g_anSpyroSlopeSpeedTargets[1])) >>
           12);
      a = (a * LookupCosine(ang)) >> 12;
      b = (b * LookupSine(ang)) >> 12;
      g_nSpyroMoveTargetMag = IntegerSqrt(a * a + b * b);
    } else {
      g_nSpyroMoveTargetMag = g_anSpyroSlopeSpeedTargets[0];
    }
    if (SPY[SPY_STATE] == SPYRO_ST_STOP) {
      CopyVector(puff, &SPY[SPY_MOTION]);
      RShiftVector3(puff, 6);
      ((ParticleSpawnFn)g_pfnLevelEmitParticles)(1, 0x21, puff, 0);
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
    ((ParticleSpawnFn)g_pfnLevelEmitParticles)(1, 0x21, dir, 0);
    break;
  case SPYRO_ST_HURT_TINT: {
    unsigned char frame;

    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    ApplyGravityAlongSpyroSlopeNormal();
    frame = g_bSpyroAnimFrame;
    if (frame < 10) {
      g_abSpyroFxParticleRgba[3] = 0xFF;
      if (frame & 1) {
        g_abSpyroFxParticleRgba[2] = 0xFF;
        g_abSpyroFxParticleRgba[1] = 0xFF;
        g_abSpyroFxParticleRgba[0] = 0xFF;
      } else {
        g_abSpyroFxParticleRgba[2] = 0x40;
        g_abSpyroFxParticleRgba[1] = 0x40;
        g_abSpyroFxParticleRgba[0] = 0x40;
      }
    } else {
      g_abSpyroFxParticleRgba[3] = 0;
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
        if (g_nSpyroSubstate == 3 &&
            CollideSphereWithWorldAndActors(&SPY[0], 0x164, 0x164) != 0) {
          ((void (*)(void))g_pfnLevelFallImpactCallback)();
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
      g_nSpyroGroundHeightZ = FindGroundHeightBelow(&SPY[0], 0x10000);
    }
    break;
  case SPYRO_ST_08:
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
      CopyVector(dust, &SPY[0]);
      dust[0] += (GetRandomU32() & 0xFE) - 0x7F;
      dust[1] += (GetRandomU32() & 0xFE) - 0x7F;
      CopyVector(dustVel, &SPY[SPY_MOTION]);
      RShiftVector3(dustVel, 6);
      ((ParticleSpawnFn)g_pfnLevelEmitParticles)(1, 9, dust, dustVel);
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
    int *timer;
    int n;

    timer = &SPY[SPY_HAZARD_SINK];
    if (*timer != 0) {
      n = *timer + 8;
      *timer = n;
      g_nSpyroWorldPosZ = g_nSpyroTriggerEntryZ + 0x164;
      if (g_nSparxHealth >= 0) {
        if (n > 0x96) {
          *timer = 0x96;
        }
      } else if (n > 0x240) {
        *timer = 0x240;
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
    CopyVector(trail, &SPY[0]);
    trail[0] += (GetRandomU32() & 0xFE) - 0x7F;
    trail[1] += (GetRandomU32() & 0xFE) - 0x7F;
    trail[2] -= 0x164;
    CopyVector(trailVel, &SPY[SPY_MOTION]);
    RShiftVector3(trailVel, 6);
    ((ParticleSpawnFn)g_pfnLevelEmitParticles)(1, 9, trail, trailVel);
    break;
  }
  if (SPY[SPY_STATE] == SPYRO_ST_HAZARD) {
    g_nSpyroWorldPosZ -= g_nSpyroHazardSinkDepth;
    if (g_nSpyroGroundHeightZ != 0) {
      g_nSpyroHazardFloorZ = g_nSpyroGroundHeightZ;
    } else {
      CopyVector(pos, &SPY[0]);
      pos[2] += 0x800;
      g_nSpyroHazardFloorZ = FindGroundHeightBelow(pos, 0xC00);
    }
  } else {
    g_nSpyroHazardFloorZ = 0;
  }
}
