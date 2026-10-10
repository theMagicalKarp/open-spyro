#include "globals.h"
#include "sdata.h"

extern int DispatchSpyroPadStateChanges(int mask);
extern int DispatchSpyroChargeStateChanges(void);
extern void ChangeSpyroState(int state);
extern void RestartSpyroAnimWithState(int state);
extern void CycleSpyroIdleAnim(void);
extern void ResetSpyroLinearMotion(void);
extern void TriggerRespawnOrGameOver(void);
extern void BuildSpyroVelocityFromBodyEuler(void);
extern int AbsAngleDelta12(int a, int b);
extern int VectorLength(int *vec, int include_z);
extern void CopyVector(int *dst, int *src);

extern unsigned short g_anCosineLut[];
extern int D_80078A60;        /* g_anSpyroWorldPos[2] */
extern int g_nSpyroVelocityZ; /* g_anSpyroVelocity[2] */
extern int D_800756B4;
extern int D_80075700;
extern int D_80075728;
extern int D_8006EA40[];

SDATA(D_800756B4, 4);

/* One view of the Spyro record from its world position (0x80078a58). */
extern int g_anSpyroWorldPosBlock[];

#define SPY g_anSpyroWorldPosBlock
#define SPY_FLAGS 0x1F        /* g_nSpyroStateFlags */
#define SPY_AIRBORNE 0x27     /* g_nSpyroAirborneFrames */
#define SPY_CARRY 0x3A        /* g_anSpyroCarryMotionVec */
#define SPY_VELOCITY 0x3D     /* g_anSpyroVelocity */
#define SPY_READY 0x59        /* g_nLevelReadyFlag */
#define SPY_COUNTDOWN 0x7C    /* g_nSpyroPadSnapshotCountdown */
#define SPY_CHARGE_HOLD 0x94  /* g_nSpyroChargeHoldFlag */
#define SPY_ANIM_CURRENT 0x19 /* g_bSpyroAnimCurrent (byte offset) */
#define SPY_ANIM_FRAME 0x1F   /* g_bSpyroAnimFrame (byte offset) */

/* Opaque copy: the result is a fresh value to cse, so a block address stays
   in its register and later fields are addressed off it. */
static inline int *Opaque(int *p) {
  int *q;

  __asm__("" : "=r"(q) : "0"(p));
  return q;
}

static inline int OpaqueInt(int p) {
  int q;

  __asm__("" : "=r"(q) : "0"(p));
  return q;
}

#define PARTICLE_SPAWN(a, b, c, d)                                             \
  ((void (*)(int, int, int *, int))g_pfnLevelOverlayParticleSpawn)(a, b, c, d)

/* Landing: pick the ground state from the speed target and current speed. */
#define LAND_ON_GROUND()                                                       \
  if (g_nSpyroSpeedTarget > 0) {                                               \
    if (g_nSpyroMotionSpeed > 0xF00) {                                         \
      ChangeSpyroState(2);                                                     \
    } else if (g_nSpyroMotionSpeed > 0x780) {                                  \
      ChangeSpyroState(21);                                                    \
    } else {                                                                   \
      ChangeSpyroState(1);                                                     \
    }                                                                          \
  } else if (g_nSpyroMotionSpeed >= 0x780) {                                   \
    ChangeSpyroState(3);                                                       \
  } else {                                                                     \
    ResetSpyroLinearMotion();                                                  \
    ChangeSpyroState(0);                                                       \
  }

