#include "hud.h"
#include "actor_type.h"
#include "emit_record.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "match_asm.h"
#include "vecmath.h"

/* 0x800542e4 (0x11c) — write `value` into the HUD counter as decimal digits,
   one 0x58-byte icon record per slot starting at `slot`, most significant
   digit in the leftmost used slot (digits are popped off a scratch stack).
   HUD_DIGITS_PENDING stores each digit as the pending one (->subMode) for a
   digit roll to swap in; HUD_DIGITS_SHOW shows the digit is shown at once
   (->type = ACTOR_TYPE_GEM_DIGIT_0 + digit, ->cullRadius 0xFF) and the slots
   past the number are hidden (ACTOR_CULL_HIDDEN). */
void SetHudCounterDigits(int slot, int count, int value, int show) {
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
      if (show == HUD_DIGITS_PENDING) {
        int off = (slot * 11) << 3;
        HUD_ICON(off)->subMode = *p;
      } else {
        int off = (slot * 11) << 3;
        int digit = *p;
        HUD_ICON(off)->cullRadius = ff;
        HUD_ICON(off)->type = ACTOR_TYPE_GEM_DIGIT_0 + digit;
      }
    } else {
      if (show != HUD_DIGITS_PENDING) {
        int off = (slot * 11) << 3;
        HUD_ICON(off)->cullRadius = ACTOR_CULL_HIDDEN;
      }
    }
    i += 1;
    slot += 1;
    if (i < count) {
      goto top;
    }
  }
}

/* Shift the position of `count` consecutive HUD icon actors by `delta`,
   starting at icon slot `base`. Rest poses live in a 12-byte-per-record table
   (word at +4), and the live values are written into the 88-byte icon actor
   records (word at +0x10). (0x80054400, 92 bytes.) */
void OffsetHudIconRollAxis(int base, int count, int delta) {
  int i;
  for (i = 0; i < count; i++) {
    int o1 = (base + i) * 12;
    int o2 = (base + i) * 88;
    HUD_ICON(o2)->posY = HUD_REST_POS(o1)[1] + delta;
  }
}

/* Shift the Y origin of `count` consecutive HUD digit sprites by `delta`,
   starting at digit slot `base`. Each HUD rect record is 8 bytes (4 shorts);
   the Y field sits at byte offset 2. The rolled positions are read from the
   rest-pose table and written into the live sprite table. (0x8005445c, 76
   bytes.) */
void OffsetHudDigitRollAxis(int base, int count, int delta) {
  int i;
  for (i = 0; i < count; i++) {
    int off = (base + i) << 3;
    HUD_RECT(g_anHudSpriteRects, off)->y =
        (unsigned short)HUD_RECT(g_anHudDigitRestRects, off)->y + delta;
  }
}

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

/* Refresh the HUD level-gem counters (0x80054578): caches the collected
   count for the current level intro index and re-derives the displayed
   digits and the counter state bytes that sit just below the cache. */

void RefreshHudLevelGemCounter(void) {
  int *pc = g_anHudLevelGemCachedBlock;
  unsigned char *ps = ((unsigned char *)g_anHudLevelGemCachedBlock) - 0x20;
  unsigned char *pr = ((unsigned char *)g_anHudLevelGemCachedBlock) - 0x1B;
  unsigned char state;
  int gems = g_anLevelGemsCollected[g_nLevelIndex];
  *pc = gems;
  SetHudCounterDigits(HUD_SLOT_GEM_DIGITS, 4, gems, HUD_DIGITS_SHOW);
  /* the do/while(0) is load-bearing: it pins the state load below the
     SetHudCounterDigits call instead of letting it hoist into the arg block */
  do {
    state = *ps;
  } while (0);
  if (((state == 1) || (state == 2)) || (state == 4)) {
    *ps = 3;
    *pr = 0xD;
  }
}

/* Source order of the straight-line block is enforced by gcc 2.7.2
 * true_dependence's "both MEMs volatile" clause: the anchor/display/roll/hold
 * stores and the idx/counter/lives loads are VOL(), the cached-count stores are
 * not. `dragons` is a `$2` register local: as a pseudo it loses v0 to the
 * short-lived `lives` qty and the counter batch rotates. The frame needs an
 * unreferenced 8-byte local (`pad[2]`). */

#define VOL(x) (*(volatile __typeof__(x) *)&(x))

