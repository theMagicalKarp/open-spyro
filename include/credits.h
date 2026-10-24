/* credits.h — record layouts shared by the credits overlay
 * (Credits_Update builds them, Credits_Draw renders the glyphs). */

#ifndef OPEN_SPYRO_CREDITS_H
#define OPEN_SPYRO_CREDITS_H

/* One 0xC-byte line record of the credits script (table at g_pCreditsLineTable). */
typedef struct CredLine {
  unsigned char *text;
  short y;
  short showTimer; /* countdown; expiry marks the line scrolled past */
  short nextDelay; /* countdown before the next line starts */
  short state; /* 1 = laid out, 2 = scrolled past */
} CredLine;

/* One 0x1C-byte glyph record of the credits text pool. */
typedef struct CredGlyph {
  int pad00;
  unsigned char state; /* 0 free, 1 fly in, 2 settled, 3 fall out, 4 done */
  unsigned char r, g, b;
  unsigned char dr, dg, db;
  unsigned char timer;
  short dx, dy, dz;
  short x, y, z;
  short line;
  short glyph;
} CredGlyph;

/* Credits-sequence globals (main .sdata/.bss slots the credits overlay owns). */
extern char Credits_OverlayEnd[];    /* end of the overlay image: glyph pool, then
                                      * line table (+0xAF0) and scratch (+0x2800) */
extern char *g_pCreditsLineTable;    /* 0x80075708: relocated CredLine table */
extern char *g_pCreditsScratchBuf;   /* 0x80075768: CD page scratch / draw buffers */
extern char *g_pCreditsScrollHeader; /* 0x800757bc: camera/scroll track (CamTrack) */
extern char *g_pCreditsLoadBuf;      /* 0x800757ec: CD read target for the next page */
extern int g_nCreditsLoadSpan;       /* 0x80075958: byte span of the page's second read */
extern int g_anCreditsPageSectors[]; /* 0x8007a960: per-page sector offsets, 2 words/page */

#endif /* OPEN_SPYRO_CREDITS_H */
