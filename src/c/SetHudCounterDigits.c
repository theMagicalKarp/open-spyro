#include "actor_type.h"
#include "globals.h"
#include "hud.h"

/* 0x800542e4 (0x11c) — write `value` into the HUD counter as decimal digits,
   one 0x58-byte icon record per slot starting at `slot`, most significant
   digit in the leftmost used slot (digits are popped off a scratch stack).
   icon == 0 writes the pending glyph byte (+0x49) for a digit roll; else the
   committed glyph halfword (+0x36, biased 0x104) and alpha (+0x50, 0xFF) are
   set directly, and slots past the number get their alpha cleared. */
void SetHudCounterDigits(int slot, int count, int value, int icon) {
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
      if (icon == 0) {
        int off = (slot * 11) << 3;
        HUD_ICON(off)->subMode = *p;
      } else {
        int off = (slot * 11) << 3;
        int digit = *p;
        HUD_ICON(off)->cullRadius = ff;
        HUD_ICON(off)->type = ACTOR_TYPE_GEM_DIGIT_0 + digit;
      }
    } else {
      if (icon != 0) {
        int off = (slot * 11) << 3;
        HUD_ICON(off)->cullRadius = 0;
      }
    }
    i += 1;
    slot += 1;
    if (i < count) {
      goto top;
    }
  }
}
