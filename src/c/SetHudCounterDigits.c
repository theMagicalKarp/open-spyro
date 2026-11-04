#include "actor_type.h"
#include "globals.h"
#include "hud.h"

/* 0x800542e4 (0x11c) — write `value` into the HUD counter as decimal digits,
   one 0x58-byte icon record per slot starting at `slot`, most significant
   digit in the leftmost used slot (digits are popped off a scratch stack).
   HUD_DIGITS_PENDING stores each digit as the pending one (->subMode) for a
   digit roll to swap in; HUD_DIGITS_SHOW shows the digit is shown at once
   (->type = ACTOR_TYPE_GEM_DIGIT_0 + digit, ->cullRadius 0xFF) and the slots
   past the number are hidden (ACTOR_CULL_HIDDEN). */
void SetHudCounterDigits(int slot, int count, int value, int show) {
  unsigned char buf[16];
  unsigned char *p;
  int i;
  int ff;

  p = buf;
  while (value != 0) {
    int q = value / 10;
    *p++ = value - q * 10;
    value = q;
  }
  if (p == buf) {
    buf[0] = 0;
    p = buf + 1;
  }

  i = 0;
  if (count > 0) {
    ff = 0xFF;
  top:
    if (p != buf) {
      p -= 1;
      if (show == HUD_DIGITS_PENDING) {
        int off = (slot * 11) << 3;
        HUD_ICON(off)->subMode = *p;
      } else {
        int off = (slot * 11) << 3;
        int digit = *p;
        HUD_ICON(off)->cullRadius = ff;
        HUD_ICON(off)->type = ACTOR_TYPE_GEM_DIGIT_0 + digit;
      }
    } else {
      if (show != HUD_DIGITS_PENDING) {
        int off = (slot * 11) << 3;
        HUD_ICON(off)->cullRadius = ACTOR_CULL_HIDDEN;
      }
    }
    i += 1;
    slot += 1;
    if (i < count) {
      goto top;
    }
  }
}
