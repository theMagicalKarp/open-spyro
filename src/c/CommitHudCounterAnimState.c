#include "globals.h"

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
  if (*(unsigned char *)((char *)g_abHudIconActorRecords + o1 + 0x46) != 0 ||
      (target = *(short *)((char *)g_abHudIconActorRecords + o1 + 0x36),
       cur = *(unsigned char *)((char *)g_abHudIconActorRecords + o1 + 0x49),
       cur != target - 0x104)) {
    *(unsigned char *)((char *)g_abHudIconActorRecords + o1 + 0x46) = state;
  }
  o2 = (slot * 11) << 3;
  cur2 = *(unsigned char *)((char *)g_abHudIconActorRecords + o2 + 0x49);
  if (cur2 != ff && state == commit) {
    *(short *)((char *)g_abHudIconActorRecords + o2 + 0x36) = cur2 + 0x104;
    *(unsigned char *)((char *)g_abHudIconActorRecords + o2 + 0x50) = ff;
  }
  slot += 1;
  if (slot < end) {
    goto top;
  }
}
