#include "actor_type.h"
#include "funcs.h"
#include "globals.h"
#include "vecmath.h"

/* The queue tail is re-read from the global at every field access. */
#define CURSOR (*(Actor *volatile *)&g_pSpriteRecordWriteCursor)
#define CURSOR_P g_pSpriteRecordWriteCursor

/* Emit a glyph-sprite chain for a string: per non-space byte allocate a
   0x58-byte SPRT slot at g_pSpriteRecordWriteCursor (queue tail, grows down),
   map '0'-'9' and 'A'-'Z' (+ the specials '/', '?', '%', '^', '+') to font
   tile indices, copy the pen position from *pen and advance it by nAdvance
   between glyphs (0x80017fe4, 0x1c8).

   Load-bearing, do not "clean" any of it up:
     - CURSOR is the volatile-cast macro (14 separate lui/lw reloads) and
       CURSOR_P is the same address read PLAINLY. Which one a site uses is a
       scheduling decision, not a style choice: a volatile read is pinned in
       sched2's order, so it emits its two-instruction symbol load at the head
       of the block, and reorg's fill_slots_from_thread stops there because an
       8-byte insn is not delay-slot eligible. The four plain sites are exactly
       the ones whose value insn has to reach a delay slot -- the allocation
       write-back (so `li a2,0x58` sinks into the `jal FillWord` slot instead of
       filling the load-delay hole), and the digit / letter / default arms (so
       each arm's `addiu v0,a0,K` leads its thread and lands in the preceding
       branch's slot, which also stops jump.c from cross-jumping those three
       `sh`s into the shared tail). The six equality arms keep the volatile
       read and DO get merged, which is what the original has.
     - the FillWord argument is the LOCAL `rec`, not a reload, so the
       allocation is `rec = CURSOR - 1; CURSOR_P = rec;` as two
   statements; the embedded assignment form routes the value through v0 (+3
   insns).
     - the range tests are on an `int ch = *pszText;` (a0, unmasked) and the six
       equality tests on `(unsigned char)ch`, which is what emits the single
       `andi v1,a0,0xFF` shared by the whole ladder.
     - per-arm stores, no tile local: a shared `int tile` (function-scope) costs
       a register (+10 words), and block-scoped per-arm temps are byte-identical
       to the plain form. */
void *BuildTextSpriteChain(unsigned char *pszText, int *pPen, int nAdvance,
                           int nPalette) {
  Actor *rec;
  int ch;

  while (*pszText != 0) {
    if (*pszText != ' ') {
      rec = CURSOR - 1;
      CURSOR_P = rec;
      FillWord(rec, 0, 0x58);
      CopyVector(&CURSOR->posX, pPen);

      ch = *pszText;
      if (ch >= '0' && ch <= '9') {
        CURSOR_P->type = ACTOR_TYPE_DIGIT_GLYPH(ch);
      } else if (ch >= 'A' && ch <= 'Z') {
        CURSOR_P->type = ACTOR_TYPE_LETTER_GLYPH(ch);
      } else if ((unsigned char)ch == '/') {
        CURSOR->type = ACTOR_TYPE_GLYPH_SLASH;
      } else if ((unsigned char)ch == '?') {
        CURSOR->type = ACTOR_TYPE_GLYPH_QUESTION;
      } else if ((unsigned char)ch == '%') {
        CURSOR->type = ACTOR_TYPE_GLYPH_PERCENT;
      } else if ((unsigned char)ch == '^') {
        CURSOR->type = ACTOR_TYPE_GLYPH_CARET;
      } else if ((unsigned char)ch == '+') {
        CURSOR->type = ACTOR_TYPE_GLYPH_PLUS;
      } else {
        CURSOR_P->type = ACTOR_TYPE_GLYPH_PERIOD;
      }

      CURSOR->otBias = 0x7F;
      CURSOR->tintMode = nPalette;
      CURSOR->cullRadius = 0xFF;
    }
    *pPen += nAdvance;
    pszText++;
  }
  return CURSOR;
}
