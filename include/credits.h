/* credits.h — record layouts shared by the credits overlay
 * (Credits_Update builds them, Credits_Draw renders the glyphs). */

#ifndef OPEN_SPYRO_CREDITS_H
#define OPEN_SPYRO_CREDITS_H

#include "credits_mode.h"

enum CredLineState {
  CRED_LINE_PENDING = 0,
  CRED_LINE_LAID_OUT = 1, /* glyphs allocated */
  CRED_LINE_SCROLLED = 2, /* showTimer expired */
};

enum CredGlyphState {
  CRED_GLYPH_FREE = 0,
  CRED_GLYPH_FLY_IN = 1,
  CRED_GLYPH_SETTLED = 2,
  CRED_GLYPH_FALL_OUT = 3,
  CRED_GLYPH_DONE = 4, /* freed on the next pass */
};

/* One 0xC-byte line record of the credits script (table at
 * g_pCreditsLineTable). */
typedef struct CredLine {
  unsigned char *text;
  short y;
  short showTimer; /* countdown; expiry marks the line scrolled past */
  short nextDelay; /* countdown before the next line starts */
  short state;     /* enum CredLineState */
} CredLine;

/* One 0x1C-byte glyph record of the credits text pool. */
typedef struct CredGlyph {
  int pad00;
  unsigned char state; /* enum CredGlyphState */
  unsigned char r, g, b;
  unsigned char dr, dg, db;
  unsigned char timer;
  short dx, dy, dz;
  short x, y, z;
  short line;
  short glyph;
} CredGlyph;

/* Credits-sequence globals (main .sdata/.bss slots the credits overlay
 * owns). */
/* end of the overlay image: glyph pool, then line table (+0xAF0) and scratch
 * (+0x2800) */
extern char Credits_OverlayEnd[];
extern char *g_pCreditsLineTable; /* 0x80075708: relocated CredLine table */
/* CD page scratch / draw buffers */
extern char *g_pCreditsScratchBuf; /* 0x80075768 */
/* camera/scroll track (CamTrack) */
extern char *g_pCreditsScrollHeader; /* 0x800757bc */
/* CD read target for the next page */
extern char *g_pCreditsLoadBuf; /* 0x800757ec */
/* byte span of the page's second read */
extern int g_nCreditsLoadSpan; /* 0x80075958 */
/* per-page sector offsets, 2 words/page */
extern int g_anCreditsPageSectors[]; /* 0x8007a960 */

#endif /* OPEN_SPYRO_CREDITS_H */
