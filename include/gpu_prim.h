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

/* A draw-mode packet followed by the quad it applies to. */
typedef struct ModePolyF4 {
  DrMode mode;  /* 0x00 */
  PolyF4 poly;  /* 0x0C */
} ModePolyF4;   /* 0x24 */

#endif /* OPEN_SPYRO_GPU_PRIM_H */
