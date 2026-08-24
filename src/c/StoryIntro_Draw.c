#include "globals.h"

/* 0x8001e6b8: Draw routine for the story intro (gamestate 0xD). While substate
 * 2 is past frame 0x8C it lays out one of four message strings with
 * BuildTextSprites, rewinds the sprite-record cursor by the number of glyph
 * slots the message still has to reveal, then runs a per-glyph wave: each
 * record's +0x46 alpha byte ramps linearly for the first 0x38 frames of its
 * life and rides the cosine LUT after that. Then the standard sprite flush and
 * the simple (no pace loop) gamestate frame-submit tail. */

extern unsigned char D_80010CF0[];
extern unsigned char D_80010D0C[];
extern unsigned char D_80010D28[];
extern unsigned char D_80010D40[];

/* Held-base view of g_nStoryIntroTimer: the original keeps `&timer` in a
   register across the fade-threshold test and the whole glyph loop, which a
   4-byte `extern int` cannot do (sdata cost 1 wins the tie, §B-i rule 2). */
extern int g_anStoryIntroTimerBlock[];
extern unsigned short g_anCosineLut[];

extern void *BuildTextSprites(unsigned char *str, int *pos, int *col, int adv,
                              int pal);
extern void EnqueuePendingSpritePrims(void);
extern void RasterizeSpritePrimQueue(void);
extern void RasterizePairedActor(void);
extern void FillWord(void *dst, unsigned int value, int byte_count);

void StoryIntro_Draw(void) {
  int pos[4];
  int col[3];
  int rec;
  int alpha;
  int limit;
  int i;
  int phase;
  int t;
  int age;
  int born;
  int *tp;
  unsigned short *cosp;

  if (g_nStoryIntroSubstate == 2 && g_nStoryIntroTimer >= 0x8C) {
    pos[1] = 0x78;
    pos[2] = 0x1400;
    rec = (int)g_pSpriteRecordWriteCursor;
    col[0] = 0xE;
    col[1] = 1;
    col[2] = 0x1600;
    if (g_nStoryIntroMessage == 0) {
      pos[0] = 0x5C;
      BuildTextSprites(D_80010CF0, pos, col, 0x10, 0xB);
      limit = 0xB8;
    } else if (g_nStoryIntroMessage == 1) {
      if (g_abLevelVisitedFlag[0] != 0) {
        pos[0] = 0x50;
        BuildTextSprites(D_80010D0C, pos, col, 0x10, 0xB);
        limit = 0xBC;
      } else {
        pos[0] = 0x64;
        BuildTextSprites(D_80010D28, pos, col, 0x10, 0xB);
        limit = 0xB6;
      }
    } else {
      pos[0] = 0x68;
      BuildTextSprites(D_80010D40, pos, col, 0x10, 0xB);
      limit = 0xB2;
    }

    if (g_anStoryIntroTimerBlock[0] < limit) {
      g_pSpriteRecordWriteCursor =
          (char *)g_pSpriteRecordWriteCursor +
          ((limit - g_anStoryIntroTimerBlock[0]) >> 1) * 0x58;
    }

    rec -= 0x58;
    if (rec >= (int)g_pSpriteRecordWriteCursor) {
      i = 0;
      tp = g_anStoryIntroTimerBlock;
      cosp = g_anCosineLut;
      alpha = rec + 0x46;
      phase = 0;
      do {
        t = tp[0];
        born = i + 0x8C;
        age = t - born;
        if (age < 0x38) {
          *(char *)alpha = (char)((age << 3) + 0x40);
        } else {
          *(char *)alpha = (char)(cosp[((t << 2) + phase) & 0xFF] >> 7);
        }
        phase += 0xC;
        i += 1;
        alpha -= 0x58;
        rec -= 0x58;
      } while (rec >= (int)g_pSpriteRecordWriteCursor);
    }
  }

  g_abFrameDrawEnv0.r0 = 0;
  g_abFrameDrawEnv0.g0 = 0;
  g_abFrameDrawEnv0.b0 = 0;
  g_abFrameDrawEnv1.r0 = 0;
  g_abFrameDrawEnv1.g0 = 0;
  g_abFrameDrawEnv1.b0 = 0;
  g_apSpritePrimQueue[0] = 0;
  EnqueuePendingSpritePrims();
  FillWord(&g_apSpritePrimQueue[-0x900], 0, 0x900);
  RasterizeSpritePrimQueue();

  if (g_nStoryIntroSubstate == 2) {
    RasterizePairedActor();
    ((int *)g_pOtDepthBinArrayBase)[0x40] = g_pOtActiveDepthSlot[0];
    ((int *)g_pOtDepthBinArrayBase)[0x41] = g_pOtActiveDepthSlot[1];
    g_pOtActiveDepthSlot[0] = 0;
    g_pOtActiveDepthSlot[1] = 0;
  }

  DrawSync(0);
  VSync(0);
  PutDispEnv((DISPENV *)((char *)g_pActiveFrameDrawEnv + 0x5c));
  PutDrawEnv(g_pActiveFrameDrawEnv);
  DrawOTag(LinkOTPrimitives(0x800));
}

/* Four levers, in the order they paid (A93 is the whole preheader story):
 *  - the sprite-record cursor comparisons are SIGNED (`slt`), so `rec` is an
 *    `int`, not a `char *` — a pointer compare emits `sltu`;
 *  - `age = t - born` with `born = i + 0x8C` computed first: written as
 *    `t - (i + 0x8C)` gcc reassociates to `(t - 0x8C) - i`, which also costs
 *    the load-delay nop the `addiu` should fill;
 *  - the two decrements at the loop bottom go `alpha` then `rec`: with `rec`
 *    first the cursor re-load loses its delay-slot filler (+1 insn);
 *  - the preheader is A93 exactly. The original's order is
 *    [i=0][tp copy][cos base][alpha][phase], and loop.c-hoisted invariants are
 *    inserted just before loop_start, i.e. AFTER every explicit assignment. So
 *    BOTH bases have to be explicit pre-loop locals declared in that order:
 *    left to loop.c they land last, `alpha` is defined while the timer base is
 *    still live, the two allocnos conflict (i.i.greg: `111 conflicts ... 73`),
 *    and the base is pushed off a1 onto t0 — which is the whole 8-insn residue
 *    this park sat on. `tp` also has to be assigned INSIDE the loop's guarded
 *    block: hoisting it above the fade test costs 100 of 196 insns. */
