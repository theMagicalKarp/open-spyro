#include "funcs.h"
#include "gem_tally.h"
#include "globals.h"
#include "level_sfx.h"
#include "sample_bank.h"
#include "vecmath.h"

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
