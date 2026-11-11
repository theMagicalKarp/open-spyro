/* gem_tally.h — the level-transition treasure tally's gem particles
 * (AnimateGemTallyParticles animates them, DrawLevelTransitionOverlay draws
 * them). The pool is 64 sixteen-byte records borrowed from
 * g_anActorShadowQueue (g_pGemTallyParticleRecords). */

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
#define GEM_TALLY_POOL 0x40      /* records freed when the tally arms */
#define GEM_TALLY_ICONS_MAX 0x20 /* icons animated and drawn */

/* The tally's g_nLevelTransitionPhase timeline (AnimateGemTallyParticles
 * advances it by the frame step, DrawLevelTransitionOverlay places text by
 * it). Every slide in or out takes TALLY_SLIDE. */
#define TALLY_SLIDE 0x20
#define TALLY_PANEL_IN 0x40   /* "TREASURE FOUND" starts sliding up */
#define TALLY_GEMS_IN 0x80    /* icon i flies in at + i * TALLY_ICON_FRAMES;
                               * the level's count starts counting */
#define TALLY_ICON_FRAMES 2
#define TALLY_HOLD_BASE 0x8C  /* g_nGemTallyHoldEndPhase = base + 2 per icon, */
#define TALLY_HOLD_CAP 0xC0   /* capped here; the panel slides down after it */
#define TALLY_TOTAL_IN 0xE0   /* "TOTAL TREASURE" starts sliding up */
#define TALLY_SPAWN_END 0x100 /* no icon wakes after this */
#define TALLY_TOTAL_COUNT 0x110 /* the game-wide total starts counting */
#define TALLY_OUT 0x180       /* banner and panel slide out */
#define TALLY_END 0x1A0       /* past it the tally is over */
/* The count-up lasts (icons + 1) * 2 frames, at most TALLY_COUNT_MAX. */
#define TALLY_COUNT_MAX 0x40
/* The panel's screen y: up (held) and down (off the bottom edge). */
#define TALLY_PANEL_Y_UP 0xD0
#define TALLY_PANEL_Y_DOWN 0x110
/* A particle's own phase: rise, hang and fall arcs end here, then it lands. */
#define GEM_ARC_RISE_END 0x30
#define GEM_ARC_HANG_END 0x60
#define GEM_ARC_FALL_END 0x93

#endif /* OPEN_SPYRO_GEM_TALLY_H */
