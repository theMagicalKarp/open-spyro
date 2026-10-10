#include "globals.h"
#include "sdata.h"

extern void ChangeSpyroState(int state);
extern void SubtractVector(int *dst, int *a, int *b);
extern int ArcTan2(int y, int x, int high_precision);
extern void SetVector3Magnitude(int *vec, int mag);
extern void CopyVector(int *dst, int *src);
extern void ResetSpyroLinearMotion(void);
extern int abs(int x);

extern int D_800756A0;

SDATA(g_pSpyroContactActor, 4);

/* Charge-state trigger dispatcher (0x80041270, 744 bytes), called from the
   charge (0xB) and flame (0x14) states. Masks the trigger-event flags (fewer
   while the input lockout runs) and routes them: a bump (bits 0x6 == 4, or
   == 2 with the contact actor within +-0x200 of Spyro's heading) kicks state
   0xC; a combo (== 6) spends a life-gated lockout and picks the target state
   from the flag bits; 0x8000 rebounds off the first contact normal; 0x800
   forces state 0x11; 0x400 spends a level-ready credit for state 0x1D. Bit
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
  __asm__("" : "=r"(lock) : "0"(&g_nSpyroInputLockoutCountdown));
  if (lock[0] != 0) {
    flags = 0xFE0E;
  }
  flags &= g_dwSpyroTriggerEventFlags;
  switch (flags & 6) {
  case 2:
    SubtractVector(d, (int *)g_pSpyroContactActor + 3, lock - 0x58);
    angle = (ArcTan2(d[0], d[1], 1) - g_nSpyroBodyYaw) & 0xFFF;
    if (angle > 0x800) {
      angle -= 0x1000;
    }
    if (abs(angle) >= 0x200) {
      break;
    }
    /* FALLTHROUGH */
  case 4:
    ChangeSpyroState(0xC);
    changed = 1;
    break;
  case 6:
    if (lock[0] == 0 && lock[1] >= 0) {
      if (D_800756A0 == 0) {
        lock[1]--;
      }
      lock[0] = 0x5A;
      if (flags & 0x10) {
        state = 0x19;
        if (lock[1] < 0) {
          state = 0x1F;
        }
      } else if (flags & 0x20) {
        state = 7;
      } else if (flags & 0x40) {
        state = 0x1B;
      } else if (flags & 0x80) {
        state = 0x1C;
      } else if (flags & 0x100) {
        state = 0x16;
      } else {
        state = 0xE;
        if (flags & 0x400) {
          state = 0x1D;
        }
      }
      ChangeSpyroState(state);
      return 1;
    }
    break;
  }
  hi = flags & 0xFC00;
  if (flags & 0x8000) {
    ChangeSpyroState(0xC);
    SetVector3Magnitude(g_anSpyroFirstContactNormal, 0x800);
    CopyVector(g_anSpyroFirstContactNormal - 0x25, g_anSpyroFirstContactNormal);
    changed = 1;
  }
  if ((flags & 0x800) && g_nSpyroState != 0x11) {
    ChangeSpyroState(0x11);
    changed = 1;
  }
  if (hi & 0x400) {
    __asm__("" : "=r"(ready) : "0"(&g_nLevelReadyFlag));
    if (*ready < 0) {
      goto hold;
    }
    ResetSpyroLinearMotion();
    if (D_800756A0 == 0) {
      (*ready)--;
    }
    if (g_nSpyroInputLockoutCountdown < 0x5A) {
      g_nSpyroInputLockoutCountdown = 0x5A;
    }
    ChangeSpyroState(0x1D);
    changed = 1;
  }
hold:
  if (hi & 0x2000) {
    g_nSpyroChargeHoldFlag = 1;
  } else {
    g_nSpyroChargeHoldFlag = 0;
  }
  return changed;
}
