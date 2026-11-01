#include "camera.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "spyro_state.h"

/* g_nSpyroGravityStep_blk / g_nSpyroStateTimer_blk (globals.h) are the
   register-form (held-base) views of the clamped / incremented globals. */

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
      if (g_nSpyroSubstate == 0) {
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
    if ((g_nSpyroSubstate & 1) && g_nSpyroContactedActorFlag != 0) {
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
