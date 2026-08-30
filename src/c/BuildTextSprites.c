#include "globals.h"

extern void FillWord(void *dst, unsigned int value, int byte_count);
extern void CopyVector(int *dst, int *src);

/* Emit a glyph-sprite chain for a UI string, tracking per-glyph kerning: per
   non-space byte allocate a 0x58-byte SPRT slot at g_pSpriteRecordWriteCursor
   (queue tail, grows down), copy the pen from *param_2, map the byte to a font
   tile ('0'-'9', 'A'-'Z', and specials '!',',','?','.') and advance the pen by
   the digit-advance param_4 after digits / '!'/'?', else by the glyph width
   param_3->unk0. Spaces advance the pen by 3/4 of the glyph width.
   (0x800181ac, 0x2a0 bytes.) */
/* Three things in the body below are load-bearing; do not "clean" them up.
   1. The do/while(0) nests are a ref-count dial (A238). global.c weights
      REG_N_REFS by loop depth, and the three long-lived pseudos here race for
      s0: flag 15 refs/109 insns (1.65) beats param_1 14/118 (1.42) beats
      param_2 13/118 (1.32), where the original wants param_2 -> s0,
      param_1 -> s1, flag -> s2. The 2-deep nest on `param_1 += 1;` takes
      param_1 to 16 refs and the 6-deep nest on `*param_2 = adv;` takes param_2
      to 19; 5 is not enough (the pair ties and the lower allocno, param_1,
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

void *BuildTextSprites(unsigned char *param_1, int *param_2, int *param_3,
                       int param_4, int param_5) {
  unsigned char bVar1;
  int iVar1;
  int ch;
  int flag;
  bVar1 = *param_1;
  flag = 1;
  while (bVar1 != 0) {
    if ((bVar1 & 0xFF) != 0x20) {
      void *rec =
          (void *)(((int)(*((void *volatile *)(&g_pSpriteRecordWriteCursor)))) -
                   0x58);
      g_pSpriteRecordWriteCursor = rec;
      FillWord(rec, 0, 0x58);
      CopyVector(
          (int *)(((int)(*((void *volatile *)(&g_pSpriteRecordWriteCursor)))) +
                  0xc),
          param_2);
      bVar1 = *param_1;
      if ((bVar1 == 0x21) || (bVar1 == 0x3f)) {
        flag = 1;
      }
      if (flag == 0) {
        void *r = *((void *volatile *)(&g_pSpriteRecordWriteCursor));
        *((int *)(((int)r) + 0x10)) =
            (*((int *)(((int)r) + 0x10))) + param_3[1];
        *((int *)(((int)r) + 0x14)) = param_3[2];
      }
      ch = *param_1;
      if (((unsigned int)(ch - 0x30)) < 10) {
        *((unsigned short *)(((int)(*(&g_pSpriteRecordWriteCursor))) + 0x36)) =
            ch + 0xd4;
      } else if (((unsigned int)(ch - 0x41)) < 0x1a) {
        *((unsigned short *)(((int)(*(&g_pSpriteRecordWriteCursor))) + 0x36)) =
            ch + 0x169;
      } else if ((ch & 0xFF) == 0x21) {
        *((unsigned short
               *)(((int)(*((void *volatile *)(&g_pSpriteRecordWriteCursor)))) +
                  0x36)) = 0x4b;
      } else if ((ch & 0xFF) == 0x2c) {
        *((unsigned short
               *)(((int)(*((void *volatile *)(&g_pSpriteRecordWriteCursor)))) +
                  0x36)) = 0x4c;
      } else if ((ch & 0xFF) == 0x3f) {
        *((unsigned short
               *)(((int)(*((void *volatile *)(&g_pSpriteRecordWriteCursor)))) +
                  0x36)) = 0x116;
      } else if ((ch & 0xFF) == 0x2e) {
        *((unsigned short
               *)(((int)(*((void *volatile *)(&g_pSpriteRecordWriteCursor)))) +
                  0x36)) = 0x147;
      } else {
        void *r = *((void *volatile *)(&g_pSpriteRecordWriteCursor));
        *((unsigned short *)(((int)r) + 0x36)) = 0x4c;
        *((int *)(((int)r) + 0x10)) =
            (*((int *)(((int)r) + 0x10))) - ((param_3[0] * 2) / 3);
      }
      *((unsigned char
             *)(((int)(*((void *volatile *)(&g_pSpriteRecordWriteCursor)))) +
                0x47)) = 0x7f;
      *((unsigned char
             *)(((int)(*((void *volatile *)(&g_pSpriteRecordWriteCursor)))) +
                0x4f)) = param_5;
      *((unsigned char
             *)(((int)(*((void *volatile *)(&g_pSpriteRecordWriteCursor)))) +
                0x50)) = 0xff;
      if (flag != 0) {
        int adv = (*param_2) + param_4;
        do {
          do {
            do {
              do {
                do {
                  do {
                    *param_2 = adv;
                  } while (0);
                } while (0);
              } while (0);
            } while (0);
          } while (0);
        } while (0);
      } else {
        int adv = (*param_2) + param_3[0];
        do {
          do {
            do {
              do {
                do {
                  do {
                    *param_2 = adv;
                  } while (0);
                } while (0);
              } while (0);
            } while (0);
          } while (0);
        } while (0);
      }
      {
        int t = (*param_1) - 0x30;
        flag = ((unsigned int)t) < 10;
      }
    } else {
      iVar1 = param_3[0] * 3;
      flag = 1;
      if (iVar1 < 0) {
        iVar1 += 3;
      }
      *param_2 += iVar1 >> 2;
    }
    do {
      do {
        param_1 += 1;
      } while (0);
    } while (0);
    bVar1 = *param_1;
  }

  return *((void *volatile *)(&g_pSpriteRecordWriteCursor));
}
