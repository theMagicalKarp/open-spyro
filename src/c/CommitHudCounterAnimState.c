#include "actor_type.h"
#include "globals.h"
#include "hud.h"

/* 0x800544a8 — step the roll of `count` HUD digit records from `slot`. A
   digit already turned (rotZ != 0), or whose pending digit (subMode) differs
   from the one it shows (type - ACTOR_TYPE_GEM_DIGIT_0), takes rotZ =
   `state`, the counter's roll phase. At HUD_DIGIT_SWAP_PHASE every digit with
   a pending value (subMode != 0xFF) switches its type to that digit and gets
   cullRadius 0xFF, which makes it drawable. */
void CommitHudCounterAnimState(int slot, int count, int state) {
  int ff, commit, end, o1, o2, cur2;
  register int target, cur;
  if (count <= 0) {
    return;
  }
  ff = 0xFF;
  commit = HUD_DIGIT_SWAP_PHASE;
  end = count + slot;
top:
  o1 = (slot * 11) << 3;
  if (HUD_ICON(o1)->rotZ != 0 ||
      (target = HUD_ICON(o1)->type, cur = HUD_ICON(o1)->subMode,
       cur != target - ACTOR_TYPE_GEM_DIGIT_0)) {
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
