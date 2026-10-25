#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "sample_bank.h"
#include "spyro_block.h"
#include "spyro_state.h"
#include "vecmath.h"

/* Map this frame's trigger events to Spyro state transitions (0x80040F68,
   0x308).

   `pad` is the mask of events the calling state accepts; it is first gated by
   the input lockout (0x5A post-hit invulnerability frames, which drop the
   damage events 0x1F1, and the hazard 0x400 too while Spyro is still rising)
   and then by g_dwSpyroTriggerEventFlags, the events raised this frame. The
   0x5F1 group is damage: a one-of ladder that costs one g_nSparxHealth, picks
   a hurt state (or, once health is gone, a death state), and re-arms the
   lockout. The remaining bits (0xFC00) are independent: hazard (0x400,
   SPYRO_ST_HAZARD), headbutt rebound (0x8000), lift hold (0x800,
   SPYRO_ST_LIFT) and the charge-hold flag (0x2000). Returns 1 when a
   transition was dispatched. */

int DispatchSpyroPadStateChanges(int pad) {
  int *ev;
  int *lock;
  int *ready;
  int ret;
  int held;
  int next;

  ret = 0;
  if (g_nCurrentLevelId != g_nActiveLevelId) {
    return 0;
  }
  if (g_nSpyroInputLockoutCountdown != 0) {
    pad &= ~0x1F1;
    if (g_anSpyroMotionVec[2] > 0) {
      pad &= ~0x400;
    }
  }
  ev = g_adwSpyroTriggerEventBlock;
  pad &= ev[0];

  if ((pad & 0x5F1) && g_nSparxHealth >= 0) {
    if (g_bDebugNoDepletion == 0) {
      g_nSparxHealth = g_nSparxHealth - 1;
    }
    if (pad & 0x10) {
      if (g_nSparxHealth < 0) {
        next = SPYRO_ST_DEATH_PINNED;
      } else {
        next = SPYRO_ST_HURT_PINNED;
      }
      ChangeSpyroState(next);
    } else if (pad & 0x20) {
      PlaySoundEffect(LEVEL_SFX(0x1F), (int)&ev[-SPY_TRIGGER_FLAGS], 4,
                      (char *)ev + (SPYB_VOICE_MARKER - SPY_TRIGGER_FLAGS * 4));
      ChangeSpyroState(SPYRO_ST_HURT_TINT);
    } else if (pad & 0x40) {
      ChangeSpyroState(SPYRO_ST_HURT_LONG);
    } else if (pad & 0x80) {
      ChangeSpyroState(SPYRO_ST_KNOCKBACK_2);
    } else if (pad & 0x100) {
      ChangeSpyroState(SPYRO_ST_KNOCKBACK);
    } else if (pad & 0x400) {
      ChangeSpyroState(SPYRO_ST_HAZARD);
      if (*(int *)g_pSpyroOrbDropTriggerData != 0 && g_bDebugNoDepletion == 0) {
        g_nSparxHealth = -1;
      }
    } else {
      ChangeSpyroState(SPYRO_ST_HURT);
    }
    lock = g_anSpyroInputLockoutBlock;
    if (lock[0] < 0x5A) {
      lock[0] = 0x5A;
    }
    return 1;
  }

  held = pad & 0xFC00;
  if (pad & 0x400) {
    ready = g_anSparxHealthBlock;
    if (ready[0] >= 0) {
      ResetSpyroLinearMotion();
      if (g_bDebugNoDepletion == 0) {
        ready[0] = ready[0] - 1;
      }
      if (g_nSpyroInputLockoutCountdown < 0x5A) {
        g_nSpyroInputLockoutCountdown = 0x5A;
      }
      ChangeSpyroState(SPYRO_ST_HAZARD);
    }
    ret = 1;
  }

  if (held & 0x8000) {
    ChangeSpyroState(SPYRO_ST_CHARGE_BONK);
    SetVector3Magnitude(g_anSpyroFirstContactNormal, 0x800);
    CopyVector(g_anSpyroFirstContactNormal - 0x25, g_anSpyroFirstContactNormal);
    ret = 1;
  }
  if (held & 0x800) {
    if (g_nSpyroState != SPYRO_ST_LIFT) {
      ChangeSpyroState(SPYRO_ST_LIFT);
      ret = 1;
    }
  } else if (g_nSpyroState == SPYRO_ST_LIFT) {
    ChangeSpyroState(SPYRO_ST_GLIDE);
  }
  if (g_nSpyroState != SPYRO_ST_SUPERCHARGE) {
    if (held & 0x2000) {
      g_nSpyroChargeHoldFlag = 1;
    } else {
      g_nSpyroChargeHoldFlag = 0;
    }
  }
  return ret;
}
