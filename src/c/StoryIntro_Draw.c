#include "frame_db.h"
#include "funcs.h"
#include "globals.h"
#include "level_id.h"
#include "main_shared.h"
#include "title_state.h"

/* 0x8001e6b8: Draw routine for the story intro (gamestate 0xD). While substate
 * 2 is past frame 0x8C it lays out one of four message strings with
 * BuildTextSprites, rewinds the sprite-record cursor by the number of glyph
 * slots the message still has to reveal, then runs a per-glyph wave: each
 * record's rotZ byte (its roll angle) ramps from 0x40 by 8 a frame for the
 * first 0x38 frames of its life, so the glyph spins in and lands upright
 * (0x200), and rocks on the cosine LUT after that. Then the standard sprite
 * flush and the simple (no pace loop) gamestate frame-submit tail. */

/* Held-base view of g_nStoryIntroTimer: the original keeps `&timer` in a
   register across the fade-threshold test and the whole glyph loop, which a
   4-byte `extern int` cannot do (sdata cost 1 wins the tie, §B-i rule 2). */

void StoryIntro_Draw(void) {
  int pos[4];
  int col[3];
  int rec;
  int roll;
  int limit;
  int i;
  int phase;
  int t;
  int age;
  int born;
  int *tp;
  unsigned short *cosp;

  if (g_nStoryIntroSubstate == STORY_INTRO_FLY && g_nStoryIntroTimer >= 0x8C) {
    pos[1] = 0x78;
    pos[2] = TEXT_SMALL_DEPTH;
    rec = (int)g_pSpriteRecordWriteCursor;
    col[0] = 0xE;
    col[1] = 1;
    col[2] = 0x1600;
    if (g_nTitleExitPath == TITLE_EXIT_NEW_GAME) {
      pos[0] = 0x5C;
      BuildTextSprites(g_szInTheWorldOfDragons, pos, col, 0x10, TINT_HUD);
      limit = 0xB8;
    } else if (g_nTitleExitPath == TITLE_EXIT_LOADED_GAME) {
      if (g_abLevelVisitedFlag[LEVEL_SLOT(LEVEL_ARTISANS)] != 0) {
        pos[0] = 0x50;
        BuildTextSprites(g_szTheAdventureContinues, pos, col, 0x10, TINT_HUD);
        limit = 0xBC;
      } else {
        pos[0] = 0x64;
        BuildTextSprites(g_szTheAdventureBegins, pos, col, 0x10, TINT_HUD);
        limit = 0xB6;
      }
    } else {
      pos[0] = 0x68;
      BuildTextSprites(g_szEnteringDemoMode, pos, col, 0x10, TINT_HUD);
      limit = 0xB2;
    }

    if (g_anStoryIntroTimerBlock[0] < limit) {
      g_pSpriteRecordWriteCursor += (limit - g_anStoryIntroTimerBlock[0]) >> 1;
    }

    rec -= ACTOR_SIZE;
    if (rec >= (int)g_pSpriteRecordWriteCursor) {
      i = 0;
      tp = g_anStoryIntroTimerBlock;
      cosp = (unsigned short *)g_anCosineLut;
      roll = rec + 0x46;
      phase = 0;
      do {
        t = tp[0];
        born = i + 0x8C;
        age = t - born;
        if (age < 0x38) {
          *(char *)roll = (char)((age << 3) + 0x40);
        } else {
          *(char *)roll = (char)(cosp[((t << 2) + phase) & 0xFF] >> 7);
        }
        phase += 0xC;
        i += 1;
        roll -= 0x58;
        rec -= ACTOR_SIZE;
      } while (rec >= (int)g_pSpriteRecordWriteCursor);
    }
  }

  g_FrameDrawEnv0.r0 = 0;
  g_FrameDrawEnv0.g0 = 0;
  g_FrameDrawEnv0.b0 = 0;
  g_FrameDrawEnv1.r0 = 0;
  g_FrameDrawEnv1.g0 = 0;
  g_FrameDrawEnv1.b0 = 0;
  g_apSpritePrimQueue[0] = 0;
  EnqueuePendingSpritePrims();
  FillWord(&g_apSpritePrimQueue[-0x900], 0,
           0x900); /* 0x900 bytes from g_apMainDrawEntityList */
  RasterizeSpritePrimQueue();

  if (g_nStoryIntroSubstate == STORY_INTRO_FLY) {
    RasterizeSpyroModel();
    ((int *)g_pOtDepthBinArrayBase)[0x40] = g_pOtActiveDepthSlot[0];
    ((int *)g_pOtDepthBinArrayBase)[0x41] = g_pOtActiveDepthSlot[1];
    g_pOtActiveDepthSlot[0] = 0;
    g_pOtActiveDepthSlot[1] = 0;
  }

  DrawSync(0);
  VSync(0);
  PutDispEnv(&FRAME_DB(g_pActiveFrameDrawEnv)->disp);
  PutDrawEnv(g_pActiveFrameDrawEnv);
  DrawOTag(LinkOTPrimitives(0x800));
}

/* Four levers, in the order they paid (A93 is the whole preheader story):
 *  - the sprite-record cursor comparisons are SIGNED (`slt`), so `rec` is an
 *    `int`, not a `char *` — a pointer compare emits `sltu`;
 *  - `age = t - born` with `born = i + 0x8C` computed first: written as
 *    `t - (i + 0x8C)` gcc reassociates to `(t - 0x8C) - i`, which also costs
 *    the load-delay nop the `addiu` should fill;
 *  - the two decrements at the loop bottom go `roll` then `rec`: with `rec`
 *    first the cursor re-load loses its delay-slot filler (+1 insn);
 *  - the preheader is A93 exactly. The original's order is
 *    [i=0][tp copy][cos base][roll][phase], and loop.c-hoisted invariants are
 *    inserted just before loop_start, i.e. AFTER every explicit assignment. So
 *    BOTH bases have to be explicit pre-loop locals declared in that order:
 *    left to loop.c they land last, `roll` is defined while the timer base is
 *    still live, the two allocnos conflict (i.i.greg: `111 conflicts ... 73`),
 *    and the base is pushed off a1 onto t0 — which is the whole 8-insn residue
 *    this park sat on. `tp` also has to be assigned INSIDE the loop's guarded
 *    block: hoisting it above the fade test costs 100 of 196 insns. */
