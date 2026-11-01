/* gem_tally.h — the level-transition treasure tally's gem particles
 * (AnimateGemTallyParticles animates them, DrawLevelTransitionOverlay draws
 * them). The pool is 64 sixteen-byte records borrowed from
 * g_apSecondaryEntityList (g_pGemTallyParticleRecords). */

#ifndef OPEN_SPYRO_GEM_TALLY_H
#define OPEN_SPYRO_GEM_TALLY_H

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

#endif /* OPEN_SPYRO_GEM_TALLY_H */