/* HUD init for entering a gameplay-with-HUD state (0x80054600, 0x388).

   Wipes the 12 big-icon actor records, then per slot hides the render record,
   seeds its rest position from g_anHudIconRestPos and writes the default
   render bytes (otBias 0x20, cullRadius 0xFF so it draws, subMode 0xFF =
   no pending digit, tintMode TINT_HUD). Wipes the
   sprite-rect table and copies the 12 digit rest rects into it. Places the
   four fixed HUD anchors, arms all five counter state machines (phase and
   hold timer cleared), snapshots the live gem/dragon/life/egg/butterfly
   counts, and draws the initial digits for the gem, dragon and life counters.

   With `layout_butterflies` set it also clears the egg roll request and
   lays the sparx-food butterflies out around the lives icon's (slot 10) screen
   position (its record's posX / posY) using the g_abHudButterflyRingXY ring
   offsets. */

void InitHudCounters(int layout_butterflies) {
  short *anchor;
  int *icon;
  byte *rec;
  int *pos;
  int i;
  int off;
  int vec;
  int rect;
  int ring;
  int food;
  int ff;
  int idx;
  int gems;
  int lives;
  int rectY;
  /* Hard-register pins (A374 / F14j): each holds the register the
     original uses for it, which settles the competing temps. */
  register int dragons asm("$2");
  int worldEggs;
  int keyState;
  int count;
  int size;
  int dst;
  int pad[2];
  FillWord(g_aHudIconActorRecords, 0, 0x420);
  i = 0;
  ff = 0xFF;
  pos = &HUD_ICON(0)->posX;
  rec = g_aHudIconActorRecords;
  off = 0;
  vec = 0;
icon_slot:
  HideActorRenderRecord((Actor *)rec);

  CopyVector(pos, HUD_REST_POS(vec));
  pos += sizeof(Actor) / sizeof(int);
  rec += ACTOR_SIZE;
  HUD_ICON(off)->otBias = OT_BIAS_SCREEN;
  HUD_ICON(off)->cullRadius = ff;
  HUD_ICON(off)->subMode = ff;
  HUD_ICON(off)->tintMode = TINT_HUD;
  off += ACTOR_SIZE;
  i += 1;
  vec += 0xC;
  if (i < 12) {
    goto icon_slot;
  }
  FillWord(g_anHudSpriteRects, 0, 0x100);
  rect = 0;
  do {
    HUD_RECT(g_anHudSpriteRects, rect)->x =
        HUD_RECT(g_anHudDigitRestRects, rect)->x;
    HUD_RECT(g_anHudSpriteRects, rect)->y =
        HUD_RECT(g_anHudDigitRestRects, rect)->y;
    HUD_RECT(g_anHudSpriteRects, rect)->w =
        HUD_RECT(g_anHudDigitRestRects, rect)->w;
    HUD_RECT(g_anHudSpriteRects, rect)->h =
        HUD_RECT(g_anHudDigitRestRects, rect)->h;
    rect += 8;
  } while (rect < 0x60);
  anchor = &HUD_ICON_SLOT(HUD_SLOT_GEMS)->type;
  idx = VOL(g_nLevelIndex);
  VOL(anchor[0]) = ACTOR_TYPE_HUD_GEM_ICON;
  VOL(HUD_ICON_SLOT(HUD_SLOT_DRAGONS)->type) = ACTOR_TYPE_HUD_DRAGON_ICON;
  VOL(HUD_ICON_SLOT(HUD_SLOT_LIVES)->type) = ACTOR_TYPE_HUD_LIVES_ICON;
  VOL(HUD_ICON_SLOT(HUD_SLOT_KEY)->type) = ACTOR_TYPE_KEY;
  VOL(HUD_ICON_SLOT(HUD_SLOT_KEY)->rotZ) = 0x40;
  VOL(g_abHudCounterDisplayState[HUD_CTR_GEMS]) = HUD_CTR_ROLL_IN;
  VOL(g_abHudCounterDisplayState[HUD_CTR_DRAGONS]) = HUD_CTR_ROLL_IN;
  VOL(g_abHudCounterDisplayState[HUD_CTR_LIVES]) = HUD_CTR_ROLL_IN;
  VOL(g_abHudCounterDisplayState[HUD_CTR_EGGS]) = HUD_CTR_ROLL_IN;
  VOL(g_abHudCounterDisplayState[HUD_CTR_KEY]) = HUD_CTR_ROLL_IN;
  dragons = VOL(g_nTotalDragonsRescued);
  worldEggs = VOL(g_nTotalEggsRecovered);
  keyState = VOL(g_nLevelKeyState);
  food = VOL(g_nSparxFoodCount);
  VOL(g_bHudLevelGemRollPhase) = 0;
  VOL(g_bHudDragonRollPhase) = 0;
  VOL(g_bHudLivesRollPhase) = 0;
  VOL(g_bHudEggRollPhase) = 0;
  VOL(g_bHudKeyRollPhase) = 0;
  VOL(g_nHudLevelGemHoldTimer) = 0;
  VOL(g_nHudDragonHoldTimer) = 0;
  VOL(g_nHudLivesHoldTimer) = 0;
  VOL(g_nHudEggHoldTimer) = 0;
  VOL(g_nHudKeyHoldTimer) = 0;
  gems = g_anLevelGemsCollected[idx];
  lives = VOL(g_nExtraLives);
  g_nHudDragonCachedCount = dragons;
  g_nHudEggCachedCount = worldEggs;
  g_nHudKeyCachedState = keyState;
  g_nHudButterflyCachedCount = food;
  g_nHudLivesCachedCount = lives;
  g_nHudLevelGemCachedCount = gems;
  SetHudCounterDigits(HUD_SLOT_GEM_DIGITS, 4, gems, HUD_DIGITS_SHOW);
  SetHudCounterDigits(HUD_SLOT_DRAGON_DIGITS, 2, g_nHudDragonCachedCount,
                      HUD_DIGITS_SHOW);
  SetHudCounterDigits(HUD_SLOT_LIVES_DIGITS, 2, g_nHudLivesCachedCount,
                      HUD_DIGITS_SHOW);
  if (layout_butterflies != 0) {
    g_nHudEggRollRequest = 0;
    i = 0;
    count = g_nSparxFoodCount;
    if (count > 0) {
      icon = &HUD_ICON_SLOT(HUD_SLOT_LIVES)->posX;
      size = 8;
      ring = 0;
      dst = 0x60;
      do {
        int x = icon[0];
        i += 1;
        HUD_RECT(g_anHudSpriteRects, dst)->x =
            (g_abHudButterflyRingXY[ring] + x) - 0x1D;
        x = icon[1];
        rectY = (g_abHudButterflyRingXY[ring + 1] + x) - 0x1C;
        HUD_RECT(g_anHudSpriteRects, dst)->w = size;
        HUD_RECT(g_anHudSpriteRects, dst)->h = size;
        HUD_RECT(g_anHudSpriteRects, dst)->y = rectY;
        ring += 2;
        dst += 8;
      } while (i < count);
    }
  }
}

