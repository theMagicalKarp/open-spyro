#include "globals.h"

/* 0x8002da74 — level-transition gem tally sparkle animation (1320 b).

   The tally screen reuses g_apSecondaryEntityList as a pool of 64 sixteen-byte
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
extern short g_anCosineLut[]; /* 0x8006cc78: g_anSineLut + quarter turn */
extern unsigned int GetRandomU32(void);
extern int PlaySoundEffect(unsigned int id, int owner, unsigned int mode,
                           void *pos);

typedef struct {
  short x;              /* 0x00 */
  short y;              /* 0x02 */
  short vx;             /* 0x04 */
  short vy;             /* 0x06 */
  unsigned char live;   /* 0x08 */
  unsigned char phase;  /* 0x09 */
  unsigned char driftx; /* 0x0a */
  unsigned char drifty; /* 0x0b */
  unsigned char spin;   /* 0x0c */
  unsigned char signx;  /* 0x0d */
  unsigned char signy;  /* 0x0e */
  unsigned char spinv;  /* 0x0f */
} GemTallyParticle;

#define PARTICLES ((GemTallyParticle *)g_pGemTallyParticleRecords)

void AnimateGemTallyParticles(void) {
  int i;
  int spawn;

  g_pGemTallyParticleRecords = g_apSecondaryEntityList;
  if (g_nLevelTransitionPhase == 0) {
    g_nGemTallyHoldEndPhase = g_nGemTallyIconCount * 2 + 0x8C;
    if (g_nGemTallyHoldEndPhase > 0xC0) {
      g_nGemTallyHoldEndPhase = 0xC0;
    }
    i = 0;
    do {
      PARTICLES[i].live = 0;
      i++;
    } while (i < 0x40);
  }

  g_nLevelTransitionPhase += g_nFrameStep;
  if (g_nLevelTransitionPhase >= 0x1A1) {
    g_nLevelTransitionTallyActive = 0;
    return;
  }
  if (g_nLevelTransitionPhase < 0x80) {
    return;
  }

  for (i = 0; i < 0x20; i++) {
    if (i >= g_nGemTallyIconCount) {
      continue;
    }
    if (g_nLevelTransitionPhase < i * 2 + 0x80) {
      continue;
    }
    if (g_nLevelTransitionPhase < 0x100 && PARTICLES[i].live == 0) {
      PARTICLES[i].live = 1;
      spawn = i * 2 - 0x80;
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
        PlaySoundEffect(*(unsigned char *)g_pLevelSampleBankHeader, 0, 0x10, 0);
      }
    }
    if (PARTICLES[i].live == 0) {
      continue;
    }

    PARTICLES[i].phase += g_nFrameStep;
    PARTICLES[i].spin += PARTICLES[i].spinv;
    if (PARTICLES[i].phase < 0x30) {
      PARTICLES[i].x = (PARTICLES[i].vx *
                        (0x1000 - g_anCosineLut[PARTICLES[i].phase * 4 / 3])) >>
                       12;
      PARTICLES[i].y =
          (PARTICLES[i].vy * g_anSineLut[PARTICLES[i].phase * 4 / 3]) >> 12;
    } else if (PARTICLES[i].phase < 0x60) {
      PARTICLES[i].x = PARTICLES[i].vx +
                       ((PARTICLES[i].vx *
                         g_anSineLut[(PARTICLES[i].phase - 0x30) * 8 / 3]) >>
                        13);
      PARTICLES[i].y =
          (PARTICLES[i].vy >> 1) +
          ((PARTICLES[i].vy *
            (g_anCosineLut[(PARTICLES[i].phase - 0x30) * 8 / 3] + 0x1000)) >>
           14);
    } else if (PARTICLES[i].phase < 0x93) {
      PARTICLES[i].x = PARTICLES[i].vx -
                       ((PARTICLES[i].vx *
                         g_anSineLut[(PARTICLES[i].phase - 0x60) * 4 / 3]) >>
                        12);
      PARTICLES[i].y = (PARTICLES[i].vy *
                        g_anCosineLut[(PARTICLES[i].phase - 0x60) * 4 / 3]) >>
                       13;
    } else {
      PARTICLES[i].live = 0;
      if (i % 3 == 0) {
        PlaySoundEffect(((unsigned char *)g_pLevelSampleBankHeader)[0x2F], 0,
                        0x10, 0);
      }
    }
  }
}
