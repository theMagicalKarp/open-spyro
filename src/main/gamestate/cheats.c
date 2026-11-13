#include "actor_cutscene.h"
#include "camera.h"
#include "cd_stream.h"
#include "funcs.h"
#include "gem_tally.h"
#include "globals.h"
#include "level_id.h"
#include "level_sfx.h"
#include "main_shared.h"
#include "pad_buttons.h"
#include "sample_bank.h"
#include "spyro_state.h"
#include "vecmath.h"

/* Zero the 16-slot pad-press history ring and reset its write index. */
void ClearPadHistoryRing(void) {
  int i;

  for (i = 15; i >= 0; i--) {
    g_anPadHistoryRing[i] = 0;
  }
  g_nPadHistoryIndex = 0;
}

/* Level-select (cheat menu) pad handler (0x8002d580, 0x290 bytes).
 * Circle/Cross/Square/Triangle/Right/Down/Left pick a slot 0-6; Up steps
 * g_nNextLevelId to the next level (wrapping past sub-level 5 into the
 * next world, and past the last level back to 10) and arms its world.
 * With a world already chosen, a slot press either reverts (out of range /
 * sub-level >= 6) or tears down into the level-load transition (gamestate
 * GS_LEVEL_LOAD, CD stream kick, level-transition camera); with no world
 * chosen the slot picks the world. */
void HandleLevelSelectInput(void) {
  int pad;
  int sel;
  pad = g_dwPadPressed;
  if (pad != 0) {
    sel = -1;
    switch (pad) {
    case PAD_CIRCLE:
      sel = 0;
      break;
    case PAD_CROSS:
      sel = 1;
      break;
    case PAD_SQUARE:
      sel = 2;
      break;
    case PAD_TRIANGLE:
      sel = 3;
      break;
    case PAD_RIGHT:
      sel = 4;
      break;
    case PAD_DOWN:
      sel = 5;
      break;
    case PAD_LEFT:
      sel = 6;
      break;
    case PAD_UP:
      g_nNextLevelId = g_nActiveLevelId + 1;
      if (LEVEL_IN_WORLD(g_nNextLevelId) >= LEVELS_PER_WORLD) {
        g_nNextLevelId = (g_nNextLevelId / 10 + 1) * 10;
      }
      if ((unsigned int)(g_nNextLevelId - LEVEL_ARTISANS) >=
          LEVEL_GNASTYS_LOOT - LEVEL_ARTISANS + 1U) {
        g_nNextLevelId = LEVEL_ARTISANS;
      }
      g_nLevelSelectWorldIndex = g_nNextLevelId / 10;
      sel = g_nNextLevelId % 10;
      break;
    }
    if (sel >= 0) {
      if (g_nLevelSelectWorldIndex != 0) {
        g_nNextLevelId = g_nLevelSelectWorldIndex * 10 + sel;
        g_nLevelSelectSubIndex = sel;
        if ((unsigned int)(g_nNextLevelId - LEVEL_ARTISANS) >=
                LEVEL_GNASTYS_LOOT - LEVEL_ARTISANS + 1U ||
            sel >= 6) {
          g_nNextLevelId = g_nActiveLevelId;
        } else {
          StopAllSoundExceptMask(0);
          InitLightVectorConstants();
          g_nReturnPortalLevelId = 0;
          g_nEnteredPortalIdx = -1;
          ResetSpyroEntity(RESET_SPYRO_SOFT);
          RestartSpyroAnimWithState(SPYRO_ST_GLIDE);
          g_nGamestate = GS_LEVEL_LOAD;
          g_nCdStreamState = LSTREAM_FRESH;
          g_nPortalTransitionActive = 0;
          g_nLevelTransitionTallyActive = 0;
          g_nSkipGamestateDraw = 1;
          g_nCameraNextMode = (CAM_HELD | CAM_MODE_LEVEL_TRANSITION);
        }
        g_nLevelSelectModeActive = 0;
        return;
      }
      g_nLevelSelectWorldIndex = sel;
    }
  }
}

/* g_anCheatCodeSequences slots that do something (CheckCheatCodeMatch passes
   the matched slot). Slots 0, 1 and 5..7 have sequences too and still match,
   but have no case here. */
enum CheatCode {
  CHEAT_CREDITS = 2,    /* roll the credits, then return to this level */
  CHEAT_ALL_WORLDS = 3, /* every level visited, every balloonist open */
  CHEAT_99_LIVES = 4,
};

/* Apply a matched cheat code (0x8002d810): CHEAT_CREDITS jumps to the credits
   (remembering the level to return to); CHEAT_ALL_WORLDS marks levels 0..0x1F
   visited and every world's balloonist BALLOONIST_OPEN; both play the reward
   sound. CHEAT_99_LIVES sets 99 lives. */

