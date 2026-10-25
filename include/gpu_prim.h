/* gpu_prim.h — GPU packet layouts the game builds in the primitive buffer
 * (g_pPrimBufferWriteCursor). Field order follows the GPU command words:
 * a tag word (OT link + length), then the command (colour + code) and the
 * vertex/texture words. */

#ifndef OPEN_SPYRO_GPU_PRIM_H
#define OPEN_SPYRO_GPU_PRIM_H

/* Draw-mode packet (SetDrawMode): tag + two GPU words. */
typedef struct DrMode {
  unsigned int tag;     /* 0x00 */
  unsigned int code[2]; /* 0x04 */
} DrMode;               /* 0x0C */

/* Flat-shaded quad (GPU code 0x28, +2 semi-transparent). */
typedef struct PolyF4 {
  unsigned int tag;        /* 0x00 */
  unsigned char r0, g0, b0; /* 0x04 */
  unsigned char code;      /* 0x07 */
  short x0, y0;            /* 0x08 */
  short x1, y1;            /* 0x0C */
  short x2, y2;            /* 0x10 */
  short x3, y3;            /* 0x14 */
} PolyF4;                  /* 0x18 */

/* Flat line (GPU code 0x40, +2 semi-transparent). */
typedef struct LineF2 {
  unsigned int tag;         /* 0x00 */
  unsigned char r0, g0, b0; /* 0x04 */
  unsigned char code;       /* 0x07 */
  short x0, y0;             /* 0x08 */
  short x1, y1;             /* 0x0C */
} LineF2;                   /* 0x10 */

/* Textured quad (GPU code 0x2C, +1 raw texture, +2 semi-transparent). */
typedef struct PolyFT4 {
  unsigned int tag;         /* 0x00 */
  unsigned char r0, g0, b0; /* 0x04 */
  unsigned char code;       /* 0x07 */
  short x0, y0;             /* 0x08 */
  unsigned char u0, v0;     /* 0x0C */
  unsigned short clut;      /* 0x0E */
  short x1, y1;             /* 0x10 */
  unsigned char u1, v1;     /* 0x14 */
  unsigned short tpage;     /* 0x16 */
  short x2, y2;             /* 0x18 */
  unsigned char u2, v2;     /* 0x1C */
  unsigned short pad1E;     /* 0x1E */
  short x3, y3;             /* 0x20 */
  unsigned char u3, v3;     /* 0x24 */
  unsigned short pad26;     /* 0x26 */
} PolyFT4;                  /* 0x28 */

/* Flat rectangle of any size (GPU code 0x60, +2 semi-transparent). */
typedef struct Tile {
  unsigned int tag;         /* 0x00 */
  unsigned char r0, g0, b0; /* 0x04 */
  unsigned char code;       /* 0x07 */
  short x0, y0;             /* 0x08 */
  short w, h;               /* 0x0C */
} Tile;                     /* 0x10 */

/* A draw-mode packet followed by the quad it applies to. */
typedef struct ModePolyF4 {
  DrMode mode;  /* 0x00 */
  PolyF4 poly;  /* 0x0C */
} ModePolyF4;   /* 0x24 */

#endif /* OPEN_SPYRO_GPU_PRIM_H */
