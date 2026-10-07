#include "globals.h"

extern void AverageSpyroPlatformMotion(void);
extern void AdvanceSpyroPhysics(void);
extern void ProbeSpyroGroundContact(void);
extern void StepSpyroOntoLocalFloor(void);
extern void ApplyGravityAlongSpyroSlopeNormal(void);
extern void ApplyGravityAlongSpyroSlopeTangent(void);
extern void ProbeSpyroLedgeForward(void);
extern int ArcTan2(int y, int x, int high_precision);
extern int LookupCosine(int angle);
extern int LookupSine(int angle);
extern int IntegerSqrt(int n);
extern void CopyVector(int *dst, int *src);
extern void RShiftVector3(int *vec, int shift);
extern void RotateVectorByMatrix(int *mtx, int *src, int *dst);
extern int CollideSphereWithWorldAndActors(int *pos, int radius, int height);
extern int FindGroundHeightBelow(int *pos, int depth);
extern int GetRandomU32(void);
extern int abs(int x);

/* One view of the Spyro record from its world position (0x80078a58), so that
   field addresses taken in one block are derived from a single held base. */
extern int g_anSpyroWorldPosBlock[];
extern int g_nSpyroWorldPosZ; /* 0x80078a60 — g_anSpyroWorldPos[2] */