void ApplyCheatCodeEffect(int code) {
  unsigned char val;
  int n;
  unsigned char *p;
  unsigned char *q;
  switch (code) {
  case CHEAT_CREDITS:
    g_nCreditsReturnLevelId = g_nActiveLevelId;
    BeginCreditsSequence(0);
    PlaySoundEffect(LEVEL_SFX(SFX_REWARD), 0, SFX_MODE_PANNED, 0);
    break;
  case CHEAT_ALL_WORLDS:
    val = 1;
    n = LEVEL_SLOT(LEVEL_GNORC_COVE); /* every level through Gnorc Cove */
    p = &g_abLevelVisitedFlag[LEVEL_SLOT(LEVEL_GNORC_COVE)];
    do {
      *p = val;
      n -= 1;
      p -= 1;
    } while (n >= 0);
    val = BALLOONIST_OPEN;
    n = 5;
    q = &g_abWorldBalloonistState[5];
    do {
      *q = val;
      n -= 1;
      q -= 1;
    } while (n >= 0);
    PlaySoundEffect(LEVEL_SFX(SFX_REWARD), 0, SFX_MODE_PANNED, 0);
    break;
  case CHEAT_99_LIVES:
    g_nExtraLives = MAX_EXTRA_LIVES;
    break;
  }
}

/* Record the newly pressed buttons in the pad-history ring and scan it
   against the cheat-code sequence table (0x8002d8ec, 0x188). For each of
   the 8 sequences (0x40 bytes apart) counts its length, rewinds the ring
   cursor that many entries, and compares; on a full match applies the
   cheat effect and clears the ring. Only active while the CD stream is
   idle. */
void CheckCheatCodeMatch(void) {
  int pad[2];
  int idx;
  int off;
  int next;
  int *ring;
  int *base;
  int *seq;
  int *p;
  int *q;
  int *h;
  int len;
  int start;
  int i;
  int match;
  int code;
  if (g_nCdStreamState < LSTREAM_PRESERVE) {
    code = g_dwPadPressed;
    if (code != 0) {
      idx = g_nPadHistoryIndex;
      next = idx + 1;
      off = idx << 2;
      ring = g_anPadHistoryRing;
      *((int *)(((char *)ring) + off)) = code;
      g_nPadHistoryIndex = next;
      if (next >= 0x10) {
        g_nPadHistoryIndex = 0;
      }
      code = 0;
      base = ring;
      seq = g_anCheatCodeSequences;
      do {
        len = 0;
        if (seq[0] != 0) {
          p = seq;
        count_top:
          if (len < 0x10) {
            p += 1;
            len += 1;
            if ((*p) != 0) {
              goto count_top;
            }
          }
        }
        if (len == 0) {
          goto advance;
        }
        {
          start = g_nPadHistoryIndex;
          for (i = 0; i < len; i++) {
            start -= 1;
            if (start < 0) {
              start = 0xF;
            }
          }

          match = 1;
          i = 0;
          if (len > 0) {
            q = seq;
            h = (int *)((start << 2) + ((int)base));
            do {
              if (match == 0) {
                goto advance;
              }
              if ((*h) != (*q)) {
                match = 0;
              }
              h += 1;
              start += 1;
              if (start >= 0x10) {
                h = base;
                start = 0;
              }
              i += 1;
              q += 1;
            } while (i < len);
          }
          if (match != 0) {
            ApplyCheatCodeEffect(code);
            ClearPadHistoryRing();
            return;
          }
        }
      advance:
        code += 1;

        seq += 0x10;
      } while (code < 8);
    }
  }
}

/* 0x8002da74 — level-transition gem tally sparkle animation (1320 b).

   The tally screen reuses g_anActorShadowQueue as a pool of 64 sixteen-byte
   particle records (one per gem icon, plus slack). Phase 0 of the transition
   arms the pool: the hold-end phase is 0x8C + two frames per icon, capped at
   0xC0, and every record is marked free.

   The transition phase then ages by the frame step; past 0x1A0 the tally is
   over, below 0x80 nothing has spawned yet. In between, icon i wakes at
   phase i*2+0x80 (spawns only while the phase is still under 0x100) with a
   random velocity, spin and drift, chirping the bank's first sample every
   eighth phase step.

   A live particle ages its own phase byte and spins, then traces a three-part
   arc off the quarter-turn sine/cosine views: rise (phase < 0x30), hang
   (< 0x60) and fall (< 0x93), each indexing the LUT through a /3 of the
   scaled phase. Past 0x93 the particle retires, and every third one plays the
   bank's 0x2f sample as it lands. */

