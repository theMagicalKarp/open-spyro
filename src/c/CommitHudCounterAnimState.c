#include "actor_type.h"
#include "globals.h"
#include "hud.h"

/* Commit the digit-roll animation state byte (+0x46 of each 0x58-byte HUD
   icon record) for `count` slots starting at `slot` (0x800544a8). A slot
   already mid-roll, or whose pending glyph (+0x49) differs from its
   committed glyph halfword (+0x36, biased by 0x104), takes `state`. On an
   end-of-roll commit (state == 0xC0) each still-rolling slot also latches
   the committed glyph from the pending one and forces alpha (+0x50) to
   0xFF. */
void CommitHudCounterAnimState(int slot, int count, int state) {
  int ff, commit, end, o1, o2, cur2;
  register int target, cur;
  if (count <= 0) {
    return;
  }
  ff = 0xFF;
  commit = 0xC0;
  end = count + slot;
top:
  o1 = (slot * 11) << 3;
  if (HUD_ICON(o1)->rotZ != 0 ||
      (target = HUD_ICON(o1)->type, cur = HUD_ICON(o1)->subMode,
       cur != target - 0x104)) {
    HUD_ICON(o1)->rotZ = state;
  }
  o2 = (slot * 11) << 3;
  cur2 = HUD_ICON(o2)->subMode;
  if (cur2 != ff && state == commit) {
    HUD_ICON(o2)->type = ACTOR_TYPE_GEM_DIGIT_0 + cur2;
    HUD_ICON(o2)->cullRadius = ff;
  }
  slot += 1;
  if (slot < end) {
    goto top;
  }
}
