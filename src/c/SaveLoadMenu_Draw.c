#include "globals.h"

extern void *BuildTextSprites(char *str, int *pos, int *size, int a, int b);
extern void DrawShadedMenuBox(int x, int y, int w, int h);
extern void BuildRenderEntityLists(void);
extern void ComposeFrameScene(void);
extern void EnqueuePendingSpritePrims(void);
extern void RasterizeSpritePrimQueue(void);
extern void SetupFrameOT(void);
extern void DrawActors(void);
extern void RasterizeEmitList(void);
extern void DrawCinematicLetterbox(void);
extern void DrawSync(int mode);
extern int VSync(int mode);
extern DISPENV *PutDispEnv(DISPENV *env);
extern void PutDrawEnv(void *env);
extern void *LinkOTPrimitives(int depth_max);
extern void DrawOTag(void *ot);

extern int g_anSaveMenuBlock[];      /* [1] phase, [2] cursor, [4] x offset */
extern int g_anVsyncFrameEndBlock[]; /* held-base alias: [-1] = anchor */
extern short g_anCosineLut[];
typedef struct {
  short x, y, w, h;
} MenuBox;
extern MenuBox D_8006F350[]; /* per-substate menu box */
extern char D_80010BA8[];
extern char D_80010BB4[];
extern char D_80010BC0[];
extern char D_80010B6C[];
extern char D_80010BD0[];
extern char D_80010BE0[];
extern char D_80010BF4[];
extern char D_80010C08[];
extern char D_80010C1C[];
extern char D_80010C28[];
extern char D_80010C38[];
extern char D_80010C4C[];
extern char D_80010C60[];
extern char D_80010C74[];
extern char D_80010C80[];
extern char D_80010C94[];
extern char D_80010CA4[];
extern char D_80010CB8[];
extern char D_80010CC4[];

#define SM g_anSaveMenuBlock

/* Bobs the glyphs of the highlighted line on a cosine wave that travels
   along it: each glyph's y offset (+0x46 of its 0x58-byte sprite record). */
#define WAVE(rec, n, phase)                                                    \
  for (i = 0; i < (n); i++) {                                                  \
    (rec)[0x46] = (g_anCosineLut[((phase) * 4 + i * 0xC) & 0xFF] * 3) >> 9;    \
    (rec) += 0x58;                                                             \
  }

/* 0x8001d718 (0xb34): draws the memory-card save/load menu: the substate's
   shaded box, its lines of text (the highlighted line waving), the selected
   slot label, then the scene and the standard frame-flush tail. */