#undef VOL

/* Matching notes (UpdateHudCounters).
   - The empty `do { } while (0);` after the head breaks cse's held-base fold
     (A200), so the gem machine keeps its 18 register-based accesses off one
     `la` of g_adwHudIconSpinAngleBlock.
   - The mod-9 cycle is split across that barrier: the quotient*9 is formed
     above it and the subtract/store below it, so the spin-angle re-read sits
     between the two as in the original.  `q9` is pinned to $2 (A374) so the
     re-read `idx` (a block-local) avoids v0 while q9 is still live; `q` is
     pinned to $3 and given an asm use so local-alloc does not hand it q9's
     v0 suggestion; the volatile asm (A382) keeps the subtract and store
     beside the re-read instead of below the cosine load.
   - Roll-phase/hold-timer stores go through one-field struct views (A426)
     and the butterfly ring is a byte-offset walk (A179/A35). */

/* 0x80054988, 3652 bytes — the per-frame HUD tick.  Turns the HUD icon spin
   angle (the gem icon spins on it and tilts by its cosine), then runs five
   independent roll-animation state machines (one per counter: level gems,
   dragons, lives+butterflies, world eggs, the level key) out of
   g_abHudCounterDisplayState[0..4].  Each machine is the same four states: 0
   idle (watch the live value), 1 roll-in (14 steps off g_anHudRollInStepTable),
   2 hold/ratchet (advance the cached count toward the live one, driving the
   digit glyphs through SetHudCounterDigits + CommitHudCounterAnimState), 3
   roll-out (14 steps off g_anHudRollOutStepTable).  Between the lives and egg
   machines it lays the sparx-food butterflies out around their ring, and it
   closes by converting every 20 butterflies into an extra life (capped at 99).
 */

/* Held-base views (A20/A22/A25).  The original materializes ONE base per group
   and reaches the neighbours off it; every other site of the same address stays
   on the plain absolute symbol, which is the per-site A240 split. */

/* the spin-angle base reaches the whole gem machine's state at -0x3C..-0x1C */
#define FIELDW(sym) (((struct { int v; } *)&(sym))->v)
#define FIELD(sym) (((struct { unsigned char v; } *)&(sym))->v)

#define HUD ((unsigned char *)g_adwHudIconSpinAngleBlock)
#define SPIN (*(unsigned int *)&HUD[0])
#define GEM_STATE (HUD[-0x3C])
#define GEM_PHASE (HUD[-0x37])
#define GEM_TIMER (*(int *)&HUD[-0x30])
#define GEM_CACHED (*(int *)&HUD[-0x1C])

