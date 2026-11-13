#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "vecmath.h"

/* Empty stub at 0x80037E98 — bare `jr $ra; nop`. Every animation queue in
   the actor code (the ANIM_* / ENTER_POSE macros and their open-coded
   copies) calls it right after setting nextAnim/nextFrame. */
void OnActorAnimQueued(void) {}

/* Uniform random integer in the inclusive range [lo, hi]: reduce a fresh 32-bit
   draw modulo the span (hi - lo + 1) and bias by lo. The draw is taken as
   signed, matching the signed remainder of the original. */
int RandomIntInRange(int lo, int hi) {
  return (int)GetRandomU32() % (hi - lo + 1) + lo;
}

/* Random signed offset whose magnitude is uniform in [lo, hi]: draw once,
   reduce it modulo the span (hi - lo + 1) biased by lo for the magnitude, then
   flip the sign on the draw's low bit. */
int RandomSignedInRange(int lo, int hi) {
  int r = (int)GetRandomU32();
  int v = r % (hi - lo + 1) + lo;
  if (r & 1) {
    return v;
  }
  return -v;
}

/* Step a countdown value of the given width (4/2/1 bytes) down by the global
 * delta g_nActorFrameDelta, clamping at zero. Returns:
 *   0 - still counting down (value was above the delta)
 *   2 - reached zero on this call (value was positive, now clamped to 0)
 *   1 - already at zero (nothing to do) */
int TickCountdownByFrameDelta(void *pValue, int width) {
  if (width == 4) {
    if (*((int *)pValue) <= g_nActorFrameDelta) {
      if (*((int *)pValue) == 0) {
        goto already_zero;
      }
      *((int *)pValue) = 0;
      return 2;
    }
    *((int *)pValue) = *((int *)pValue) - g_nActorFrameDelta;
    return 0;
  }

  if (width == 2) {
    if (*((short *)pValue) <= g_nActorFrameDelta) {
      if (*((short *)pValue) == 0) {
        goto already_zero;
      }
      *((short *)pValue) = 0;
      return 2;
    }
    *((short *)pValue) = *((short *)pValue) - g_nActorFrameDelta;
    return 0;
  }

  if (width == 1) {
    if (*((char *)pValue) <= g_nActorFrameDelta) {
      if (*((char *)pValue) != 0) {
        *((char *)pValue) = 0;
        return 2;
      }
    already_zero:
      return 1;
    }
    *((char *)pValue) = *((char *)pValue) - g_nActorFrameDelta;
    return 0;
  }
}

/* Sum two values and wrap into a single signed byte's range via (x + 0x100) %
   0x100. The +0x100 biases the sum before the signed remainder so the result
   tracks an 8-bit angle/offset. */
int WrapAddAngle8(int a, int b) { return (a + b + 0x100) % 0x100; }

/* Step byte angle `from` toward `to` by at most `maxStep`: if the shortest
   signed delta (from - to) is larger in magnitude than the limit `maxStep`,
   move `from` by `maxStep` in the delta's direction; otherwise it is already
   within range, so return `to`. */
int StepAngle8Toward(int to, int from, int maxStep, int unused) {
  int delta = WrapAngleDelta8(to, from);
  int absd = delta >= 0 ? delta : -delta;
  if (maxStep < absd) {
    if (delta < 0) {
      return WrapAddAngle8(from, maxStep);
    }
    return WrapAddAngle8(from, -maxStep);
  }
  return to;
}

/* Interpolate byte angle `from` toward `to` by the byte fraction `frac`/256:
   take the shortest signed delta (to - from), scale it by frac, and wrap the
   scaled step back into byte range around `from`. */
int LerpAngle8(int to, int from, unsigned char frac) {
  return WrapAddAngle8(from, (WrapAngleDelta8(from, to) * frac) >> 8);
}

/* Step byte angle `from` toward `to` by limit `maxStep` (StepAngle8Toward),
   then ease the result the rest of the way by fraction `frac`/256 (LerpAngle8).
 */
int StepAndLerpAngle8(int to, int from, int maxStep, int frac) {
  return LerpAngle8(StepAngle8Toward(to, from, maxStep, frac), from,
                    frac & 0xFF);
}

/* Signed 8-bit angle delta: wrap (b - a) into the range (-128, 128]. Used to
   find the shortest turn between two byte angles. */
int WrapAngleDelta8(int a, int b) {
  int d = b - a;

  if (d > ANGLE8_HALF) {
    d -= ANGLE8_TURN;
  }
  if (d < -ANGLE8_HALF) {
    d += ANGLE8_TURN;
  }
  return d;
}
