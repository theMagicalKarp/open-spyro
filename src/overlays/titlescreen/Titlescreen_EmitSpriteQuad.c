#include "globals.h"
#include "funcs.h"
#include "gpu_prim.h"
/* Titlescreen_EmitSpriteQuad (0x8007CD38, titlescreen overlay).
 *
 * Emit one textured quad (PolyFT4) for a title-screen UI sprite: position it
 * at (nX, nY) sized from the sprite table, tint it from the selection-color
 * table, submit it to the ordering table, and bump the primitive allocator. A
 * negative sprite id draws the sprite horizontally mirrored (the U coordinates
 * are rotated after submission).
 *
 * The setXY4/setUVWH2 PSY-Q macro expansions are written out inline; externs
 * keep our splat autolabel names, PROVIDE()d by address at link.
 *
 * Verified byte-identical inside the relinked titlescreen.ovl.
 */


typedef struct {
  unsigned short nTpage; /* 0x0 */
  unsigned short nClut;  /* 0x2 */
  unsigned char nW;      /* 0x4 */
  unsigned char nH;      /* 0x5 */
  unsigned char nU;      /* 0x6 */
  unsigned char nV;      /* 0x7 */
} TitlescreenSprite;

extern unsigned int g_adwTitleSelectColors[];       /* selection color words (P_CODEs) */
extern TitlescreenSprite g_aTitlescreenSprites[];  /* title-screen sprite table */


void Titlescreen_EmitSpriteQuad(int nX, int nY, int nSprite, int nColorId) {
  PolyFT4 *p;
  int bMirrored = 0;

  if (nSprite < 0) {
    bMirrored = 1;
    nSprite = -nSprite;
  }

  p = g_pPrimBufferWriteCursor;
  p->tag = 0x09000000;
  ((unsigned int *)p)[1] = g_adwTitleSelectColors[nColorId];

  /* setXY4(p, nX, nY, w + nX, nY, nX, h + nY, w + nX, h + nY) */
  p->x0 = nX, p->y0 = nY,
  p->x1 = g_aTitlescreenSprites[nSprite].nW + nX, p->y1 = nY,
  p->x2 = nX, p->y2 = g_aTitlescreenSprites[nSprite].nH + nY,
  p->x3 = g_aTitlescreenSprites[nSprite].nW + nX, p->y3 = g_aTitlescreenSprites[nSprite].nH + nY;

  /* setUVWH2(p, u, v, w, h): u3=u1, v3=v2 reuse the computed corners */
  p->u0 = g_aTitlescreenSprites[nSprite].nU, p->v0 = g_aTitlescreenSprites[nSprite].nV,
  p->u1 = p->u0 + g_aTitlescreenSprites[nSprite].nW, p->v1 = p->v0,
  p->u2 = p->u0, p->v2 = p->v0 + g_aTitlescreenSprites[nSprite].nH,
  p->u3 = p->u1, p->v3 = p->v2;

  p->tpage = g_aTitlescreenSprites[nSprite].nTpage;
  p->clut = g_aTitlescreenSprites[nSprite].nClut;

  AddPrimToOT(p);

  g_pPrimBufferWriteCursor = p + 1;

  if (bMirrored) {
    p->u0 = p->u1;
    p->u1 = p->u2;
    p->u2 = p->u0;
    p->u3 = p->u1;
  }
}