/* the gem cached-count base reaches the roll phase at -0x1B */
#define GEMCACHE (g_anHudLevelGemCachedBlock[0])
#define GEMCACHE_PHASE (((unsigned char *)g_anHudLevelGemCachedBlock)[-0x1B])

/* the sprite-rect walk is a BYTE offset off the array, which keeps one
   scaled index register and the field constant in %lo(sym+K) (A179/A35) */

#define EGG_STATE (g_abHudEggIconBlock[0])
#define KEY_STATE (g_abHudKeyCounterBlock[0])

void UpdateHudCounters(void) {
  int nPhase;
  int nDelta;
  int i;
  int n;
  int j;
  unsigned char p;
  int k;
  int nTint;
  int nCos;
  int av[3];
  int pad[2];
  int t;
  register int q9 asm("$2");
  register int q asm("$3");
  unsigned int idx;

  t = g_nHudFrameCycleMod9 + 1;
  nPhase = (SPIN - g_nFrameStep) & 0xFF;
  SPIN = nPhase;
  q = t / 9;
  __asm__("" : : "r"(q));
  q9 = q * 9;
  EBB_BREAK();
  idx = g_dwHudIconSpinAngle;
  g_nHudFrameCycleMod9 = t - q9;
  SCHED_FENCE();
  nCos = g_anCosineLut[idx];
  HUD_ICON_SLOT(HUD_SLOT_GEMS)->rotZ = nPhase;
  HUD_ICON_SLOT(HUD_SLOT_GEMS)->rotY = nCos >> 9;

  /* --- counter 0: level gems ------------------------------------------ */
  if (GEM_STATE == HUD_CTR_HIDDEN) {
    if (GEM_CACHED != g_anLevelGemsCollected[g_nLevelIndex]) {
      GEM_STATE = HUD_CTR_ROLL_IN;
      GEM_PHASE = 0;
    }
  } else if (GEM_STATE == HUD_CTR_ROLL_IN) {
    p = g_bHudLevelGemRollPhase;
    if (p == HUD_ROLL_STEPS) {
      GEM_STATE = HUD_CTR_SHOWN;
      FIELD(g_bHudLevelGemRollPhase) = 0;
      GEM_TIMER = 0;
      if (GEM_CACHED == g_anLevelGemsCollected[g_nLevelIndex]) {
        GEM_TIMER = -HUD_HOLD_DELAY;
      }
    } else {
      FIELD(g_bHudLevelGemRollPhase) = p + 1;
      OffsetHudIconRollAxis(HUD_SLOT_GEM_DIGITS, 5, g_anHudRollInStepTable[p]);
    }
  } else if (GEM_STATE == HUD_CTR_SHOWN) {
    if (GEM_CACHED == g_anLevelGemsCollected[g_nLevelIndex] && GEM_PHASE == 0) {
      GEM_TIMER = GEM_TIMER + 1;
      if (GEM_TIMER == HUD_HOLD_FRAMES) {
        GEM_STATE = HUD_CTR_ROLL_OUT;
        GEM_PHASE = HUD_ROLL_STEPS;
      }
    } else {
      FIELDW(g_nHudLevelGemHoldTimer) = 0;
      if (g_anLevelGemsCollected[g_nLevelIndex] - g_nHudLevelGemCachedCount >=
              3 &&
          (g_bHudLevelGemRollPhase & 0x1F) == 0) {
        g_bHudLevelGemRollPhase -= 0x20;
      } else {
        g_abHudGemRollPhaseBlock[0] -= 0x10;
      }
      {
        int *gc;

        OPAQUE_COPY(gc, &GEMCACHE);
        SetHudCounterDigits(HUD_SLOT_GEM_DIGITS, 4, gc[0] + 1,
                            HUD_DIGITS_PENDING);
        CommitHudCounterAnimState(0, 4, ((unsigned char *)gc)[-0x1B]);
        nDelta = g_anLevelGemsCollected[g_nLevelIndex] - gc[0];
        if (nDelta > HUD_GEM_FAST_DELTA) {
          gc[0] = gc[0] + HUD_GEM_FAST_STEP;
        } else if (nDelta > HUD_GEM_STEP_DELTA ||
                   ((unsigned char *)gc)[-0x1B] == HUD_DIGIT_SWAP_PHASE) {
          gc[0] = gc[0] + 1;
        }
      }
      if (g_nHudLevelGemCachedCount == g_anLevelTotalGemsTable[g_nLevelIndex] &&
          g_bHudLevelGemRollPhase == 0) {
        g_abHudCounterDisplayState[HUD_CTR_GEMS] = HUD_CTR_ALL_GEMS;
      }
    }
  } else if (GEM_STATE == HUD_CTR_ROLL_OUT) {
    if (g_nHudLevelGemCachedCount != g_anLevelGemsCollected[g_nLevelIndex]) {
      GEM_STATE = HUD_CTR_ROLL_IN;
    } else if (g_bHudLevelGemRollPhase == 0) {
      GEM_STATE = HUD_CTR_HIDDEN;
    } else {
      {
        unsigned char t = g_bHudLevelGemRollPhase - 1;
        FIELD(g_bHudLevelGemRollPhase) = t;
        OffsetHudIconRollAxis(HUD_SLOT_GEM_DIGITS, 5,
                              g_anHudRollOutStepTable[t]);
      }
    }
  } else if (GEM_STATE == HUD_CTR_ALL_GEMS) {
    GEM_TIMER = GEM_TIMER + g_nFrameStep;
    if (GEM_TIMER >= HUD_ALL_GEMS_HOLD) {
      GEM_STATE = HUD_CTR_ROLL_OUT;
      GEM_PHASE = HUD_ROLL_STEPS;
    }
  }

  /* --- counter 1: dragons --------------------------------------------- */
  /* the RMWs stay byte-indexed: through the struct form gcc holds the
     address in a register instead of re-forming %hi/%lo for the store */
  HUD_ICON_BYTE(HUD_SLOT_DRAGONS, subMode) += 8;
  HUD_ICON_SLOT(HUD_SLOT_DRAGONS)->rotZ =
      (unsigned short)g_anCosineLut[HUD_ICON_SLOT(HUD_SLOT_DRAGONS)->subMode] >>
      8;
  if (g_abHudCounterDisplayState[HUD_CTR_DRAGONS] == HUD_CTR_ROLL_IN) {
    p = g_bHudDragonRollPhase;
    if (p == HUD_ROLL_STEPS) {
      g_abHudCounterDisplayState[HUD_CTR_DRAGONS] = HUD_CTR_SHOWN;
      g_bHudDragonRollPhase = 0;
      g_nHudDragonHoldTimer = -HUD_HOLD_DELAY;
    } else {
      FIELD(g_bHudDragonRollPhase) = p + 1;
      OffsetHudIconRollAxis(HUD_SLOT_DRAGON_DIGITS, 3,
                            g_anHudRollInStepTable[p]);
    }
  } else if (g_abHudCounterDisplayState[HUD_CTR_DRAGONS] == HUD_CTR_SHOWN) {
    g_nHudDragonHoldTimer = g_nHudDragonHoldTimer + 1;
    if (g_nHudDragonHoldTimer == HUD_HOLD_FRAMES) {
      g_abHudCounterDisplayState[HUD_CTR_DRAGONS] = HUD_CTR_ROLL_OUT;
      g_bHudDragonRollPhase = HUD_ROLL_STEPS;
    }
  } else if (g_abHudCounterDisplayState[HUD_CTR_DRAGONS] == HUD_CTR_ROLL_OUT) {
    if (g_bHudDragonRollPhase == 0) {
      g_abHudCounterDisplayState[HUD_CTR_DRAGONS] = HUD_CTR_HIDDEN;
    } else {
      {
        unsigned char t = g_bHudDragonRollPhase - 1;
        FIELD(g_bHudDragonRollPhase) = t;
        OffsetHudIconRollAxis(HUD_SLOT_DRAGON_DIGITS, 3,
                              g_anHudRollOutStepTable[t]);
      }
    }
  }

  /* --- counter 2: lives + sparx butterflies ---------------------------- */
  HUD_ICON_SLOT(HUD_SLOT_LIVES)->rotZ = g_dwHudIconSpinAngle;
  if (g_abHudCounterDisplayState[HUD_CTR_LIVES] == HUD_CTR_HIDDEN) {
    if (g_nHudLivesCachedCount != g_nExtraLives ||
        g_nHudButterflyCachedCount != g_nSparxFoodCount) {
      g_abHudCounterDisplayState[HUD_CTR_LIVES] = HUD_CTR_ROLL_IN;
      g_bHudLivesRollPhase = 0;
    }
  } else if (g_abHudCounterDisplayState[HUD_CTR_LIVES] == HUD_CTR_ROLL_IN) {
    p = g_bHudLivesRollPhase;
    if (p == HUD_ROLL_STEPS) {
      g_abHudCounterDisplayState[HUD_CTR_LIVES] = HUD_CTR_SHOWN;
      g_bHudLivesRollPhase = 0;
      g_nHudLivesHoldTimer = 0;
      if (g_nHudLivesCachedCount == g_nExtraLives &&
          g_nHudButterflyCachedCount == g_nSparxFoodCount) {
        g_nHudLivesHoldTimer = -HUD_HOLD_DELAY;
      }
    } else {
      FIELD(g_bHudLivesRollPhase) = p + 1;
      OffsetHudIconRollAxis(HUD_SLOT_LIVES_DIGITS, 3,
                            g_anHudRollInStepTable[p]);
    }
  } else if (g_abHudCounterDisplayState[HUD_CTR_LIVES] == HUD_CTR_SHOWN) {
    if (g_nHudLivesCachedCount == g_nExtraLives && g_bHudLivesRollPhase == 0) {
      if (g_nHudButterflyCachedCount == g_nSparxFoodCount) {
        g_nHudLivesHoldTimer = g_nHudLivesHoldTimer + 1;
        if (g_nHudLivesHoldTimer == HUD_HOLD_FRAMES) {
          g_abHudCounterDisplayState[HUD_CTR_LIVES] = HUD_CTR_ROLL_OUT;
          g_bHudLivesRollPhase = HUD_ROLL_STEPS;
        }
      } else {
        g_nHudLivesHoldTimer = 0;
        g_nHudButterflyCachedCount = g_nSparxFoodCount;
      }
    } else {
      g_nHudLivesHoldTimer = 0;
      g_bHudLivesRollPhase -= 0x10;
      SetHudCounterDigits(HUD_SLOT_LIVES_DIGITS, 2, g_nHudLivesCachedCount + 1,
                          HUD_DIGITS_PENDING);
      CommitHudCounterAnimState(8, 2, g_bHudLivesRollPhase);
      if (g_bHudLivesRollPhase == HUD_DIGIT_SWAP_PHASE) {
        g_nHudLivesCachedCount += 1;
      }
    }
  } else if (g_abHudCounterDisplayState[HUD_CTR_LIVES] == HUD_CTR_ROLL_OUT) {
    if (g_nHudLivesCachedCount != g_nExtraLives ||
        g_nHudButterflyCachedCount != g_nSparxFoodCount) {
      g_abHudCounterDisplayState[HUD_CTR_LIVES] = HUD_CTR_ROLL_IN;
      g_nHudButterflyCachedCount = g_nSparxFoodCount;
    } else if (g_bHudLivesRollPhase == 0) {
      g_abHudCounterDisplayState[HUD_CTR_LIVES] = HUD_CTR_HIDDEN;
    } else {
      {
        unsigned char t = g_bHudLivesRollPhase - 1;
        FIELD(g_bHudLivesRollPhase) = t;
        OffsetHudIconRollAxis(HUD_SLOT_LIVES_DIGITS, 3,
                              g_anHudRollOutStepTable[t]);
      }
    }
  }

  /* lay the butterflies out around their ring anchor */
  if (g_anHudButterflyCachedBlock[0] > 0) {
    n = 0;
    do {
      k = n * 8 + 0x60;
      j = n * 2;
      HUD_RECT(g_anHudSpriteRects, k)->x =
          (g_anHudButterflyCachedBlock[HUD_BFLY_TO_LIVES_ICON_X] +
           g_abHudButterflyRingXY[j]) -
          0x1D;
      HUD_RECT(g_anHudSpriteRects, k)->y =
          (g_anHudButterflyCachedBlock[HUD_BFLY_TO_LIVES_ICON_Y] +
           g_abHudButterflyRingXY[j + 1]) -
          0x1C;
      HUD_RECT(g_anHudSpriteRects, k)->w = 8;
      HUD_RECT(g_anHudSpriteRects, k)->h = 8;
      n += 1;
    } while (n < g_anHudButterflyCachedBlock[0]);
  }

  /* --- counter 3: world eggs ------------------------------------------ */
  switch (EGG_STATE) {
  case HUD_CTR_HIDDEN:
    if (g_nHudEggCachedCount != g_nTotalEggsRecovered ||
        g_nHudEggRollRequest != 0) {
      EGG_STATE = HUD_CTR_ROLL_IN;
      g_bHudEggRollPhase = 0;
    }
    break;
  case HUD_CTR_ROLL_IN:
    p = g_bHudEggRollPhase;
    if (p == HUD_ROLL_STEPS) {
      EGG_STATE = HUD_CTR_SHOWN;
      g_nHudEggHoldTimer = 0;
      if (g_nHudEggCachedCount == g_nTotalEggsRecovered &&
          g_nHudEggRollRequest == 0) {
        g_nHudEggHoldTimer = -HUD_HOLD_DELAY;
      }
    } else {
      FIELD(g_bHudEggRollPhase) = p + 1;
      OffsetHudDigitRollAxis(0, 0xC, -g_anHudRollInStepTable[p]);
    }
    break;
  case HUD_CTR_SHOWN:
    if (g_nHudEggCachedCount == g_nTotalEggsRecovered) {
      if (g_nHudEggRollRequest != 0) {
        g_nHudEggHoldTimer = 0;
      } else if (g_nHudEggHoldTimer == HUD_HOLD_FRAMES) {
        EGG_STATE = HUD_CTR_ROLL_OUT;
        g_bHudEggRollPhase = HUD_ROLL_STEPS;
      }
    } else if (g_nHudEggHoldTimer == 1) {
      av[0] = g_nHudEggCachedCount * 0x144 - 0x9C4;
      av[1] = 0x3F0;
      av[2] = 0x1000;
      g_pfnLevelEmitParticles(0x10, EMIT_DRIFT_STREAK, av, 0);
    } else if (g_nHudEggHoldTimer >= 9) {
      g_nHudEggCachedCount += 1;
      g_nHudEggHoldTimer = 0;
    }
    g_anHudEggTimerBlock[0] = g_anHudEggTimerBlock[0] + 1;
    break;
  case HUD_CTR_ROLL_OUT:
    if (g_nHudEggCachedCount != g_nTotalEggsRecovered ||
        g_nHudEggRollRequest != 0) {
      EGG_STATE = HUD_CTR_ROLL_IN;
    } else if (g_bHudEggRollPhase == 0) {
      EGG_STATE = HUD_CTR_HIDDEN;
    } else {
      {
        unsigned char t = g_bHudEggRollPhase - 1;
        FIELD(g_bHudEggRollPhase) = t;
        OffsetHudDigitRollAxis(0, 0xC, -g_anHudRollOutStepTable[t]);
      }
    }
    break;
  }

  /* --- counter 4: the level key (g_nLevelKeyState) ------------------ */
  if (*(volatile int *)&g_nLevelKeyState != KEYSTATE_CARRIED) {
    g_nHudKeyCachedState = g_nLevelKeyState;
  }
  switch (KEY_STATE) {
  case HUD_CTR_HIDDEN:
    if (g_nHudKeyCachedState != g_nLevelKeyState) {
      KEY_STATE = HUD_CTR_ROLL_IN;
      g_bHudKeyRollPhase = 0;
    }
    break;
  case HUD_CTR_ROLL_IN:
    p = g_bHudKeyRollPhase;
    if (p == HUD_ROLL_STEPS) {
      KEY_STATE = HUD_CTR_SHOWN;
      g_nHudKeyHoldTimer = 0;
      if (g_nHudKeyCachedState == g_nLevelKeyState) {
        g_nHudKeyHoldTimer = -HUD_HOLD_DELAY;
      }
    } else {
      FIELD(g_bHudKeyRollPhase) = p + 1;
      OffsetHudIconRollAxis(HUD_SLOT_KEY, 1, -g_anHudRollInStepTable[p]);
    }
    break;
  case HUD_CTR_SHOWN:
    if (g_nHudKeyCachedState != g_nLevelKeyState) {
      if (g_nHudKeyHoldTimer == 0) {
        i = 0;
        do {
          g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK,
                                  HUD_ICON_SLOT(HUD_SLOT_KEY),
                                  ((i + 6) << 24) + 0x8080);
          i += 1;
        } while (i < 6);
      } else if (g_nHudKeyHoldTimer >= 0xC) {
        HUD_ICON_SLOT(HUD_SLOT_KEY)->subMode = 0x40;
        g_nHudKeyHoldTimer = 0;
        g_nHudKeyCachedState = g_nLevelKeyState;
      }
    } else if (g_nHudKeyHoldTimer == HUD_HOLD_FRAMES) {
      KEY_STATE = HUD_CTR_ROLL_OUT;
      g_bHudKeyRollPhase = HUD_ROLL_STEPS;
    }
    g_anHudKeyTimerBlock[0] = g_anHudKeyTimerBlock[0] + 1;
    break;
  case HUD_CTR_ROLL_OUT:
    if (g_nHudKeyCachedState != g_nLevelKeyState) {
      KEY_STATE = HUD_CTR_ROLL_IN;
      g_bHudKeyRollPhase = 0;
    } else if (g_bHudKeyRollPhase == 0) {
      KEY_STATE = HUD_CTR_HIDDEN;
    } else {
      {
        unsigned char t = g_bHudKeyRollPhase - 1;
        FIELD(g_bHudKeyRollPhase) = t;
        OffsetHudIconRollAxis(HUD_SLOT_KEY, 1, -g_anHudRollOutStepTable[t]);
      }
    }
    break;
  }

  /* --- the key icon spin, and butterflies -> extra lives -------------- */
  HUD_ICON_BYTE(HUD_SLOT_KEY, subMode) += 4;
  HUD_ICON_SLOT(HUD_SLOT_KEY)->rotX =
      (unsigned short)g_anCosineLut[HUD_ICON_SLOT(HUD_SLOT_KEY)->subMode] >> 7;
  HUD_ICON_SLOT(HUD_SLOT_KEY)->rotY =
      (unsigned short)g_anSineLut[HUD_ICON_SLOT(HUD_SLOT_KEY)->subMode] >> 7;
  if (g_nSparxFoodCount >= SPARX_FOOD_PER_LIFE) {
    g_nExtraLives = g_nExtraLives + 1;
    if (g_nExtraLives > MAX_EXTRA_LIVES) {
      g_nExtraLives = MAX_EXTRA_LIVES;
    }
    g_nSparxFoodCount = g_nSparxFoodCount - SPARX_FOOD_PER_LIFE;
  }
}