void AnimateGemTallyParticles(void) {
  int i;
  int spawn;

  g_pGemTallyParticleRecords = g_anActorShadowQueue;
  if (g_nLevelTransitionPhase == 0) {
    g_nGemTallyHoldEndPhase =
        g_nGemTallyIconCount * TALLY_ICON_FRAMES + TALLY_HOLD_BASE;
    if (g_nGemTallyHoldEndPhase > TALLY_HOLD_CAP) {
      g_nGemTallyHoldEndPhase = TALLY_HOLD_CAP;
    }
    i = 0;
    do {
      PARTICLES[i].live = 0;
      i++;
    } while (i < GEM_TALLY_POOL);
  }

  g_nLevelTransitionPhase += g_nFrameStep;
  if (g_nLevelTransitionPhase > TALLY_END) {
    g_nLevelTransitionTallyActive = 0;
    return;
  }
  if (g_nLevelTransitionPhase < TALLY_GEMS_IN) {
    return;
  }

  for (i = 0; i < GEM_TALLY_ICONS_MAX; i++) {
    if (i >= g_nGemTallyIconCount) {
      continue;
    }
    if (g_nLevelTransitionPhase < i * TALLY_ICON_FRAMES + TALLY_GEMS_IN) {
      continue;
    }
    if (g_nLevelTransitionPhase < TALLY_SPAWN_END && PARTICLES[i].live == 0) {
      PARTICLES[i].live = 1;
      spawn = i * TALLY_ICON_FRAMES - TALLY_GEMS_IN;
      PARTICLES[i].phase = g_nLevelTransitionPhase - spawn - g_nFrameStep;
      PARTICLES[i].vx = (GetRandomU32() & 0xFF) - 0x80;
      PARTICLES[i].vy = (GetRandomU32() & 0x3F) + 0x60;
      PARTICLES[i].driftx = (GetRandomU32() & 0x3F) - 0x20;
      PARTICLES[i].drifty = (GetRandomU32() & 0x3F) - 0x20;
      PARTICLES[i].spin = GetRandomU32();
      PARTICLES[i].signx = (GetRandomU32() & 2) - 1;
      PARTICLES[i].signy = (GetRandomU32() & 2) - 1;
      PARTICLES[i].spinv = (GetRandomU32() & 0xF) - 8;
      if ((g_nLevelTransitionPhase & 7) == 0) {
        PlaySoundEffect(LEVEL_SFX(SFX_REWARD), 0, SFX_MODE_PANNED, 0);
      }
    }
    if (PARTICLES[i].live == 0) {
      continue;
    }

    PARTICLES[i].phase += g_nFrameStep;
    PARTICLES[i].spin += PARTICLES[i].spinv;
    if (PARTICLES[i].phase < GEM_ARC_RISE_END) {
      PARTICLES[i].x = (PARTICLES[i].vx *
                        (0x1000 - g_anCosineLut[PARTICLES[i].phase * 4 / 3])) >>
                       12;
      PARTICLES[i].y =
          (PARTICLES[i].vy * g_anSineLut[PARTICLES[i].phase * 4 / 3]) >> 12;
    } else if (PARTICLES[i].phase < GEM_ARC_HANG_END) {
      PARTICLES[i].x =
          PARTICLES[i].vx +
          ((PARTICLES[i].vx *
            g_anSineLut[(PARTICLES[i].phase - GEM_ARC_RISE_END) * 8 / 3]) >>
           13);
      PARTICLES[i].y =
          (PARTICLES[i].vy >> 1) +
          ((PARTICLES[i].vy *
            (g_anCosineLut[(PARTICLES[i].phase - GEM_ARC_RISE_END) * 8 / 3] +
             0x1000)) >>
           14);
    } else if (PARTICLES[i].phase < GEM_ARC_FALL_END) {
      PARTICLES[i].x =
          PARTICLES[i].vx -
          ((PARTICLES[i].vx *
            g_anSineLut[(PARTICLES[i].phase - GEM_ARC_HANG_END) * 4 / 3]) >>
           12);
      PARTICLES[i].y =
          (PARTICLES[i].vy *
           g_anCosineLut[(PARTICLES[i].phase - GEM_ARC_HANG_END) * 4 / 3]) >>
          13;
    } else {
      PARTICLES[i].live = 0;
      if (i % 3 == 0) {
        PlaySoundEffect(LEVEL_SFX(SFX_GEM_BOUNCE), 0, SFX_MODE_PANNED, 0);
      }
    }
  }
}
