#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "sample_bank.h"
#include "vecmath.h"

/* Map this frame's pad intent to Spyro state transitions (0x80040F68, 0x308).

   `pad` is the raw intent mask; it is first gated by the input lockout (which
   suppresses the ground moves 0x1F1, and the jump 0x400 too while Spyro is
   still rising) and then by g_dwSpyroTriggerEventFlags, the set of transitions
   the current state actually permits. The 0x5F1 group (attack/interact moves)
   is a one-of ladder that ends in a single ChangeSpyroState and re-arms the
   lockout; the remaining bits (0xFC00) are independent modifiers — headbutt
   (0x8000), swim hold (0x800) and the charge-hold flag (0x2000). Returns 1
   when a transition was dispatched. */

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
        next = 0x1F;
      } else {
        next = 0x19;
      }
      ChangeSpyroState(next);
    } else if (pad & 0x20) {
      PlaySoundEffect(LEVEL_SFX(0x1F), (int)&ev[-0xB], 4, (char *)ev + 0x274);
      ChangeSpyroState(7);
    } else if (pad & 0x40) {
      ChangeSpyroState(0x1B);
    } else if (pad & 0x80) {
      ChangeSpyroState(0x1C);
    } else if (pad & 0x100) {
      ChangeSpyroState(0x16);
    } else if (pad & 0x400) {
      ChangeSpyroState(0x1D);
      if (*(int *)g_pSpyroOrbDropTriggerData != 0 && g_bDebugNoDepletion == 0) {
        g_nSparxHealth = -1;
      }
    } else {
      ChangeSpyroState(0xE);
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
      ChangeSpyroState(0x1D);
    }
    ret = 1;
  }

  if (held & 0x8000) {
    ChangeSpyroState(0xC);
    SetVector3Magnitude(g_anSpyroFirstContactNormal, 0x800);
    CopyVector(g_anSpyroFirstContactNormal - 0x25, g_anSpyroFirstContactNormal);
    ret = 1;
  }
  if (held & 0x800) {
    if (g_nSpyroState != 0x11) {
      ChangeSpyroState(0x11);
      ret = 1;
    }
  } else if (g_nSpyroState == 0x11) {
    ChangeSpyroState(0xF);
  }
  if (g_nSpyroState != 0x2C) {
    if (held & 0x2000) {
      g_nSpyroChargeHoldFlag = 1;
    } else {
      g_nSpyroChargeHoldFlag = 0;
    }
  }
  return ret;
}