#undef FIELDW
#undef FIELD
#undef HUD
#undef GEM_STATE
#undef GEM_PHASE
#undef GEM_TIMER
#undef GEM_CACHED
#undef GEMCACHE
#undef GEMCACHE_PHASE
#undef EGG_STATE
#undef KEY_STATE

/* The per-digit sub-record hung off a digit actor's ->state pointer. */
struct Digit {
  char pad00[4];
  int place;  /* 0x04 digit value scaled by its decimal place */
  int dir[3]; /* 0x08 camera-facing unit direction */
};

/* SpawnFloatingDigitActors - VMA 0x800557cc, 400 bytes.

   Spawn one floating digit actor (id 0x104 + digit) per decimal digit of
   `value`, least significant first, walking away from `src` along the camera
   yaw so the digits read left to right on screen. Each digit carries the
   camera-facing direction (cos/sin of yaw - 0x400, plus a 0x80 z that steps
   down 0x10 per digit) and its own decimal place.

   Matching notes:
   - The overlay init hook takes TWO arguments (m2c reads the divide's
     `mfhi $a2` as a third), and the digit split is `d = value % 10;
     q = value / 10;` in that order, which gives the original's `move s0,s1`.
   - Each LUT load goes through an empty asm copy so combine cannot fuse the
     sign-extend into the shift (B13: `lh; sra 4`, not `lhu; sll; sra`).
   - The yaw store's load goes through a one-field struct view (A400) so it
     depends on the in-struct `rec->posZ` store and z lands in v1.
   - Tail: both LUT loads, then the rot[2] RMW, then a volatile barrier, then
     the asm copies. The birthing-boosted rot[2] reload is placed in the
     slot after its addiu's predecessor; with the cos asm copy below the
     barrier that slot goes to the cos LOAD, so the reload precedes it in
     sched1, idx2/cos take a0 and rot[2] v0, and sched2 can hoist the lhu up
     to the yaw lbu. The barrier also keeps the sh above the x/y updates
     (a free store would win the hazard tie and sink to the delay slot). */