#define SPY g_anSpyroWorldPosBlock
#define SPY_STATE 0x1E         /* g_nSpyroState */
#define SPY_STATE_FLAGS 0x1F   /* g_nSpyroStateFlags */
#define SPY_GROUND_SLOPE 0x2C  /* g_nSpyroGroundSlope */
#define SPY_WALL_SLIDE 0x2D    /* g_nSpyroWallSlideContributedFlag */
#define SPY_CARRY_MOTION 0x3A  /* g_anSpyroCarryMotionVec */
#define SPY_VELOCITY 0x3D      /* g_anSpyroVelocity */
#define SPY_MOTION 0x43        /* g_anSpyroMotionVec */
#define SPY_BREATH_TIMER 0x5A  /* g_nSpyroBreathTimer */
#define SPY_FALL_TRACKING 0x93 /* g_nSpyroFallTrackingFlag */

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
    if (g_nActiveLevelId == 0x40 &&                                            \
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
  int bubble[3];
  int bubbleVel[3];

  g_nCameraSpringSuppressFlag = 0;
  switch (g_nSpyroState) {
  case 0:
  case 12:
  case 13:
  case 18:
  case 35:
  case 36:
  case 37:
  case 38:
  case 39:
  case 40:
  case 41:
  case 42:
  case 43:
    if (g_nPadDirectionalIdleFlag != 0 && (g_dwPadHeld & 0x10)) {
      g_nCameraNextMode = 0x80000009;
    }
    AverageSpyroPlatformMotion();
    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    if (g_dwSpyroRequestMask == 0) {
      StepSpyroOntoLocalFloor();
    }
    break;
  case 1:
  case 2:
  case 3:
  case 21: {
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
    if (SPY[SPY_STATE] == 3) {
      CopyVector(puff, &SPY[SPY_MOTION]);
      RShiftVector3(puff, 6);
      ((ParticleSpawnFn)g_pfnLevelOverlayParticleSpawn)(1, 0x21, puff, 0);
    }
  } break;
  case 4:
    AverageSpyroPlatformMotion();
    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    ApplyGravityAlongSpyroSlopeNormal();
    dir[0] = 0x20;
    dir[1] = 0;
    dir[2] = 0;
    RotateVectorByMatrix(g_anSpyroBodyMtx, dir, dir);
    ((ParticleSpawnFn)g_pfnLevelOverlayParticleSpawn)(1, 0x21, dir, 0);
    break;
  case 7: {
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
  case 25:
  case 27:
    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    break;
  case 5:
  case 17:
    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    if (g_nSpyroWallSlideContributedFlag == 0) {
      ProbeSpyroLedgeForward();
    }
    break;
  case 6:
  case 16:
    if (SPY[SPY_FALL_TRACKING] == 0) {
      g_nSpyroFloorReferenceZ = g_nSpyroWorldPosZ;
    }
    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    if (SPY[SPY_FALL_TRACKING] != 0) {
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
  case 23:
    if (SPY[SPY_FALL_TRACKING] == 0) {
      g_nSpyroFloorReferenceZ = g_nSpyroWorldPosZ;
    }
    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    if (SPY[SPY_FALL_TRACKING] != 0) {
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
  case 32:
  case 33:
  case 34:
    if (SPY[SPY_FALL_TRACKING] == 0) {
      g_nSpyroFloorReferenceZ = g_nSpyroWorldPosZ;
    }
    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    if (SPY[SPY_FALL_TRACKING] != 0) {
      if (g_nFlightLevelActive != 0) {
        TRACK_FLOOR_REFERENCE();
        if (g_nSpyroStateFlags == 3 &&
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
  case 15:
    if (SPY[SPY_FALL_TRACKING] == 0) {
      g_nSpyroFloorReferenceZ = g_nSpyroWorldPosZ;
    }
    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    if (SPY[SPY_FALL_TRACKING] != 0) {
      CLAMP_FALL_REFERENCE();
    }
    if (SPY[SPY_WALL_SLIDE] == 0) {
      ProbeSpyroLedgeForward();
    }
    if ((g_dwSpyroRequestMask & 0x4000) && g_nSpyroStateFlags == 9) {
      g_nSpyroGroundHeightZ = FindGroundHeightBelow(&SPY[0], 0x10000);
    }
    break;
  case 8:
    AverageSpyroPlatformMotion();
    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    StepSpyroOntoLocalFloor();
    break;
  case 11:
    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    ApplyGravityAlongSpyroSlopeNormal();
    ApplyGravityAlongSpyroSlopeTangent();
    StepSpyroOntoLocalFloor();
    if (g_nSpyroWallSlideContributedFlag == 0) {
      ProbeSpyroLedgeForward();
    }
    break;
  case 19:
    AdvanceSpyroPhysics();
    CopyVector(&SPY[SPY_CARRY_MOTION], &SPY[SPY_MOTION]);
    ProbeSpyroGroundContact();
    ApplyGravityAlongSpyroSlopeNormal();
    ApplyGravityAlongSpyroSlopeTangent();
    break;
  case 20:
  case 24:
    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    if (SPY[SPY_STATE_FLAGS] & 0x40) {
      CopyVector(dust, &SPY[0]);
      dust[0] += (GetRandomU32() & 0xFE) - 0x7F;
      dust[1] += (GetRandomU32() & 0xFE) - 0x7F;
      CopyVector(dustVel, &SPY[SPY_MOTION]);
      RShiftVector3(dustVel, 6);
      ((ParticleSpawnFn)g_pfnLevelOverlayParticleSpawn)(1, 9, dust, dustVel);
    }
    break;
  case 9:
  case 10:
  case 14:
  case 22:
  case 26:
  case 28:
  case 30:
  case 31:
    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    ApplyGravityAlongSpyroSlopeNormal();
    break;
  case 29: {
    int *timer;
    int n;

    timer = &SPY[SPY_BREATH_TIMER];
    if (*timer != 0) {
      n = *timer + 8;
      *timer = n;
      g_nSpyroWorldPosZ = g_nSpyroTriggerEntryZ + 0x164;
      if (g_nLevelReadyFlag >= 0) {
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
  case 44:
    AdvanceSpyroPhysics();
    ProbeSpyroGroundContact();
    ApplyGravityAlongSpyroSlopeNormal();
    ApplyGravityAlongSpyroSlopeTangent();
    CopyVector(bubble, &SPY[0]);
    bubble[0] += (GetRandomU32() & 0xFE) - 0x7F;
    bubble[1] += (GetRandomU32() & 0xFE) - 0x7F;
    bubble[2] -= 0x164;
    CopyVector(bubbleVel, &SPY[SPY_MOTION]);
    RShiftVector3(bubbleVel, 6);
    ((ParticleSpawnFn)g_pfnLevelOverlayParticleSpawn)(1, 9, bubble, bubbleVel);
    break;
  }
  if (SPY[SPY_STATE] == 0x1D) {
    g_nSpyroWorldPosZ -= g_nSpyroBreathTimer;
    if (g_nSpyroGroundHeightZ != 0) {
      g_nSpyroSwimWaterFloorZ = g_nSpyroGroundHeightZ;
    } else {
      CopyVector(pos, &SPY[0]);
      pos[2] += 0x800;
      g_nSpyroSwimWaterFloorZ = FindGroundHeightBelow(pos, 0xC00);
    }
  } else {
    g_nSpyroSwimWaterFloorZ = 0;
  }
}
