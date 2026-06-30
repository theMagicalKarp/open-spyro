#include "globals.h"

/* Scan the 0x18 SPU voice records (0x1C bytes each) for the one owned by
   `owner` with matching sub-id `subId`. Returns 2 if that voice's flag word has
   bit 0x100 set (one-shot/keyed-on), 1 if matched without the flag, or 0 if no
   voice matches. (0x80056dc4, 120 bytes.)

   The matched-voice body is kept on one line so the flag load and its mask stay
   on a single source line: splitting them lets a -g3 line label fall between
   the lhu and the andi, which makes maspsx drop the R3000 load-delay nop. */
int GetSoundVoiceStatusByOwner(int owner, unsigned int subId) {
  int i;

  for (i = 0; i < 0x18; i++) {
    int off = i * 0x1C;
    if (owner == *(int *)&g_abSpuCommonAttr[0x28 + off] &&
        g_abSpuCommonAttr[0x35 + off] == subId)
    /* clang-format off */
      { if (*(unsigned short *)&g_abSpuCommonAttr[0x36 + off] & 0x100) return 2; return 1; }
    /* clang-format on */
  }
  return 0;
}
