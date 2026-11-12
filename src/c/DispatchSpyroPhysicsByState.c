#include "camera.h"
#include "emit_record.h"
#include "funcs.h"
#include "globals.h"
#include "level_id.h"
#include "main_shared.h"
#include "pad_buttons.h"
#include "spyro_block.h"
#include "spyro_state.h"
#include "vecmath.h"

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