void SpawnFloatingDigitActors(int value, Actor *src) {
  short faceDir[3];
  Actor *rec;
  struct Digit *st;
  int x;
  int y;
  int place;
  int digit;
  int yawIdx;
  int z;
  short *cam;
  int sinYaw;
  int cosYaw;
  int prod;
  cam = g_anCameraEulerYawBlock;
  place = 1;
  x = src->posX;
  y = src->posY;
  faceDir[0] = LookupCosine(cam[0] - 0x400) >> 8;
  faceDir[1] = LookupSine(cam[0] - 0x400) >> 8;
  faceDir[2] = 0x80;
  if (value != 0) {
    do {
      int q;
      digit = value % 10;
      q = value / 10;
      prod = digit + ACTOR_TYPE_GEM_DIGIT_0;
      rec = g_pfnLevelSpawnActor(prod, 0);
      rec->posX = x;
      rec->posY = y;
      z = src->posZ;
      prod = digit * place;
      rec->rotX = 0;
      rec->rotY = 0;
      rec->posZ = z;
      st = (struct Digit *)rec->state;
      rec->rotZ = ((struct { unsigned short v; } *)&g_nCameraEulerYaw)->v >> 4;
      st->dir[0] = faceDir[0];
      st->dir[1] = faceDir[1];
      st->dir[2] = faceDir[2];
      value = q;
      place *= 10;
      st->place = prod;
      yawIdx = rec->rotZ;
      sinYaw = g_anSineLut[yawIdx];
      cosYaw = g_anCosineLut[yawIdx];
      faceDir[2] -= 0x10;
      SCHED_FENCE();
      OPAQUE(sinYaw);
      OPAQUE(cosYaw);
      x -= sinYaw >> 4;
      y += cosYaw >> 4;
    } while (value != 0);
  }
}
