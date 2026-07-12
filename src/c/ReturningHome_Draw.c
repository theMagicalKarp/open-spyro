#include "globals.h"

extern void *BuildTextSprites(unsigned char *str, int *pos, int *size, int a,
                              int b);
extern void EnqueuePendingSpritePrims(void);
extern void FillWord(void *dst, unsigned int value, int byte_count);
extern void RasterizeSpritePrimQueue(void);
extern void DrawSync(int mode);
extern int VSync(int mode);
extern DISPENV *PutDispEnv(DISPENV *env);
extern void PutDrawEnv(void *env);
extern void *LinkOTPrimitives(int depth_max);
extern void DrawOTag(void *ot);

extern unsigned char D_80010ACC[];     /* "RETURNING HOME..." */
extern unsigned short g_anCosineLut[]; /* g_anSineLut + 0x80 */
extern int g_anVsyncFrameEndBlock[];   /* held-base alias: [-1] = anchor */

/* Draw gamestate 0xF's post-credits "RETURNING HOME..." card with the animated
   per-glyph brightness wave (g_anCosineLut stepped by g_nReturningHomePhase),
   then the standard sprite-queue rasterize + frame-flush tail (0x8001eb80). */
void ReturningHome_Draw(void) {
  void *pvVar1;
  void **queue;
  int iVar2;
  int iVar3;
  int sizeVec[3];
  int posVec[3];

  pvVar1 = g_pSpriteRecordWriteCursor;
  sizeVec[0] = 0x10;
  sizeVec[1] = 1;
  sizeVec[2] = 0x1400;
  posVec[0] = 0x76;
  posVec[1] = 0x6e;
  posVec[2] = 0x1100;
  g_abFrameDrawEnv0.r0 = 0;
  g_abFrameDrawEnv0.g0 = 0;
  g_abFrameDrawEnv0.b0 = 0;
  g_abFrameDrawEnv1.r0 = 0;
  g_abFrameDrawEnv1.g0 = 0;
  g_abFrameDrawEnv1.b0 = 0;
  BuildTextSprites(D_80010ACC, posVec, sizeVec, 0x12, 0xb);
  iVar3 = (int)pvVar1 - 0x58;
  iVar2 = 0;
  if ((int)g_pSpriteRecordWriteCursor <= iVar3) {
    do {
      *(char *)(iVar3 + 0x46) =
          g_anCosineLut[(g_nReturningHomePhase * 2 + iVar2) & 0xff] >> 7;
      iVar3 -= 0x58;
      iVar2 += 0xc;
    } while ((int)g_pSpriteRecordWriteCursor <= iVar3);
  }
  queue = g_apSpritePrimQueue;
  queue[0] = 0;
  EnqueuePendingSpritePrims();
  FillWord((char *)queue - 0x2400, 0, 0x900);
  RasterizeSpritePrimQueue();
  DrawSync(0);
  g_nVsyncFrameEndCount = VSync(-1);
  if (g_nVsyncFrameEndCount - g_nVsyncFramePaceAnchor < 2) {
    int *end = g_anVsyncFrameEndBlock;
    do {
      VSync(0);
      g_nVsyncFrameEndCount = VSync(-1);
    } while (g_nVsyncFrameEndCount - end[-1] < 2);
  }
  g_nVsyncFramePaceAnchor = VSync(-1);
  PutDispEnv((DISPENV *)((char *)g_pActiveFrameDrawEnv + 0x5c));
  PutDrawEnv(g_pActiveFrameDrawEnv);
  DrawOTag(LinkOTPrimitives(0x800));
}
