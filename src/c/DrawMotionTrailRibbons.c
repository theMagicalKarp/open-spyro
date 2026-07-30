#include "globals.h"

/* 0x800189f0 — the motion-trail ribbons (1344 b), drawn from ComposeFrameScene.

   Up to three ribbons are queued per frame (see the level-33 pusher): each
   entry is a point list of 0x1c-byte slices plus its slice count.  Every slice
   is projected to screen space, then a shaded quad is emitted between each
   consecutive pair: the rim offset is the screen-space perpendicular of the
   slice-to-slice delta, scaled by the ribbon half-width over the slice depth,
   and the vertex colours fade out with the slice age.  The half-width ramps in
   over the first three slices and collapses to 0 on the last-but-one; Gnasty's
   world (level 0x3f) quarters it and swaps red with green. */

extern int D_800772C8[];   /* 0x800772c8: per-ribbon slice count */
extern void *D_80078658[]; /* 0x80078658: per-ribbon slice list */

extern void ProjectWorldPointGTE(int *dst, int *src, unsigned int flags);
extern void SubtractVector(int *dst, int *a, int *b);
extern int IntegerSqrt(int n);
extern void SetDrawMode(void *prim, int dfe, int dtd, unsigned int tpage,
                        void *clip);
extern void AddPrimToOTSlot(void *prim, int slot);

typedef struct {  /* 0x1c */
  int wx, wy, wz; /* 0x00 world position */
  int sx, sy;     /* 0x0c projected screen position */
  int z;          /* 0x14 projected depth */
  int age;        /* 0x18 slice age (fade + cull) */
} TrailSlice;

typedef struct {                  /* 0x30: DR_MODE + POLY_G4 */
  int mode[3];                    /* 0x00 */
  int tag;                        /* 0x0c */
  unsigned char r0, g0, b0, code; /* 0x10 */
  short x0, y0;                   /* 0x14 */
  unsigned char r1, g1, b1, p1;   /* 0x18 */
  short x1, y1;                   /* 0x1c */
  unsigned char r2, g2, b2, p2;   /* 0x20 */
  short x2, y2;                   /* 0x24 */
  unsigned char r3, g3, b3, p3;   /* 0x28 */
  short x3, y3;                   /* 0x2c */
} TrailQuad;

void DrawMotionTrailRibbons(void) {
  int d[3];
  int i;
  int x0, x1, y0, y1;
  int px0, px1, py0, py1;
  int drawn;
  TrailSlice *pts;
  int k;
  int wide;
  int dx, dy;
  int len;
  int slot;
  int r0, g0, b0, r1, g1, b1;
  int hold;
  TrailQuad *prim;

  drawn = 0;
  for (i = 0; i < g_nMotionTrailRibbonCount; i++) {
    pts = (TrailSlice *)D_80078658[i];
    for (k = 0; k < D_800772C8[i]; k++) {
      ProjectWorldPointGTE(&pts[k].sx, &pts[k].wx, 1);
    }
    for (k = 0; k < D_800772C8[i] - 1; k++) {
      wide = 0x11170;
      wide = -(k != D_800772C8[i] - 2) & wide;
      if (k < 3) {
        wide = (k * 0x271) << 5;
      }
      if (g_nActiveLevelId == 0x3F) {
        wide >>= 2;
      }
      if (pts[k].age < 0x51) {
        SubtractVector(d, &pts[k].sx, &pts[k + 1].sx);
        dx = d[0];
        dy = d[1];
        d[0] = dy;
        d[1] = -dx;
        drawn++;
        len = d[0] * d[0] + d[1] * d[1];
        len = IntegerSqrt(len);
        if (len == 0 || pts[k].z == 0) {
          d[0] = 0;
          d[1] = 0;
        } else {
          d[0] = d[0] * wide / (pts[k].z * len);
          d[1] = d[1] * wide / (pts[k].z * len);
        }
        x0 = pts[k].sx + d[0];
        x1 = pts[k].sx - d[0];
        y0 = pts[k].sy + d[1];
        y1 = pts[k].sy - d[1];
        if (drawn >= 2) {
          slot = pts[k].z >> 7;
          if ((unsigned int)slot < 0x7D0) {
            r0 = 0x96 - pts[k].age * 2;
            g0 = 0x96 - pts[k].age * 4;
            b0 = 0x96 - pts[k].age * 10;
            r1 = 0x96 - pts[k - 1].age * 2;
            g1 = 0x96 - pts[k - 1].age * 4;
            b1 = 0x96 - pts[k - 1].age * 10;
            if (r0 < 0) {
              r0 = 0;
            }
            if (g0 < 0) {
              g0 = 0;
            }
            if (b0 < 0) {
              b0 = 0;
            }
            if (r1 < 0) {
              r1 = 0;
            }
            if (g1 < 0) {
              g1 = 0;
            }
            if (b1 < 0) {
              b1 = 0;
            }
            if (g_nActiveLevelId == 0x3F) {
              hold = r0;
              r0 = g0;
              g0 = hold;
              hold = r1;
              r1 = g1;
              g1 = hold;
            }
            prim = (TrailQuad *)g_pPrimBufferWriteCursor;
            SetDrawMode(prim, 1, 0, 0x20, 0);
            AddPrimToOTSlot(prim, slot);
            prim->tag = 0x8000000;
            prim->code = 0x3A;
            prim->x0 = x0;
            prim->x1 = x1;
            prim->x2 = px0;
            prim->x3 = px1;
            prim->y0 = y0;
            prim->y1 = y1;
            prim->y2 = py0;
            prim->y3 = py1;
            prim->r0 = r0;
            prim->g0 = g0;
            prim->b0 = b0;
            prim->r1 = r0;
            prim->g1 = g0;
            prim->b1 = b0;
            prim->r2 = r1;
            prim->g2 = g1;
            prim->b2 = b1;
            prim->r3 = r1;
            prim->g3 = g1;
            prim->b3 = b1;
            AddPrimToOTSlot(&prim->tag, slot);
            g_pPrimBufferWriteCursor = prim + 1;
          }
        }
        px0 = x0;
        px1 = x1;
        py0 = y0;
        py1 = y1;
      }
    }
  }
  g_nMotionTrailRibbonCount = 0;
}
