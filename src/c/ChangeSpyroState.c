#include "globals.h"
#include "sdata.h"

extern void StopSoundVoicesByOwner(int *owner, int mask);
extern void PlaySoundEffect(int sfx, int *pos, int flags, unsigned char *voice);
extern unsigned int GetRandomU32(void);
extern int ArcTan2(int y, int x, int high_precision);
extern int VectorLength(int *vec, int include_z);
extern void ApplyGravityAlongSpyroSlopeNormal(void);
extern void ApplyGravityAlongSpyroSlopeTangent(void);
extern int LookupCosine(int angle);
extern int LookupSine(int angle);
extern int IntegerSqrt(int n);
extern void CopyVector(int *dst, int *src);
extern void ZeroVector(int *vec);
extern void SubtractVector(int *dst, int *a, int *b);
extern void RShiftVector3(int *vec, int shift);
extern void ScaleVector3ByRatio(int *vec, int num, int den);
extern void RotateVectorByMatrix(int *mtx, int *src, int *dst);
extern void NudgeSpyroFromWallProbes(void);
extern int abs(int x);

extern unsigned short g_anCosineLut[];
extern int D_80078A60;           /* g_anSpyroWorldPos[2] */
extern int g_nSpyroMotionZ;      /* g_anSpyroMotionVec[2] */
extern int g_nSpyroCarryMotionZ; /* g_anSpyroCarryMotionVec[2] */
extern int D_80075700;
extern int D_80075728;
extern int D_8006EA34[];
extern int D_800756B4;
extern int D_80075668;
extern int D_80075724;
extern int D_8007578C;
extern void *D_80075790;
extern int D_800757F0;

SDATA(D_80075668, 4);
SDATA(D_800756B4, 4);
SDATA(D_80075724, 4);
SDATA(D_8007578C, 4);
SDATA(D_80075790, 4);
SDATA(D_800757F0, 4);

/* One view of the Spyro record from its world position (0x80078a58). */
extern int g_anSpyroWorldPosBlock[];

#define SPY g_anSpyroWorldPosBlock
#define SPY_STATE 0x1E          /* g_nSpyroState */
#define SPY_FLAGS 0x1F          /* g_nSpyroStateFlags */
#define SPY_CARRY 0x3A          /* g_anSpyroCarryMotionVec */
#define SPY_VELOCITY 0x3D       /* g_anSpyroVelocity */
#define SPY_MOTION 0x43         /* g_anSpyroMotionVec */
#define SPY_SPEED 0x46          /* g_nSpyroMotionSpeed */
#define SPY_BODY_YAW_STEP 0x52  /* g_nSpyroBodyYawStep */
#define SPY_SCRIPTED_ACTOR 0x89 /* g_pScriptedCameraActor */
#define SPY_VOICE 0x2A0         /* g_bSpyroSfxVoiceMarker (byte offset) */

#define SFX(off) (((unsigned char *)g_pLevelSampleBankHeader)[off])
#define PARTICLE_SPAWN(a, b, c, d)                                             \
  ((void (*)(int, int, int *, int))g_pfnLevelOverlayParticleSpawn)(a, b, c, d)

/* Charge-speed latch: the first charge frame takes the carried speed, at
   least 0x1F80, as the speed target. */
#define LATCH_CHARGE_SPEED(vec)                                                \
  {                                                                            \
    int v;                                                                     \
                                                                               \
    g_nSpyroFloorReferenceZ = D_80078A60;                                      \
    v = VectorLength(vec, 1);                                                  \
    if (v < 0x1F80) {                                                          \
      v = 0x1F80;                                                              \
    }                                                                          \
    g_nSpyroSpeedTarget = v;                                                   \
  }

/* Spyro state transition (0x8003ea68, 4960 bytes). Runs the exit side of the
   old state (stop the charge/glide loops, play the landing or splash sound),
   then the entry setup of the new one: motion carry-over, gravity step and
   enable, speed targets and sounds, the rolling charge target search (11),
   the dizzy stars (12) and the scripted-move / ledge setups. Finally swaps
   the state, restarts the state timer and loads the state's animation
   play rate. */