/* Jump: glide-jump in flight levels and level 0x40, else the plain jump. */
#define JUMP_STATE()                                                           \
  if (g_nFlightLevelActive != 0 || g_nActiveLevelId == 0x40) {                 \
    ChangeSpyroState(32);                                                      \
  } else {                                                                     \
    ChangeSpyroState(15);                                                      \
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
      v[0] = (short)g_anCosineLut[a] >> 7;                                     \
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
  case 0:
    if (DispatchSpyroPadStateChanges(0xFFF9)) {
      break;
    }
    if (g_dwPadPressed & 0x40) {
      ChangeSpyroState(5);
      break;
    }
    if (g_nSpyroAirborneFrames >= 4 || g_nSpyroGroundSlope >= 0x17) {
      ChangeSpyroState(6);
      break;
    }
    if (g_dwPadHeld & 0x80) {
      ChangeSpyroState(11);
      break;
    }
    if (g_dwPadPressed & 4) {
      ChangeSpyroState(9);
      break;
    }
    if (g_dwPadPressed & 8) {
      ChangeSpyroState(10);
      break;
    }
    if (g_nSpyroFrameMotionLength > 0x400) {
      ChangeSpyroState(3);
      g_nSpyroStateFlags = 7;
      break;
    }
    if (g_nCameraNextMode == 0x80000009) {
      break;
    }
    if (g_nPadDirectionalIdleFlag == 0) {
      ChangeSpyroState(1);
      break;
    }
    if (g_nSpyroPadSnapshotCountdown != 0) {
      break;
    }
    if (g_nSpyroCliffEdgeHint != 0) {
      ChangeSpyroState(13);
      break;
    }
    if (g_nSpyroStateTimer >= g_nSpyroIdleAnimTimeout &&
        g_nSpyroHornStrikeState != 1) {
      CycleSpyroIdleAnim();
    }
    break;
  case 1:
    if (DispatchSpyroPadStateChanges(0xFFF9)) {
      break;
    }
    if (g_dwPadPressed & 0x40) {
      ChangeSpyroState(5);
      break;
    }
    if (g_nSpyroAirborneFrames >= 4 || g_nSpyroGroundSlope >= 0x17) {
      ChangeSpyroState(6);
      break;
    }
    if (g_dwPadHeld & 0x80) {
      ChangeSpyroState(11);
      break;
    }
    if (g_dwPadPressed & 4) {
      ChangeSpyroState(9);
      break;
    }
    if (g_dwPadPressed & 8) {
      ChangeSpyroState(10);
      break;
    }
    if (g_nSpyroMotionSpeed == 0 && g_nSpyroSpeedTarget == 0 &&
        g_nSpyroStateTimer >= 0x10) {
      ChangeSpyroState(0);
      break;
    }
    if (g_nSpyroMotionSpeed > 0xF00) {
      ChangeSpyroState(2);
    } else if (g_nSpyroMotionSpeed > 0x780) {
      ChangeSpyroState(21);
    }
    break;
  case 2:
    if (DispatchSpyroPadStateChanges(0xFFF9)) {
      break;
    }
    if (g_dwPadPressed & 0x40) {
      ChangeSpyroState(5);
      break;
    }
    if (g_nSpyroAirborneFrames >= 4 || g_nSpyroGroundSlope >= 0x17) {
      ChangeSpyroState(6);
      break;
    }
    if (g_dwPadHeld & 0x80) {
      ChangeSpyroState(11);
      break;
    }
    if (g_dwPadPressed & 4) {
      ChangeSpyroState(9);
      break;
    }
    if (g_dwPadPressed & 8) {
      ChangeSpyroState(10);
      break;
    }
    if (g_nSpyroSpeedTarget == 0) {
      ChangeSpyroState(3);
      break;
    }
    if (AbsAngleDelta12(g_nSpyroMoveTargetYaw, g_nSpyroBodyYaw) > 0x500) {
      register int a0 asm("$4");

      __asm__("" : "=r"(a0));
      if (g_nPadStickActiveFlag != 0) {
        int d;

        __asm__ volatile("" : : "r"(a0));
        d = ((unsigned char *)g_pPadSubstepState)[0x17] - 0x7F;

        if (d < 0) {
          d = -d;
        }
        if (d > 0x30) {
          ChangeSpyroState(4);
        } else {
          ChangeSpyroState(3);
        }
      } else {
        ChangeSpyroState(4);
      }
      break;
    }
    if (g_nSpyroSpeedTarget == 0) {
      ChangeSpyroState(0);
      break;
    }
    if (g_nSpyroMotionSpeed < 0x640) {
      ChangeSpyroState(1);
      break;
    }
    if (g_nSpyroMotionSpeed < 0xC80) {
      ChangeSpyroState(21);
    }
    break;
  case 3:
    if (DispatchSpyroPadStateChanges(0xFFF9)) {
      break;
    }
    if (g_dwPadPressed & 0x40) {
      ChangeSpyroState(5);
      break;
    }
    if (g_nSpyroAirborneFrames >= 4 || g_nSpyroGroundSlope >= 0x17) {
      ChangeSpyroState(6);
      break;
    }
    if (g_dwPadHeld & 0x80) {
      ChangeSpyroState(11);
      break;
    }
    if (g_dwPadPressed & 4) {
      ChangeSpyroState(9);
      break;
    }
    if (g_dwPadPressed & 8) {
      ChangeSpyroState(10);
      break;
    }
    if (g_nSpyroMotionSpeed == 0 && g_nSpyroSpeedTarget == 0 &&
        g_nSpyroFrameMotionLength < 0x100) {
      ChangeSpyroState(0);
      break;
    }
    if (g_nSpyroSpeedTarget > 0 && g_nSpyroStateTimer < 0x12 &&
        AbsAngleDelta12(g_nSpyroMoveTargetYaw, g_nSpyroBodyYaw) > 0x500) {
      ChangeSpyroState(4);
      break;
    }
    if (g_nSpyroMotionSpeed > 0xF00 && g_nSpyroSpeedTarget > 0) {
      ChangeSpyroState(2);
      break;
    }
    if (g_nSpyroMotionSpeed > 0x780 && g_nSpyroSpeedTarget > 0) {
      ChangeSpyroState(21);
      break;
    }
    if (g_nSpyroMotionSpeed > 0 && g_nSpyroSpeedTarget > 0) {
      ChangeSpyroState(1);
    }
    break;
  case 4:
    if (DispatchSpyroPadStateChanges(0xFFF9)) {
      break;
    }
    if (g_nSpyroAirborneFrames >= 4 || g_nSpyroGroundSlope >= 0x17) {
      ChangeSpyroState(6);
      break;
    }
    if (g_nSpyroStateTimer < 0x25) {
      break;
    }
    if (g_nPadDirectionalIdleFlag != 0) {
      ChangeSpyroState(0);
    } else {
      g_nSpyroMotionSpeed = 0xA00;
      ChangeSpyroState(1);
    }
    break;
  case 5:
    if (DispatchSpyroPadStateChanges(0xFFF9)) {
      break;
    }
    if (g_nSpyroGroundRunFlag == 0 && (g_dwPadPressed & 0x40)) {
      g_nSpyroJumpHoldLatch = 1;
    }
    if (g_anSpyroMotionVec[2] < 0) {
      g_nSpyroStateFlags = 1;
    }
    if (g_nSpyroStateFlags == 0 && !(g_dwPadHeld & 0x40) && D_800756B4 == 0) {
      g_nSpyroStateFlags = 2;
    }
    if (g_nSpyroAirborneFrames == 0 && g_nSpyroStateTimer >= 0x10) {
      if (g_nSpyroGroundSlope >= 0x17) {
        ChangeSpyroState(19);
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
    if (g_dwPadHeld & 0x80) {
      ChangeSpyroState(20);
      g_nSpyroMotionSpeed = 0x1F80;
      break;
    }
    if (g_nSpyroGroundRunFlag == 0 && g_nSpyroStateFlags == 1 &&
        g_nSpyroJumpHoldLatch != 0) {
      JUMP_STATE();
      break;
    }
    if (g_anSpyroMotionVec[2] < -0x1900 || g_nSpyroStateTimer >= 0x79) {
      ChangeSpyroState(6);
    }
    break;
  case 6:
    if (DispatchSpyroPadStateChanges(0xFFF9)) {
      break;
    }
    if (g_dwPadPressed & 0x40) {
      if (g_nSpyroGravityEnableFlag != 0 && g_nSpyroStateTimer < 4) {
        ChangeSpyroState(5);
        break;
      }
      if ((g_dwPadPressed & 0x40) && g_nSpyroStateFlags == 0) {
        if (g_nFlightLevelActive != 0 || g_nActiveLevelId == 0x40) {
          if (g_nSpyroGroundRunFlag == 0 || g_nSpyroStateTimer >= 0x10) {
            ChangeSpyroState(32);
          }
          break;
        }
        if (g_nSpyroGroundRunFlag == 0 || g_nSpyroStateTimer >= 0x1F) {
          ChangeSpyroState(15);
        }
        break;
      }
    }
    if (D_80078A60 >= g_anLevelDeathPlaneZTable[g_nLevelIntroIndex] &&
        g_nSpyroAirborneFrames == 0) {
      int next;
      int fwd[3];
      int back[3];

      if (g_nSpyroGroundSlope >= 0x17) {
        if (g_nSpyroStateTimer >= 9) {
          ChangeSpyroState(19);
        }
        break;
      }
      if (g_nSpyroSpeedTarget > 0) {
        if (g_nSpyroMotionSpeed > 0xF00) {
          next = 2;
        } else {
          next = 1;
          if (g_nSpyroMotionSpeed > 0x780) {
            next = 21;
          }
        }
        ChangeSpyroState(next);
        LANDING_DUST(fwd);
      } else {
        next = 3;
        if (g_nSpyroMotionSpeed < 0x780) {
          ResetSpyroLinearMotion();
          next = 0;
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
        ((void (*)(void))g_pfnGamestate0EarlyHook)();
      } else {
        g_nExtraLives++;
        TriggerRespawnOrGameOver();
      }
    }
    break;
  case 8:
    if (DispatchSpyroPadStateChanges(0xFBF9)) {
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
      ChangeSpyroState(0);
    } else {
      ChangeSpyroState(1);
      g_nSpyroMotionSpeed = 0x15;
    }
    break;
  case 9:
    if (DispatchSpyroPadStateChanges(0xFFF9)) {
      break;
    }
    if (SPY[SPY_AIRBORNE] >= 4 || g_nSpyroGroundSlope >= 0x17) {
      ChangeSpyroState(6);
      break;
    }
    if (g_dwPadHeld & 4) {
      break;
    }
    if (g_nSpyroStateTimer < 0x15) {
      break;
    }
    CopyVector(&SPY[SPY_VELOCITY], &SPY[SPY_CARRY]);
    g_nSpyroMotionSpeed = 0;
    ChangeSpyroState(0);
    break;
  case 10:
    if (DispatchSpyroPadStateChanges(0xFFF9)) {
      break;
    }
    if (SPY[SPY_AIRBORNE] >= 4 || g_nSpyroGroundSlope >= 0x17) {
      ChangeSpyroState(6);
      break;
    }
    if (g_dwPadHeld & 8) {
      break;
    }
    if (g_nSpyroStateTimer < 0x15) {
      break;
    }
    CopyVector(&SPY[SPY_VELOCITY], &SPY[SPY_CARRY]);
    g_nSpyroMotionSpeed = 0;
    ChangeSpyroState(0);
    break;
  case 11:
    if (DispatchSpyroChargeStateChanges()) {
      break;
    }
    if ((g_dwPadHeld & 0xC0) == 0xC0 && g_nSpyroGroundSlope < 0xC) {
      ChangeSpyroState(20);
      g_nSpyroVelocityZ += 0xDC0;
      g_nSpyroAirborneFrames++;
      break;
    }
    if (g_nSpyroAirborneFrames >= 4) {
      ChangeSpyroState(20);
      break;
    }
    if (g_nSpyroGroundSlope >= 0x17) {
      ChangeSpyroState(6);
      break;
    }
    if (!(g_dwPadHeld & 0x80)) {
      ChangeSpyroState(2);
      g_nSpyroStateTimer = g_nSpyroPrevStateTimer * 2;
      break;
    }
    if (g_nSpyroMotionSpeed > 0xC80 &&
        g_nSpyroFrameMotionLength * 2 < g_nSpyroMotionSpeed &&
        g_nScriptedRespawnFlag == 0) {
      ChangeSpyroState(12);
      break;
    }
    if (SPY[SPY_CHARGE_HOLD] != 0) {
      if (g_nSpyroMotionSpeed > 0x3000) {
        ChangeSpyroState(44);
        break;
      }
      if (!(g_nSpyroStateFlags & 0x80)) {
        int v;

        g_nSpyroFloorReferenceZ = D_80078A60;
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
  case 12:
    if (DispatchSpyroPadStateChanges(0xFFF9)) {
      break;
    }
    if (g_nSpyroStateTimer > 0x10 &&
        (g_nSpyroAirborneFrames != 0 || g_nSpyroGroundSlope >= 0x17)) {
      ChangeSpyroState(6);
      break;
    }
    if (g_nSpyroStateTimer >= 0x19) {
      ChangeSpyroState(0);
    }
    break;
  case 13:
    if (DispatchSpyroPadStateChanges(0xFFF9)) {
      break;
    }
    if (g_dwPadPressed & 0x40) {
      ChangeSpyroState(5);
      break;
    }
    if (g_nSpyroAirborneFrames >= 4 || g_nSpyroGroundSlope >= 0x17) {
      ChangeSpyroState(6);
      break;
    }
    if (g_dwPadHeld & 0x80) {
      ChangeSpyroState(11);
      break;
    }
    if (g_dwPadPressed & 4) {
      ChangeSpyroState(9);
      break;
    }
    if (g_dwPadPressed & 8) {
      ChangeSpyroState(10);
      break;
    }
    if (g_nCameraNextMode == 0x80000009) {
      ChangeSpyroState(0);
      break;
    }
    if (g_nPadDirectionalIdleFlag == 0) {
      ChangeSpyroState(1);
      break;
    }
    if (g_nSpyroPadSnapshotCountdown != 0) {
      ChangeSpyroState(0);
      break;
    }
    if (g_nSpyroStateTimer < 0x1F) {
      break;
    }
    if (g_nSpyroCliffEdgeHint == 0) {
      ChangeSpyroState(0);
    }
    break;
  case 7:
    if (g_dwSpyroTriggerEventFlags & 0x20) {
      break;
    }
    if (g_nSpyroStateTimer < 0x18) {
      break;
    }
    if (g_nLevelReadyFlag < 0) {
      ChangeSpyroState(30);
      break;
    }
    goto land;
  case 14:
  case 22:
  case 28:
    if (g_nSpyroStateTimer < 0x18) {
      break;
    }
    if (g_nLevelReadyFlag < 0) {
      ChangeSpyroState(30);
      break;
    }
    goto land;
  case 27:
    if (g_nSpyroStateTimer < 0x30) {
      break;
    }
    if (g_nLevelReadyFlag < 0) {
      ChangeSpyroState(30);
      break;
    }
    goto land;
  case 15:
    if (DispatchSpyroPadStateChanges(0xFFF9)) {
      break;
    }
    if (g_nSpyroAirborneFrames == 0) {
      if (g_nSpyroGroundSlope >= 0x17) {
        ChangeSpyroState(6);
      } else {
        LAND_ON_GROUND();
      }
      break;
    }
    if (g_nSpyroGroundProbeSuspendFlag == 0 ||
        g_nSpyroFrameMotionLength < 0x400) {
      ChangeSpyroState(6);
      break;
    }
    if (g_dwPadHeld & 0x80) {
      ChangeSpyroState(20);
      break;
    }
    if (g_nSpyroStateTimer > 0x10 && (g_dwPadPressed & 0x10)) {
      ChangeSpyroState(16);
      break;
    }
    if (g_nSpyroFallTrackingFlag != 0) {
      int d;
      int pad;

      if (g_nSpyroStateFlags != 0) {
        break;
      }
      d = D_80078A60;
      if (d - g_nSpyroFloorReferenceZ > 0x200) {
        ChangeSpyroState(6);
        break;
      }
      pad = ((int *)g_pPadSubstepState)[1];
      if (pad & 8) {
        g_nSpyroStateFlags = 1;
      } else if (pad & 4) {
        g_nSpyroStateFlags = 2;
        D_80075700 = d;
      }
      break;
    }
    if (g_nSpyroStateFlags == 0 &&
        g_nSpyroFloorReferenceZ + 0x18 < D_80078A60) {
      ChangeSpyroState(6);
      break;
    }
    if (g_nSpyroMotionSpeed < 0xC80 && g_nSpyroStateTimer > 0x10) {
      ChangeSpyroState(23);
    }
    break;
  case 17:
    DispatchSpyroPadStateChanges(0xFFF9);
    break;
  case 18:
  case 36:
  case 37:
  case 38:
  case 39:
  case 40:
  case 41:
  case 42:
  case 43: {
    int *air;

    if (DispatchSpyroPadStateChanges(0xFFF9)) {
      break;
    }
    if (g_dwPadPressed & 0x40) {
      ChangeSpyroState(5);
      break;
    }
    air = Opaque(&SPY[SPY_AIRBORNE]);
    if (*air >= 4 || g_nSpyroGroundSlope >= 0x17) {
      ChangeSpyroState(6);
      break;
    }
    if (g_dwPadHeld & 0x80) {
      ChangeSpyroState(11);
      break;
    }
    if (g_dwPadPressed & 4) {
      ChangeSpyroState(9);
      break;
    }
    if (g_dwPadPressed & 8) {
      ChangeSpyroState(10);
      break;
    }
    if (g_nSpyroFrameMotionLength > 0x400) {
      ChangeSpyroState(3);
      break;
    }
    if (g_nCameraNextMode != 0x80000009) {
      if (g_nPadDirectionalIdleFlag == 0) {
        ChangeSpyroState(1);
        break;
      }
      if (g_nSpyroCliffEdgeHint != 0) {
        RestartSpyroAnimWithState(13);
        break;
      }
      if (g_nSpyroPadSnapshotCountdown == 0) {
        unsigned char *b = (unsigned char *)air;

        if (b[SPY_ANIM_CURRENT - 4 * SPY_AIRBORNE] ==
                g_abSpyroStateAnimIndexMap[g_nSpyroState] &&
            b[SPY_ANIM_FRAME - 4 * SPY_AIRBORNE] >=
                g_abSpyroAnimDescTable[b[SPY_ANIM_CURRENT - 4 * SPY_AIRBORNE] *
                                           4 +
                                       1] -
                    1) {
          ChangeSpyroState(0);
          break;
        }
        if (g_nSpyroHornStrikeState != 1) {
          break;
        }
      }
    }
    RestartSpyroAnimWithState(0);
    break;
  }
  case 16:
    if (DispatchSpyroPadStateChanges(0xFFF9)) {
      break;
    }
    if (D_80078A60 >= g_anLevelDeathPlaneZTable[g_nLevelIntroIndex] &&
        g_nSpyroAirborneFrames == 0) {
      if (g_nSpyroStateTimer >= 0x18 || g_nSpyroGroundSlope >= 0x17) {
        ChangeSpyroState(6);
        g_nSpyroStateTimer = g_nSpyroPrevStateTimer;
      } else {
        ChangeSpyroState(0);
      }
      if (SPY[SPY_COUNTDOWN] < 8) {
        SPY[SPY_COUNTDOWN] = 8;
      }
      ResetSpyroLinearMotion();
    }
    if (g_dwPadPressed & 0x40) {
      if (g_nSpyroGroundRunFlag == 0 || g_nSpyroStateTimer >= 0x10) {
        ChangeSpyroState(15);
      }
      break;
    }
    goto fall_timeout;
  case 19:
    if (DispatchSpyroPadStateChanges(0xFFF9)) {
      break;
    }
    if ((g_dwPadHeld & 0x40) && g_nSpyroStateTimer < 4) {
      ChangeSpyroState(5);
      break;
    }
    if ((g_dwPadHeld & 0x80) && g_nSpyroMotionSpeed > 0x1F00) {
      ChangeSpyroState(20);
      break;
    }
    if (g_nSpyroAirborneFrames >= 8) {
      ChangeSpyroState(6);
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
        ((void (*)(void))g_pfnGamestate0EarlyHook)();
      } else {
        g_nExtraLives++;
        TriggerRespawnOrGameOver();
      }
    }
    break;
  case 20:
    if (g_nSpyroGroundRunFlag == 0 && (g_dwPadPressed & 0x40)) {
      g_nSpyroJumpHoldLatch = 1;
    }
    if (!(g_nSpyroStateFlags & 0x40) && DispatchSpyroChargeStateChanges()) {
      break;
    }
    if (g_nSpyroGroundSlope >= 0x17 && g_nScriptedRespawnFlag == 0 &&
        g_anSpyroVelocity[0] * g_anSpyroGroundNormal[0] +
                g_anSpyroVelocity[1] * g_anSpyroGroundNormal[1] <
            0) {
      ChangeSpyroState(12);
      break;
    }
    if (g_nSpyroContactedActorFlag == 0 && g_nSpyroMotionSpeed > 0xC80 &&
        g_nSpyroFrameMotionLength * 2 < g_nSpyroMotionSpeed &&
        g_nScriptedRespawnFlag == 0) {
      BuildSpyroVelocityFromBodyEuler();
      ChangeSpyroState(12);
      break;
    }
    if (g_nSpyroAirborneFrames == 0) {
      if (g_nSpyroStateFlags & 0x40) {
        ChangeSpyroState(44);
      } else {
        ChangeSpyroState(11);
      }
      break;
    }
    if (g_anSpyroVelocity[2] < 0 && g_nSpyroJumpHoldLatch != 0) {
      JUMP_STATE();
    }
    break;
  case 21:
    if (DispatchSpyroPadStateChanges(0xFFF9)) {
      break;
    }
    if (g_dwPadPressed & 0x40) {
      ChangeSpyroState(5);
      break;
    }
    if (g_nSpyroAirborneFrames >= 4 || g_nSpyroGroundSlope >= 0x17) {
      ChangeSpyroState(6);
      break;
    }
    if (g_dwPadHeld & 0x80) {
      ChangeSpyroState(11);
      break;
    }
    if (g_dwPadPressed & 4) {
      ChangeSpyroState(9);
      break;
    }
    if (g_dwPadPressed & 8) {
      ChangeSpyroState(10);
      break;
    }
    if (g_nSpyroMotionSpeed > 0xF00) {
      ChangeSpyroState(2);
    } else if (g_nSpyroMotionSpeed < 0x640) {
      ChangeSpyroState(1);
    }
    break;
  case 23:
    if (DispatchSpyroPadStateChanges(0xFFF9)) {
      break;
    }
    if (g_nSpyroAirborneFrames == 0) {
      if (g_nSpyroGroundSlope >= 0x17) {
        ChangeSpyroState(6);
      } else {
        LAND_ON_GROUND();
      }
      break;
    }
    if (g_nSpyroGroundProbeSuspendFlag == 0 ||
        g_nSpyroFrameMotionLength < 0x400) {
      ChangeSpyroState(6);
      break;
    }
    if (g_dwPadHeld & 0x80) {
      ChangeSpyroState(20);
      break;
    }
    if (g_dwPadPressed & 0x10) {
      ChangeSpyroState(16);
      break;
    }
    if (g_nSpyroFloorReferenceZ + 0x18 < D_80078A60) {
      ChangeSpyroState(6);
      break;
    }
    if (g_nSpyroMotionSpeed > 0xC80) {
      JUMP_STATE();
    }
    break;
  case 24: {
    int t;

    if (g_nSpyroStateTimer >= 4) {
      g_nSpyroGravityEnableFlag = 0;
    }
    if ((g_dwPadHeld & 0x40) && g_nSpyroGravityEnableFlag != 0) {
      g_nSpyroGravityEnableFlag = 0;
      g_nSpyroVelocityZ += 0xDC0;
    } else if ((g_dwPadPressed & 0x40) && g_nSpyroGroundRunFlag == 0) {
      g_nSpyroJumpHoldLatch = 1;
    }
    if (g_nSpyroGroundSlope >= 0x17 && g_nScriptedRespawnFlag == 0) {
      t = g_anSpyroVelocity[0] * g_anSpyroGroundNormal[0] +
          g_anSpyroVelocity[1] * g_anSpyroGroundNormal[1];
      if (t < 0) {
        ChangeSpyroState(12);
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
      ChangeSpyroState(12);
      break;
    }
    if (g_nSpyroAirborneFrames == 0) {
      ChangeSpyroState(11);
      break;
    }
    if (g_anSpyroVelocity[2] < 0) {
      if (g_nSpyroJumpHoldLatch == 0) {
        ChangeSpyroState(20);
      } else {
        JUMP_STATE();
      }
    }
    break;
  }
  case 25:
    if (g_dwSpyroTriggerEventFlags & 0x10) {
      break;
    }
    if (g_nSpyroStateTimer < 0x18) {
      break;
    }
    if (g_nLevelReadyFlag < 0) {
      ChangeSpyroState(30);
      break;
    }
  land: {
    int next = 6;

    if (g_nSpyroAirborneFrames == 0) {
      next = OpaqueInt(0);
    }
    ChangeSpyroState(next);
    if (SPY[SPY_COUNTDOWN] < 0xC) {
      SPY[SPY_COUNTDOWN] = 0xC;
    }
    break;
  }
  case 26:
    if (DispatchSpyroPadStateChanges(0xFFF9)) {
      break;
    }
    if (g_dwPadPressed & 0x40) {
      ChangeSpyroState(5);
      break;
    }
    if (g_nSpyroAirborneFrames >= 4 || g_nSpyroGroundSlope >= 0x17) {
      ChangeSpyroState(6);
      break;
    }
    if (g_nSpyroMotionSpeed != 0 || g_nSpyroSpeedTarget != 0 ||
        g_nSpyroStateTimer < 0x10) {
      ChangeSpyroState(1);
      break;
    }
    ChangeSpyroState(0);
    break;
  case 29:
    if (DispatchSpyroPadStateChanges(0xFBF9)) {
      break;
    }
    if (SPY[SPY_READY] < 0) {
      break;
    }
    if (g_nSpyroStateTimer > 0x3C) {
      SPY[SPY_READY] = -1;
      ((void (*)(int, int))g_pfnLevelOverlayInitHook)(D_8006EA40[D_80075728],
                                                      0);
      break;
    }
    if (g_nSpyroStateTimer >= 0x10 && (g_dwPadHeld & 0x40)) {
      ChangeSpyroState(5);
    }
    break;
  case 32:
    if (DispatchSpyroPadStateChanges(0xFFF9)) {
      break;
    }
    if (g_nSpyroAirborneFrames == 0) {
      if (g_nSpyroGroundSlope < 0x17) {
        LAND_ON_GROUND();
      } else {
        ChangeSpyroState(6);
      }
    } else if (g_nSpyroGroundProbeSuspendFlag == 0 ||
               g_nSpyroFrameMotionLength < 0x400) {
      ChangeSpyroState(6);
    } else if (g_anSpyroMotionVec[2] > 0x800 || (g_dwPadPressed & 0x40)) {
      ChangeSpyroState(33);
    } else if (g_anSpyroMotionVec[2] < -0x1000) {
      ChangeSpyroState(34);
    } else if (g_nSpyroStateFlags == 0) {
      int d = D_80078A60;

      if (d - g_nSpyroFloorReferenceZ > 0x200) {
        ChangeSpyroState(6);
      } else {
        int pad = ((int *)g_pPadSubstepState)[1];

        if (pad & 8) {
          g_nSpyroStateFlags = 1;
        } else if (pad & 4) {
          g_nSpyroStateFlags = 2;
          D_80075700 = d;
        }
      }
    }
    if (g_nFlightLevelActive != 0) {
      break;
    }
    if (g_dwPadHeld & 0x80) {
      ChangeSpyroState(20);
      break;
    }
    if (g_nSpyroStateTimer > 0x10 && (g_dwPadPressed & 0x10)) {
      ChangeSpyroState(16);
    }
    break;
  case 33:
    if (DispatchSpyroPadStateChanges(0xFFF9)) {
      break;
    }
    if (g_nSpyroAirborneFrames == 0) {
      if (g_nSpyroGroundSlope < 0x17) {
        LAND_ON_GROUND();
      } else {
        ChangeSpyroState(6);
      }
    } else if (g_nSpyroGroundProbeSuspendFlag == 0 ||
               g_nSpyroFrameMotionLength < 0x400) {
      ChangeSpyroState(6);
    } else if (g_nSpyroStateTimer >= 0x1F && g_anSpyroMotionVec[2] < 0x600) {
      ChangeSpyroState(32);
    } else if (SPY[SPY_FLAGS] == 0) {
      int d = D_80078A60;

      if (d - g_nSpyroFloorReferenceZ > 0x200) {
        ChangeSpyroState(6);
      } else {
        int pad = ((int *)g_pPadSubstepState)[1];

        if (pad & 8) {
          SPY[SPY_FLAGS] = 1;
        } else if (pad & 4) {
          SPY[SPY_FLAGS] = 2;
          D_80075700 = d;
        }
      }
    }
    if (g_nFlightLevelActive != 0) {
      break;
    }
    if (g_dwPadHeld & 0x80) {
      ChangeSpyroState(20);
      break;
    }
    if (g_nSpyroStateTimer > 0x10 && (g_dwPadPressed & 0x10)) {
      ChangeSpyroState(16);
    }
    break;
  case 34:
    if (DispatchSpyroPadStateChanges(0xFFF9)) {
      break;
    }
    if (g_nSpyroAirborneFrames == 0) {
      if (g_nSpyroGroundSlope < 0x17) {
        LAND_ON_GROUND();
      } else {
        ChangeSpyroState(6);
      }
    } else if (g_nSpyroGroundProbeSuspendFlag == 0 ||
               g_nSpyroFrameMotionLength < 0x400) {
      ChangeSpyroState(6);
    } else if (g_dwPadPressed & 0x40) {
      ChangeSpyroState(33);
    } else if (g_nSpyroStateTimer >= 0x1F && g_anSpyroMotionVec[2] >= -0xBFF) {
      ChangeSpyroState(32);
    } else if (SPY[SPY_FLAGS] == 0) {
      int d = D_80078A60;

      if (d - g_nSpyroFloorReferenceZ > 0x200) {
        ChangeSpyroState(6);
      } else {
        int pad = ((int *)g_pPadSubstepState)[1];

        if (pad & 8) {
          SPY[SPY_FLAGS] = 1;
        } else if (pad & 4) {
          SPY[SPY_FLAGS] = 2;
          D_80075700 = d;
        }
      }
    }
    if (g_nFlightLevelActive != 0) {
      break;
    }
    if (g_dwPadHeld & 0x80) {
      ChangeSpyroState(20);
      break;
    }
    if (g_nSpyroStateTimer > 0x10 && (g_dwPadPressed & 0x10)) {
      ChangeSpyroState(16);
    }
    break;
  case 35:
    if (DispatchSpyroPadStateChanges(0xFFF9)) {
      break;
    }
    if (g_dwSpyroRequestMask == 0) {
      ChangeSpyroState(0);
    }
    break;
  case 44:
    if (DispatchSpyroPadStateChanges(0x8400)) {
      break;
    }
    if (g_dwPadHeld & 0x40) {
      if (D_80075700 >= 10 || g_nSpyroAirborneFrames != 0) {
        goto jump;
      }
      D_80075700 += g_nFrameStep;
    }
    if (g_dwPadReleased & 0x40) {
    jump: {
      int *vz;

      ChangeSpyroState(24);
      vz = &SPY[SPY_VELOCITY + 2];
      g_nSpyroGravityEnableFlag = 0;
      *vz += 0xDC0;
      g_nSpyroAirborneFrames++;
      break;
    }
    }
    if (g_nSpyroAirborneFrames >= 4) {
      ChangeSpyroState(24);
      g_nSpyroGravityEnableFlag = 1;
      break;
    }
    if (g_nSpyroGroundSlope >= 0x17) {
      ChangeSpyroState(6);
      break;
    }
    if (g_nSpyroStateTimer >= 0xF &&
        !(*(volatile unsigned int *)&g_dwPadHeld & 0x80)) {
      ChangeSpyroState(11);
      break;
    }
    if (g_nSpyroContactedActorFlag == 0 && g_nSpyroMotionSpeed > 0xC80 &&
        g_nSpyroFrameMotionLength * 2 < g_nSpyroMotionSpeed &&
        g_nScriptedRespawnFlag == 0) {
      ChangeSpyroState(12);
      break;
    }
    if (g_nSpyroMotionSpeed < 0x1E00) {
      ChangeSpyroState(11);
    }
    break;
  }
}