void SaveLoadMenu_Draw(void) {
  int pos[3];
  int size[3];
  unsigned char *rec;
  unsigned char n;
  int i;

  if (g_pSpriteRecordBufferTop == g_pRenderScratchPrimTop) {
    g_pSpriteRecordBufferTop = (char *)g_pPrimBufferWriteCursor + 0x1BA00;
    g_pSpriteRecordWriteCursor = g_pSpriteRecordBufferTop;
  }
  if (g_nSaveMenuMode == 1) {
    DrawShadedMenuBox(g_nSaveMenuXOffset + D_8006F350[g_nSaveMenuSubstate].x,
                      g_nSaveMenuXOffset + D_8006F350[g_nSaveMenuSubstate].y,
                      D_8006F350[g_nSaveMenuSubstate].w,
                      D_8006F350[g_nSaveMenuSubstate].h);
    size[0] = 0x10;
    size[2] = 0x1400;
    size[1] = 1;
    pos[2] = 0x1100;
    switch (g_nSaveMenuSubstate) {
    case 0:
      pos[1] = 0x32;
      pos[0] = SM[4] + 0x40;
      BuildTextSprites(D_80010BA8, pos, size, 0x12, 0xB);
      pos[1] += 0x18;
      pos[0] = SM[4] + 0x4C;
      BuildTextSprites(D_80010BB4, pos, size, 0x12, 0xB);
      pos[1] += 0x13;
      if (SM[2] == 0) {
        rec = g_pSpriteRecordWriteCursor;
        n = 8;
      }
      pos[0] = SM[4] + 0x4C;
      BuildTextSprites(D_80010BC0, pos, size, 0x12, 0xB);
      pos[1] += 0x13;
      if (SM[2] == 1) {
        rec = g_pSpriteRecordWriteCursor;
        n = 0xC;
      }
      pos[0] = SM[4] + 0x4C;
      BuildTextSprites(D_80010B6C, pos, size, 0x12, 0xB);
      if (SM[2] == 2) {
        rec = g_pSpriteRecordWriteCursor;
        n = 8;
      }
      WAVE(rec, n, SM[1]);
      break;
    case 1:
      pos[1] = 0x32;
      pos[0] = SM[4] + 0x52;
      BuildTextSprites(D_80010BD0, pos, size, 0x12, 0xB);
      rec = g_pSpriteRecordWriteCursor;
      pos[1] += 0x1A;
      WAVE(rec, 10, SM[1]);
      size[0] = 0xE;
      size[1] = 1;
      size[2] = 0x1600;
      pos[2] = 0x1400;
      pos[0] = SM[4] + 0x2D;
      BuildTextSprites(D_80010BE0, pos, size, 0x10, 0xB);
      pos[1] += 0x11;
      pos[0] = SM[4] + 0x43;
      BuildTextSprites(D_80010BF4, pos, size, 0x10, 0xB);
      pos[1] += 0x11;
      pos[0] = SM[4] + 0x38;
      BuildTextSprites(D_80010C08, pos, size, 0x10, 0xB);
      break;
    case 2:
      pos[1] = 0x52;
      pos[0] = SM[4] + 0x6A;
      BuildTextSprites(D_80010C1C, pos, size, 0x12, 0xB);
      rec = g_pSpriteRecordWriteCursor;
      WAVE(rec, 9, SM[1]);
      break;
    case 3:
    case 4:
      pos[1] = 0x32;
      if (g_nSaveMenuSubstate == 3) {
        pos[0] = g_nSaveMenuXOffset + 0x42;
        BuildTextSprites(D_80010C28, pos, size, 0x12, 0xB);
        n = 0xC;
      } else {
        pos[0] = g_nSaveMenuXOffset + 0x54;
        BuildTextSprites(D_80010BD0, pos, size, 0x12, 0xB);
        n = 0xA;
      }
      rec = g_pSpriteRecordWriteCursor;
      pos[1] += 0x1A;
      WAVE(rec, n, SM[1]);
      size[0] = 0xE;
      size[1] = 1;
      size[2] = 0x1600;
      pos[2] = 0x1400;
      pos[0] = SM[4] + 0x3B;
      BuildTextSprites(D_80010C38, pos, size, 0x10, 0xB);
      pos[1] += 0x11;
      pos[0] = SM[4] + 0x43;
      BuildTextSprites(D_80010C4C, pos, size, 0x10, 0xB);
      pos[1] += 0x11;
      pos[0] = SM[4] + 0x2D;
      BuildTextSprites(D_80010C60, pos, size, 0x10, 0xB);
      break;
    case 5:
      pos[1] = 0x32;
      pos[0] = SM[4] + 0x5E;
      BuildTextSprites(D_80010C74, pos, size, 0x12, 0xB);
      rec = g_pSpriteRecordWriteCursor;
      pos[1] += 0x1A;
      WAVE(rec, 9, SM[1]);
      size[0] = 0xE;
      size[1] = 1;
      size[2] = 0x1600;
      pos[2] = 0x1400;
      pos[0] = SM[4] + 0x3B;
      BuildTextSprites(D_80010C80, pos, size, 0x10, 0xB);
      pos[1] += 0x11;
      pos[0] = SM[4] + 0x49;
      BuildTextSprites(D_80010C94, pos, size, 0x10, 0xB);
      pos[1] += 0x11;
      pos[0] = SM[4] + 0x36;
      BuildTextSprites(D_80010CA4, pos, size, 0x10, 0xB);
      break;
    case 6:
      pos[1] = 0x35;
      pos[0] = SM[4] + 0x56;
      BuildTextSprites(D_80010CB8, pos, size, 0x12, 0xB);
      pos[1] += 0x28;
      pos[0] = SM[4] + 0x87;
      BuildTextSprites(g_szUiRetry, pos, size, 0x12, 0xB);
      pos[1] += 0x13;
      if (SM[2] == 0) {
        rec = g_pSpriteRecordWriteCursor;
        n = 5;
      }
      pos[0] = SM[4] + 0x87;
      BuildTextSprites(g_szUiAbort, pos, size, 0x12, 0xB);
      if (SM[2] == 1) {
        rec = g_pSpriteRecordWriteCursor;
        n = 5;
      }
      WAVE(rec, n, SM[1]);
      break;
    case 7:
      pos[1] = 0x52;
      pos[0] = SM[4] + 0x5E;
      BuildTextSprites(D_80010CC4, pos, size, 0x12, 0xB);
      rec = g_pSpriteRecordWriteCursor;
      WAVE(rec, 9, SM[1]);
      break;
    }
    if (g_nSaveMenuSubstate >= 2) {
      size[0] = 0xD;
      size[1] = 1;
      size[2] = 0x1A00;
      pos[1] = 0x1D;
      pos[2] = 0x1600;
      if (g_nSaveSlotSelected == 0) {
        pos[0] = g_nSaveMenuXOffset + 0x30;
        BuildTextSprites(g_szUiSlot1, pos, size, 0xE, 0xB);
      } else {
        pos[0] = 0xE6;
        BuildTextSprites(g_szUiSlot2, pos, size, 0xE, 0xB);
      }
    }
  }
  BuildRenderEntityLists();
  ComposeFrameScene();
  if (g_nSaveMenuMode == 1) {
    g_apSpritePrimQueue[0] = 0;
    EnqueuePendingSpritePrims();
    RasterizeSpritePrimQueue();
  }
  SetupFrameOT();
  DrawActors();
  RasterizeEmitList();
  DrawCinematicLetterbox();
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
  PutDispEnv((DISPENV *)((char *)g_pActiveFrameDrawEnv + 0x5c));
  PutDrawEnv(g_pActiveFrameDrawEnv);
  DrawOTag(LinkOTPrimitives(0x800));
}
