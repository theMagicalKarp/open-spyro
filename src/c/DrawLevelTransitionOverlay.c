#include "globals.h"

/* 0x8001973c — the level-entry/exit banner and post-level treasure tally
   (2324 b), drawn by Gamestate01_09_Draw.

   The banner line is built from g_nCurrentLevelId: level 0 of a world is
   "RETURNING HOME...", the boss level (4) of a world and the Gnasty fight
   (0x3f) get the "CONFRONTING %s..." wording, everything else "ENTERING
   %s...", with the name taken from g_apLevelNameStrings at the flattened
   (world-1)*6 + level index. It is centred by string length and slid in and
   out of frame off the sine LUT at the two ends of g_nLevelTransitionPhase.

   From phase 0x40 the results panel joins in: "TREASURE FOUND" until phase
   0xe0, then "TOTAL TREASURE", each sliding through the same 0x110-y arc with
   a hold at 0xd0 between g_nGemTallyHoldEndPhase and +0x20. The number under
   it counts up over the gem fly-in (2 frames per icon, capped at 0x40) and
   then counts on from the world total, and one gem sprite is emitted per live
   g_pGemTallyParticleRecords record (U from g_abGemTallyIconUvTable) plus the
   treasure icon itself. Every glyph's alpha is pulsed from the cosine LUT
   stepped by the phase.

   The queue is flushed here rather than by the caller: clear the head slot,
   EnqueuePendingSpritePrims, then rasterize. */

extern unsigned char D_80010ACC[];     /* "RETURNING HOME..." */
extern unsigned char D_80010AE0[];     /* "CONFRONTING %s..." */
extern unsigned char D_80010AF4[];     /* "ENTERING %s..." */
extern unsigned char D_80010B04[];     /* "TREASURE FOUND" */
extern unsigned char D_80010B14[];     /* "TOTAL TREASURE" */
extern unsigned short g_anCosineLut[]; /* 0x8006cc78: g_anSineLut + 0x80 */

extern void *BuildTextSprites(unsigned char *str, int *pos, int *step, int a,
                              int b);
extern void *BuildTextSpriteChain(unsigned char *str, int *pos, int a, int b);
extern void EnqueuePendingSpritePrims(void);
extern void RasterizeSpritePrimQueue(void);
extern void FillWord(void *dst, unsigned int value, int byte_count);
extern int sprintf();
extern int strlen();

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

typedef struct {
  int unk00[3];    /* 0x00 */
  int x;           /* 0x0c */
  int y;           /* 0x10 */
  int scale;       /* 0x14 */
  int unk18[7];    /* 0x18 */
  short unk34;     /* 0x34 */
  short uv;        /* 0x36 */
  int unk38[3];    /* 0x38 */
  unsigned char r; /* 0x44 */
} TallySprite;

#define SPR ((TallySprite *)g_pSpriteRecordWriteCursor)
#define PARTICLES ((GemTallyParticle *)g_pGemTallyParticleRecords)

