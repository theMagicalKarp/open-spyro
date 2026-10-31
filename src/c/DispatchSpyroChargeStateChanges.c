#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "match_asm.h"
#include "sdata.h"
#include "spyro_state.h"
#include "vecmath.h"

SDATA(g_pSpyroContactActor, 4);

/* Charge-state trigger dispatcher (0x80041270, 744 bytes), called from the
   charge states (SPYRO_ST_CHARGE, SPYRO_ST_CHARGE_AIR). Masks the
   trigger-event flags (fewer while the input lockout runs) and routes them: a
   bump (bits 0x6 == 4, or
   == 2 with the contact actor within +-0x200 of Spyro's heading) kicks
   SPYRO_ST_CHARGE_BONK; a hit (== 6) outside the lockout costs one
   g_nSparxHealth, re-arms the lockout and picks the hurt (or, past 0, death)
   state from the flag bits; 0x8000 rebounds off the first contact normal; 0x800
   forces SPYRO_ST_LIFT; 0x400 spends Sparx health for SPYRO_ST_HAZARD. Bit
   0x2000 latches the charge hold. Returns nonzero once any state changed. */
int DispatchSpyroChargeStateChanges(void) {
  int flags = 0xFFFF;
  int changed = 0;
  int *lock;
  int *ready;
  int hi;
  int d[3];
  int angle;
  int state;

  if (g_nCurrentLevelId != g_nActiveLevelId) {
    return 0;
  }
  OPAQUE_COPY(lock, &g_nSpyroInputLockoutCountdown);
  if (lock[0] != 0) {
    flags = 0xFFFF & ~SPYRO_EV_HITS;
  }
  flags &= g_dwSpyroTriggerEventFlags;
  switch (flags & SPYRO_EV_CHARGE_HIT) {
  case SPYRO_EV_CONTACT_FRONT:
    SubtractVector(
        d, &g_pSpyroContactActor->posX,
        lock -
            0x58); /* g_anSpyroWorldPos, 0x58 words below the lockout counter */
    angle = (ArcTan2(d[0], d[1], 1) - g_nSpyroBodyYaw) & 0xFFF;
    if (angle > 0x800) {
      angle -= 0x1000;
    }
    if (abs(angle) >= 0x200) {
      break;
    }
    /* FALLTHROUGH */
  case SPYRO_EV_CONTACT:
    ChangeSpyroState(SPYRO_ST_CHARGE_BONK);
    changed = 1;
    break;
  case SPYRO_EV_CHARGE_HIT:
    if (lock[0] == 0 && lock[1] >= 0) {
      if (g_bDebugNoDepletion == 0) {
        lock[1]--;
      }
      lock[0] = 0x5A;
      if (flags & SPYRO_EV_HIT_PINNED) {
        state = SPYRO_ST_HURT_PINNED;
        if (lock[1] < 0) {
          state = SPYRO_ST_DEATH_PINNED;
        }
      } else if (flags & SPYRO_EV_HIT_TINT) {
        state = SPYRO_ST_HURT_TINT;
      } else if (flags & SPYRO_EV_HIT_LONG) {
        state = SPYRO_ST_HURT_LONG;
      } else if (flags & SPYRO_EV_KNOCKBACK_2) {
        state = SPYRO_ST_KNOCKBACK_2;
      } else if (flags & SPYRO_EV_KNOCKBACK) {
        state = SPYRO_ST_KNOCKBACK;
      } else {
        state = SPYRO_ST_HURT;
        if (flags & SPYRO_EV_HAZARD) {
          state = SPYRO_ST_HAZARD;
        }
      }
      ChangeSpyroState(state);
      return 1;
    }
    break;
  }
  hi = flags & 0xFC00;
  if (flags & SPYRO_EV_REBOUND) {
    ChangeSpyroState(SPYRO_ST_CHARGE_BONK);
    SetVector3Magnitude(g_anSpyroFirstContactNormal, 0x800);
    CopyVector(g_anSpyroFirstContactNormal - 0x25, g_anSpyroFirstContactNormal);
    changed = 1;
  }
  if ((flags & SPYRO_EV_LIFT) && g_nSpyroState != SPYRO_ST_LIFT) {
    ChangeSpyroState(SPYRO_ST_LIFT);
    changed = 1;
  }
  if (hi & SPYRO_EV_HAZARD) {
    OPAQUE_COPY(ready, &g_nSparxHealth);
    if (*ready < 0) {
      goto hold;
    }
    ResetSpyroLinearMotion();
    if (g_bDebugNoDepletion == 0) {
      (*ready)--;
    }
    if (g_nSpyroInputLockoutCountdown < 0x5A) {
      g_nSpyroInputLockoutCountdown = 0x5A;
    }
    ChangeSpyroState(SPYRO_ST_HAZARD);
    changed = 1;
  }
hold:
  if (hi & SPYRO_EV_CHARGE_HOLD) {
    g_nSpyroChargeHoldFlag = 1;
  } else {
    g_nSpyroChargeHoldFlag = 0;
  }
  return changed;
}
