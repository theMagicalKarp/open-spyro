/* Matching notes (UpdateHudCounters).
   - The empty `do { } while (0);` after the head breaks cse's held-base fold
     (A200), so the gem machine keeps its 18 register-based accesses off one
     `la` of g_adwHudBobSinePhaseBlock.
   - The mod-9 cycle is split across that barrier: the quotient*9 is formed
     above it and the subtract/store below it, so the bob-phase re-read sits
     between the two as in the original.  `q9` is pinned to $2 (A374) so the
     re-read `idx` (a block-local) avoids v0 while q9 is still live; `q` is
     pinned to $3 and given an asm use so local-alloc does not hand it q9's
     v0 suggestion; the volatile asm (A382) keeps the subtract and store
     beside the re-read instead of below the cosine load.
   - Roll-phase/hold-timer stores go through one-field struct views (A426)
     and the butterfly ring is a byte-offset walk (A179/A35). */
#include "funcs.h"
#include "globals.h"

/* 0x80054988, 3652 bytes — the per-frame HUD tick.  Bobs the two sine-driven
   icon anchors, then runs five independent roll-animation state machines (one
   per counter: level gems, dragons, lives+butterflies, world eggs, level eggs)
   out of g_abHudCounterDisplayState[0..4].  Each machine is the same four
   states: 0 idle (watch the live value), 1 roll-in (14 steps off
   g_anHudRollInStepTable), 2 hold/ratchet (advance the cached count toward the
   live one, driving the digit glyphs through SetHudCounterDigits +
   CommitHudCounterAnimState), 3 roll-out (14 steps off
   g_anHudRollOutStepTable).  Between the lives and world-egg machines it lays
   the sparx-food butterflies out around their ring, and it closes by converting
   every 20 butterflies into an extra life (capped at 99). */

/* Held-base views (A20/A22/A25).  The original materializes ONE base per group
   and reaches the neighbours off it; every other site of the same address stays
   on the plain absolute symbol, which is the per-site A240 split. */

/* the bob-phase base reaches the whole gem machine's state at -0x3C..-0x1C */
#define FIELDW(sym) (((struct { int v; } *)&(sym))->v)
#define FIELD(sym) (((struct { unsigned char v; } *)&(sym))->v)

#define HUD ((unsigned char *)g_adwHudBobSinePhaseBlock)
#define BOB (*(unsigned int *)&HUD[0])
#define GEM_STATE (HUD[-0x3C])
#define GEM_PHASE (HUD[-0x37])
#define GEM_TIMER (*(int *)&HUD[-0x30])
#define GEM_CACHED (*(int *)&HUD[-0x1C])

/* the gem cached-count base reaches the roll phase at -0x1B */
#define GEMCACHE (g_anHudLevelGemCachedBlock[0])
#define GEMCACHE_PHASE (((unsigned char *)g_anHudLevelGemCachedBlock)[-0x1B])

/* the sprite-rect walk is a BYTE offset off the array, which keeps one
   scaled index register and the field constant in %lo(sym+K) (A179/A35) */
#define RECT(off) (*(short *)((char *)g_anHudSpriteRects + (off)))

