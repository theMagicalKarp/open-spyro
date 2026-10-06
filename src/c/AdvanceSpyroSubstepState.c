#include "globals.h"

extern void DampenSpyroFreeFallAttitude(void);
extern void UpdateSpyroFlightAttitudeNearGround(void);
extern void AlignSpyroBodyToVelocity(void);

/* register-form (held-base) views of the clamped / incremented globals */
extern int g_nSpyroAnimPlayRate_blk[];
extern int g_nCameraNextMode_blk[];
extern int g_nSpyroGravityStep_blk[];
extern int g_nSpyroStateTimer_blk[];

/* 0x8004888c (0x310): per-substep Spyro state advance. Runs the state's
   attitude controller (free-fall damping for state 6, velocity alignment for
   0x14/0x18, the near-ground flight controller for the rest), derives the
   anim play rate from the motion speed for the moving states (1, 0x13, 0x15;
   frame-driven for 5), then bumps g_nSpyroStateTimer. */
void AdvanceSpyroSubstepState(void) {
  int pad[2];
  int *p;
  unsigned char frame;

  switch (g_nSpyroState) {
  case 6:
    DampenSpyroFreeFallAttitude();
    break;
  case 1:
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
      if (g_dwActiveCameraOptions & 0x10) {
        int *cam = g_nCameraNextMode_blk;
        int m = 0x80000000;
        if (*cam >= 0) {
          *cam = m;
        }
      }
    }
    break;
  case 5:
    UpdateSpyroFlightAttitudeNearGround();
    if (g_nSpyroAnimMode == 0) {
      if (g_nSpyroStateFlags == 0) {
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
  case 11:
    UpdateSpyroFlightAttitudeNearGround();
    if ((g_nSpyroStateFlags & 1) && g_nSpyroContactedActorFlag != 0) {
      g_nSpyroMotionSpeed = 0xA00;
    }
    break;
  case 19:
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
  case 20:
    AlignSpyroBodyToVelocity();
    break;
  case 21:
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
  case 44:
    if (g_nSpyroAirborneFrames != 0) {
      break;
    }
  case 0:
  case 2:
  case 3:
  case 4:
  case 7:
  case 8:
  case 9:
  case 10:
  case 12:
  case 13:
  case 14:
  case 16:
  case 17:
  case 18:
  case 22:
  case 25:
  case 26:
  case 27:
  case 28:
  case 29:
  case 30:
  case 31:
  case 35:
  case 36:
  case 37:
  case 38:
  case 39:
  case 40:
  case 41:
  case 42:
  case 43:
    UpdateSpyroFlightAttitudeNearGround();
    break;
  case 24:
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
