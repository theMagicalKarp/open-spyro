#include "globals.h"

extern void PutDrawEnv(void *env);
extern void LoadImage(RECT *rect, short *pix);
extern void DrawSync(int mode);
extern void AddPrimToOTSlot(int prim, int slot);
extern unsigned long long ComputeDepthCuedColor();
extern void EmitStaticActorMeshListFogged(void);
extern void FillWord(void *dst, unsigned int value, int byte_count);
extern void RasterizePairedActor(void);
extern void *LinkOTPrimitives(int depth_max);
extern void DrawOTag(void *ot);
extern int VSync(int mode);
extern DISPENV *PutDispEnv(DISPENV *env);

extern int g_anVsyncFrameEndBlock[]; /* held-base alias: [-1] = pace anchor */

/* Draw gamestate 0xA (pause -> world-map transition, 0x8001c694, 0x3a4).
   Phase 0: re-darken the frozen gray backdrop by re-loading the CLUT ramp
   shifted by the idle counter, then draw the 4 backdrop quads (clut 0x3820)
   plus a black line into OT slot 0x3ff. Phase 1: fog cross-fade — feed
   (0x20-idle)*0x80 through ComputeDepthCuedColor into the back DRAWENV's
   r0/g0/b0 and emit the fogged static mesh list. Both phases: OT clear,
   RasterizePairedActor, DrawOTag, then the standard 2-vblank frame pace.

   The quad loop is written field by field the way libgpu's set* macros
   expand: each derived vertex re-reads x0/y0/u0/v0 from the primitive, and
   gcc 2.7.2 keeps those as real narrow loads. `slot` is set before the
   loop and again at the end of every pass (two sets, so it stays a pseudo
   in a1 instead of being rematerialised at the call). Case 1: `env` is a
   `$4` register local, and the color spill store plus the active-env load
   are both volatile, so the spill stays above the compare load. */

typedef struct {
  unsigned int tag;
  unsigned char r0, g0, b0, code;
  short x0, y0;
  unsigned char u0, v0;
  unsigned short clut;
  short x1, y1;
  unsigned char u1, v1;
  unsigned short tpage;
  short x2, y2;
  unsigned char u2, v2;
  unsigned short pad1;
  short x3, y3;
  unsigned char u3, v3;
  unsigned short pad2;
} POLY_FT4;

void Gamestate0A_Draw(void) {
  POLY_FT4 *q;
  RECT rect;
  int color;
  unsigned char *p;
  unsigned char *next;
  int i;
  int tag;
  int lineCode;
  int slot;
  int prog;

  g_nPrimBufferOverflowFlag = 0;
  g_pPrimBufferWriteCursor = g_pRenderScratchRegionBase;
  g_pPrimBufferLimit = (char *)g_pRenderScratchRegionBase + 0x1C000;
  switch (g_nPauseMenuTransitionFrames) {
  case 0: {
    DRAWENV *env;

    env = &g_abFrameDrawEnv0;
    if (g_pActiveFrameDrawEnv == &g_abFrameDrawEnv0) {
      env = (DRAWENV *)((char *)env + 0x84);
    }
    i = 0;
    PutDrawEnv(env);
    rect.y = 0xE0;
    rect.h = 1;
    rect.x = g_nPauseMenuIdleFrames + 0x200;
    rect.w = 0x20 - g_nPauseMenuIdleFrames;
    LoadImage(&rect, g_anMenuBackdropGrayClut);
    DrawSync(0);
    slot = 0x3FF;
  quad:
    q = (POLY_FT4 *)g_pPrimBufferWriteCursor;
    *(int *)q = 0x9000000;
    q->code = 0x2C;
    q->r0 = 0x4C;
    q->g0 = 0x80;
    q->x0 = i << 7;
    tag = i + 0x88;
    q->y0 = 8;
    q->x1 = q->x0 + 0x80;
    q->y1 = q->y0;
    q->x2 = q->x0;
    q->y2 = q->y0 + 0xDF;
    q->x3 = q->x0 + 0x80;
    q->y3 = q->y0 + 0xDF;
    i += 1;
    lineCode = 0x40;
    q->b0 = lineCode;
    q->u0 = 0;
    q->v0 = 0;
    q->u1 = q->u0 + 0x80;
    q->v1 = q->v0;
    q->u2 = q->u0;
    q->v2 = q->v0 - 0x21;
    q->u3 = q->u0 + 0x80;
    q->v3 = q->v0 - 0x21;
    q->clut = 0x3820;
    q->tpage = tag;
    p = (unsigned char *)q;
    AddPrimToOTSlot((int)p, slot);
    next = p + 0x28;
    g_pPrimBufferWriteCursor = next;
    slot = 0x3FF;
    if (i < 4) {
      goto quad;
    }
    *(int *)(p + 0x28) = 0x3000000;
    p[0x2F] = lineCode;
    *(short *)(p + 0x30) = 0;
    *(short *)(p + 0x32) = 0xE7;
    *(short *)(p + 0x34) = 0x200;
    *(short *)(p + 0x36) = 0xE7;
    p[0x2C] = 0;
    p[0x2D] = 0;
    p[0x2E] = 0;
    AddPrimToOTSlot((int)next, slot);
    g_pPrimBufferWriteCursor = p + 0x38;
    break;
  }
  case 1: {
    register DRAWENV *env asm("$4");

    g_dwActorMeshFadeColor = 0;
    prog = (0x20 - g_nPauseMenuIdleFrames) << 7;
    g_nFadeProgress = prog;
    *(volatile int *)&color = ComputeDepthCuedColor(g_dwWorldFogColor, 0, prog);
    env = &g_abFrameDrawEnv0;
    if (*(DRAWENV *volatile *)&g_pActiveFrameDrawEnv == &g_abFrameDrawEnv0) {
      g_abFrameDrawEnv1.r0 = ((unsigned char *)&color)[0];
      g_abFrameDrawEnv1.g0 = ((unsigned char *)&color)[1];
      g_abFrameDrawEnv1.b0 = ((unsigned char *)&color)[2];
      env = (DRAWENV *)((char *)env + 0x84);
    } else {
      g_abFrameDrawEnv0.r0 = ((unsigned char *)&color)[0];
      g_abFrameDrawEnv0.g0 = ((unsigned char *)&color)[1];
      g_abFrameDrawEnv0.b0 = ((unsigned char *)&color)[2];
    }
    PutDrawEnv(env);
    EmitStaticActorMeshListFogged();
    break;
  }
  }
  FillWord(&g_pOtDepthBinHead0, 0, 0x900);
  RasterizePairedActor();
  DrawOTag(LinkOTPrimitives(0x800));
  DrawSync(0);
  if (g_nDeathRespawnPending != 0) {
    VSync(0);
  }
  g_nVsyncFrameEndCount = VSync(-1);
  if (g_nVsyncFrameEndCount - g_nVsyncFramePaceAnchor < 2) {
    int *end = g_anVsyncFrameEndBlock;
    do {
      VSync(0);
      g_nVsyncFrameEndCount = VSync(-1);
    } while (g_nVsyncFrameEndCount - end[-1] < 2);
  }
  g_nVsyncFramePaceAnchor = VSync(-1);
  PutDispEnv((DISPENV *)((char *)g_pActiveFrameDrawEnv + 0x5C));
}
