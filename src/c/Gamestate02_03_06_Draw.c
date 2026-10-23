#include "actor.h"
#include "globals.h"
#include "main_shared.h"

/* The sprite record being written (Actor layout). */
#define SREC ((Actor *)g_pSpriteRecordWriteCursor)
/* Position stores stay as raw int casts: SREC->posX (MEM_IN_STRUCT_P) and an
   int-array view both let sched move them past the neighbouring global loads
   (cookbook A357); only the raw cast keeps the original order. */

extern void DrawSync(int mode);
extern int VSync(int mode);
extern DISPENV *PutDispEnv(DISPENV *env);
extern void StoreImage(RECT *r, void *dst);
extern void LoadImage(RECT *r, void *src);
extern void ConvertRGB15ToLuma8(void *buf, int count);
extern void AddPrimToOT(int prim);
extern void SetDrawMode(int prim, int dfe, int dtd, unsigned int tpage,
                        RECT *tw);
extern void FillWord(void *dst, unsigned int value, int byte_count);
extern void EmitRadialShadedLine(int x0, int y0, int x1, int y1);
extern void EmitTexturedQuad(short *rect, int *uv, int *rgb);
extern void EnqueueBlinkingMarkerSprite(int *pos, unsigned int phase, int dir);
extern void *BuildTextSpriteChain(char *text, int *pos, int step, int flags);
extern void *BuildTextSprites(char *text, int *pos, int *style, int step,
                              int flags);
extern int ComputeInventoryProgressPercent(void);
extern int strlen(char *s);
extern int sprintf(char *buf, char *fmt, ...);

/* Held-base alias for g_pRenderScratchPrimTop (0x800785f0);
   [-2] is g_pRenderScratchRegionBase. */
/* Held-base view of g_nVsyncFrameEndCount; [-1] = g_nVsyncFramePaceAnchor. */
/* cosine view of the sine LUT (g_anSineLut + 0x80). */
extern short g_anCosineLut[];
/* Held-base alias for g_dwHudBobSinePhase (0x80077fe4): [1] is the mod-9 HUD
   frame counter and [0x14c] is &g_anLevelSpriteTexWordPairs[2]. */
extern int g_adwHudBobSinePhaseBlock[];
/* Same block seen from g_nHudFrameCycleMod9 (0x80077fe8): [0x14b] is
   &g_anLevelSpriteTexWordPairs[2]. */
extern int g_anHudFrameCycleBlock[];
/* Held-base (incomplete-array) views of four tables the inventory row loop
   keeps in one register each; the plain sized decls in globals.h stay in use
   at the sites the original addresses absolutely (A240). */
extern unsigned char g_abLevelTotalDragonsBlock[];
extern int g_anLevelGemsBlock[];
extern unsigned char g_abWorldTitleGlyphBlock[];
/* menu label blobs that have no friendly name yet. */
extern char g_szQuitGamePrompt[]; /* "QUIT GAME?"  */
extern char g_szSoundEffects[];   /* "SOUND FX"    */
extern char g_szMusicVolume[];    /* "MUSIC"       */
extern char g_szSpeakerSetup[];   /* "SOUND"       */
extern char g_szVibration[];      /* "VIBRATION"   */
extern char g_szInventory[];      /* "INVENTORY"   */
extern char g_szExitLevel[];      /* "EXIT LEVEL"  */
extern char g_szQuitGame[];       /* "EXIT WORLD"  */

/* Draw for gamestates 2 (pause menu), 3 (inventory) and 6 (dialog overlay)
   — 0x8001a40c, 8840 bytes.

   On the entry frame (g_nPauseMenuTransitionFrames == 0) it renders one last
   full gameplay scene, then freezes it: StoreImage pulls the frame back out
   of VRAM, ConvertRGB15ToLuma8 packs it to 8bpp luma four 0x80-wide strips at
   a time, each strip is re-uploaded at x=0x200 and the gray CLUT is loaded at
   (0x200,0xe0).  Every later frame re-draws that backdrop as four textured
   quads, draws the menu on top, and submits the frame with the shared
   two-vblank pacing tail.

   Match notes (the non-obvious levers):
   - h0..h7 are never-allocated locals whose only reference is an empty "g"
     asm: each takes one 8-byte reload slot above `rows` (sp+0xA0), which
     together with the USE-only pseudos combine leaves behind reproduces the
     original's 336-byte frame.
   - `pp`/`bp` pinned to s4/s6 hold &pos/&buf for the inventory row loop; the
     empty volatile asms around them are scheduling fences (a hard-reg set
     otherwise inherits the preceding call's sched2 priority and sinks to the
     row-loop preheader tail).
   - The gs3 Gems address and record-2 frames load use the same fence /
     `$2` pin to get the original's la/sll and load/store order.
   - The second marker test is nested so `pa = pos` sits in the guard block,
     where reorg moves it into the branch delay slot. */
typedef struct {
  int tag;
  unsigned char r0, g0, b0, code;
  short x0, y0, x1, y1, x2, y2, x3, y3;
} PolyF4;

