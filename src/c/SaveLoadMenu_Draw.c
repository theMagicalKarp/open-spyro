#include "globals.h"
#include "main_shared.h"

extern void *BuildTextSprites(char *str, int *pos, int *size, int a, int b);
extern void DrawShadedMenuBox(int x, int y, int w, int h);
extern void DrawSync(int mode);
extern int VSync(int mode);
extern DISPENV *PutDispEnv(DISPENV *env);

extern short g_anCosineLut[];
typedef struct {
  short x, y, w, h;
} MenuBox;
extern MenuBox D_8006F350[]; /* per-substate menu box */
extern char g_szHiSpyro[];
extern char g_szSaveGame[];
extern char g_szReplayDragon[];
extern char g_szNoSaveFile[];
extern char g_szPleaseRestartWith[];
extern char g_szAMemoryCardTo[];
extern char g_szEnableGameSaves[];
extern char g_szSaving[];
extern char g_szNoMemoryCard[];
extern char g_szPleaseInsertThe[];
extern char g_szMemoryCardWith[];
extern char g_szThisGameSaveFile[];
extern char g_szSaveError[];
extern char g_szPleaseCheckThat[];
extern char g_szTheMemoryCard[];
extern char g_szIsStillInPlace[];
extern char g_szSaveFailed[];
extern char g_szGameSaved[];

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
      BuildTextSprites(g_szHiSpyro, pos, size, 0x12, 0xB);
      pos[1] += 0x18;
      pos[0] = SM[4] + 0x4C;
      BuildTextSprites(g_szSaveGame, pos, size, 0x12, 0xB);
      pos[1] += 0x13;
      if (SM[2] == 0) {
        rec = g_pSpriteRecordWriteCursor;
        n = 8;
      }
      pos[0] = SM[4] + 0x4C;
      BuildTextSprites(g_szReplayDragon, pos, size, 0x12, 0xB);
      pos[1] += 0x13;
      if (SM[2] == 1) {
        rec = g_pSpriteRecordWriteCursor;
        n = 0xC;
      }
      pos[0] = SM[4] + 0x4C;
      BuildTextSprites(g_szContinue, pos, size, 0x12, 0xB);
      if (SM[2] == 2) {
        rec = g_pSpriteRecordWriteCursor;
        n = 8;
      }
      WAVE(rec, n, SM[1]);
      break;
    case 1:
      pos[1] = 0x32;
      pos[0] = SM[4] + 0x52;
      BuildTextSprites(g_szNoSaveFile, pos, size, 0x12, 0xB);
      rec = g_pSpriteRecordWriteCursor;
      pos[1] += 0x1A;
      WAVE(rec, 10, SM[1]);
      size[0] = 0xE;
      size[1] = 1;
      size[2] = 0x1600;
      pos[2] = 0x1400;
      pos[0] = SM[4] + 0x2D;
      BuildTextSprites(g_szPleaseRestartWith, pos, size, 0x10, 0xB);
      pos[1] += 0x11;
      pos[0] = SM[4] + 0x43;
      BuildTextSprites(g_szAMemoryCardTo, pos, size, 0x10, 0xB);
      pos[1] += 0x11;
      pos[0] = SM[4] + 0x38;
      BuildTextSprites(g_szEnableGameSaves, pos, size, 0x10, 0xB);
      break;
    case 2:
      pos[1] = 0x52;
      pos[0] = SM[4] + 0x6A;
      BuildTextSprites(g_szSaving, pos, size, 0x12, 0xB);
      rec = g_pSpriteRecordWriteCursor;
      WAVE(rec, 9, SM[1]);
      break;
    case 3:
    case 4:
      pos[1] = 0x32;
      if (g_nSaveMenuSubstate == 3) {
        pos[0] = g_nSaveMenuXOffset + 0x42;
        BuildTextSprites(g_szNoMemoryCard, pos, size, 0x12, 0xB);
        n = 0xC;
      } else {
        pos[0] = g_nSaveMenuXOffset + 0x54;
        BuildTextSprites(g_szNoSaveFile, pos, size, 0x12, 0xB);
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
      BuildTextSprites(g_szPleaseInsertThe, pos, size, 0x10, 0xB);
      pos[1] += 0x11;
      pos[0] = SM[4] + 0x43;
      BuildTextSprites(g_szMemoryCardWith, pos, size, 0x10, 0xB);
      pos[1] += 0x11;
      pos[0] = SM[4] + 0x2D;
      BuildTextSprites(g_szThisGameSaveFile, pos, size, 0x10, 0xB);
      break;
    case 5:
      pos[1] = 0x32;
      pos[0] = SM[4] + 0x5E;
      BuildTextSprites(g_szSaveError, pos, size, 0x12, 0xB);
      rec = g_pSpriteRecordWriteCursor;
      pos[1] += 0x1A;
      WAVE(rec, 9, SM[1]);
      size[0] = 0xE;
      size[1] = 1;
      size[2] = 0x1600;
      pos[2] = 0x1400;
      pos[0] = SM[4] + 0x3B;
      BuildTextSprites(g_szPleaseCheckThat, pos, size, 0x10, 0xB);
      pos[1] += 0x11;
      pos[0] = SM[4] + 0x49;
      BuildTextSprites(g_szTheMemoryCard, pos, size, 0x10, 0xB);
      pos[1] += 0x11;
      pos[0] = SM[4] + 0x36;
      BuildTextSprites(g_szIsStillInPlace, pos, size, 0x10, 0xB);
      break;
    case 6:
      pos[1] = 0x35;
      pos[0] = SM[4] + 0x56;
      BuildTextSprites(g_szSaveFailed, pos, size, 0x12, 0xB);
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
      BuildTextSprites(g_szGameSaved, pos, size, 0x12, 0xB);
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