void DrawLevelTransitionOverlay(void) {
  int pos[3];
  int step[3];
  char buf[32];
  char *msg;
  TallySprite *spr;
  int idx;
  int sub;
  int slide;
  int i; /* doubles as the treasure digit-count scratch and the icon index */
  int x0;
  int span;
  int shown;
  int value;
  int y;
  int gx;

  idx = (g_nCurrentLevelId / 10 - 1) * 6 + g_nCurrentLevelId % 10;
  sub = g_nCurrentLevelId % 10;
  if (sub == 0) {
    sprintf(buf, D_80010ACC);
  } else if ((g_nCurrentLevelId < 0x3c && sub == 4) ||
             g_nCurrentLevelId == 0x3f) {
    sprintf(buf, D_80010AE0, g_apLevelNameStrings[idx]);
  } else {
    sprintf(buf, D_80010AF4, g_apLevelNameStrings[idx]);
  }

  pos[0] = 0x100 - (strlen(buf) - 1) * 8;
  if (g_nLevelTransitionPhase < 0x20) {
    pos[1] = g_anSineLut[g_nLevelTransitionPhase * 2] >> 7;
  } else if (g_nLevelTransitionPhase >= 0x181) {
    pos[1] = g_anSineLut[(0x1a0 - g_nLevelTransitionPhase) * 2] >> 7;
  } else {
    pos[1] = 0x20;
  }

  {
    char *glyph = g_pSpriteRecordWriteCursor;
    pos[2] = 0x1100;
    step[0] = 0x10;
    step[1] = 1;
    step[2] = 0x1400;
    BuildTextSprites((unsigned char *)buf, pos, step, 0x12, 2);
    glyph -= 0x58;
    if ((int)g_pSpriteRecordWriteCursor <= (int)glyph) {
      unsigned short *lut = g_anCosineLut;
      int wave = 0;
      do {
        glyph[0x46] = lut[(g_nLevelTransitionPhase * 2 + wave) & 0xff] >> 7;
        glyph -= 0x58;
        wave += 0xc;
      } while ((int)g_pSpriteRecordWriteCursor <= (int)glyph);
    }
  }

  if (g_nLevelTransitionPhase >= 0x40) {
    i = g_nWorldTreasureCollected;
    x0 = 0x28;
    while (i >= 10) {
      i /= 10;
      x0 -= 10;
    }

    pos[0] = x0 + 0x28;
    pos[2] = 0x1100;
    if (g_nLevelTransitionPhase < 0x60) {
      slide = g_nLevelTransitionPhase - 0x40;
      pos[1] = 0x110 - (g_anSineLut[slide * 2] >> 6);
    } else if (g_nLevelTransitionPhase < g_nGemTallyHoldEndPhase) {
      pos[1] = 0xd0;
    } else if (g_nLevelTransitionPhase < g_nGemTallyHoldEndPhase + 0x20) {
      slide = g_nLevelTransitionPhase - 0x20;
      pos[1] =
          0x110 - (g_anSineLut[(g_nGemTallyHoldEndPhase - slide) * 2] >> 6);
    } else if (g_nLevelTransitionPhase < 0xe0) {
      pos[1] = 0x110;
    } else if (g_nLevelTransitionPhase < 0x100) {
      slide = g_nLevelTransitionPhase - 0xe0;
      pos[1] = 0x110 - (g_anSineLut[slide * 2] >> 6);
    } else if (g_nLevelTransitionPhase < 0x180) {
      pos[1] = 0xd0;
    } else if (g_nLevelTransitionPhase < 0x1a0) {
      pos[1] =
          0x110 - (g_anSineLut[(0x1a0 - g_nLevelTransitionPhase) * 2] >> 6);
    } else {
      pos[1] = 0x110;
    }

    if (g_nLevelTransitionPhase < 0xe0) {
      sprintf(buf, D_80010B04);
    } else {
      sprintf(buf, D_80010B14);
      pos[0] -= 0x10;
    }
    {
      char *glyph = g_pSpriteRecordWriteCursor;
      int wave;
      BuildTextSprites((unsigned char *)buf, pos, step, 0x12, 2);
      glyph -= 0x58;
      wave = 0;
      if ((int)g_pSpriteRecordWriteCursor <= (int)glyph) {
        do {
          glyph[0x46] =
              g_anCosineLut[(g_nLevelTransitionPhase * 2 + wave) & 0xff] >> 7;
          glyph -= 0x58;
          wave += 0xc;
        } while ((int)g_pSpriteRecordWriteCursor <= (int)glyph);
      }
    }

    if (g_nLevelTransitionPhase < 0x80) {
      value =
          g_anLevelGemsCollected[g_nPrevLevelIntroIndex] - g_nLevelGemsAtEntry;
    } else if (g_nLevelTransitionPhase < 0xe0) {
      span = (g_nGemTallyIconCount + 1) * 2;
      if (span >= 0x41) {
        span = 0x40;
      }
      slide = g_nLevelTransitionPhase - 0x80;
      shown = span - slide;
      if (shown < 0) {
        shown = 0;
      }
      if (shown > span) {
        shown = span;
      }
      value = (g_anLevelGemsCollected[g_nPrevLevelIntroIndex] -
               g_nLevelGemsAtEntry) *
              shown / span;
    } else if (g_nLevelTransitionPhase < 0x110) {
      value = g_nWorldTreasureCollected -
              (g_anLevelGemsCollected[g_nPrevLevelIntroIndex] -
               g_nLevelGemsAtEntry);
    } else {
      span = (g_nGemTallyIconCount + 1) * 2;
      if (span >= 0x41) {
        span = 0x40;
      }
      slide = g_nLevelTransitionPhase - 0x110;
      shown = span - slide;
      if (shown < 0) {
        shown = 0;
      }
      if (shown > span) {
        shown = span;
      }
      value = g_nWorldTreasureCollected -
              (g_anLevelGemsCollected[g_nPrevLevelIntroIndex] -
               g_nLevelGemsAtEntry) *
                  shown / span;
    }

    msg = buf;
    pos[0] = x0 + 0x178;
    pos[2] = 0xf80;
    sprintf(msg, g_szFmtPercentD, value);
    {
      char *glyph = g_pSpriteRecordWriteCursor;
      BuildTextSpriteChain((unsigned char *)msg, pos, 0x14, 2);
      glyph -= 0x58;
      if ((int)g_pSpriteRecordWriteCursor <= (int)glyph) {
        unsigned short *lut = g_anCosineLut;
        int wave = 0;
        do {
          glyph[0x46] = lut[(g_nLevelTransitionPhase * 2 + wave) & 0xff] >> 7;
          glyph -= 0x58;
          wave += 0xc;
        } while ((int)g_pSpriteRecordWriteCursor <= (int)glyph);
      }
    }

    i = 0;
    do {
      if (PARTICLES[i].live != 0) {
        g_pSpriteRecordWriteCursor =
            (void *)((int)g_pSpriteRecordWriteCursor - 0x58);
        FillWord(g_pSpriteRecordWriteCursor, 0, 0x58);
        SPR->uv = g_abGemTallyIconUvTable[i];
        gx = PARTICLES[i].x + 0x140;
        SPR->x = x0 + gx;
        SPR->y = 0xc6 - PARTICLES[i].y;
        SPR->scale = 0x1000;
        SPR->r = PARTICLES[i].driftx;
        *(unsigned char *)((int)g_pSpriteRecordWriteCursor + 0x45) =
            PARTICLES[i].drifty;
        *(unsigned char *)((int)g_pSpriteRecordWriteCursor + 0x46) =
            PARTICLES[i].spin;
        *(unsigned char *)((int)g_pSpriteRecordWriteCursor + 0x47) = 0x7f;
        *(unsigned char *)((int)g_pSpriteRecordWriteCursor + 0x4f) =
            *(unsigned char *)((int)g_pSpriteRecordWriteCursor + 0x36) - 0x52;
        *(unsigned char *)((int)g_pSpriteRecordWriteCursor + 0x50) = 0xff;
      }
      i++;
    } while (i < 0x20);

    g_pSpriteRecordWriteCursor =
        (void *)((int)g_pSpriteRecordWriteCursor - 0x58);
    FillWord(g_pSpriteRecordWriteCursor, 0, 0x58);
    spr = g_pSpriteRecordWriteCursor;
    spr->uv = 0x1d9;
    spr->x = x0 + 0x140;
    if (g_nLevelTransitionPhase < 0xe0) {
      y = pos[1];
      spr->scale = 0x800;
      spr->r = 6;
      y += 8;
    } else {
      y = pos[1];
      spr->scale = 0x600;
      spr->r = 6;
      y += 0xc;
    }
    spr->y = y;
    *(unsigned char *)((int)g_pSpriteRecordWriteCursor + 0x46) = 0xb0;
    *(unsigned char *)((int)g_pSpriteRecordWriteCursor + 0x47) = 0x7f;
    *(unsigned char *)((int)g_pSpriteRecordWriteCursor + 0x4f) = 0xb;
    *(unsigned char *)((int)g_pSpriteRecordWriteCursor + 0x50) = 0xff;
  }

  g_apSpritePrimQueue[0] = 0;
  EnqueuePendingSpritePrims();
  RasterizeSpritePrimQueue();
}