void Gamestate02_03_06_Draw(void) {
  RECT rect;          /* sp18 */
  char buf[0x20];     /* sp20 */
  int pos[3];         /* sp40 */
  int styA[3];        /* sp50 */
  int styB[3];        /* sp60 */
  int styC[3];        /* sp70 */
  short quad[4];      /* sp80 */
  short quad2[4];     /* sp88 */
  int styD[3];        /* sp90 */
  unsigned char rows; /* spA0 */
  int h0, h1, h2, h3, h4, h5, h6, h7;

  if (g_nPauseMenuTransitionFrames == 0) {
    int i;
    int h;
    int strip;
    int y;
    void **img;
    void *arg;
    void **img2;
    DRAWENV *envBase;
    register int k18 asm("$18");
    i = 0;
    __asm__("" : "=r"(k18));
    BuildRenderEntityLists();
    h = 0xe0;
    ComposeFrameScene();
    do {
      strip = 0x200;
      RasterizeEmitList();
      DrawActors();
      SetupFrameOT();
      DrawSync(0);
      VSync(0);
      envBase = &g_abFrameDrawEnv0;
    } while (0);
    do {
      PutDispEnv((DISPENV *)((char *)g_pActiveFrameDrawEnv + 0x5c));
      img = (void **)g_apRenderScratchPrimTopBlock;
    } while (0);
    __asm__ volatile("" : : "r"(k18));
    img2 = img - 2;
    PutDrawEnv(g_pActiveFrameDrawEnv);
    DrawOTag(LinkOTPrimitives(0x800));
    DrawSync(0);
    VSync(0);
    PutDispEnv((DISPENV *)((char *)g_pActiveFrameDrawEnv + 0x5c));
    rect.x = 0x200;
    rect.y = 0;
    rect.w = 0x100;
    rect.h = 0xe1;
    StoreImage(&rect, (void *)((char *)img[0] + 0xFFFE3E00));
    y = 8;
    do {
      __asm__ volatile("" : : "r"(strip));
      rect.x = i << 7;
      if (g_pActiveFrameDrawEnv != envBase) {
        y = 0xf8;
      }
      rect.y = y;
      rect.w = 0x80;
      rect.h = h;
      ((void (*)())StoreImage)(&rect, img2[0], y);
      i += 1;
      DrawSync(0);
      ConvertRGB15ToLuma8(img2[0], 0x7000);
      rect.x = strip;
      rect.y = 0;
      rect.w = 0x40;
      rect.h = h;
      LoadImage(&rect, img2[0]);
      strip += 0x40;
      y = 8;
    } while (i < 4);
    rect.x = 0x200;
    rect.y = 0xe0;
    rect.w = 0x20;
    rect.h = 1;
    LoadImage(&rect, g_anMenuBackdropGrayClut);
    DrawSync(0);
    g_nVsyncFramePaceAnchor = VSync(-1);
  } else {
    void *prim;
    int i;
    int h;
    char *env;
    char *cur;
    char *p;
    char *next;
    char *tv;
    register int b asm("$20");
    int a;
    int d;
    int tpage;
    int x;
    unsigned short bh;
    unsigned short ch;
    unsigned char bq;
    unsigned char cq;
    cur = (*(char **)&g_pActiveFrameDrawEnv);
    env = ((char *)&g_abFrameDrawEnv0);
    if (cur == env) {
      env += 0x84;
    }
    i = 0;
    PutDrawEnv((DRAWENV *)env);
    {
      int off, bias;
      do {
        off = 0x1C000;
        bias = 0xFFFE3E00;
      } while (0);
      {
        register char *e8 asm("$3");
        register char *f0 asm("$2");
        e8 = (*(char **)&g_pRenderScratchRegionBase);
        f0 = (*(char **)&g_pRenderScratchPrimTop);
        g_nPrimBufferOverflowFlag = 0;
        (*(char **)&g_pPrimBufferWriteCursor) = e8;
        (*(char **)&g_pPrimBufferLimit) = e8 + off;
        tv = f0 + bias;
      }
    }
    (*(char **)&g_pSpriteRecordBufferTop) = tv;
    (*(char **)&g_pSpriteRecordWriteCursor) = tv;
  quad: {
    register int xx asm("$3");
    register int tp asm("$7");
    xx = i << 7;

    tp = i + 0x88;
    p = (*(char **)&g_pPrimBufferWriteCursor);
    {
      register int a asm("$2");
      register int d asm("$3");
      register unsigned short bh asm("$5");
      register unsigned short ch asm("$6");
      register unsigned char bq asm("$5");
      register unsigned char cq asm("$6");
      *((volatile int *)p) = 0x9000000;
      *((volatile unsigned char *)(p + 0x7)) = 0x2C;
      *((volatile unsigned char *)(p + 0x4)) = 0x4C;
      *((volatile unsigned char *)(p + 0x5)) = 0x80;
      *((volatile short *)(p + 0x8)) = xx;
      *((volatile short *)(p + 0xA)) = 8;
      a = *((volatile unsigned short *)(p + 0x8));
      bh = *((volatile unsigned short *)(p + 0xA));
      ch = *((volatile unsigned short *)(p + 0x8));
      d = *((volatile unsigned short *)(p + 0xA));
      *((volatile short *)(p + 0x10)) = a + 0x80;
      a = *((volatile unsigned short *)(p + 0x8));
      *((volatile short *)(p + 0x1A)) = d + 0xDF;
      d = *((volatile unsigned short *)(p + 0xA));
      i += 1;
      *((volatile unsigned char *)(p + 0xD)) = 0;
      *((volatile short *)(p + 0x12)) = bh;
      bq = *((volatile unsigned char *)(p + 0xD));
      b = 0x40;
      *((volatile unsigned char *)(p + 0xC)) = 0;
      *((volatile short *)(p + 0x18)) = ch;
      cq = *((volatile unsigned char *)(p + 0xC));
      *((volatile short *)(p + 0x20)) = a + 0x80;
      a = *((volatile unsigned char *)(p + 0xC));
      *((volatile short *)(p + 0x22)) = d + 0xDF;
      d = *((volatile unsigned char *)(p + 0xD));
      *((volatile unsigned char *)(p + 0x6)) = b;
      *((volatile unsigned char *)(p + 0x15)) = bq;
      *((volatile unsigned char *)(p + 0x1C)) = cq;
      *((volatile unsigned char *)(p + 0x14)) = a + 0x80U;
      a = *((volatile unsigned char *)(p + 0xC));
      *((volatile unsigned char *)(p + 0x1D)) = d - 0x21;
      d = *((volatile unsigned char *)(p + 0xD));
      *((volatile unsigned char *)(p + 0x24)) = a + 0x80U;
      *((volatile unsigned char *)(p + 0x25)) = d - 0x21;
      *((short *)(p + 0xE)) = 0x3820;
      *((short *)(p + 0x16)) = tp;
      AddPrimToOT((int)p);
    }
  }
    next = p + 0x28;
    (*(char **)&g_pPrimBufferWriteCursor) = next;
    if (i < 4) {
      goto quad;
    }
    *((int *)(p + 0x28)) = 0x3000000;
    *((unsigned char *)(p + 0x2F)) = b;
    *((short *)(p + 0x30)) = 0;
    *((short *)(p + 0x32)) = 0xE7;
    *((short *)(p + 0x34)) = 0x200;
    *((short *)(p + 0x36)) = 0xE7;
    *((unsigned char *)(p + 0x2C)) = 0;
    *((unsigned char *)(p + 0x2D)) = 0;
    *((unsigned char *)(p + 0x2E)) = 0;
    AddPrimToOT((int)next);
    (*(char **)&g_pPrimBufferWriteCursor) = p + 0x38;
    prim = p;
    h = b;
    if (g_nGamestate == 2) {
      unsigned char *rec;
      unsigned char *fr;
      short *lut1, *lut2, *lut3, *lut4, *lut6, *lut7, *lut8, *lut9;
      unsigned char nGlyphs;
      int cur;
      char *rp;
      int n1, n2, n3, n4, n5, n6;
      int phase;
      int v;
      int phase1, v1, phase2, v2, phase3, v3, phase4, phase5, v4, phase6,
          phase7, v5, phase8, v6, phase9, v7, phase10, v8;
      int y;
      void *box = (void *)((int)prim + 0x38);
      SetDrawMode((int)box, 1, 0, 0x40, (RECT *)0);
      AddPrimToOT((int)box);
      {
        int k;
        k = 0x5000000;
        *((int *)((int)prim + 0x44)) = k;
        k = 0x2a;
        *((unsigned char *)((int)prim + 0x4b)) = k;
        k = 0x43;
        *((unsigned char *)((int)prim + 0x48)) = h;
        *((unsigned char *)((int)prim + 0x49)) = h;
        *((unsigned char *)((int)prim + 0x4a)) = h;
        ((PolyF4 *)((int)prim + 0x44))->y0 = k;
        ((PolyF4 *)((int)prim + 0x44))->y1 = k;
      }
      box = (void *)((int)prim + 0x44);
      if (g_nPauseMenuSubstate == 1) {
        ((PolyF4 *)((int)prim + 0x44))->x0 = 0x54;
        ((PolyF4 *)((int)prim + 0x44))->x1 = 0x1ac;
        rows = g_nPadIsDualshockFlag != 0;
        y = rows * 0x10 + 0xb6;
        ((PolyF4 *)((int)prim + 0x44))->y2 = y;
        ((PolyF4 *)((int)prim + 0x44))->y3 = y;
      } else {
        rows = 1;
        ((PolyF4 *)((int)prim + 0x44))->x0 = 0x8c;
        ((PolyF4 *)((int)prim + 0x44))->x1 = 0x174;
        ((PolyF4 *)((int)prim + 0x44))->y2 = 0xb0;
        ((PolyF4 *)((int)prim + 0x44))->y3 = 0xb0;
      }
      *((short *)((int)box + 0x10)) = *((unsigned short *)((int)box + 8));
      *((short *)((int)box + 0x14)) = *((unsigned short *)((int)box + 0xc));
      AddPrimToOT((int)box);
      g_pPrimBufferWriteCursor = (void *)((int)box + 0x18);
      EmitRadialShadedLine(0xe0, 0x61, 0x120, 0x61);
      if (g_nPauseMenuSubstate == 1) {
        EmitRadialShadedLine(0x54, 0x43, 0x1ac, 0x43);
        {
          int ve = rows * 0x10 + 0xb6;
          EmitRadialShadedLine(0x1ac, 0x43, 0x1ac, ve);
          EmitRadialShadedLine(0x1ac, ve, 0x54, ve);
          EmitRadialShadedLine(0x54, ve, 0x54, 0x43);
        }
      } else {
        EmitRadialShadedLine(0x8c, 0x43, 0x174, 0x43);
        {
          int ve = rows * 0x12 + 0x9e;
          EmitRadialShadedLine(0x174, 0x43, 0x174, ve);
          EmitRadialShadedLine(0x174, ve, 0x8c, ve);
          EmitRadialShadedLine(0x8c, ve, 0x8c, 0x43);
        }
      }
      pos[0] = 0xba;
      pos[1] = 0x52;
      pos[2] = 0xc00;
      BuildTextSpriteChain(g_szUiPaused, pos, 0x1c, 0xb);
      if (g_nPauseMenuSubstate == 2) {
        styA[0] = 0x10;
        styA[1] = 1;
        styA[2] = 0x1400;
        pos[0] = 0xb7;
        pos[1] = 0x77;
        pos[2] = 0x1100;
        BuildTextSprites(g_szQuitGamePrompt, pos, styA, 0x12, 0xb);
        pos[0] = 0xc0;
        pos[1] = 0x94;
        pos[2] = 0x1100;
        BuildTextSprites(g_szUiYes, pos, styA, 0x12, 0xb);
        if (g_nPauseMenuCursor == 0) {
          rec = (unsigned char *)g_pSpriteRecordWriteCursor;
          nGlyphs = 3;
        }
        pos[0] = 0x128;
        pos[1] = 0x94;
        pos[2] = 0x1100;
        BuildTextSprites(g_szUiNo, pos, styA, 0x12, 0xb);
        if (g_nPauseMenuCursor == 1) {
          rec = (unsigned char *)g_pSpriteRecordWriteCursor;
          nGlyphs = 2;
        }
        n1 = nGlyphs;
        i = 0;
        if (n1 > 0) {
          lut1 = g_anCosineLut;
          phase1 = 0;
          do {
            i++;
            v1 = lut1[(g_nPauseMenuTransitionFrames * 8 + phase1) & 0xff];
            phase1 += 0xc;
            rec[0x46] = v1 * 3 >> 9;
            rec += 0x58;
          } while (i < n1);
        }
      } else if (g_nPauseMenuSubstate == 1) {
        styB[0] = 0xf;
        styB[2] = 0x1600;
        styB[1] = 1;
        pos[0] = 0x6b;
        pos[1] = 0x6c;
        pos[2] = 0x1400;
        BuildTextSprites(g_szSoundEffects, pos, styB, 0x10, 0xb);
        if (g_nPauseMenuCursor == 0) {
          rec = (unsigned char *)g_pSpriteRecordWriteCursor;
          nGlyphs = 0xb;
        }
        pos[0] = 0x79;
        pos[1] = 0x7c;
        pos[2] = 0x1400;
        BuildTextSprites(g_szMusicVolume, pos, styB, 0x10, 0xb);
        if (g_nPauseMenuCursor == 1) {
          rec = (unsigned char *)g_pSpriteRecordWriteCursor;
          nGlyphs = 0xb;
        }
        pos[0] = 0x6b;
        pos[1] = 0x8c;
        pos[2] = 0x1400;
        BuildTextSprites(g_szSpeakerSetup, pos, styB, 0x10, 0xb);
        if (g_nPauseMenuCursor == 2) {
          rec = (unsigned char *)g_pSpriteRecordWriteCursor;
          nGlyphs = 0xc;
        }
        if (g_nPadIsDualshockFlag != 0) {
          pos[0] = 0xa3;
          pos[1] = 0x9c;
          pos[2] = 0x1400;
          BuildTextSprites(g_szVibration, pos, styB, 0x10, 0xb);
          if (g_nPauseMenuCursor == 3) {
            rec = (unsigned char *)g_pSpriteRecordWriteCursor;
            nGlyphs = 9;
          }
        }
        pos[0] = 0xd0;
        pos[2] = 0x1100;
        v = rows;
        v <<= 4;
        pos[1] = v + 0x9c;
        BuildTextSprites(g_szUiCamera, pos, styB, 0x10, 0xb);
        if (g_nPauseMenuCursor == 4) {
          rec = (unsigned char *)g_pSpriteRecordWriteCursor;
          nGlyphs = 6;
        }
        pos[0] = 0xee;
        pos[1] = v + 0xac;
        pos[2] = 0x1100;
        BuildTextSprites(g_szUiDone, pos, styB, 0x10, 0xb);
        if (g_nPauseMenuCursor == 5) {
          rec = (unsigned char *)g_pSpriteRecordWriteCursor;
          nGlyphs = 4;
        }
        n2 = nGlyphs;
        i = 0;
        if (n2 > 0) {
          lut2 = g_anCosineLut;
          phase2 = 0;
          do {
            i++;
            v2 = lut2[(g_nPauseMenuTransitionFrames * 8 + phase2) & 0xff];
            phase2 += 0xc;
            rec[0x46] = v2 * 3 >> 9;
            rec += 0x58;
          } while (i < n2);
        }
        if (g_nSfxVolume == 0) {
          pos[0] = 0x142;
          pos[1] = 0x6c;
          pos[2] = 0x1400;
          BuildTextSprites(g_szUiOff, pos, styB, 0x10, 0xb);
          phase3 = 0;
          if (g_nPauseMenuCursor == 0) {
            rec = (unsigned char *)g_pSpriteRecordWriteCursor;
            i = 0;
            do {
              i++;
              v3 = g_anCosineLut[(g_nPauseMenuTransitionFrames * 8 + phase3) &
                                 0xff];
              phase3 += 0xc;
              rec[0x46] = v3 * 3 >> 9;
              rec += 0x58;
            } while (i < 3);
          }
        } else {
          i = 0;
          if (g_nSfxVolume > 0) {
            lut4 = g_anCosineLut;
            phase4 = 0;
            x = 0x140;
            do {
              g_pSpriteRecordWriteCursor =
                  (unsigned char *)g_pSpriteRecordWriteCursor - 0x58;
              FillWord(g_pSpriteRecordWriteCursor, 0, 0x58);
              rp = (char *)g_pSpriteRecordWriteCursor;
              cur = g_nPauseMenuCursor;
              *((short *)((int)rp + 0x36)) = 0x1b2;
              *((int *)((int)rp + 0x10)) = 0x6d;
              *((int *)((int)rp + 0xc)) = x;
              *((int *)((int)rp + 0x14)) = 0x1400;
              if (cur == 0) {
                *((unsigned char *)((int)rp + 0x46)) =
                    lut4[(g_nPauseMenuTransitionFrames * 8 + phase4) & 0xff] *
                        3 >>
                    9;
              } else {
                *((unsigned char *)((int)rp + 0x46)) = 0;
              }
              phase4 += 0xc;
              SREC->otBias = 0x7f;
              SREC->tintMode = 0xb;
              SREC->cullRadius = 0xff;
              x += 0xa;
              i++;
            } while (i < g_nSfxVolume);
          }
        }
        if (g_nMusicVolume == 0) {
          pos[0] = 0x142;
          pos[1] = 0x7c;
          pos[2] = 0x1400;
          BuildTextSprites(g_szUiOff, pos, styB, 0x10, 0xb);
          phase5 = 0;
          if (g_nPauseMenuCursor == 1) {
            rec = (unsigned char *)g_pSpriteRecordWriteCursor;
            i = 0;
            do {
              i++;
              v4 = g_anCosineLut[(g_nPauseMenuTransitionFrames * 8 + phase5) &
                                 0xff];
              phase5 += 0xc;
              rec[0x46] = v4 * 3 >> 9;
              rec += 0x58;
            } while (i < 3);
          }
        } else {
          i = 0;
          if (g_nMusicVolume > 0) {
            lut6 = g_anCosineLut;
            phase6 = 0;
            x = 0x140;
            do {
              g_pSpriteRecordWriteCursor =
                  (unsigned char *)g_pSpriteRecordWriteCursor - 0x58;
              FillWord(g_pSpriteRecordWriteCursor, 0, 0x58);
              rp = (char *)g_pSpriteRecordWriteCursor;
              cur = g_nPauseMenuCursor;
              *((short *)((int)rp + 0x36)) = 0x1b2;
              *((int *)((int)rp + 0x10)) = 0x7d;
              *((int *)((int)rp + 0x14)) = 0x1400;
              *((int *)((int)rp + 0xc)) = x;
              if (cur == 1) {
                *((unsigned char *)((int)rp + 0x46)) =
                    lut6[(g_nPauseMenuTransitionFrames * 8 + phase6) & 0xff] *
                        3 >>
                    9;
              } else {
                *((unsigned char *)((int)rp + 0x46)) = 0;
              }
              phase6 += 0xc;
              SREC->otBias = 0x7f;
              SREC->tintMode = 0xb;
              SREC->cullRadius = 0xff;
              x += 0xa;
              i++;
            } while (i < g_nMusicVolume);
          }
        }
        pos[0] = 0x142;
        pos[1] = 0x8c;
        pos[2] = 0x1400;
        if (g_nSoundMonoMixFlag != 0) {
          BuildTextSprites(g_szUiMono, pos, styB, 0x10, 0xb);
          n3 = 4;
        } else {
          BuildTextSprites(g_szUiStereo, pos, styB, 0x10, 0xb);
          n3 = 6;
        }
        if (g_nPauseMenuCursor == 2) {
          rec = (unsigned char *)g_pSpriteRecordWriteCursor;
          i = 0;
          if (n3 != 0) {
            lut7 = g_anCosineLut;
            phase7 = 0;
            do {
              i++;
              v5 = lut7[(g_nPauseMenuTransitionFrames * 8 + phase7) & 0xff];
              phase7 += 0xc;
              rec[0x46] = v5 * 3 >> 9;
              rec += 0x58;
            } while (i < n3);
          }
        }
        if (g_nPadIsDualshockFlag != 0) {
          pos[0] = 0x142;
          pos[1] = 0x9c;
          pos[2] = 0x1400;
          if (g_nOptionVibrationEnabled != 0) {
            BuildTextSprites(g_szUiOn, pos, styB, 0x10, 0xb);
            n4 = 2;
          } else {
            BuildTextSprites(g_szUiOff, pos, styB, 0x10, 0xb);
            n4 = 3;
          }
          if (g_nPauseMenuCursor == 3) {
            rec = (unsigned char *)g_pSpriteRecordWriteCursor;
            i = 0;
            if (n4 != 0) {
              lut8 = g_anCosineLut;
              phase8 = 0;
              do {
                i++;
                v6 = lut8[(g_nPauseMenuTransitionFrames * 8 + phase8) & 0xff];
                phase8 += 0xc;
                rec[0x46] = v6 * 3 >> 9;
                rec += 0x58;
              } while (i < n4);
            }
          }
        }
        pos[0] = 0x142;
        pos[1] = rows * 0x10 + 0x9c;
        pos[2] = 0x1400;
        if (g_dwActiveCameraOptions == 2) {
          BuildTextSprites(g_szUiActive, pos, styB, 0x10, 0xb);
          n5 = 6;
        } else {
          BuildTextSprites(g_szUiPassive, pos, styB, 0x10, 0xb);
          n5 = 7;
        }
        if (g_nPauseMenuCursor == 4) {
          rec = (unsigned char *)g_pSpriteRecordWriteCursor;
          i = 0;
          if (n5 != 0) {
            lut9 = g_anCosineLut;
            phase9 = 0;
            do {
              i++;
              v7 = lut9[(g_nPauseMenuTransitionFrames * 8 + phase9) & 0xff];
              phase9 += 0xc;
              rec[0x46] = v7 * 3 >> 9;
              rec += 0x58;
            } while (i < n5);
          }
        }
      } else {
        styC[0] = 0x10;
        styC[2] = 0x1400;
        pos[0] = 0xc7;
        styC[1] = 1;
        pos[1] = 0x6e;
        pos[2] = 0x1100;
        BuildTextSprites(g_szContinue, pos, styC, 0x12, 0xb);
        if (g_nPauseMenuCursor == 0) {
          rec = (unsigned char *)g_pSpriteRecordWriteCursor;
          nGlyphs = 8;
        }
        pos[0] = 0xcf;
        pos[1] = 0x80;
        pos[2] = 0x1100;
        BuildTextSprites(g_szUiOptions, pos, styC, 0x12, 0xb);
        if (g_nPauseMenuCursor == 1) {
          rec = (unsigned char *)g_pSpriteRecordWriteCursor;
          nGlyphs = 7;
        }
        pos[0] = 0xbf;
        pos[1] = 0x92;
        pos[2] = 0x1100;
        BuildTextSprites(g_szInventory, pos, styC, 0x12, 0xb);
        if (g_nPauseMenuCursor == 2) {
          rec = (unsigned char *)g_pSpriteRecordWriteCursor;
          nGlyphs = 9;
        }
        if (g_nFlightLevelActive != 0) {
          pos[0] = 0xe7;
          pos[1] = 0xa4;
          pos[2] = 0x1100;
          BuildTextSprites(g_szUiQuit, pos, styC, 0x12, 0xb);
          if (g_nPauseMenuCursor == 3) {
            rec = (unsigned char *)g_pSpriteRecordWriteCursor;
            nGlyphs = 4;
          }
          n6 = nGlyphs;
        } else if (g_nActiveLevelId != g_nActiveLevelId / 10 * 10) {
          pos[0] = 0xb7;
          pos[1] = 0xa4;
          pos[2] = 0x1100;
          BuildTextSprites(g_szExitLevel, pos, styC, 0x12, 0xb);
          if (g_nPauseMenuCursor == 3) {
            rec = (unsigned char *)g_pSpriteRecordWriteCursor;
            nGlyphs = 9;
          }
          n6 = nGlyphs;
        } else {
          pos[0] = 0xbf;
          pos[1] = 0xa4;
          pos[2] = 0x1100;
          BuildTextSprites(g_szQuitGame, pos, styC, 0x12, 0xb);
          if (g_nPauseMenuCursor == 3) {
            rec = (unsigned char *)g_pSpriteRecordWriteCursor;
            nGlyphs = 8;
          }
          n6 = nGlyphs;
        }
        i = 0;
        if (n6 > 0) {
          lut3 = g_anCosineLut;
          phase10 = 0;
          do {
            i++;
            v8 = lut3[(g_nPauseMenuTransitionFrames * 8 + phase10) & 0xff];
            phase10 += 0xc;
            rec[0x46] = v8 * 3 >> 9;
            rec += 0x58;
          } while (i < n6);
        }
      }
    } else if (g_nGamestate == 3) {
      void *prim3;
      unsigned char *rec;
      int n;
      int phase;
      int v;
      int y;
      int bx;
      int bx2;
      int pct = ComputeInventoryProgressPercent();
      int bias;
      char *fmt;
      register int *pp asm("$20");
      register char *bp asm("$22");
      pos[0] = 0x1b8;
      pos[1] = 0x2c;
      pos[2] = 0x900;
      if (pct >= 0xa) {
        pos[0] = 0x196;
      }
      if (pct >= 0x64) {
        pos[0] -= 0x22;
      }
      fmt = g_szFmtPercentD;
      sprintf(buf, fmt, pct);
      BuildTextSpriteChain(buf, pos, 0x22, 2);
      pos[0] = 0x1de;
      buf[0] = '%';
      buf[1] = 0;
      BuildTextSpriteChain(buf, pos, 0x22, 2);
      pos[0] = 0x5a;
      pos[1] = 0x24;
      pos[2] = 0xb40;
      sprintf(buf, fmt, g_nWorldTreasureCollected);
      BuildTextSpriteChain(buf, pos, 0x1b, 2);
      pos[0] = 0x14d;
      pos[1] = 0x24;
      pos[2] = 0xb40;
      if (g_nWorldDragonsRescued >= 0xa) {
        pos[0] = 0x132;
      }
      sprintf(buf, fmt, g_nWorldDragonsRescued);
      BuildTextSpriteChain(buf, pos, 0x1b, 2);
      g_pSpriteRecordWriteCursor =
          (unsigned char *)g_pSpriteRecordWriteCursor - 0x58;
      FillWord(g_pSpriteRecordWriteCursor, 0, 0x58);
      SREC->type = 0x1d7;
      *((int *)((int)g_pSpriteRecordWriteCursor + 0xc)) = 0x2e;
      *((int *)((int)g_pSpriteRecordWriteCursor + 0x10)) = 0x2e;
      *((int *)((int)g_pSpriteRecordWriteCursor + 0x14)) = 0xc00;
      SREC->rotY =
          ((unsigned short *)g_anCosineLut)[g_adwHudBobSinePhaseBlock[0]]
              << 0x10 >>
          0x19;
      SREC->rotZ = g_adwHudBobSinePhaseBlock[0];
      SREC->otBias = 0x7f;
      SREC->tintMode = 0xb;
      SREC->cullRadius = 0xff;
      g_pSpriteRecordWriteCursor =
          (unsigned char *)g_pSpriteRecordWriteCursor - 0x58;
      FillWord(g_pSpriteRecordWriteCursor, 0, 0x58);
      SREC->type = 0x1fa;
      *((int *)((int)g_pSpriteRecordWriteCursor + 0xc)) = 0x12c;
      if (g_nWorldDragonsRescued >= 0xa) {
        *((int *)((int)g_pSpriteRecordWriteCursor + 0xc)) = 0x111;
      }
      {
        int bob;
        bob = (g_nPauseMenuTransitionFrames * 0x10 & 0x1f0) >> 1;
        *((int *)((int)g_pSpriteRecordWriteCursor + 0x10)) = 0x2e;
        *((int *)((int)g_pSpriteRecordWriteCursor + 0x14)) = 0x780;
        SREC->rotZ = ((unsigned short *)g_anCosineLut)[bob] >> 8;
      }
      SREC->otBias = 0x7f;
      SREC->tintMode = 0xb;
      SREC->cullRadius = 0xff;
      {
        int x3;
        x3 = 0x24;
        if (g_nWorldEggsRecovered > 0) {
          i = 0;
          do {
            quad[1] = 0x32;
            quad[2] = 0x18;
            quad[0] = i * 0x1b + 0x18;
            quad[3] = 0x10;
            if (i >= g_nWorldEggsRecovered) {
              pos[1] = 0x3a;
              pos[0] = x3;
              pos[2] = 0x1800;
              BuildTextSpriteChain(g_szUiGlyphX, pos, 0, 2);
              pos[0] = 0x20;
              pos[1] = 0x20;
              pos[2] = 0x20;
              EmitTexturedQuad(quad, &g_anLevelSpriteTexWordPairs[2], pos);
            } else {
              EmitTexturedQuad(quad,
                               &g_adwHudBobSinePhaseBlock[0x14c] +
                                   (g_adwHudBobSinePhaseBlock[1] + i) % 9 * 2,
                               (int *)0);
            }
            x3 += 0x1b;
            i++;
          } while (i < 0xc);
        }
      }
      EmitRadialShadedLine(0x40, 0x45, 0x1c0, 0x45);
      prim3 = g_pPrimBufferWriteCursor;
      SetDrawMode((int)prim3, 1, 0, 0x40, (RECT *)0);
      AddPrimToOT((int)prim3);
      *((int *)((int)prim3 + 0xc)) = 0x5000000;
      *((unsigned char *)((int)prim3 + 0x13)) = 0x2a;
      *((unsigned char *)((int)prim3 + 0x10)) = 0x60;
      *((unsigned char *)((int)prim3 + 0x11)) = 0x60;
      *((unsigned char *)((int)prim3 + 0x12)) = 0x60;
      *((short *)((int)prim3 + 0x14)) = 0x40;
      *((short *)((int)prim3 + 0x16)) = 0x46;
      *((short *)((int)prim3 + 0x18)) = 0x1c0;
      *((short *)((int)prim3 + 0x1a)) = 0x46;
      *((short *)((int)prim3 + 0x1c)) = 0x40;
      *((short *)((int)prim3 + 0x1e)) = 0x47;
      *((short *)((int)prim3 + 0x20)) = 0x1c0;
      *((short *)((int)prim3 + 0x22)) = 0x47;
      AddPrimToOT((int)prim3 + 0xc);
      g_pPrimBufferWriteCursor = (void *)((int)prim3 + 0x24);
      EmitRadialShadedLine(0x40, 0x47, 0x1c0, 0x47);
      if (g_nSubMenuCursor > 0 &&
          g_abLevelVisitedFlagVirtualBase[g_nSubMenuCursor * 6] != 0) {
        pos[0] = 0x28;
        pos[1] = 0x48;
        pos[2] = 0x1100;
        EnqueueBlinkingMarkerSprite(pos, g_nPauseMenuTransitionFrames * 2, 1);
      }
      if (g_nSubMenuCursor < 5) {
        int *pa = pos;
        if ((&g_abLevelVisitedFlag[6])[g_nSubMenuCursor * 6] != 0) {
          pos[0] = 0x1d7;
          pos[1] = 0x48;
          pos[2] = 0x1100;
          EnqueueBlinkingMarkerSprite(pa, g_nPauseMenuTransitionFrames * 2, 0);
        }
      }
      i = 0;
      bias = (g_nSubMenuCursor < 3) * 8;
      v = strlen((&g_apTitleCardNames[1])[g_nSubMenuCursor]);
      pos[1] = 0x5c;
      pos[2] = 0xe00;
      styD[0] = 0x14;
      styD[1] = 2;
      styD[2] = 0x1000;
      v = (v - 1) * 0xa - 0x9e;
      pos[0] = g_nSubMenuScrollOffset - v;
      BuildTextSprites((&g_apTitleCardNames[1])[g_nSubMenuCursor], pos, styD,
                       0x17, 2);
      rec = (unsigned char *)g_pSpriteRecordWriteCursor;
      phase = 0;
      while (i < (int)g_abWorldTitleGlyphBlock[g_nSubMenuCursor]) {
        i++;
        rec[0x46] =
            g_anCosineLut[(g_nPauseMenuTransitionFrames * 8 + phase) & 0xff] *
                3 >>
            9;
        phase += 0xc;
        rec += 0x58;
      }
      g_pSpriteRecordWriteCursor =
          (unsigned char *)g_pSpriteRecordWriteCursor - 0x58;
      FillWord(g_pSpriteRecordWriteCursor, 0, 0x58);
      SREC->type = 0x1d7;
      {
        int bF = bias + 0x15a;
        *((int *)((int)g_pSpriteRecordWriteCursor + 0xc)) =
            g_nSubMenuScrollOffset + bF;
      }
      __asm__ volatile("");
      *((int *)((int)g_pSpriteRecordWriteCursor + 0x10)) = 0x6c;
      pp = pos;
      bp = buf;
      *((int *)((int)g_pSpriteRecordWriteCursor + 0x14)) = 0xc00;
      SREC->rotY = ((unsigned short *)g_anCosineLut)[g_dwHudBobSinePhase]
                       << 0x10 >>
                   0x19;
      __asm__ volatile("");
      SREC->rotZ = g_dwHudBobSinePhase;
      SREC->otBias = 0x7f;
      SREC->tintMode = 0xb;
      SREC->cullRadius = 0xff;
      g_pSpriteRecordWriteCursor =
          (unsigned char *)g_pSpriteRecordWriteCursor - 0x58;
      FillWord(g_pSpriteRecordWriteCursor, 0, 0x58);
      {
        int bob;
        *((int *)((int)g_pSpriteRecordWriteCursor + 0xc)) =
            g_nSubMenuScrollOffset + 0x1d1;
        *((int *)((int)g_pSpriteRecordWriteCursor + 0x10)) = 0x6e;
        {
          register int fr asm("$2");
          fr = g_nPauseMenuTransitionFrames;
          SREC->type = 0x1fa;
          bob = (fr * 0x10 & 0x1f0) >> 1;
        }
        *((int *)((int)g_pSpriteRecordWriteCursor + 0x14)) = 0x780;
        SREC->rotZ = ((unsigned short *)g_anCosineLut)[bob] >> 8;
      }
      SREC->otBias = 0x7f;
      i = 0;
      SREC->tintMode = 0xb;
      y = 0x7b;
      SREC->cullRadius = 0xff;
      do {
        if (g_abLevelVisitedFlag[g_nSubMenuCursor * 6 + i] != 0) {
          int bA, bB, bC, bD, bE;
          int *gems;
          int gx;
          int pf;
          int sc;
          int row;
          prim3 = g_pPrimBufferWriteCursor;
          SetDrawMode((int)prim3, 1, 0, 0x40, (RECT *)0);
          AddPrimToOT((int)prim3);
          pf = (int)prim3 + 0xc;
          *((int *)((int)prim3 + 0xc)) = 0x5000000;
          *((unsigned char *)((int)prim3 + 0x13)) = 0x2a;
          *((unsigned char *)((int)prim3 + 0x10)) = 0x28;
          *((unsigned char *)((int)prim3 + 0x11)) = 0x28;
          *((unsigned char *)((int)prim3 + 0x12)) = 0x28;
          sc = g_nSubMenuScrollOffset;
          *((short *)((int)prim3 + 0x16)) = y - 0xa;
          *((short *)((int)prim3 + 0x1a)) = y - 0xa;
          *((short *)((int)prim3 + 0x1e)) = y + 9;
          *((short *)((int)prim3 + 0x22)) = y + 9;
          *((short *)((int)prim3 + 0x14)) = sc;
          *((short *)((int)prim3 + 0x18)) = sc + 0x1ff;
          *((short *)((int)prim3 + 0x1c)) = sc;
          *((short *)((int)prim3 + 0x20)) = sc + 0x1ff;
          AddPrimToOT(pf);
          g_pPrimBufferWriteCursor = (void *)((int)prim3 + 0x24);
          EmitRadialShadedLine(g_nSubMenuScrollOffset + 0x70, y - 0xb,
                               g_nSubMenuScrollOffset + 0xd0, y - 0xb);
          bA = bias + 0x13a;
          bB = bias + 0x17a;
          EmitRadialShadedLine(g_nSubMenuScrollOffset + bA, y - 0xb,
                               g_nSubMenuScrollOffset + bB, y - 0xb);
          EmitRadialShadedLine(g_nSubMenuScrollOffset + 0x1ae, y - 0xb,
                               g_nSubMenuScrollOffset + 0x1ee, y - 0xb);
          pos[0] = g_nSubMenuScrollOffset + 0x24;
          pos[1] = y;
          pos[2] = 0x1100;
          styD[0] = 0x10;
          styD[1] = 1;
          styD[2] = 0x1400;
          rec = (unsigned char *)g_pSpriteRecordWriteCursor;
          BuildTextSprites(g_apLevelNameStrings[g_nSubMenuCursor * 6 + i], pos,
                           styD, 0x12, 2);
          bC = bias + 0x113;
          gx = g_nSubMenuScrollOffset + bC;
          {
            int idx = g_nSubMenuCursor * 6 + i;
            int *gb = g_anLevelGemsBlock;
            __asm__ volatile("");
            gems = gb + idx;
          }
          pos[0] = gx;
          if (*gems < 0x3e8) {
            pos[0] = gx + 0x12;
          }
          if (*gems < 0x64) {
            pos[0] += 0x12;
          }
          if (*gems < 0xa) {
            pos[0] += 0x12;
          }
          sprintf(buf, g_szFmtPercentD, *gems);
          BuildTextSpriteChain(buf, pos, 0x12, 2);
          pos[2] = 0x1800;
          bD = bias + 0x15a;
          pos[0] = g_nSubMenuScrollOffset + bD;
          BuildTextSpriteChain(g_szUiSlash, pos, 0x12, 2);
          pos[2] = 0x1100;
          bE = bias + 0x16b;
          pos[0] = g_nSubMenuScrollOffset + bE;
          sprintf(buf, g_szFmtPercentD,
                  g_anLevelTotalGemsTable[g_nSubMenuCursor * 6 + i]);
          BuildTextSpriteChain(buf, pos, 0x12, 2);
          if (g_abLevelTotalDragonsBlock[g_nSubMenuCursor * 6 + i] != 0) {
            pos[0] = g_nSubMenuScrollOffset + 0x1bd;
            sprintf(buf, g_szFmtPercentD,
                    g_anLevelDragonsRescued[g_nSubMenuCursor * 6 + i]);
            BuildTextSpriteChain(buf, pos, 0x12, 2);
            pos[2] = 0x1800;
            pos[0] = g_nSubMenuScrollOffset + 0x1ce;
            BuildTextSpriteChain(g_szUiSlash, pos, 0x12, 2);
            pos[2] = 0x1100;
            pos[0] = g_nSubMenuScrollOffset + 0x1df;
            sprintf(buf, g_szFmtPercentD,
                    g_abLevelTotalDragonsBlock[g_nSubMenuCursor * 6 + i]);
            BuildTextSpriteChain(buf, pos, 0x12, 2);
          } else {
            pos[2] = 0x1800;
            pos[0] = g_nSubMenuScrollOffset + 0x1ce;
            BuildTextSpriteChain(g_szUiSlash, pos, 0x12, 2);
            SREC->rotX = 0x29;
          }
          if (g_nSubMenuCursor * 6 + i == g_nLevelIntroIndex) {
            rec -= 0x58;
            if ((int)rec >= (int)g_pSpriteRecordWriteCursor) {
              phase = 0;
              do {
                rec[0x46] = g_anCosineLut[(g_nPauseMenuTransitionFrames * 8 +
                                           phase * 0xc) &
                                          0xff] *
                                3 >>
                            9;
                rec -= 0x58;
                phase++;
              } while ((int)rec >= (int)g_pSpriteRecordWriteCursor);
            }
          }
          if (g_nWorldEggsRecovered > 0 && g_nSubMenuCursor < 3) {
            int e = 0;
            while (e < (int)g_abLevelTotalEggsTable[g_nSubMenuCursor * 6 + i]) {
              int gx2;
              int base = g_nSubMenuScrollOffset + 0xe8;
              int gx = base + e * 0x18;
              if (g_abLevelTotalEggsTable[g_nSubMenuCursor * 6 + i] != 1) {
                gx2 = gx;
              } else {
                gx2 = gx + 0x18;
              }
              quad2[1] = y - 8;
              quad2[2] = 0x18;
              quad2[0] = gx2;
              quad2[3] = 0x10;
              if (e >= g_anLevelEggsRecovered[g_nSubMenuCursor * 6 + i]) {
                pos[0] = quad2[0] + 0xc;
                pos[1] = y;
                pos[2] = 0x1800;
                BuildTextSpriteChain(g_szUiGlyphX, pos, 0, 2);
                pos[0] = 0x20;
                pos[1] = 0x20;
                pos[2] = 0x20;
                EmitTexturedQuad(quad2, &g_anLevelSpriteTexWordPairs[2], pos);
              } else {
                EmitTexturedQuad(quad2,
                                 &g_anHudFrameCycleBlock[0x14b] +
                                     (g_anHudFrameCycleBlock[0] + i) % 9 * 2,
                                 (int *)0);
              }
              e++;
            }
          }
          y += 0x13;
        }
        i++;
      } while (i < 6);
      EmitRadialShadedLine(g_nSubMenuScrollOffset + 0x70, y - 0xb,
                           g_nSubMenuScrollOffset + 0xd0, y - 0xb);
      EmitRadialShadedLine(g_nSubMenuScrollOffset + 0x140, y - 0xb,
                           g_nSubMenuScrollOffset + 0x180, y - 0xb);
      EmitRadialShadedLine(g_nSubMenuScrollOffset + 0x1ae, y - 0xb,
                           g_nSubMenuScrollOffset + 0x1ee, y - 0xb);
    }
    g_apSpritePrimQueue[0] = (void *)0;
    if (g_nGamestate == 2 && g_nFlightLevelActive == 0) {
      EnqueueLoadingScreenSprites();
    }
    EnqueuePendingSpritePrims();
    FillWord(&g_pOtDepthBinHead0, 0, 0x900);
    RasterizeSpritePrimQueue();
    DrawOTag(LinkOTPrimitives(0));
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
  }
  __asm__ volatile(""
                   :
                   : "g"(h0), "g"(h1), "g"(h2), "g"(h3), "g"(h4), "g"(h5),
                     "g"(h6), "g"(h7));
  g_nPauseMenuTransitionFrames++;
}
