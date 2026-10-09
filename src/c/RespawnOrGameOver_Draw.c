#include "globals.h"

extern void BuildRenderEntityLists(void);
extern void ComposeFrameScene(void);
extern void RasterizeEmitList(void);
extern void DrawActors(void);
extern void SetupFrameOT(void);
extern void DrawSync(int mode);
extern int VSync(int mode);
extern DISPENV *PutDispEnv(DISPENV *env);
extern void PutDrawEnv(void *env);
extern void *LinkOTPrimitives(int depth_max);
extern void DrawOTag(void *ot);
extern void DrawFullscreenTint(int slot, int r, int g, int b);
extern void MoveImage(short *rect, unsigned int x, int y);
extern void ClearImage(short *rect, unsigned int r, unsigned int g,
                       unsigned int b);
extern void FillWord(void *dst, unsigned int value, int byte_count);
extern void EnqueuePendingSpritePrims(void);
extern void RasterizeSpritePrimQueue(void);
extern void RasterizePairedActor(void);
extern void BuildTextSprites(unsigned char *str, int *pos, int *size, int flags,
                             int slot);
extern void BuildCameraViewMatrix(void);
extern void EmitStaticActorMeshList(int lod, void *viewMtx, void *worldMtx);

/* Held-base alias for &g_nCameraEulerPitch (0x80076e1e): pitch stores plus
   the view/world matrix call args at -0x3A/-0x4E. */
extern short g_anCameraEulerPitchBlock[];
/* Game-over camera euler snapshot, read unsigned (lhu). */
extern unsigned short g_anGameOverCameraEulerU[];
extern unsigned char g_abFrameDrawEnvIsbgBlock[];
extern unsigned char D_8006F304[]; /* GAMEOVER letter glyph indices */
extern unsigned char D_80010B9C[]; /* "PRESS X..." prompt string */

/* Draw for GS_GAME_OVER (5): three sub-modes driven by g_nGameplayDrawMode.
   Mode 0 = death fade-out: frame 0 renders one full gameplay frame then
   snapshots half the framebuffer (MoveImage) as a frozen backdrop; frames
   1..15 darken it with fullscreen tints, frame 15 switches both DRAWENVs to
   black-clear and wipes VRAM. Mode 1 = GAME OVER screen: letter sprites
   revealed on a per-letter delay ramp from frame 0x3C, prompt text + sine
   wave from frame 0x1A5, over a scripted-camera world render. Mode >= 2
   mirrors mode 0 without the initial render (0x8001ca38, 1444 bytes). */

/* Returns its argument through an empty asm so cse/reload cannot fold the
   frame-15 DRAWENV block base back into absolute addresses. Inline, so both
   identical frame-15 tails still cross-jump into one. */
static inline unsigned char *Opaque(unsigned char *p) {
  unsigned char *q;

  __asm__("" : "=r"(q) : "0"(p));
  return q;
}