#define EGG_STATE (g_abHudWorldEggIconBlock[0])
#define LEGG_STATE (g_abHudLevelEggCounterBlock[0])

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
  nPhase = (BOB - g_nFrameStep) & 0xFF;
  BOB = nPhase;
  q = t / 9;
  __asm__("" : : "r"(q));
  q9 = q * 9;
  do {
  } while (0);
  idx = g_dwHudBobSinePhase;
  g_nHudFrameCycleMod9 = t - q9;
  __asm__ volatile("");
  nCos = g_anCosineLut[idx];
  g_abHudIconActorRecords[0x1A6] = nPhase;
  g_abHudIconActorRecords[0x1A5] = nCos >> 9;

  /* --- counter 0: level gems ------------------------------------------ */
  if (GEM_STATE == 0) {
    if (GEM_CACHED != g_anLevelGemsCollected[g_nLevelIntroIndex]) {
      GEM_STATE = 1;
      GEM_PHASE = 0;
    }
  } else if (GEM_STATE == 1) {
    p = g_bHudLevelGemRollPhase;
    if (p == 0xD) {
      GEM_STATE = 2;
      FIELD(g_bHudLevelGemRollPhase) = 0;
      GEM_TIMER = 0;
      if (GEM_CACHED == g_anLevelGemsCollected[g_nLevelIntroIndex]) {
        GEM_TIMER = -0x28;
      }
    } else {
      FIELD(g_bHudLevelGemRollPhase) = p + 1;
      OffsetHudIconRollAxis(0, 5, g_anHudRollInStepTable[p]);
    }
  } else if (GEM_STATE == 2) {
    if (GEM_CACHED == g_anLevelGemsCollected[g_nLevelIntroIndex] &&
        GEM_PHASE == 0) {
      GEM_TIMER = GEM_TIMER + 1;
      if (GEM_TIMER == 0x14) {
        GEM_STATE = 3;
        GEM_PHASE = 0xD;
      }
    } else {
      FIELDW(g_nHudLevelGemHoldTimer) = 0;
      if (g_anLevelGemsCollected[g_nLevelIntroIndex] -
                  g_nHudLevelGemCachedCount >=
              3 &&
          (g_bHudLevelGemRollPhase & 0x1F) == 0) {
        g_bHudLevelGemRollPhase -= 0x20;
      } else {
        g_abHudGemRollPhaseBlock[0] -= 0x10;
      }
      {
        int *gc;

        __asm__("" : "=r"(gc) : "0"(&GEMCACHE));
        SetHudCounterDigits(0, 4, gc[0] + 1, 0);
        CommitHudCounterAnimState(0, 4, ((unsigned char *)gc)[-0x1B]);
        nDelta = g_anLevelGemsCollected[g_nLevelIntroIndex] - gc[0];
        if (nDelta >= 0xC9) {
          gc[0] = gc[0] + 8;
        } else if (nDelta >= 0x15 || ((unsigned char *)gc)[-0x1B] == 0xC0) {
          gc[0] = gc[0] + 1;
        }
      }
      if (g_nHudLevelGemCachedCount ==
              g_anLevelTotalGemsTable[g_nLevelIntroIndex] &&
          g_bHudLevelGemRollPhase == 0) {
        g_abHudCounterDisplayState[0] = 4;
      }
    }
  } else if (GEM_STATE == 3) {
    if (g_nHudLevelGemCachedCount !=
        g_anLevelGemsCollected[g_nLevelIntroIndex]) {
      GEM_STATE = 1;
    } else if (g_bHudLevelGemRollPhase == 0) {
      GEM_STATE = 0;
    } else {
      {
        unsigned char t = g_bHudLevelGemRollPhase - 1;
        FIELD(g_bHudLevelGemRollPhase) = t;
        OffsetHudIconRollAxis(0, 5, g_anHudRollOutStepTable[t]);
      }
    }
  } else if (GEM_STATE == 4) {
    GEM_TIMER = GEM_TIMER + g_nFrameStep;
    if (GEM_TIMER >= 0xF0) {
      GEM_STATE = 3;
      GEM_PHASE = 0xD;
    }
  }

  /* --- counter 1: dragons --------------------------------------------- */
  g_abHudIconActorRecords[0x2B1] += 8;
  g_abHudIconActorRecords[0x2AE] =
      (unsigned short)g_anCosineLut[g_abHudIconActorRecords[0x2B1]] >> 8;
  if (g_abHudCounterDisplayState[1] == 1) {
    p = g_bHudDragonRollPhase;
    if (p == 0xD) {
      g_abHudCounterDisplayState[1] = 2;
      g_bHudDragonRollPhase = 0;
      g_nHudDragonHoldTimer = -0x28;
    } else {
      FIELD(g_bHudDragonRollPhase) = p + 1;
      OffsetHudIconRollAxis(5, 3, g_anHudRollInStepTable[p]);
    }
  } else if (g_abHudCounterDisplayState[1] == 2) {
    g_nHudDragonHoldTimer = g_nHudDragonHoldTimer + 1;
    if (g_nHudDragonHoldTimer == 0x14) {
      g_abHudCounterDisplayState[1] = 3;
      g_bHudDragonRollPhase = 0xD;
    }
  } else if (g_abHudCounterDisplayState[1] == 3) {
    if (g_bHudDragonRollPhase == 0) {
      g_abHudCounterDisplayState[1] = 0;
    } else {
      {
        unsigned char t = g_bHudDragonRollPhase - 1;
        FIELD(g_bHudDragonRollPhase) = t;
        OffsetHudIconRollAxis(5, 3, g_anHudRollOutStepTable[t]);
      }
    }
  }

  /* --- counter 2: lives + sparx butterflies ---------------------------- */
  g_abHudIconActorRecords[0x3B6] = g_dwHudBobSinePhase;
  if (g_abHudCounterDisplayState[2] == 0) {
    if (g_nHudLivesCachedCount != g_nExtraLives ||
        g_nHudButterflyCachedCount != g_nSparxFoodCount) {
      g_abHudCounterDisplayState[2] = 1;
      g_bHudLivesRollPhase = 0;
    }
  } else if (g_abHudCounterDisplayState[2] == 1) {
    p = g_bHudLivesRollPhase;
    if (p == 0xD) {
      g_abHudCounterDisplayState[2] = 2;
      g_bHudLivesRollPhase = 0;
      g_nHudLivesHoldTimer = 0;
      if (g_nHudLivesCachedCount == g_nExtraLives &&
          g_nHudButterflyCachedCount == g_nSparxFoodCount) {
        g_nHudLivesHoldTimer = -0x28;
      }
    } else {
      FIELD(g_bHudLivesRollPhase) = p + 1;
      OffsetHudIconRollAxis(8, 3, g_anHudRollInStepTable[p]);
    }
  } else if (g_abHudCounterDisplayState[2] == 2) {
    if (g_nHudLivesCachedCount == g_nExtraLives && g_bHudLivesRollPhase == 0) {
      if (g_nHudButterflyCachedCount == g_nSparxFoodCount) {
        g_nHudLivesHoldTimer = g_nHudLivesHoldTimer + 1;
        if (g_nHudLivesHoldTimer == 0x14) {
          g_abHudCounterDisplayState[2] = 3;
          g_bHudLivesRollPhase = 0xD;
        }
      } else {
        g_nHudLivesHoldTimer = 0;
        g_nHudButterflyCachedCount = g_nSparxFoodCount;
      }
    } else {
      g_nHudLivesHoldTimer = 0;
      g_bHudLivesRollPhase -= 0x10;
      SetHudCounterDigits(8, 2, g_nHudLivesCachedCount + 1, 0);
      CommitHudCounterAnimState(8, 2, g_bHudLivesRollPhase);
      if (g_bHudLivesRollPhase == 0xC0) {
        g_nHudLivesCachedCount += 1;
      }
    }
  } else if (g_abHudCounterDisplayState[2] == 3) {
    if (g_nHudLivesCachedCount != g_nExtraLives ||
        g_nHudButterflyCachedCount != g_nSparxFoodCount) {
      g_abHudCounterDisplayState[2] = 1;
      g_nHudButterflyCachedCount = g_nSparxFoodCount;
    } else if (g_bHudLivesRollPhase == 0) {
      g_abHudCounterDisplayState[2] = 0;
    } else {
      {
        unsigned char t = g_bHudLivesRollPhase - 1;
        FIELD(g_bHudLivesRollPhase) = t;
        OffsetHudIconRollAxis(8, 3, g_anHudRollOutStepTable[t]);
      }
    }
  }

  /* lay the butterflies out around their ring anchor */
  if (g_anHudButterflyCachedBlock[0] > 0) {
    n = 0;
    do {
      k = n * 8 + 0x60;
      j = n * 2;
      RECT(k) =
          (g_anHudButterflyCachedBlock[0xE2] + g_abHudButterflyRingXY[j]) -
          0x1D;
      RECT(k + 2) =
          (g_anHudButterflyCachedBlock[0xE3] + g_abHudButterflyRingXY[j + 1]) -
          0x1C;
      RECT(k + 4) = 8;
      RECT(k + 6) = 8;
      n += 1;
    } while (n < g_anHudButterflyCachedBlock[0]);
  }

  /* --- counter 3: world eggs ------------------------------------------ */
  switch (EGG_STATE) {
  case 0:
    if (g_nHudWorldEggCachedCount != g_nWorldEggsRecovered ||
        g_nHudWorldEggRollRequest != 0) {
      EGG_STATE = 1;
      g_bHudWorldEggRollPhase = 0;
    }
    break;
  case 1:
    p = g_bHudWorldEggRollPhase;
    if (p == 0xD) {
      EGG_STATE = 2;
      g_nHudWorldEggHoldTimer = 0;
      if (g_nHudWorldEggCachedCount == g_nWorldEggsRecovered &&
          g_nHudWorldEggRollRequest == 0) {
        g_nHudWorldEggHoldTimer = -0x28;
      }
    } else {
      FIELD(g_bHudWorldEggRollPhase) = p + 1;
      OffsetHudDigitRollAxis(0, 0xC, -g_anHudRollInStepTable[p]);
    }
    break;
  case 2:
    if (g_nHudWorldEggCachedCount == g_nWorldEggsRecovered) {
      if (g_nHudWorldEggRollRequest != 0) {
        g_nHudWorldEggHoldTimer = 0;
      } else if (g_nHudWorldEggHoldTimer == 0x14) {
        EGG_STATE = 3;
        g_bHudWorldEggRollPhase = 0xD;
      }
    } else if (g_nHudWorldEggHoldTimer == 1) {
      av[0] = g_nHudWorldEggCachedCount * 0x144 - 0x9C4;
      av[1] = 0x3F0;
      av[2] = 0x1000;
      (*(void (*)(int, int, int *, int))g_pfnLevelEmitParticles)(0x10, 0x4D, av,
                                                                 0);
    } else if (g_nHudWorldEggHoldTimer >= 9) {
      g_nHudWorldEggCachedCount += 1;
      g_nHudWorldEggHoldTimer = 0;
    }
    g_anHudWorldEggTimerBlock[0] = g_anHudWorldEggTimerBlock[0] + 1;
    break;
  case 3:
    if (g_nHudWorldEggCachedCount != g_nWorldEggsRecovered ||
        g_nHudWorldEggRollRequest != 0) {
      EGG_STATE = 1;
    } else if (g_bHudWorldEggRollPhase == 0) {
      EGG_STATE = 0;
    } else {
      {
        unsigned char t = g_bHudWorldEggRollPhase - 1;
        FIELD(g_bHudWorldEggRollPhase) = t;
        OffsetHudDigitRollAxis(0, 0xC, -g_anHudRollOutStepTable[t]);
      }
    }
    break;
  }

  /* --- counter 4: level eggs ------------------------------------------ */
  if (*(volatile int *)&g_nLevelEggCount != 1) {
    g_nHudLevelEggCachedCount = g_nLevelEggCount;
  }
  switch (LEGG_STATE) {
  case 0:
    if (g_nHudLevelEggCachedCount != g_nLevelEggCount) {
      LEGG_STATE = 1;
      g_bHudLevelEggRollPhase = 0;
    }
    break;
  case 1:
    p = g_bHudLevelEggRollPhase;
    if (p == 0xD) {
      LEGG_STATE = 2;
      g_nHudLevelEggHoldTimer = 0;
      if (g_nHudLevelEggCachedCount == g_nLevelEggCount) {
        g_nHudLevelEggHoldTimer = -0x28;
      }
    } else {
      FIELD(g_bHudLevelEggRollPhase) = p + 1;
      OffsetHudIconRollAxis(0xB, 1, -g_anHudRollInStepTable[p]);
    }
    break;
  case 2:
    if (g_nHudLevelEggCachedCount != g_nLevelEggCount) {
      if (g_nHudLevelEggHoldTimer == 0) {
        i = 0;
        do {
          (*(void (*)(int, int, unsigned char *, int))g_pfnLevelEmitParticles)(
              1, 0xC, &g_abHudIconActorRecords[0x3C8],
              ((i + 6) << 24) + 0x8080);
          i += 1;
        } while (i < 6);
      } else if (g_nHudLevelEggHoldTimer >= 0xC) {
        g_abHudIconActorRecords[0x411] = 0x40;
        g_nHudLevelEggHoldTimer = 0;
        g_nHudLevelEggCachedCount = g_nLevelEggCount;
      }
    } else if (g_nHudLevelEggHoldTimer == 0x14) {
      LEGG_STATE = 3;
      g_bHudLevelEggRollPhase = 0xD;
    }
    g_anHudLevelEggTimerBlock[0] = g_anHudLevelEggTimerBlock[0] + 1;
    break;
  case 3:
    if (g_nHudLevelEggCachedCount != g_nLevelEggCount) {
      LEGG_STATE = 1;
      g_bHudLevelEggRollPhase = 0;
    } else if (g_bHudLevelEggRollPhase == 0) {
      LEGG_STATE = 0;
    } else {
      {
        unsigned char t = g_bHudLevelEggRollPhase - 1;
        FIELD(g_bHudLevelEggRollPhase) = t;
        OffsetHudIconRollAxis(0xB, 1, -g_anHudRollOutStepTable[t]);
      }
    }
    break;
  }

  /* --- the level-egg icon spin, and butterflies -> extra lives --------- */
  g_abHudIconActorRecords[0x411] += 4;
  g_abHudIconActorRecords[0x40C] =
      (unsigned short)g_anCosineLut[g_abHudIconActorRecords[0x411]] >> 7;
  g_abHudIconActorRecords[0x40D] =
      (unsigned short)g_anSineLut[g_abHudIconActorRecords[0x411]] >> 7;
  if (g_nSparxFoodCount >= 0x14) {
    g_nExtraLives = g_nExtraLives + 1;
    if (g_nExtraLives >= 0x64) {
      g_nExtraLives = 0x63;
    }
    g_nSparxFoodCount = g_nSparxFoodCount - 0x14;
  }
}
