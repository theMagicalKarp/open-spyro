/* Credits_Draw (0x8007BFD0, credits overlay).
 *
 * Credits-sequence frame draw. While the sequence gate (g_nGamestate0fMode) is -1,
 * only scrolls the double-height framebuffer: DrawSync/VSync, then MoveImage
 * (MoveImage) of the 512x224 strip at y=8 (front env active) or y=0xF8
 * to the opposite half, and a final DrawSync. Otherwise draws a full frame:
 * copies the fade color bytes (g_dwWorldFogColor..52) into both frame DRAWENVs,
 * builds one 0x58-byte sprite prim per credits line (records at D_8007589C,
 * 0x1C stride; prim cursor g_pSpriteRecordWriteCursor grows downward; record type 0x115 gets
 * a +0x29 glyph offset and depth 0x1C00), submits the pending sprite queue,
 * clears the OT bins, emits the static world, then runs the standard
 * 2-vblank pace loop and PutDispEnv/PutDrawEnv/DrawOTag submit.
 */

#include "level_globals.h"
#include "level_funcs.h"

extern void MoveImage();
extern void DrawFullscreenTint();
extern void RasterizeSpritePrimQueue(void);
extern void FillBlock16();
extern void RenderWorldChunks();
extern void EmitStaticActorMeshList();
extern void *LinkOTPrimitives();

extern int g_nGamestate0fMode;
extern unsigned char *g_pActiveFrameDrawEnv;
extern unsigned char g_abFrameDrawEnv0[];
extern unsigned char g_dwWorldFogColor;
extern unsigned char D_80078A51;
extern unsigned char D_80078A52;
extern unsigned char D_80076EF9;
extern unsigned char D_80076EFA;
extern unsigned char D_80076EFB;
extern unsigned char D_80076F7D;
extern unsigned char D_80076F7E;
extern unsigned char D_80076F7F;
extern int D_800756E4;
extern char *g_pSpriteRecordWriteCursor;
extern char *D_8007589C;
extern int g_nGenericCountdown;
extern int g_apSpritePrimQueue[];
extern int g_nFarPlaneDrawDistance;
extern int D_80076DE4[];
extern int g_nVsyncFrameEndCount[];

void Credits_Draw(void) {
  int i;

  if (g_nGamestate0fMode == -1) {
    struct {
      short x, y, w, h;
    } rect;
    int y;
    DrawSync(0);
    VSync(0);
    y = 8;
    rect.x = 0;
    if (g_pActiveFrameDrawEnv != g_abFrameDrawEnv0) {
      y = 0xF8;
    }
    rect.w = 0x200;
    rect.y = y;
    rect.h = 0xE0;
    MoveImage(&rect, 0, 0x100 - y);
    DrawSync(0);
    return;
  }
  {
    int r = g_dwWorldFogColor;
    int g = D_80078A51;
    int b = D_80078A52;
    D_80076EF9 = r;
    D_80076EFA = g;
    D_80076EFB = b;
    D_80076F7D = r;
    D_80076F7E = g;
    D_80076F7F = b;
  }
  for (i = 0; i < D_800756E4; i++) {
    {
      char *rec;
      char *prim = g_pSpriteRecordWriteCursor - 0x58;
      g_pSpriteRecordWriteCursor = prim;
      rec = D_8007589C + i * 0x1C;
      FillWord(prim, 0, 0x58);
      *(int *)(g_pSpriteRecordWriteCursor + 0xC) = *(short *)(rec + 0x12);
      *(int *)(g_pSpriteRecordWriteCursor + 0x10) = *(short *)(rec + 0x14);
      *(int *)(g_pSpriteRecordWriteCursor + 0x14) = *(short *)(rec + 0x16);
      *(unsigned char *)(g_pSpriteRecordWriteCursor + 0x44) = rec[5];
      if (*(short *)(rec + 0x1A) == 0x115) {
        *(unsigned char *)(g_pSpriteRecordWriteCursor + 0x44) += 0x29;
        *(int *)(g_pSpriteRecordWriteCursor + 0x14) = 0x1C00;
      }
      *(unsigned char *)(g_pSpriteRecordWriteCursor + 0x45) = rec[6];
      *(unsigned char *)(g_pSpriteRecordWriteCursor + 0x46) = rec[7];
      *(unsigned short *)(g_pSpriteRecordWriteCursor + 0x36) = *(unsigned short *)(rec + 0x1A);
      *(unsigned char *)(g_pSpriteRecordWriteCursor + 0x4F) = 0xB;
      *(unsigned char *)(g_pSpriteRecordWriteCursor + 0x50) = 0xFF;
      *(unsigned char *)(g_pSpriteRecordWriteCursor + 0x47) = 0x7F;
    }
  }
  {
    int n = g_nGenericCountdown;
    if (n != 0) {
      int s = n << 4;
      DrawFullscreenTint(2, s, s, s);
    }
  }
  {
    int *q = g_apSpritePrimQueue;
    char *ot;
    q[0] = 0;
    EnqueuePendingSpritePrims();
    ot = (char *)q - 0x2400;
    FillWord(ot, 0, 0x900);
    RasterizeSpritePrimQueue();
    FillBlock16(ot, 0, 0x1C00);
    g_nFarPlaneDrawDistance = 0x28000;
    RenderWorldChunks(-1);
    EmitStaticActorMeshList(-1, D_80076DE4, (char *)D_80076DE4 - 0x14);
  }
  DrawSync(0);
  if (g_nDeathRespawnPending != 0) {
    VSync(0);
  }
  {
    int *end;
    *(end = g_nVsyncFrameEndCount) = VSync(-1);
    if (end[0] - g_nVsyncFramePaceAnchor < 2) {
      int *p = end;
      do {
        VSync(0);
        p[0] = VSync(-1);
      } while (p[0] - p[-1] < 2);
    }
  }
  g_nVsyncFramePaceAnchor = VSync(-1);
  PutDispEnv(g_pActiveFrameDrawEnv + 0x5C);
  PutDrawEnv(g_pActiveFrameDrawEnv);
  DrawOTag(LinkOTPrimitives(0x800));
}