void ChangeSpyroState(int state) {
  int d[3];
  int v[3];

  switch (g_nSpyroState) {
  case 11:
  case 15:
  case 32:
  case 44:
    StopSoundVoicesByOwner(g_anSpyroWorldPos, 2);
    break;
  case 7:
    g_abSpyroFxParticleRgba[3] = 0;
    /* FALLTHROUGH */
  case 14:
  case 22:
  case 27:
  case 28:
    PlaySoundEffect(SFX(0x1B), g_anSpyroWorldPos, 4,
                    (unsigned char *)g_anSpyroWorldPos + SPY_VOICE);
    break;
  case 25:
    PlaySoundEffect(SFX(0x20), g_anSpyroWorldPos, 4,
                    (unsigned char *)g_anSpyroWorldPos + SPY_VOICE);
    break;
  }
  switch (state) {
  case 0: {
    int t;

    g_nSpyroStateFlags = 0;
    t = g_abSpyroIdleDelayRandTable[GetRandomU32() & 7] *
        g_abSpyroAnimDescTable[2];
    g_nSpyroGravityStep = -0x80;
    g_nSpyroGravityEnableFlag = 1;
    g_nSpyroIdleAnimTimeout = t - 2;
    break;
  }
  case 1:
  case 2:
  case 3:
  case 21: {
    int a;

    SPY[SPY_FLAGS] = 0;
    a = (ArcTan2(SPY[SPY_MOTION], g_anSpyroMotionVec[1], 1) - g_nSpyroBodyYaw) &
        0xFFF;
    if ((unsigned)(a - 0x401) < 0x7FF) {
      g_nSpyroMotionSpeed = 0;
    } else {
      g_nSpyroMotionSpeed = VectorLength(&SPY[SPY_MOTION], 0);
    }
    if (state == 3 && SPY[SPY_SPEED] > 0x1400) {
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
  case 4:
    g_nSpyroStateFlags = 0;
    g_nSpyroGravityStep = -0xC00;
    goto slope_gravity;
  case 5:
    CopyVector(g_anSpyroVelocity, g_anSpyroVelocity + 6);
    g_anSpyroCarryMotionVec[2] = 0xDC0;
    g_nSpyroGravityStep = -0xC0;
    g_nSpyroStateFlags = 0;
    g_nSpyroJumpHoldLatch = 0;
    g_nSpyroGravityEnableFlag = 0;
    g_nSpyroFloorReferenceZ = D_80078A60 + (g_anSpyroVelocity[2] >> 6);
    if (g_nSpyroState == 0x1D) {
      D_800756B4 = 1;
    } else {
      D_800756B4 = 0;
    }
    break;
  case 6:
    SPY[SPY_FLAGS] = 0;
    CopyVector(&SPY[SPY_VELOCITY], &SPY[SPY_MOTION]);
    if (g_nSpyroGroundProbeSuspendFlag == 0 && g_nSpyroGroundSlope < 0x20) {
      NudgeSpyroFromWallProbes();
    }
    g_nSpyroGravityStep = -0xC0;
    g_nSpyroJumpHoldLatch = 0;
    g_nSpyroGravityEnableFlag = 0;
    break;
  case 16:
    g_nSpyroStateFlags = 0;
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
  case 9:
  case 10:
    SPY[SPY_FLAGS] = 0;
    CopyVector(&SPY[SPY_CARRY], &SPY[SPY_MOTION]);
    g_nSpyroMotionSpeed = 0;
    g_nSpyroGravityStep = -0xC00;
    goto slope_gravity;
  case 11: {
    int best;
    unsigned char *actor;
    int *me;

    CopyVector(g_anSpyroCarryMotionVec, g_anSpyroCarryMotionVec + 9);
    if (g_nSpyroChargeHoldFlag != 0) {
      if (!(g_nSpyroStateFlags & 0x80)) {
        LATCH_CHARGE_SPEED(g_anSpyroCarryMotionVec);
      }
      g_nSpyroStateFlags = 0x81;
    } else {
      g_nSpyroStateFlags = 1;
    }
    SPY[SPY_BODY_YAW_STEP] = 0;
    g_nSpyroGravityStep = -0x240;
    me = SPY;
    ApplyGravityAlongSpyroSlopeNormal();
    ApplyGravityAlongSpyroSlopeTangent();
    g_nSpyroGravityEnableFlag = 1;
    PlaySoundEffect(SFX(4), me, 4, (unsigned char *)SPY + SPY_VOICE + 1);
    D_80075790 = 0;
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
        if (actor[0x48] < 0x7F && *(int *)(actor + 8) != 0) {
          int a;
          int da;

          SubtractVector(d, (int *)(actor + 0xC), from);
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
              x = VectorLength(d, 1);
              if (x < 0x1800) {
                x += da * 4;
                if (x < best) {
                  best = x;
                  D_80075790 = actor;
                }
              }
            }
          }
        }
      next:
        actor += 0x58;
      } while (actor < (unsigned char *)g_pActorPoolDynBase);
    }
    break;
  }
  case 12: {
    int i;
    int ang;

    ang = GetRandomU32() & 0x3F;
    for (i = 0; i < 4; i++) {
      v[0] = 0;
      v[1] = (short)g_anCosineLut[ang] >> 7;
      v[2] = (short)g_anSineLut[ang] >> 7;
      RotateVectorByMatrix(g_anSpyroBodyMtx, v, v);
      PARTICLE_SPAWN(1, 0x21, v, 1);
      ang = (ang + 0x40) & 0xFF;
    }
    SPY[SPY_FLAGS] = 0;
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
  case 13:
    g_nSpyroGravityStep = -0xC00;
    goto stand;
  case 7: {
    int len;

    SPY[SPY_FLAGS] = 0;
    ZeroVector(&SPY[SPY_VELOCITY]);
    SubtractVector(&SPY[SPY_VELOCITY], &SPY[SPY_VELOCITY], &SPY[SPY_MOTION]);
    len = VectorLength(&SPY[SPY_VELOCITY], 1);
    if (len > 0x1E00) {
      ScaleVector3ByRatio(&SPY[SPY_VELOCITY], len, 0x1E00);
    }
    g_nSpyroGravityStep = -0x300;
    if (g_nSpyroAirborneFrames == 0) {
      ApplyGravityAlongSpyroSlopeNormal();
    }
    g_nSpyroGravityEnableFlag = 1;
    PARTICLE_SPAWN(5, 0xA, 0, 0);
    if (g_nVibrationLevel < 0x2D) {
      g_nVibrationLevel = 0x2D;
    }
    break;
  }
  case 14: {
    int len;

    SPY[SPY_FLAGS] = 0;
    ZeroVector(&SPY[SPY_VELOCITY]);
    SubtractVector(&SPY[SPY_VELOCITY], &SPY[SPY_VELOCITY], &SPY[SPY_MOTION]);
    len = VectorLength(&SPY[SPY_VELOCITY], 1);
    if (len > 0x1E00) {
      ScaleVector3ByRatio(&SPY[SPY_VELOCITY], len, 0x1E00);
    }
    g_nSpyroGravityStep = -0x300;
    if (g_nSpyroAirborneFrames == 0) {
      ApplyGravityAlongSpyroSlopeNormal();
    }
    g_nSpyroGravityEnableFlag = 1;
    PARTICLE_SPAWN(5, 0xA, 0, 0);
    if (g_nHitRumbleTimer < 0xF) {
      g_nHitRumbleTimer = 0xF;
    }
    PlaySoundEffect(SFX(0x3B), SPY, 4, (unsigned char *)SPY + SPY_VOICE);
    break;
  }
  case 25:
    SPY[SPY_FLAGS] = 0;
    ZeroVector(&SPY[SPY_VELOCITY]);
    g_anSpyroVelocity[2] = -0xC00;
    g_nSpyroGravityEnableFlag = 1;
    PARTICLE_SPAWN(5, 0xA, 0, 0);
    if (g_nHitRumbleTimer < 0xF) {
      g_nHitRumbleTimer = 0xF;
    }
    PlaySoundEffect(SFX(0x3A), SPY, 4, (unsigned char *)SPY + SPY_VOICE);
    break;
  case 27:
    SPY[SPY_FLAGS] = 0;
    ZeroVector(&SPY[SPY_VELOCITY]);
    g_nSpyroGravityStep = -0x300;
    g_nSpyroGravityEnableFlag = 1;
    PARTICLE_SPAWN(5, 0xA, 0, 0);
    if (g_nHitRumbleTimer < 0xF) {
      g_nHitRumbleTimer = 0xF;
    }
    break;
  case 15:
  case 23:
  case 32:
  case 33:
  case 34: {
    int *st = &SPY[SPY_STATE];

    if (*st == 0xF || *st == 0x17 || (unsigned)(*st - 0x20) < 2 ||
        *st == 0x22) {
      g_nSpyroGravityEnableFlag = 0;
      break;
    }
    g_nSpyroStateFlags = 0;
    g_nSpyroGroundRunFlag = 1;
    g_nSpyroBodyYawStep = 0;
    g_nSpyroPitchRateAccum = 0;
    g_nSpyroTurnRateAccum = 0;
    D_80075700 = 0;
    if (g_nSpyroMotionZ > 0) {
      g_nSpyroMotionZ = 0;
    }
    g_nSpyroMotionSpeed = VectorLength(&st[SPY_VELOCITY - SPY_STATE], 0);
    if (g_nSpyroMotionSpeed > 0x1900) {
      g_nSpyroMotionSpeed = 0x1900;
    }
    if (g_nSpyroMotionSpeed < 0x780) {
      g_nSpyroMotionSpeed = 0x780;
    }
    if (*st != 6 || g_nFlightLevelActive ||
        g_nSpyroFloorReferenceZ < D_80078A60) {
      g_nSpyroFloorReferenceZ = D_80078A60;
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
  case 17: {
    int t;

    if (SPY[SPY_SCRIPTED_ACTOR] != 0) {
      ZeroVector(&SPY[SPY_VELOCITY]);
      D_8007578C = g_nSpyroBodyYaw;
      g_nSpyroFloorReferenceZ = g_anSpyroRequestTargetPos[2];
      D_80075724 = g_anSpyroRequestTargetPos[2] - D_80078A60 - 0x800;
      D_80075668 = D_80078A60;
      D_800757F0 = ((D_80075724 << 10) >> 10) + 0x400;
      D_800757F0 = ((((unsigned char *)g_pScriptedCameraActor)[0x46] << 4) -
                    g_nSpyroBodyYaw - D_800757F0) &
                   0xFFF;
      if (D_800757F0 > 0x800) {
        D_800757F0 -= 0x1000;
      }
    } else {
      CopyVector(&SPY[SPY_VELOCITY], &SPY[SPY_MOTION]);
    }
    g_nSpyroGravityStep = 0x80;
    g_nSpyroGravityEnableFlag = 0;
    break;
  }
  case 18:
  case 36:
  case 37:
  case 38:
  case 39:
  case 40:
  case 41:
  case 42:
  case 43: {
    int t;

    g_nSpyroStateFlags = 0;
    t = g_abSpyroAnimDescTable[state * 4 + 2];
    g_nSpyroGravityStep = -0xC00;
    g_nSpyroIdleAnimTimeout = t << 4;
    break;
  }
  case 19:
    SPY[SPY_FLAGS] = 0;
    CopyVector(&SPY[SPY_CARRY], &SPY[SPY_MOTION]);
    if (g_nSpyroCarryMotionZ > 0) {
      g_nSpyroCarryMotionZ = 0;
    }
    g_nSpyroGravityStep = -0x200;
    ApplyGravityAlongSpyroSlopeNormal();
    ApplyGravityAlongSpyroSlopeTangent();
    g_nSpyroGravityEnableFlag = 0;
    break;
  case 20:
    CopyVector(g_anSpyroVelocity, g_anSpyroVelocity + 6);
    CopyVector(g_anSpyroVelocity - 3, g_anSpyroVelocity + 6);
    g_nSpyroBodyYawStep = 0;
    if (g_nSpyroChargeHoldFlag != 0) {
      if (!(g_nSpyroStateFlags & 0x80)) {
        LATCH_CHARGE_SPEED(g_anSpyroVelocity - 3);
      }
      SPY[SPY_FLAGS] |= 0x80;
    } else {
      g_nSpyroStateFlags = 0;
      g_anSpyroVelocity[2] = 0;
      g_anSpyroCarryMotionVec[2] = 0;
    }
    g_nSpyroGravityStep = -0x240;
    g_nSpyroJumpHoldLatch = 0;
    g_nSpyroGravityEnableFlag = 0;
    break;
  case 24:
    CopyVector(g_anSpyroVelocity, g_anSpyroVelocity + 6);
    CopyVector(g_anSpyroVelocity - 3, g_anSpyroVelocity + 6);
    g_nSpyroBodyYawStep = 0;
    g_nSpyroStateFlags = 0xC0;
    g_nSpyroGravityStep = 0;
    g_nSpyroJumpHoldLatch = 0;
    break;
  case 26:
    SPY[SPY_FLAGS] = 0;
    if ((unsigned)(((ArcTan2(SPY[SPY_MOTION], g_anSpyroMotionVec[1], 1) -
                     g_nSpyroBodyYaw) &
                    0xFFF) -
                   0x401) < 0x7FF) {
      g_nSpyroMotionSpeed = 0;
    } else {
      g_nSpyroMotionSpeed = VectorLength(&SPY[SPY_MOTION], 0);
    }
    g_nSpyroGravityStep = -0xC00;
    ApplyGravityAlongSpyroSlopeNormal();
    g_nSpyroMoveTargetMag = 0x280;
    g_nSpyroTurnRateAccum = 0;
    g_nSpyroGravityEnableFlag = 1;
    break;
  case 22:
  case 28:
    g_anSpyroVelocity[0] = g_anSpyroRequestVelocity[0] << 6;
    g_nSpyroStateFlags = 0;
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
    PARTICLE_SPAWN(5, 0xA, 0, 0);
    PlaySoundEffect(SFX(0x3B), SPY, 4, (unsigned char *)SPY + SPY_VOICE);
    if (g_nHitRumbleTimer < 0xF) {
      g_nHitRumbleTimer = 0xF;
    }
    break;
  case 29: {
    int *p = &SPY[SPY_FLAGS];
    int *me;

    *p = 0;
    g_nSpyroGravityStep = -0x80;
    g_nSpyroGravityEnableFlag = 1;
    if (g_nSpyroInputLockoutCountdown == 0) {
      PARTICLE_SPAWN(5, 0xA, 0, 0);
    }
    ((void (*)(int, int))g_pfnLevelOverlayInitHook)(D_8006EA34[D_80075728], 0);
    me = SPY;
    PlaySoundEffect(SFX(0x1D), me, 4, (unsigned char *)SPY + SPY_VOICE);
    PlaySoundEffect(SFX(0x1B), me, 4, (unsigned char *)SPY + SPY_VOICE);
    if (g_nHitRumbleTimer < 0xF) {
      g_nHitRumbleTimer = 0xF;
    }
    break;
  }
  case 30:
    SPY[SPY_FLAGS] = 0;
    CopyVector(&SPY[SPY_CARRY], &SPY[SPY_MOTION]);
    g_nSpyroGravityStep = -0x300;
    if (g_nSpyroAirborneFrames == 0) {
    slope_gravity:
      ApplyGravityAlongSpyroSlopeNormal();
    }
    g_nSpyroGravityEnableFlag = 1;
    break;
  case 31:
    SPY[SPY_FLAGS] = 0;
    CopyVector(&SPY[SPY_CARRY], &SPY[SPY_MOTION]);
    g_nSpyroGravityStep = -0x300;
    if (g_nSpyroAirborneFrames == 0) {
      ApplyGravityAlongSpyroSlopeNormal();
    }
    g_nSpyroGravityEnableFlag = 1;
    PARTICLE_SPAWN(5, 0xA, 0, 0);
    break;
  case 35:
    g_nSpyroGravityStep = -0x80;
    /* FALLTHROUGH */
  case 8:
  stand:
    g_nSpyroStateFlags = 0;
    g_nSpyroGravityEnableFlag = 1;
    break;
  case 44:
    CopyVector(g_anSpyroCarryMotionVec, g_anSpyroCarryMotionVec + 9);
    if (!(g_nSpyroStateFlags & 0x80)) {
      LATCH_CHARGE_SPEED(g_anSpyroCarryMotionVec);
    }
    SPY[SPY_FLAGS] = 0xC0;
    g_nSpyroBodyYawStep = 0;
    g_nSpyroGravityStep = -0x240;
    ApplyGravityAlongSpyroSlopeNormal();
    ApplyGravityAlongSpyroSlopeTangent();
    g_nSpyroGravityEnableFlag = 1;
    D_80075700 = 0;
    PlaySoundEffect(SFX(5), SPY, 4, (unsigned char *)SPY + SPY_VOICE + 1);
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
