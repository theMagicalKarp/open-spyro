#include "actor_type.h"
#include "funcs.h"
#include "globals.h"
#include "vecmath.h"

/* The sprite-record cursor as an Actor, read through a volatile view or
   plainly (see note 3 below). */
#define CURSOR_V ((Actor *)*(void *volatile *)&g_pSpriteRecordWriteCursor)
#define CURSOR ((Actor *)g_pSpriteRecordWriteCursor)

/* Emit a glyph-sprite chain for a UI string, tracking per-glyph kerning: per
   non-space byte allocate a 0x58-byte SPRT slot at g_pSpriteRecordWriteCursor
   (queue tail, grows down), copy the pen from *pos, map the byte to a font
   tile ('0'-'9', 'A'-'Z', and specials '!',',','?','.') and advance the pen by
   the digit-advance spacing after digits / '!'/'?', else by the glyph width
   attr[0]. Spaces advance the pen by 3/4 of the glyph width.
   (0x800181ac, 0x2a0 bytes.) */
/* Three things in the body below are load-bearing; do not "clean" them up.
   1. The do/while(0) nests are a ref-count dial (A238). global.c weights
      REG_N_REFS by loop depth, and the three long-lived pseudos here race for
      s0: flag 15 refs/109 insns (1.65) beats text 14/118 (1.42) beats
      pos 13/118 (1.32), where the original wants pos -> s0,
      text -> s1, flag -> s2. The 2-deep nest on `text += 1;` takes
      text to 16 refs and the 6-deep nest on `*pos = adv;` takes pos
      to 19; 5 is not enough (the pair ties and the lower allocno, text,
      wins).
   2. The pen advance is stored once PER ARM through a block-scoped `adv`
      (A201). A shared carrier is a global allocno and can only reach v1; the
      per-arm temps are local qtys and take v0, and jump.c cross-jumps the two
      identical stores back into one.
   3. Only SOME of the cursor reads are volatile, and which ones is not
      cosmetic. A volatile read is pinned in sched2's order, so a block whose
      cursor read is volatile emits the (two-instruction, delay-slot-ineligible)
      symbol load FIRST and reorg's fill_slots_from_thread stops there. That is
      why the letter arm reads the cursor plainly -- its `addiu v0,ch,0x169` has
      to be the first insn of the fall-through thread for the `beqz`'s delay
      slot -- and why the allocation store is written back plainly, which lets
      `li a2,0x58` sink to the `jal FillWord` delay slot instead of filling the
      load-delay hole. */

void *BuildTextSprites(char *text, int *pos, int *attr, int spacing,
                       int flags) {
  unsigned char c;
  int w3;
  int ch;
  int flag;
  c = *text;
  flag = 1;
  while (c != 0) {
    if ((c & 0xFF) != 0x20) {
      Actor *rec = CURSOR_V - 1;
      g_pSpriteRecordWriteCursor = rec;
      FillWord(rec, 0, 0x58);
      CopyVector(&CURSOR_V->posX, pos);
      c = *text;
      if ((c == 0x21) || (c == 0x3f)) {
        flag = 1;
      }
      if (flag == 0) {
        Actor *r = CURSOR_V;
        r->posY = (r->posY) + attr[1];
        r->posZ = attr[2];
      }
      ch = *text;
      if (((unsigned int)(ch - 0x30)) < 10) {
        CURSOR->type = ch + 0xd4;
      } else if (((unsigned int)(ch - 0x41)) < 0x1a) {
        CURSOR->type = ch + 0x169;
      } else if ((ch & 0xFF) == 0x21) {
        CURSOR_V->type = 0x4b;
      } else if ((ch & 0xFF) == 0x2c) {
        CURSOR_V->type = 0x4c;
      } else if ((ch & 0xFF) == 0x3f) {
        CURSOR_V->type = 0x116;
      } else if ((ch & 0xFF) == 0x2e) {
        CURSOR_V->type = 0x147;
      } else {
        Actor *r = CURSOR_V;
        r->type = 0x4c;
        r->posY = (r->posY) - ((attr[0] * 2) / 3);
      }
      CURSOR_V->otBias = 0x7f;
      CURSOR_V->tintMode = flags;
      CURSOR_V->cullRadius = 0xff;
      if (flag != 0) {
        int adv = (*pos) + spacing;
        do {
          do {
            do {
              do {
                do {
                  do {
                    *pos = adv;
                  } while (0);
                } while (0);
              } while (0);
            } while (0);
          } while (0);
        } while (0);
      } else {
        int adv = (*pos) + attr[0];
        do {
          do {
            do {
              do {
                do {
                  do {
                    *pos = adv;
                  } while (0);
                } while (0);
              } while (0);
            } while (0);
          } while (0);
        } while (0);
      }
      {
        int t = (*text) - 0x30;
        flag = ((unsigned int)t) < 10;
      }
    } else {
      w3 = attr[0] * 3;
      flag = 1;
      if (w3 < 0) {
        w3 += 3;
      }
      *pos += w3 >> 2;
    }
    do {
      do {
        text += 1;
      } while (0);
    } while (0);
    c = *text;
  }

  return *((void *volatile *)(&g_pSpriteRecordWriteCursor));
}