void RespawnOrGameOver_Draw(void) {
  short rect[4];
  int pos[3];
  int size[3];
  int i;
  int k;
  int t;
  int d;
  short *ramp;
  char *rec;
  int bright;
  int f;
  void **q;
  int sk;
  int st;
  char *sc;
  short *sbase;

  if (g_nGameplayDrawMode == 0) {
    if (g_nGameplayDrawFrame == 0) {
      BuildRenderEntityLists();
      ComposeFrameScene();
      RasterizeEmitList();
      DrawActors();
      SetupFrameOT();
      DrawSync(0);
      VSync(0);
      PutDispEnv((DISPENV *)((char *)g_pActiveFrameDrawEnv + 0x5c));
      PutDrawEnv(g_pActiveFrameDrawEnv);
      DrawOTag(LinkOTPrimitives(0x800));
      DrawSync(0);
      VSync(0);
      bright = 8;
      rect[0] = 0;
      if (g_pActiveFrameDrawEnv != &g_abFrameDrawEnv0) {
        bright = 0xF8;
      }
      rect[2] = 0x200;
      rect[1] = bright;
      rect[3] = 0xE0;
      MoveImage(rect, 0, 0x100 - bright);
      DrawSync(0);
      g_abFrameDrawEnv0.isbg = 0;
      g_abFrameDrawEnv1.isbg = 0;
    } else if (g_nGameplayDrawFrame < 0x10) {
      if (g_nGameplayDrawFrame == 1) {
        DrawFullscreenTint(2, 0x10, 0x10, 0x10);
      } else {
        DrawFullscreenTint(2, 0x20, 0x20, 0x20);
      }
      DrawSync(0);
      VSync(0);
      PutDispEnv((DISPENV *)((char *)g_pActiveFrameDrawEnv + 0x5c));
      PutDrawEnv(g_pActiveFrameDrawEnv);
      DrawOTag(LinkOTPrimitives(0x800));
      if (g_nGameplayDrawFrame == 0xF) {
        {
          unsigned char *e = Opaque(g_abFrameDrawEnvIsbgBlock);

          e[0] = 1;
          g_abFrameDrawEnv1.isbg = 1;
          g_abFrameDrawEnv0.r0 = 0;
          g_abFrameDrawEnv0.g0 = 0;
          g_abFrameDrawEnv0.b0 = 0;
          e[0x85] = 0;
          e[0x86] = 0;
          e[0x87] = 0;
        }
        DrawSync(0);
        VSync(0);
        rect[2] = 0x200;
        rect[0] = 0;
        rect[1] = 0;
        rect[3] = 0x1E0;
        ClearImage(rect, 0, 0, 0);
      }
    }
  } else if (g_nGameplayDrawMode == 1) {
    if (g_nGameplayDrawFrame >= 0x3C) {
      i = 0;
      ramp = g_anGameOverRevealScaleRamp;
      k = 0;
      do {
        d = g_abGameOverLetterDelay[i] + 0x3C;
        t = g_nGameplayDrawFrame - d;
        if (t >= 0) {
          rec = (char *)g_pSpriteRecordWriteCursor - 0x58;
          g_pSpriteRecordWriteCursor = rec;
          FillWord(rec, 0, 0x58);
          if (t < 0x18) {
            *(int *)((char *)g_pSpriteRecordWriteCursor + 0x14) =
                *(short *)(t * 2 + (int)ramp);
          } else {
            *(int *)((char *)g_pSpriteRecordWriteCursor + 0x14) = 0x400;
          }
          *(short *)((char *)g_pSpriteRecordWriteCursor + 0x36) =
              D_8006F304[i] + 0x169;
          *(int *)((char *)g_pSpriteRecordWriteCursor + 0xC) =
              *(short *)((char *)g_anGameOverLetterPosXY + k);
          *(int *)((char *)g_pSpriteRecordWriteCursor + 0x10) =
              *(short *)((char *)g_anGameOverLetterPosXY + k + 2);
          *(unsigned char *)((char *)g_pSpriteRecordWriteCursor + 0x46) =
              g_abGameOverLetterAngle[i];
          *(unsigned char *)((char *)g_pSpriteRecordWriteCursor + 0x47) = 4;
          *(unsigned char *)((char *)g_pSpriteRecordWriteCursor + 0x4F) = 0xB;
          *(unsigned char *)((char *)g_pSpriteRecordWriteCursor + 0x50) = 0x20;
        }
        i += 1;
        k += 4;
      } while (i < 8);
      if (g_nCdStreamState >= 0xB && g_nGameplayDrawFrame >= 0x1A5) {
        size[0] = 0x10;
        size[1] = 1;
        size[2] = 0x1400;
        pos[0] = 0xAE;
        pos[1] = 0xD0;
        pos[2] = 0x1100;
        BuildTextSprites(D_80010B9C, pos, size, 0x12, 0xB);
        i = 0;
        sbase = &g_anSineLut[0x40];
        sk = 0;
        sc = (char *)g_pSpriteRecordWriteCursor;
        do {
          i += 1;
          st = sbase[((g_nGameplayDrawFrame * 4) + sk) & 0xFF];
          sk += 0xC;
          sc[0x46] = (st * 3) >> 9;
          sc += 0x58;
        } while (i < 0xA);
      }
      q = g_apSpritePrimQueue;
      q[0] = 0;
      EnqueuePendingSpritePrims();
      FillWord((char *)q - 0x2400, 0, 0x900);
      RasterizeSpritePrimQueue();
      if (g_nGameplayDrawFrame >= 0xB5) {
        RasterizePairedActor();
      }
    }
    g_anCameraEulerPitchBlock[0] = g_anGameOverCameraEulerU[2];
    g_nCameraEulerYaw = g_anGameOverCameraEulerU[3];
    BuildCameraViewMatrix();
    EmitStaticActorMeshList(-1, (char *)g_anCameraEulerPitchBlock - 0x3A,
                            (char *)g_anCameraEulerPitchBlock - 0x4E);
    g_anCameraEulerPitchBlock[0] = g_anGameOverCameraEulerU[0];
    g_nCameraEulerYaw = g_anGameOverCameraEulerU[1];
    if (g_nGameplayDrawFrame < 0x20) {
      f = (0x1F - g_nGameplayDrawFrame) * 8;
      DrawFullscreenTint(2, f, f, f);
    }
    DrawSync(0);
    VSync(0);
    PutDispEnv((DISPENV *)((char *)g_pActiveFrameDrawEnv + 0x5c));
    PutDrawEnv(g_pActiveFrameDrawEnv);
    DrawOTag(LinkOTPrimitives(0x800));
  } else {
    if (g_nGameplayDrawFrame == 0) {
      bright = 8;
      rect[0] = 0;
      if (g_pActiveFrameDrawEnv != &g_abFrameDrawEnv0) {
        bright = 0xF8;
      }
      rect[2] = 0x200;
      rect[1] = bright;
      rect[3] = 0xE0;
      MoveImage(rect, 0, 0x100 - bright);
      DrawSync(0);
      g_abFrameDrawEnv0.isbg = 0;
      g_abFrameDrawEnv1.isbg = 0;
    } else if (g_nGameplayDrawFrame < 0x10) {
      if (g_nGameplayDrawFrame == 1) {
        DrawFullscreenTint(2, 0x10, 0x10, 0x10);
      } else {
        DrawFullscreenTint(2, 0x20, 0x20, 0x20);
      }
      DrawSync(0);
      VSync(0);
      PutDispEnv((DISPENV *)((char *)g_pActiveFrameDrawEnv + 0x5c));
      PutDrawEnv(g_pActiveFrameDrawEnv);
      DrawOTag(LinkOTPrimitives(0x800));
      if (g_nGameplayDrawFrame == 0xF) {
        {
          unsigned char *e = Opaque(g_abFrameDrawEnvIsbgBlock);

          e[0] = 1;
          g_abFrameDrawEnv1.isbg = 1;
          g_abFrameDrawEnv0.r0 = 0;
          g_abFrameDrawEnv0.g0 = 0;
          g_abFrameDrawEnv0.b0 = 0;
          e[0x85] = 0;
          e[0x86] = 0;
          e[0x87] = 0;
        }
        DrawSync(0);
        VSync(0);
        rect[2] = 0x200;
        rect[0] = 0;
        rect[1] = 0;
        rect[3] = 0x1E0;
        ClearImage(rect, 0, 0, 0);
      }
    }
  }
}
