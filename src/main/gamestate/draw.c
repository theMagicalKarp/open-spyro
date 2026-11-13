#include "actor.h"
#include "actor_cutscene.h"
#include "actor_type.h"
#include "angle.h"
#include "camera.h"
#include "cd_stream.h"
#include "credits_mode.h"
#include "dragon_rescue.h"
#include "frame_db.h"
#include "funcs.h"
#include "gamestate.h"
#include "gem_tally.h"
#include "globals.h"
#include "gpu_prim.h"
#include "hud.h"
#include "level_id.h"
#include "main_shared.h"
#include "match_asm.h"
#include "pause_menu.h"
#include "save_menu.h"
#include "title_state.h"

/* The sprite record being written (Actor layout). */

/* 0x8001973c — the level-entry/exit banner and post-level treasure tally
   (2324 b), drawn by LevelTransition_Draw.

   The banner line is built from g_nNextLevelId: level 0 of a world is
   "RETURNING HOME...", the boss level (4) of a world and the Gnasty fight
   (0x3f) get the "CONFRONTING %s..." wording, everything else "ENTERING
   %s...", with the name taken from g_apLevelNameStrings at the flattened
   (world-1)*6 + level index. It is centred by string length and slid in and
   out of frame off the sine LUT at the two ends of g_nLevelTransitionPhase.

   From phase 0x40 the results panel joins in: "TREASURE FOUND" until phase
   0xe0, then "TOTAL TREASURE", each sliding through the same 0x110-y arc with
   a hold at 0xd0 between g_nGemTallyHoldEndPhase and +0x20. The number under
   it counts up over the gem fly-in (2 frames per icon, capped at 0x40) and
   then counts on from the game-wide total, and one gem sprite is emitted per
   live g_pGemTallyParticleRecords record (U from g_abGemTallyIconUvTable) plus
   the treasure icon itself. Every glyph rocks on the cosine LUT (its rotZ)
   stepped by the phase.

   The queue is flushed here rather than by the caller: clear the head slot,
   EnqueuePendingSpritePrims, then rasterize. */

typedef struct {
  int pad00[3];    /* 0x00 */
  int x;           /* 0x0c */
  int y;           /* 0x10 */
  int scale;       /* 0x14 */
  int pad18[7];    /* 0x18 */
  short pad34;     /* 0x34 */
  short uv;        /* 0x36 */
  int pad38[3];    /* 0x38 */
  unsigned char r; /* 0x44 */
} TallySprite;

#define SPR ((TallySprite *)g_pSpriteRecordWriteCursor)

void DrawLevelTransitionOverlay(void) {
  int pos[3];
  int step[3];
  char buf[32];
  char *msg;
  TallySprite *spr;
  int idx;
  int sub;
  int slide;
  int i; /* doubles as the treasure digit-count scratch and the icon index */
  int x0;
  int span;
  int shown;
  int value;
  int y;
  int gx;

  idx = LEVEL_SLOT(g_nNextLevelId);
  sub = LEVEL_IN_WORLD(g_nNextLevelId);
  if (sub == LEVEL_HOME_SLOT) {
    sprintf(buf, g_szReturningHome);
  } else if ((g_nNextLevelId < LEVEL_GNORC_GNEXUS && sub == LEVEL_BOSS_SLOT) ||
             g_nNextLevelId == LEVEL_GNASTY_GNORC) {
    sprintf(buf, g_szConfrontingFmt, g_apLevelNameStrings[idx]);
  } else {
    sprintf(buf, g_szEnteringDotsFmt, g_apLevelNameStrings[idx]);
  }

  pos[0] = 0x100 - (strlen(buf) - 1) * 8;
  if (g_nLevelTransitionPhase < TALLY_SLIDE) {
    pos[1] = g_anSineLut[g_nLevelTransitionPhase * 2] >> 7;
  } else if (g_nLevelTransitionPhase > TALLY_OUT) {
    pos[1] = g_anSineLut[(TALLY_END - g_nLevelTransitionPhase) * 2] >> 7;
  } else {
    pos[1] = 0x20;
  }

  {
    Actor *glyph = g_pSpriteRecordWriteCursor;
    pos[2] = TEXT_DEPTH;
    step[0] = 0x10;
    step[1] = 1;
    step[2] = 0x1400;
    BuildTextSprites((unsigned char *)buf, pos, step, 0x12, TINT_GOLD);
    glyph--;
    if ((int)g_pSpriteRecordWriteCursor <= (int)glyph) {
      unsigned short *lut = (unsigned short *)g_anCosineLut;
      int wave = 0;
      do {
        glyph->rotZ = lut[(g_nLevelTransitionPhase * 2 + wave) & 0xff] >> 7;
        glyph--;
        wave += 0xc;
      } while ((int)g_pSpriteRecordWriteCursor <= (int)glyph);
    }
  }

  if (g_nLevelTransitionPhase >= TALLY_PANEL_IN) {
    i = g_nTotalTreasureCollected;
    x0 = 0x28;
    while (i >= 10) {
      i /= 10;
      x0 -= 10;
    }

    pos[0] = x0 + 0x28;
    pos[2] = TEXT_DEPTH;
    if (g_nLevelTransitionPhase < TALLY_PANEL_IN + TALLY_SLIDE) {
      slide = g_nLevelTransitionPhase - TALLY_PANEL_IN;
      pos[1] = TALLY_PANEL_Y_DOWN - (g_anSineLut[slide * 2] >> 6);
    } else if (g_nLevelTransitionPhase < g_nGemTallyHoldEndPhase) {
      pos[1] = TALLY_PANEL_Y_UP;
    } else if (g_nLevelTransitionPhase <
               g_nGemTallyHoldEndPhase + TALLY_SLIDE) {
      slide = g_nLevelTransitionPhase - TALLY_SLIDE;
      pos[1] = TALLY_PANEL_Y_DOWN -
               (g_anSineLut[(g_nGemTallyHoldEndPhase - slide) * 2] >> 6);
    } else if (g_nLevelTransitionPhase < TALLY_TOTAL_IN) {
      pos[1] = TALLY_PANEL_Y_DOWN;
    } else if (g_nLevelTransitionPhase < TALLY_TOTAL_IN + TALLY_SLIDE) {
      slide = g_nLevelTransitionPhase - TALLY_TOTAL_IN;
      pos[1] = TALLY_PANEL_Y_DOWN - (g_anSineLut[slide * 2] >> 6);
    } else if (g_nLevelTransitionPhase < TALLY_OUT) {
      pos[1] = TALLY_PANEL_Y_UP;
    } else if (g_nLevelTransitionPhase < TALLY_END) {
      pos[1] = TALLY_PANEL_Y_DOWN -
               (g_anSineLut[(TALLY_END - g_nLevelTransitionPhase) * 2] >> 6);
    } else {
      pos[1] = TALLY_PANEL_Y_DOWN;
    }

    if (g_nLevelTransitionPhase < TALLY_TOTAL_IN) {
      sprintf(buf, g_szTreasureFound);
    } else {
      sprintf(buf, g_szTotalTreasure);
      pos[0] -= 0x10;
    }
    {
      Actor *glyph = g_pSpriteRecordWriteCursor;
      int wave;
      BuildTextSprites((unsigned char *)buf, pos, step, 0x12, TINT_GOLD);
      glyph--;
      wave = 0;
      if ((int)g_pSpriteRecordWriteCursor <= (int)glyph) {
        do {
          glyph->rotZ =
              ((unsigned short *)
                   g_anCosineLut)[(g_nLevelTransitionPhase * 2 + wave) &
                                  0xff] >>
              7;
          glyph--;
          wave += 0xc;
        } while ((int)g_pSpriteRecordWriteCursor <= (int)glyph);
      }
    }

    if (g_nLevelTransitionPhase < TALLY_GEMS_IN) {
      value = g_anLevelGemsCollected[g_nPrevLevelIndex] - g_nLevelGemsAtEntry;
    } else if (g_nLevelTransitionPhase < TALLY_TOTAL_IN) {
      span = (g_nGemTallyIconCount + 1) * TALLY_ICON_FRAMES;
      if (span > TALLY_COUNT_MAX) {
        span = TALLY_COUNT_MAX;
      }
      slide = g_nLevelTransitionPhase - TALLY_GEMS_IN;
      shown = span - slide;
      if (shown < 0) {
        shown = 0;
      }
      if (shown > span) {
        shown = span;
      }
      value =
          (g_anLevelGemsCollected[g_nPrevLevelIndex] - g_nLevelGemsAtEntry) *
          shown / span;
    } else if (g_nLevelTransitionPhase < TALLY_TOTAL_COUNT) {
      value = g_nTotalTreasureCollected -
              (g_anLevelGemsCollected[g_nPrevLevelIndex] - g_nLevelGemsAtEntry);
    } else {
      span = (g_nGemTallyIconCount + 1) * TALLY_ICON_FRAMES;
      if (span > TALLY_COUNT_MAX) {
        span = TALLY_COUNT_MAX;
      }
      slide = g_nLevelTransitionPhase - TALLY_TOTAL_COUNT;
      shown = span - slide;
      if (shown < 0) {
        shown = 0;
      }
      if (shown > span) {
        shown = span;
      }
      value =
          g_nTotalTreasureCollected -
          (g_anLevelGemsCollected[g_nPrevLevelIndex] - g_nLevelGemsAtEntry) *
              shown / span;
    }

    msg = buf;
    pos[0] = x0 + 0x178;
    pos[2] = 0xf80;
    sprintf(msg, g_szFmtPercentD, value);
    {
      Actor *glyph = g_pSpriteRecordWriteCursor;
      BuildTextSpriteChain((unsigned char *)msg, pos, 0x14, TINT_GOLD);
      glyph--;
      if ((int)g_pSpriteRecordWriteCursor <= (int)glyph) {
        unsigned short *lut = (unsigned short *)g_anCosineLut;
        int wave = 0;
        do {
          glyph->rotZ = lut[(g_nLevelTransitionPhase * 2 + wave) & 0xff] >> 7;
          glyph--;
          wave += 0xc;
        } while ((int)g_pSpriteRecordWriteCursor <= (int)glyph);
      }
    }

    i = 0;
    do {
      if (PARTICLES[i].live != 0) {
        g_pSpriteRecordWriteCursor--;
        FillWord(g_pSpriteRecordWriteCursor, 0, ACTOR_SIZE);
        SPR->uv = g_abGemTallyIconUvTable[i];
        gx = PARTICLES[i].x + 0x140;
        SPR->x = x0 + gx;
        SPR->y = 0xc6 - PARTICLES[i].y;
        SPR->scale = 0x1000;
        SPR->r = PARTICLES[i].driftx;
        g_pSpriteRecordWriteCursor->rotY = PARTICLES[i].drifty;
        g_pSpriteRecordWriteCursor->rotZ = PARTICLES[i].spin;
        g_pSpriteRecordWriteCursor->otBias = OT_BIAS_SPRITE;
        g_pSpriteRecordWriteCursor->tintMode =
            ACTOR_RAW(g_pSpriteRecordWriteCursor, unsigned char, type) -
            (ACTOR_TYPE_GEM_1 - TINT_GEM_1);
        g_pSpriteRecordWriteCursor->cullRadius = ACTOR_CULL_ALWAYS;
      }
      i++;
    } while (i < GEM_TALLY_ICONS_MAX);

    g_pSpriteRecordWriteCursor--;
    FillWord(g_pSpriteRecordWriteCursor, 0, ACTOR_SIZE);
    spr = (TallySprite *)g_pSpriteRecordWriteCursor;
    spr->uv = 0x1d9;
    spr->x = x0 + 0x140;
    if (g_nLevelTransitionPhase < TALLY_TOTAL_IN) {
      y = pos[1];
      spr->scale = 0x800;
      spr->r = 6;
      y += 8;
    } else {
      y = pos[1];
      spr->scale = 0x600;
      spr->r = 6;
      y += 0xc;
    }
    spr->y = y;
    g_pSpriteRecordWriteCursor->rotZ = 0xb0;
    g_pSpriteRecordWriteCursor->otBias = OT_BIAS_SPRITE;
    g_pSpriteRecordWriteCursor->tintMode = TINT_HUD;
    g_pSpriteRecordWriteCursor->cullRadius = ACTOR_CULL_ALWAYS;
  }

  g_apSpritePrimQueue[0] = 0;
  EnqueuePendingSpritePrims();
  RasterizeSpritePrimQueue();
}

#undef SPR

/* Draw gamestates 1 and 9 (GS_LEVEL_LOAD's portal flight / GS_ARRIVAL_CAMERA):
   copies the sky colour (g_dwWorldFogColor) into both frame DRAWENVs, clears
   the OT bins, draws the transition gem tally when active, Spyro, then the
   sky — through a view matrix tilted by g_nTransitionSkyPitchTilt (the
   entered portal's sky tilt, -2/frame) until it reaches 0. Ends with the
   standard 2-vblank frame submit (0x8001a050, 956 bytes). */
void LevelTransition_Draw(void) {
  int r;
  int g;
  int b;
  int v;
  MATRIX m1;
  MATRIX m2;
  MATRIX *p2;

  r = ((unsigned char *)&g_dwWorldFogColor)[0];
  g = ((unsigned char *)&g_dwWorldFogColor)[1];
  b = ((unsigned char *)&g_dwWorldFogColor)[2];
  g_FrameDrawEnv0.r0 = r;
  g_FrameDrawEnv0.g0 = g;
  g_FrameDrawEnv0.b0 = b;
  g_FrameDrawEnv1.r0 = r;
  g_FrameDrawEnv1.g0 = g;
  g_FrameDrawEnv1.b0 = b;
  FillWord(&g_apMainDrawEntityList, 0, FRAME_ARENA_LIST_WORDS);
  if (g_nLevelTransitionTallyActive != 0) {
    DrawLevelTransitionOverlay();
  }
  RasterizeSpyroModel();
  if (g_nSkyChunkCount != 0) {
    g_nTransitionSkyPitchTilt -= 2;
    if (g_nTransitionSkyPitchTilt < 0) {
      g_nTransitionSkyPitchTilt = 0;
    }
    if (g_nTransitionSkyPitchTilt != 0) {
      FillWord(&m1, 0, 0x20);
      SetTransMatrix(&m1);
      m1.m[0][0] = 0x1000;
      v = LookupCosine(g_anCameraEulerPitchBlock[0] -
                       g_nTransitionSkyPitchTilt);
      m1.m[1][1] = v;
      v = LookupSine(g_anCameraEulerPitchBlock[0] - g_nTransitionSkyPitchTilt);
      m1.m[2][1] = v;
      v = LookupSine(g_anCameraEulerPitchBlock[0] - g_nTransitionSkyPitchTilt);
      m1.m[1][2] = -v;
      v = LookupCosine(g_anCameraEulerPitchBlock[0] -
                       g_nTransitionSkyPitchTilt);
      m1.m[2][2] = v;

      p2 = &m2;
      FillWord(p2, 0, 0x20);
      v = LookupCosine(g_nCameraEulerYaw);
      m2.m[0][0] = v;
      v = LookupSine(g_nCameraEulerYaw);
      m2.m[2][0] = -v;
      m2.m[1][1] = 0x1000;
      v = LookupSine(g_nCameraEulerYaw);
      m2.m[0][2] = v;
      v = LookupCosine(g_nCameraEulerYaw);
      m2.m[2][2] = v;
      MulMatrix(&m1, p2);

      FillWord(p2, 0, 0x20);
      v = LookupCosine(g_nCameraEulerRoll);
      m2.m[0][0] = v;
      v = LookupSine(g_nCameraEulerRoll);
      m2.m[1][0] = -v;
      v = LookupSine(g_nCameraEulerRoll);
      m2.m[0][1] = v;
      v = LookupCosine(g_nCameraEulerRoll);
      m2.m[1][1] = v;
      m2.m[2][2] = 0x1000;
      MulMatrix(&m1, p2);

      CopyWords(p2, &m1, 0x14);
      v = m1.m[1][0] * 0x140;
      if (v < 0) {
        v += 0x1ff;
      }
      m2.m[1][0] = v >> 9;
      v = m1.m[1][1] * 0x140;
      if (v < 0) {
        v += 0x1ff;
      }
      m2.m[1][1] = v >> 9;
      v = m1.m[1][2] * 0x140;
      if (v < 0) {
        v += 0x1ff;
      }
      m2.m[1][2] = v >> 9;
      EmitSkyMesh(-1, &m1, p2);
    } else {
      EmitSkyMesh(-1, (MATRIX *)g_anCameraViewMtx,
                  (MATRIX *)CAM_W2C_MTX_VIA_VIEW);
    }
  }
  DrawSync(0);
  if (g_nDemoActive != 0) {
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
  PutDispEnv(&FRAME_DB(g_pActiveFrameDrawEnv)->disp);
  PutDrawEnv(g_pActiveFrameDrawEnv);
  DrawOTag(LinkOTPrimitives(OT_BIN_COUNT));
}

/* The sprite record being written (Actor layout). */

/* The backdrop quad being built at p, read back through volatile accesses,
 * and the black line under it (y = 0xE7, LINE_F2: the code reuses the 0x40 in
 * b). */
#define QUAD ((volatile PolyFT4 *)p)
#define LINE ((LineF2 *)((PolyFT4 *)p + 1))

/* What follows the last backdrop quad: the line, then the menu box. */
typedef struct BackdropTail {
  PolyFT4 quad;   /* 0x00 */
  LineF2 line;    /* 0x28 */
  ModePolyF4 box; /* 0x38 */
} BackdropTail;

/* Position stores go through ACTOR_RAW: g_pSpriteRecordWriteCursor->posX
   (MEM_IN_STRUCT_P) and an int-array view both let sched move them past the
   neighbouring global loads (cookbook A357); only the byte-offset form keeps
   the original order. */

/* Draw for gamestates 2 (pause menu), 3 (inventory) and 6 (dialog overlay)
   — 0x8001a40c, 8840 bytes.

   On the entry frame (g_nMenuDrawFrames == 0) it renders one last
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
void PauseMenu_Draw(void) {
  RECT rect;             /* sp18 */
  char buf[0x20];        /* sp20 */
  int pos[3];            /* sp40 */
  int quitGlyph[3];      /* sp50 */
  int optionsGlyph[3];   /* sp60 */
  int pauseGlyph[3];     /* sp70 */
  short quad[4];         /* sp80 */
  short quad2[4];        /* sp88 */
  int inventoryGlyph[3]; /* sp90 */
  unsigned char rows;    /* spA0 */
  int h0, h1, h2, h3, h4, h5, h6, h7;

  if (g_nMenuDrawFrames == 0) {
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
    REG_RESERVE(k18);
    BuildRenderEntityLists();
    h = DRAW_H;
    ComposeFrameScene();
    /* The two do / while (0) groups are cse block boundaries (A200) that
       keep the original's load / call order; they never loop. */
    do {
      strip = MENU_BACKDROP_X;
      RasterizeEmitList();
      DrawPortalsAndSky();
      SetupFrameOT();
      DrawSync(0);
      VSync(0);
      envBase = &g_FrameDrawEnv0;
    } while (0);
    do {
      PutDispEnv(&FRAME_DB(g_pActiveFrameDrawEnv)->disp);
      img = g_apRenderScratchPrimTopBlock;
    } while (0);
    KEEP_IN_REG(k18);
    img2 = img - 2;
    PutDrawEnv(g_pActiveFrameDrawEnv);
    DrawOTag(LinkOTPrimitives(OT_BIN_COUNT));
    DrawSync(0);
    VSync(0);
    PutDispEnv(&FRAME_DB(g_pActiveFrameDrawEnv)->disp);
    rect.x = MENU_BACKDROP_X;
    rect.y = 0;
    rect.w = MENU_SNAPSHOT_W;
    rect.h = MENU_SNAPSHOT_H;
    StoreImage(&rect, (void *)((char *)img[0] - MENU_SNAPSHOT_BYTES));
    y = DRAW_Y;
    do {
      KEEP_IN_REG(strip);
      rect.x = i << 7;
      if (g_pActiveFrameDrawEnv != envBase) {
        y = SCREEN_H + DRAW_Y;
      }
      rect.y = y;
      rect.w = MENU_BACKDROP_STRIP_W;
      rect.h = h;
      ((void (*)())StoreImage)(&rect, img2[0], y);
      i += 1;
      DrawSync(0);
      ConvertRGB15ToLuma8(img2[0], MENU_BACKDROP_LUMA_PIXELS);
      rect.x = strip;
      rect.y = 0;
      rect.w = MENU_BACKDROP_STRIP_W / 2;
      rect.h = h;
      LoadImage(&rect, img2[0]);
      strip += MENU_BACKDROP_STRIP_W / 2;
      y = DRAW_Y;
    } while (i < MENU_BACKDROP_STRIPS);
    rect.x = MENU_BACKDROP_X;
    rect.y = MENU_BACKDROP_CLUT_Y;
    rect.w = 0x20;
    rect.h = 1;
    LoadImage(&rect, g_anMenuBackdropGrayClut);
    DrawSync(0);
    g_nVsyncFramePaceAnchor = VSync(-1);
  } else {
    BackdropTail *prim;
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
    env = ((char *)&g_FrameDrawEnv0);
    if (cur == env) {
      env += 0x84;
    }
    i = 0;
    PutDrawEnv((DRAWENV *)env);
    {
      int off, bias;
      do {
        off = PRIM_BUFFER_BYTES;
        bias = -MENU_SNAPSHOT_BYTES;
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

    tp = i + MENU_BACKDROP_TPAGE;
    p = (*(char **)&g_pPrimBufferWriteCursor);
    {
      register int a asm("$2");
      register int d asm("$3");
      register unsigned short bh asm("$5");
      register unsigned short ch asm("$6");
      register unsigned char bq asm("$5");
      register unsigned char cq asm("$6");
      QUAD->tag = PRIM_TAG(9);
      QUAD->code = GPU_CODE_POLY_FT4;
      QUAD->r0 = 0x4C;
      QUAD->g0 = 0x80;
      QUAD->x0 = xx;
      QUAD->y0 = 8;
      a = (unsigned short)QUAD->x0;
      bh = (unsigned short)QUAD->y0;
      ch = (unsigned short)QUAD->x0;
      d = (unsigned short)QUAD->y0;
      QUAD->x1 = a + 0x80;
      a = (unsigned short)QUAD->x0;
      QUAD->y2 = d + 0xDF;
      d = (unsigned short)QUAD->y0;
      i += 1;
      QUAD->v0 = 0;
      QUAD->y1 = bh;
      bq = QUAD->v0;
      b = 0x40;
      QUAD->u0 = 0;
      QUAD->x2 = ch;
      cq = QUAD->u0;
      QUAD->x3 = a + 0x80;
      a = QUAD->u0;
      QUAD->y3 = d + 0xDF;
      d = QUAD->v0;
      QUAD->b0 = b;
      QUAD->v1 = bq;
      QUAD->u2 = cq;
      QUAD->u1 = a + 0x80U;
      a = QUAD->u0;
      QUAD->v2 = d - 0x21;
      d = QUAD->v0;
      QUAD->u3 = a + 0x80U;
      QUAD->v3 = d - 0x21;
      /* Plain stores, so the tpage store fills the jal delay slot. */
      ((PolyFT4 *)p)->clut = MENU_BACKDROP_CLUT;
      ((PolyFT4 *)p)->tpage = tp;
      AddPrimToOT((int)p);
    }
  }
    next = p + 0x28;
    (*(char **)&g_pPrimBufferWriteCursor) = next;
    if (i < 4) {
      goto quad;
    }
    LINE->tag = PRIM_TAG(3);
    LINE->code = b;
    LINE->x0 = 0;
    LINE->y0 = 0xE7;
    LINE->x1 = 0x200;
    LINE->y1 = 0xE7;
    LINE->r0 = 0;
    LINE->g0 = 0;
    LINE->b0 = 0;
    AddPrimToOT((int)next);
    (*(char **)&g_pPrimBufferWriteCursor) = p + 0x38;
    prim = (BackdropTail *)p;
    h = b;
    if (g_nGamestate == GS_PAUSE_MENU) {
      Actor *rec;
      unsigned char *fr;
      short *lut1, *lut2, *lut3, *lut4, *lut6, *lut7, *lut8, *lut9;
      unsigned char
          nGlyphs; /* letters of the highlighted row the wobble rolls */
      int cur;
      Actor *rp;
      int n1, n2, n3, n4, n5, n6;
      int phase;
      int v;
      int phase1, v1, phase2, v2, phase3, v3, phase4, phase5, v4, phase6,
          phase7, v5, phase8, v6, phase9, v7, phase10, v8;
      int y;
      void *box = &prim->box;
      SetDrawMode(box, 1, 0, 0x40, 0);
      AddPrimToOT((int)box);
      {
        int k;
        k = PRIM_TAG(5);
        prim->box.poly.tag = k;
        k = (GPU_CODE_POLY_F4 | GPU_SEMI_TRANS);
        prim->box.poly.code = k;
        k = 0x43;
        prim->box.poly.r0 = h;
        prim->box.poly.g0 = h;
        prim->box.poly.b0 = h;
        prim->box.poly.y0 = k;
        prim->box.poly.y1 = k;
      }
      box = &prim->box.poly;
      if (g_nPauseMenuSubstate == PAUSE_PAGE_OPTIONS) {
        prim->box.poly.x0 = 0x54;
        prim->box.poly.x1 = 0x1ac;
        rows = g_nPadIsDualshockFlag != 0;
        y = rows * 0x10 + 0xb6;
        prim->box.poly.y2 = y;
        prim->box.poly.y3 = y;
      } else {
        rows = 1;
        prim->box.poly.x0 = 0x8c;
        prim->box.poly.x1 = 0x174;
        prim->box.poly.y2 = 0xb0;
        prim->box.poly.y3 = 0xb0;
      }
      ((PolyF4 *)box)->x2 = ((PolyF4 *)box)->x0;
      ((PolyF4 *)box)->x3 = ((PolyF4 *)box)->x1;
      AddPrimToOT((int)box);
      g_pPrimBufferWriteCursor = (PolyF4 *)box + 1;
      EmitRadialShadedLine(0xe0, 0x61, 0x120, 0x61);
      if (g_nPauseMenuSubstate == PAUSE_PAGE_OPTIONS) {
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
      pos[2] = TEXT_TITLE_DEPTH;
      BuildTextSpriteChain(g_szUiPaused, pos, 0x1c, TINT_HUD);
      if (g_nPauseMenuSubstate == PAUSE_PAGE_QUIT_CONFIRM) {
        quitGlyph[0] = 0x10;
        quitGlyph[1] = 1;
        quitGlyph[2] = TEXT_SMALL_DEPTH;
        pos[0] = 0xb7;
        pos[1] = 0x77;
        pos[2] = TEXT_DEPTH;
        BuildTextSprites(g_szQuitGamePrompt, pos, quitGlyph, 0x12, TINT_HUD);
        pos[0] = 0xc0;
        pos[1] = 0x94;
        pos[2] = TEXT_DEPTH;
        BuildTextSprites(g_szUiYes, pos, quitGlyph, 0x12, TINT_HUD);
        if (g_nPauseMenuCursor == PAUSE_QUIT_YES) {
          rec = g_pSpriteRecordWriteCursor;
          nGlyphs = 3;
        }
        pos[0] = 0x128;
        pos[1] = 0x94;
        pos[2] = TEXT_DEPTH;
        BuildTextSprites(g_szUiNo, pos, quitGlyph, 0x12, TINT_HUD);
        if (g_nPauseMenuCursor == PAUSE_QUIT_NO) {
          rec = g_pSpriteRecordWriteCursor;
          nGlyphs = 2;
        }
        n1 = nGlyphs;
        i = 0;
        if (n1 > 0) {
          lut1 = g_anCosineLut;
          phase1 = 0;
          do {
            i++;
            v1 = lut1[(g_nMenuDrawFrames * 8 + phase1) & 0xff];
            phase1 += 0xc;
            rec->rotZ = v1 * 3 >> 9;
            rec++;
          } while (i < n1);
        }
      } else if (g_nPauseMenuSubstate == PAUSE_PAGE_OPTIONS) {
        optionsGlyph[0] = 0xf;
        optionsGlyph[2] = TEXT_TINY_DEPTH;
        optionsGlyph[1] = 1;
        pos[0] = 0x6b;
        pos[1] = 0x6c;
        pos[2] = TEXT_SMALL_DEPTH;
        BuildTextSprites(g_szSoundEffects, pos, optionsGlyph, 0x10, TINT_HUD);
        if (g_nPauseMenuCursor == OPTIONS_ROW_SFX_VOLUME) {
          rec = g_pSpriteRecordWriteCursor;
          /* 11 of SOUND EFFECTS' 12 letters: the last S never wobbles. */
          nGlyphs = 0xb;
        }
        pos[0] = 0x79;
        pos[1] = 0x7c;
        pos[2] = TEXT_SMALL_DEPTH;
        BuildTextSprites(g_szMusicVolume, pos, optionsGlyph, 0x10, TINT_HUD);
        if (g_nPauseMenuCursor == OPTIONS_ROW_MUSIC_VOLUME) {
          rec = g_pSpriteRecordWriteCursor;
          nGlyphs = 0xb;
        }
        pos[0] = 0x6b;
        pos[1] = 0x8c;
        pos[2] = TEXT_SMALL_DEPTH;
        BuildTextSprites(g_szSpeakerSetup, pos, optionsGlyph, 0x10, TINT_HUD);
        if (g_nPauseMenuCursor == OPTIONS_ROW_SPEAKERS) {
          rec = g_pSpriteRecordWriteCursor;
          nGlyphs = 0xc;
        }
        if (g_nPadIsDualshockFlag != 0) {
          pos[0] = 0xa3;
          pos[1] = 0x9c;
          pos[2] = TEXT_SMALL_DEPTH;
          BuildTextSprites(g_szVibration, pos, optionsGlyph, 0x10, TINT_HUD);
          if (g_nPauseMenuCursor == OPTIONS_ROW_VIBRATION) {
            rec = g_pSpriteRecordWriteCursor;
            nGlyphs = 9;
          }
        }
        pos[0] = 0xd0;
        pos[2] = TEXT_DEPTH;
        v = rows;
        v <<= 4;
        pos[1] = v + 0x9c;
        BuildTextSprites(g_szUiCamera, pos, optionsGlyph, 0x10, TINT_HUD);
        if (g_nPauseMenuCursor == OPTIONS_ROW_CAMERA) {
          rec = g_pSpriteRecordWriteCursor;
          nGlyphs = 6;
        }
        pos[0] = 0xee;
        pos[1] = v + 0xac;
        pos[2] = TEXT_DEPTH;
        BuildTextSprites(g_szUiDone, pos, optionsGlyph, 0x10, TINT_HUD);
        if (g_nPauseMenuCursor == OPTIONS_ROW_DONE) {
          rec = g_pSpriteRecordWriteCursor;
          nGlyphs = 4;
        }
        n2 = nGlyphs;
        i = 0;
        if (n2 > 0) {
          lut2 = g_anCosineLut;
          phase2 = 0;
          do {
            i++;
            v2 = lut2[(g_nMenuDrawFrames * 8 + phase2) & 0xff];
            phase2 += 0xc;
            rec->rotZ = v2 * 3 >> 9;
            rec++;
          } while (i < n2);
        }
        if (g_nSfxVolume == 0) {
          pos[0] = 0x142;
          pos[1] = 0x6c;
          pos[2] = TEXT_SMALL_DEPTH;
          BuildTextSprites(g_szUiOff, pos, optionsGlyph, 0x10, TINT_HUD);
          phase3 = 0;
          if (g_nPauseMenuCursor == OPTIONS_ROW_SFX_VOLUME) {
            rec = g_pSpriteRecordWriteCursor;
            i = 0;
            do {
              i++;
              v3 = g_anCosineLut[(g_nMenuDrawFrames * 8 + phase3) & 0xff];
              phase3 += 0xc;
              rec->rotZ = v3 * 3 >> 9;
              rec++;
            } while (i < 3);
          }
        } else {
          i = 0;
          if (g_nSfxVolume > 0) {
            lut4 = g_anCosineLut;
            phase4 = 0;
            x = 0x140;
            do {
              g_pSpriteRecordWriteCursor--;
              FillWord(g_pSpriteRecordWriteCursor, 0, ACTOR_SIZE);
              rp = g_pSpriteRecordWriteCursor;
              cur = g_nPauseMenuCursor;
              rp->type = ACTOR_TYPE_LETTER_GLYPH('I');
              rp->posY = 0x6d;
              rp->posX = x;
              rp->posZ = 0x1400;
              if (cur == OPTIONS_ROW_SFX_VOLUME) {
                rp->rotZ =
                    lut4[(g_nMenuDrawFrames * 8 + phase4) & 0xff] * 3 >> 9;
              } else {
                rp->rotZ = 0;
              }
              phase4 += 0xc;
              g_pSpriteRecordWriteCursor->otBias = OT_BIAS_SPRITE;
              g_pSpriteRecordWriteCursor->tintMode = TINT_HUD;
              g_pSpriteRecordWriteCursor->cullRadius = ACTOR_CULL_ALWAYS;
              x += 0xa;
              i++;
            } while (i < g_nSfxVolume);
          }
        }
        if (g_nMusicVolume == 0) {
          pos[0] = 0x142;
          pos[1] = 0x7c;
          pos[2] = TEXT_SMALL_DEPTH;
          BuildTextSprites(g_szUiOff, pos, optionsGlyph, 0x10, TINT_HUD);
          phase5 = 0;
          if (g_nPauseMenuCursor == OPTIONS_ROW_MUSIC_VOLUME) {
            rec = g_pSpriteRecordWriteCursor;
            i = 0;
            do {
              i++;
              v4 = g_anCosineLut[(g_nMenuDrawFrames * 8 + phase5) & 0xff];
              phase5 += 0xc;
              rec->rotZ = v4 * 3 >> 9;
              rec++;
            } while (i < 3);
          }
        } else {
          i = 0;
          if (g_nMusicVolume > 0) {
            lut6 = g_anCosineLut;
            phase6 = 0;
            x = 0x140;
            do {
              g_pSpriteRecordWriteCursor--;
              FillWord(g_pSpriteRecordWriteCursor, 0, ACTOR_SIZE);
              rp = g_pSpriteRecordWriteCursor;
              cur = g_nPauseMenuCursor;
              rp->type = ACTOR_TYPE_LETTER_GLYPH('I');
              rp->posY = 0x7d;
              rp->posZ = 0x1400;
              rp->posX = x;
              if (cur == OPTIONS_ROW_MUSIC_VOLUME) {
                rp->rotZ =
                    lut6[(g_nMenuDrawFrames * 8 + phase6) & 0xff] * 3 >> 9;
              } else {
                rp->rotZ = 0;
              }
              phase6 += 0xc;
              g_pSpriteRecordWriteCursor->otBias = OT_BIAS_SPRITE;
              g_pSpriteRecordWriteCursor->tintMode = TINT_HUD;
              g_pSpriteRecordWriteCursor->cullRadius = ACTOR_CULL_ALWAYS;
              x += 0xa;
              i++;
            } while (i < g_nMusicVolume);
          }
        }
        pos[0] = 0x142;
        pos[1] = 0x8c;
        pos[2] = TEXT_SMALL_DEPTH;
        if (g_nSoundMonoMixFlag != 0) {
          BuildTextSprites(g_szUiMono, pos, optionsGlyph, 0x10, TINT_HUD);
          n3 = 4;
        } else {
          BuildTextSprites(g_szUiStereo, pos, optionsGlyph, 0x10, TINT_HUD);
          n3 = 6;
        }
        if (g_nPauseMenuCursor == OPTIONS_ROW_SPEAKERS) {
          rec = g_pSpriteRecordWriteCursor;
          i = 0;
          if (n3 != 0) {
            lut7 = g_anCosineLut;
            phase7 = 0;
            do {
              i++;
              v5 = lut7[(g_nMenuDrawFrames * 8 + phase7) & 0xff];
              phase7 += 0xc;
              rec->rotZ = v5 * 3 >> 9;
              rec++;
            } while (i < n3);
          }
        }
        if (g_nPadIsDualshockFlag != 0) {
          pos[0] = 0x142;
          pos[1] = 0x9c;
          pos[2] = TEXT_SMALL_DEPTH;
          if (g_nOptionVibrationEnabled != 0) {
            BuildTextSprites(g_szUiOn, pos, optionsGlyph, 0x10, TINT_HUD);
            n4 = 2;
          } else {
            BuildTextSprites(g_szUiOff, pos, optionsGlyph, 0x10, TINT_HUD);
            n4 = 3;
          }
          if (g_nPauseMenuCursor == OPTIONS_ROW_VIBRATION) {
            rec = g_pSpriteRecordWriteCursor;
            i = 0;
            if (n4 != 0) {
              lut8 = g_anCosineLut;
              phase8 = 0;
              do {
                i++;
                v6 = lut8[(g_nMenuDrawFrames * 8 + phase8) & 0xff];
                phase8 += 0xc;
                rec->rotZ = v6 * 3 >> 9;
                rec++;
              } while (i < n4);
            }
          }
        }
        pos[0] = 0x142;
        pos[1] = rows * 0x10 + 0x9c;
        pos[2] = TEXT_SMALL_DEPTH;
        if (g_dwActiveCameraOptions == CAM_OPT_ACTIVE) {
          BuildTextSprites(g_szUiActive, pos, optionsGlyph, 0x10, TINT_HUD);
          n5 = 6;
        } else {
          BuildTextSprites(g_szUiPassive, pos, optionsGlyph, 0x10, TINT_HUD);
          n5 = 7;
        }
        if (g_nPauseMenuCursor == OPTIONS_ROW_CAMERA) {
          rec = g_pSpriteRecordWriteCursor;
          i = 0;
          if (n5 != 0) {
            lut9 = g_anCosineLut;
            phase9 = 0;
            do {
              i++;
              v7 = lut9[(g_nMenuDrawFrames * 8 + phase9) & 0xff];
              phase9 += 0xc;
              rec->rotZ = v7 * 3 >> 9;
              rec++;
            } while (i < n5);
          }
        }
      } else {
        pauseGlyph[0] = 0x10;
        pauseGlyph[2] = TEXT_SMALL_DEPTH;
        pos[0] = 0xc7;
        pauseGlyph[1] = 1;
        pos[1] = 0x6e;
        pos[2] = TEXT_DEPTH;
        BuildTextSprites(g_szContinue, pos, pauseGlyph, 0x12, TINT_HUD);
        if (g_nPauseMenuCursor == PAUSE_ROW_CONTINUE) {
          rec = g_pSpriteRecordWriteCursor;
          nGlyphs = 8;
        }
        pos[0] = 0xcf;
        pos[1] = 0x80;
        pos[2] = TEXT_DEPTH;
        BuildTextSprites(g_szUiOptions, pos, pauseGlyph, 0x12, TINT_HUD);
        if (g_nPauseMenuCursor == PAUSE_ROW_OPTIONS) {
          rec = g_pSpriteRecordWriteCursor;
          nGlyphs = 7;
        }
        pos[0] = 0xbf;
        pos[1] = 0x92;
        pos[2] = TEXT_DEPTH;
        BuildTextSprites(g_szInventory, pos, pauseGlyph, 0x12, TINT_HUD);
        if (g_nPauseMenuCursor == PAUSE_ROW_INVENTORY) {
          rec = g_pSpriteRecordWriteCursor;
          nGlyphs = 9;
        }
        if (g_nFlightLevelActive != 0) {
          pos[0] = 0xe7;
          pos[1] = 0xa4;
          pos[2] = TEXT_DEPTH;
          BuildTextSprites(g_szUiQuit, pos, pauseGlyph, 0x12, TINT_HUD);
          if (g_nPauseMenuCursor == PAUSE_ROW_EXIT) {
            rec = g_pSpriteRecordWriteCursor;
            nGlyphs = 4;
          }
          n6 = nGlyphs;
        } else if (g_nActiveLevelId != g_nActiveLevelId / 10 * 10) {
          pos[0] = 0xb7;
          pos[1] = 0xa4;
          pos[2] = TEXT_DEPTH;
          BuildTextSprites(g_szExitLevel, pos, pauseGlyph, 0x12, TINT_HUD);
          if (g_nPauseMenuCursor == PAUSE_ROW_EXIT) {
            rec = g_pSpriteRecordWriteCursor;
            nGlyphs = 9;
          }
          n6 = nGlyphs;
        } else {
          pos[0] = 0xbf;
          pos[1] = 0xa4;
          pos[2] = TEXT_DEPTH;
          BuildTextSprites(g_szQuitGame, pos, pauseGlyph, 0x12, TINT_HUD);
          if (g_nPauseMenuCursor == PAUSE_ROW_EXIT) {
            rec = g_pSpriteRecordWriteCursor;
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
            v8 = lut3[(g_nMenuDrawFrames * 8 + phase10) & 0xff];
            phase10 += 0xc;
            rec->rotZ = v8 * 3 >> 9;
            rec++;
          } while (i < n6);
        }
      }
    } else if (g_nGamestate == GS_INVENTORY) {
      ModePolyF4 *prim3;
      Actor *rec;
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
      BuildTextSpriteChain(buf, pos, 0x22, TINT_GOLD);
      pos[0] = 0x1de;
      buf[0] = '%';
      buf[1] = 0;
      BuildTextSpriteChain(buf, pos, 0x22, TINT_GOLD);
      pos[0] = 0x5a;
      pos[1] = 0x24;
      pos[2] = 0xb40;
      sprintf(buf, fmt, g_nTotalTreasureCollected);
      BuildTextSpriteChain(buf, pos, 0x1b, TINT_GOLD);
      pos[0] = 0x14d;
      pos[1] = 0x24;
      pos[2] = 0xb40;
      if (g_nTotalDragonsRescued >= 0xa) {
        pos[0] = 0x132;
      }
      sprintf(buf, fmt, g_nTotalDragonsRescued);
      BuildTextSpriteChain(buf, pos, 0x1b, TINT_GOLD);
      g_pSpriteRecordWriteCursor--;
      FillWord(g_pSpriteRecordWriteCursor, 0, ACTOR_SIZE);
      g_pSpriteRecordWriteCursor->type = ACTOR_TYPE_HUD_GEM_ICON;
      g_pSpriteRecordWriteCursor->posX = 0x2e;
      g_pSpriteRecordWriteCursor->posY = 0x2e;
      g_pSpriteRecordWriteCursor->posZ = 0xc00;
      g_pSpriteRecordWriteCursor->rotY =
          ((unsigned short *)g_anCosineLut)[g_adwHudIconSpinAngleBlock[0]]
              << 0x10 >>
          0x19;
      g_pSpriteRecordWriteCursor->rotZ = g_adwHudIconSpinAngleBlock[0];
      g_pSpriteRecordWriteCursor->otBias = OT_BIAS_SPRITE;
      g_pSpriteRecordWriteCursor->tintMode = TINT_HUD;
      g_pSpriteRecordWriteCursor->cullRadius = ACTOR_CULL_ALWAYS;
      g_pSpriteRecordWriteCursor--;
      FillWord(g_pSpriteRecordWriteCursor, 0, ACTOR_SIZE);
      g_pSpriteRecordWriteCursor->type = ACTOR_TYPE_HUD_DRAGON_ICON;
      g_pSpriteRecordWriteCursor->posX = 0x12c;
      if (g_nTotalDragonsRescued >= 0xa) {
        g_pSpriteRecordWriteCursor->posX = 0x111;
      }
      {
        int bob;
        bob = (g_nMenuDrawFrames * 0x10 & 0x1f0) >> 1;
        g_pSpriteRecordWriteCursor->posY = 0x2e;
        g_pSpriteRecordWriteCursor->posZ = 0x780;
        g_pSpriteRecordWriteCursor->rotZ =
            ((unsigned short *)g_anCosineLut)[bob] >> 8;
      }
      g_pSpriteRecordWriteCursor->otBias = OT_BIAS_SPRITE;
      g_pSpriteRecordWriteCursor->tintMode = TINT_HUD;
      g_pSpriteRecordWriteCursor->cullRadius = ACTOR_CULL_ALWAYS;
      {
        int x3;
        x3 = 0x24;
        if (g_nTotalEggsRecovered > 0) {
          i = 0;
          do {
            quad[1] = 0x32;
            quad[2] = 0x18;
            quad[0] = i * 0x1b + 0x18;
            quad[3] = 0x10;
            if (i >= g_nTotalEggsRecovered) {
              pos[1] = 0x3a;
              pos[0] = x3;
              pos[2] = 0x1800;
              BuildTextSpriteChain(g_szUiGlyphX, pos, 0, TINT_GOLD);
              pos[0] = 0x20;
              pos[1] = 0x20;
              pos[2] = 0x20;
              EmitTexturedQuad(quad, &g_anLevelSpriteTexWordPairs[2], pos);
            } else {
              EmitTexturedQuad(
                  quad,
                  &g_adwHudIconSpinAngleBlock[HUD_SPIN_TO_EGG_ICON_FRAMES] +
                      (g_adwHudIconSpinAngleBlock[HUD_SPIN_TO_FRAME_CYCLE] +
                       i) %
                          9 * 2,
                  (int *)0);
            }
            x3 += 0x1b;
            i++;
          } while (i < 0xc);
        }
      }
      EmitRadialShadedLine(0x40, 0x45, 0x1c0, 0x45);
      prim3 = g_pPrimBufferWriteCursor;
      SetDrawMode(prim3, 1, 0, 0x40, 0);
      AddPrimToOT((int)prim3);
      prim3->poly.tag = PRIM_TAG(5);
      prim3->poly.code = (GPU_CODE_POLY_F4 | GPU_SEMI_TRANS);
      prim3->poly.r0 = 0x60;
      prim3->poly.g0 = 0x60;
      prim3->poly.b0 = 0x60;
      prim3->poly.x0 = 0x40;
      prim3->poly.y0 = 0x46;
      prim3->poly.x1 = 0x1c0;
      prim3->poly.y1 = 0x46;
      prim3->poly.x2 = 0x40;
      prim3->poly.y2 = 0x47;
      prim3->poly.x3 = 0x1c0;
      prim3->poly.y3 = 0x47;
      AddPrimToOT((int)&prim3->poly);
      g_pPrimBufferWriteCursor = prim3 + 1;
      EmitRadialShadedLine(0x40, 0x47, 0x1c0, 0x47);
      if (g_nSubMenuCursor > 0 &&
          g_abLevelVisitedFlagVirtualBase[g_nSubMenuCursor * 6] != 0) {
        pos[0] = 0x28;
        pos[1] = 0x48;
        pos[2] = TEXT_DEPTH;
        EnqueueBlinkingMarkerSprite(pos, g_nMenuDrawFrames * 2, 1);
      }
      if (g_nSubMenuCursor < 5) {
        int *pa = pos;
        if ((&g_abLevelVisitedFlag[LEVEL_SLOT(
                LEVEL_PEACE_KEEPERS)])[g_nSubMenuCursor * 6] != 0) {
          pos[0] = 0x1d7;
          pos[1] = 0x48;
          pos[2] = TEXT_DEPTH;
          EnqueueBlinkingMarkerSprite(pa, g_nMenuDrawFrames * 2, 0);
        }
      }
      i = 0;
      bias = (g_nSubMenuCursor < 3) * 8;
      v = strlen((&g_apWorldNames[1])[g_nSubMenuCursor]);
      pos[1] = 0x5c;
      pos[2] = 0xe00;
      inventoryGlyph[0] = 0x14;
      inventoryGlyph[1] = 2;
      inventoryGlyph[2] = 0x1000;
      v = (v - 1) * 0xa - 0x9e;
      pos[0] = g_nSubMenuScrollOffset - v;
      BuildTextSprites((&g_apWorldNames[1])[g_nSubMenuCursor], pos,
                       inventoryGlyph, 0x17, TINT_GOLD);
      rec = g_pSpriteRecordWriteCursor;
      phase = 0;
      while (i < (int)g_abWorldTitleGlyphBlock[g_nSubMenuCursor]) {
        i++;
        rec->rotZ =
            g_anCosineLut[(g_nMenuDrawFrames * 8 + phase) & 0xff] * 3 >> 9;
        phase += 0xc;
        rec++;
      }
      g_pSpriteRecordWriteCursor--;
      FillWord(g_pSpriteRecordWriteCursor, 0, ACTOR_SIZE);
      g_pSpriteRecordWriteCursor->type = ACTOR_TYPE_HUD_GEM_ICON;
      {
        int bF = bias + 0x15a;
        g_pSpriteRecordWriteCursor->posX = g_nSubMenuScrollOffset + bF;
      }
      SCHED_FENCE();
      ACTOR_RAW(g_pSpriteRecordWriteCursor, int, posY) = 0x6c;
      pp = pos;
      bp = buf;
      ACTOR_RAW(g_pSpriteRecordWriteCursor, int, posZ) = 0xc00;
      g_pSpriteRecordWriteCursor->rotY =
          ((unsigned short *)g_anCosineLut)[g_dwHudIconSpinAngle] << 0x10 >>
          0x19;
      SCHED_FENCE();
      g_pSpriteRecordWriteCursor->rotZ = g_dwHudIconSpinAngle;
      g_pSpriteRecordWriteCursor->otBias = OT_BIAS_SPRITE;
      g_pSpriteRecordWriteCursor->tintMode = TINT_HUD;
      g_pSpriteRecordWriteCursor->cullRadius = ACTOR_CULL_ALWAYS;
      g_pSpriteRecordWriteCursor--;
      FillWord(g_pSpriteRecordWriteCursor, 0, ACTOR_SIZE);
      {
        int bob;
        ACTOR_RAW(g_pSpriteRecordWriteCursor, int, posX) =
            g_nSubMenuScrollOffset + 0x1d1;
        ACTOR_RAW(g_pSpriteRecordWriteCursor, int, posY) = 0x6e;
        {
          register int fr asm("$2");
          fr = g_nMenuDrawFrames;
          g_pSpriteRecordWriteCursor->type = ACTOR_TYPE_HUD_DRAGON_ICON;
          bob = (fr * 0x10 & 0x1f0) >> 1;
        }
        g_pSpriteRecordWriteCursor->posZ = 0x780;
        g_pSpriteRecordWriteCursor->rotZ =
            ((unsigned short *)g_anCosineLut)[bob] >> 8;
      }
      g_pSpriteRecordWriteCursor->otBias = OT_BIAS_SPRITE;
      i = 0;
      g_pSpriteRecordWriteCursor->tintMode = TINT_HUD;
      y = 0x7b;
      g_pSpriteRecordWriteCursor->cullRadius = ACTOR_CULL_ALWAYS;
      do {
        if (g_abLevelVisitedFlag[g_nSubMenuCursor * 6 + i] != 0) {
          int bA, bB, bC, bD, bE;
          int *gems;
          int gx;
          int pf;
          int sc;
          int row;
          prim3 = g_pPrimBufferWriteCursor;
          SetDrawMode(prim3, 1, 0, 0x40, 0);
          AddPrimToOT((int)prim3);
          pf = (int)&prim3->poly;
          prim3->poly.tag = PRIM_TAG(5);
          prim3->poly.code = (GPU_CODE_POLY_F4 | GPU_SEMI_TRANS);
          prim3->poly.r0 = 0x28;
          prim3->poly.g0 = 0x28;
          prim3->poly.b0 = 0x28;
          sc = g_nSubMenuScrollOffset;
          prim3->poly.y0 = y - 0xa;
          prim3->poly.y1 = y - 0xa;
          prim3->poly.y2 = y + 9;
          prim3->poly.y3 = y + 9;
          prim3->poly.x0 = sc;
          prim3->poly.x1 = sc + 0x1ff;
          prim3->poly.x2 = sc;
          prim3->poly.x3 = sc + 0x1ff;
          AddPrimToOT(pf);
          g_pPrimBufferWriteCursor = prim3 + 1;
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
          pos[2] = TEXT_DEPTH;
          inventoryGlyph[0] = 0x10;
          inventoryGlyph[1] = 1;
          inventoryGlyph[2] = TEXT_SMALL_DEPTH;
          rec = g_pSpriteRecordWriteCursor;
          BuildTextSprites(g_apLevelNameStrings[g_nSubMenuCursor * 6 + i], pos,
                           inventoryGlyph, 0x12, TINT_GOLD);
          bC = bias + 0x113;
          gx = g_nSubMenuScrollOffset + bC;
          {
            int idx = g_nSubMenuCursor * 6 + i;
            int *gb = g_anLevelGemsBlock;
            SCHED_FENCE();
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
          BuildTextSpriteChain(buf, pos, 0x12, TINT_GOLD);
          pos[2] = 0x1800;
          bD = bias + 0x15a;
          pos[0] = g_nSubMenuScrollOffset + bD;
          BuildTextSpriteChain(g_szUiSlash, pos, 0x12, TINT_GOLD);
          pos[2] = TEXT_DEPTH;
          bE = bias + 0x16b;
          pos[0] = g_nSubMenuScrollOffset + bE;
          sprintf(buf, g_szFmtPercentD,
                  g_anLevelTotalGemsTable[g_nSubMenuCursor * 6 + i]);
          BuildTextSpriteChain(buf, pos, 0x12, TINT_GOLD);
          if (g_abLevelTotalDragonsBlock[g_nSubMenuCursor * 6 + i] != 0) {
            pos[0] = g_nSubMenuScrollOffset + 0x1bd;
            sprintf(buf, g_szFmtPercentD,
                    g_anLevelDragonsRescued[g_nSubMenuCursor * 6 + i]);
            BuildTextSpriteChain(buf, pos, 0x12, TINT_GOLD);
            pos[2] = 0x1800;
            pos[0] = g_nSubMenuScrollOffset + 0x1ce;
            BuildTextSpriteChain(g_szUiSlash, pos, 0x12, TINT_GOLD);
            pos[2] = TEXT_DEPTH;
            pos[0] = g_nSubMenuScrollOffset + 0x1df;
            sprintf(buf, g_szFmtPercentD,
                    g_abLevelTotalDragonsBlock[g_nSubMenuCursor * 6 + i]);
            BuildTextSpriteChain(buf, pos, 0x12, TINT_GOLD);
          } else {
            pos[2] = 0x1800;
            pos[0] = g_nSubMenuScrollOffset + 0x1ce;
            BuildTextSpriteChain(g_szUiSlash, pos, 0x12, TINT_GOLD);
            g_pSpriteRecordWriteCursor->rotX = 0x29;
          }
          if (g_nSubMenuCursor * 6 + i == g_nLevelIndex) {
            rec--;
            if ((int)rec >= (int)g_pSpriteRecordWriteCursor) {
              phase = 0;
              do {
                rec->rotZ =
                    g_anCosineLut[(g_nMenuDrawFrames * 8 + phase * 0xc) &
                                  0xff] *
                        3 >>
                    9;
                rec--;
                phase++;
              } while ((int)rec >= (int)g_pSpriteRecordWriteCursor);
            }
          }
          if (g_nTotalEggsRecovered > 0 && g_nSubMenuCursor < 3) {
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
                BuildTextSpriteChain(g_szUiGlyphX, pos, 0, TINT_GOLD);
                pos[0] = 0x20;
                pos[1] = 0x20;
                pos[2] = 0x20;
                EmitTexturedQuad(quad2, &g_anLevelSpriteTexWordPairs[2], pos);
              } else {
                EmitTexturedQuad(
                    quad2,
                    &g_anHudFrameCycleBlock[HUD_CYCLE_TO_EGG_ICON_FRAMES] +
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
    if (g_nGamestate == GS_PAUSE_MENU && g_nFlightLevelActive == 0) {
      EnqueueHudSprites();
    }
    EnqueuePendingSpritePrims();
    FillWord(&g_apMainDrawEntityList, 0, FRAME_ARENA_LIST_WORDS);
    RasterizeSpritePrimQueue();
    DrawOTag(LinkOTPrimitives(0));
    DrawSync(0);
    if (g_nDemoActive != 0) {
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
    PutDispEnv(&FRAME_DB(g_pActiveFrameDrawEnv)->disp);
  }
  __asm__ volatile(""
                   :
                   : "g"(h0), "g"(h1), "g"(h2), "g"(h3), "g"(h4), "g"(h5),
                     "g"(h6), "g"(h7));
  g_nMenuDrawFrames++;
}

#undef QUAD
#undef LINE

/* Draw GS_MENU_TRANSITION (pause -> world-map transition, 0x8001c694, 0x3a4).
   Phase 0: re-darken the frozen gray backdrop by re-loading the CLUT ramp
   shifted by the idle counter, then draw the 4 backdrop quads (clut 0x3820)
   plus a black line into OT slot 0x3ff. Phase 1: fog cross-fade — feed
   (0x20-idle)*0x80 through ComputeDepthCuedColor into the back DRAWENV's
   r0/g0/b0 and emit the fogged static mesh list. Both phases: OT clear,
   RasterizeSpyroModel, DrawOTag, then the standard 2-vblank frame pace.

   The quad loop is written field by field the way libgpu's set* macros
   expand: each derived vertex re-reads x0/y0/u0/v0 from the primitive, and
   gcc 2.7.2 keeps those as real narrow loads. `slot` is set before the
   loop and again at the end of every pass (two sets, so it stays a pseudo
   in a1 instead of being rematerialised at the call). Case 1: `env` is a
   `$4` register local, and the color spill store plus the active-env load
   are both volatile, so the spill stays above the compare load. */

void MenuTransition_Draw(void) {
  PolyFT4 *q;
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
  g_pPrimBufferLimit = (char *)g_pRenderScratchRegionBase + PRIM_BUFFER_BYTES;
  switch (g_nMenuDrawFrames) {
  case 0: {
    DRAWENV *env;

    env = &g_FrameDrawEnv0;
    if (g_pActiveFrameDrawEnv == &g_FrameDrawEnv0) {
      env = &FRAME_DB(env)[1].draw;
    }
    i = 0;
    PutDrawEnv(env);
    rect.y = MENU_BACKDROP_CLUT_Y;
    rect.h = 1;
    rect.x = g_nMenuTickFrames + MENU_BACKDROP_X;
    rect.w = 0x20 - g_nMenuTickFrames;
    LoadImage(&rect, g_anMenuBackdropGrayClut);
    DrawSync(0);
    slot = 0x3FF;
  quad:
    q = (PolyFT4 *)g_pPrimBufferWriteCursor;
    *(int *)q = PRIM_TAG(9);
    q->code = GPU_CODE_POLY_FT4;
    q->r0 = 0x4C;
    q->g0 = 0x80;
    q->x0 = i << 7;
    tag = i + MENU_BACKDROP_TPAGE;
    q->y0 = 8;
    q->x1 = q->x0 + 0x80;
    q->y1 = q->y0;
    q->x2 = q->x0;
    q->y2 = q->y0 + 0xDF;
    q->x3 = q->x0 + 0x80;
    q->y3 = q->y0 + 0xDF;
    i += 1;
    lineCode = GPU_CODE_LINE_F2;
    q->b0 = lineCode;
    q->u0 = 0;
    q->v0 = 0;
    q->u1 = q->u0 + 0x80;
    q->v1 = q->v0;
    q->u2 = q->u0;
    q->v2 = q->v0 - 0x21;
    q->u3 = q->u0 + 0x80;
    q->v3 = q->v0 - 0x21;
    q->clut = MENU_BACKDROP_CLUT;
    q->tpage = tag;
    p = (unsigned char *)q;
    AddPrimToOTSlot(p, slot);
    next = p + 0x28;
    g_pPrimBufferWriteCursor = next;
    slot = 0x3FF;
    if (i < 4) {
      goto quad;
    }
    /* a full-width line at y = 0xE7, packed right after the last quad */
#define LINE ((LineF2 *)(p + 0x28))
    LINE->tag = PRIM_TAG(3);
    LINE->code = lineCode;
    LINE->x0 = 0;
    LINE->y0 = 0xE7;
    LINE->x1 = 0x200;
    LINE->y1 = 0xE7;
    LINE->r0 = 0;
    LINE->g0 = 0;
    LINE->b0 = 0;
#undef LINE
    AddPrimToOTSlot(next, slot);
    g_pPrimBufferWriteCursor = p + 0x38;
    break;
  }
  case 1: {
    /* Hard-register pins (A374 / F14j): each holds the register the
       original uses for it, which settles the competing temps. */
    register DRAWENV *env asm("$4");

    g_dwSkyFadeColor = 0;
    prog = (0x20 - g_nMenuTickFrames) << 7;
    g_nFadeProgress = prog;
    *(volatile int *)&color = ComputeDepthCuedColor(g_dwWorldFogColor, 0, prog);
    env = &g_FrameDrawEnv0;
    if (*(DRAWENV *volatile *)&g_pActiveFrameDrawEnv == &g_FrameDrawEnv0) {
      g_FrameDrawEnv1.r0 = ((unsigned char *)&color)[0];
      g_FrameDrawEnv1.g0 = ((unsigned char *)&color)[1];
      g_FrameDrawEnv1.b0 = ((unsigned char *)&color)[2];
      env = &FRAME_DB(env)[1].draw;
    } else {
      g_FrameDrawEnv0.r0 = ((unsigned char *)&color)[0];
      g_FrameDrawEnv0.g0 = ((unsigned char *)&color)[1];
      g_FrameDrawEnv0.b0 = ((unsigned char *)&color)[2];
    }
    PutDrawEnv(env);
    EmitSkyMeshFogged();
    break;
  }
  }
  FillWord(&g_apMainDrawEntityList, 0, FRAME_ARENA_LIST_WORDS);
  RasterizeSpyroModel();
  DrawOTag(LinkOTPrimitives(OT_BIN_COUNT));
  DrawSync(0);
  if (g_nDemoActive != 0) {
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
  PutDispEnv(&FRAME_DB(g_pActiveFrameDrawEnv)->disp);
}

#undef LINE

/* The "GAME OVER" letter screen position at byte offset `k` (letter * 4) of
   g_anGameOverLetterPosXY; the draw loop steps k by 4 alongside the letter
   index. */
typedef struct LetterPos {
  short x, y;
} LetterPos;
#define LETTER_POS(k) ((LetterPos *)((char *)g_anGameOverLetterPosXY + (k)))

/* Draw for GS_GAME_OVER (5): three sub-modes driven by
   g_nRespawnOrGameOverMode. Mode 0 = death fade-out: frame 0 renders one full
   gameplay frame then snapshots half the framebuffer (MoveImage) as a frozen
   backdrop; frames 1..15 darken it with fullscreen tints, frame 15 switches
   both DRAWENVs to black-clear and wipes VRAM. Mode 1 = GAME OVER screen:
   letter sprites revealed on a per-letter delay ramp from frame 0x3C, prompt
   text + sine wave from frame 0x1A5, over a scripted-camera world render. Mode
   >= 2 mirrors mode 0 without the initial render (0x8001ca38, 1444 bytes). */

/* Returns its argument through an empty asm so cse/reload cannot fold the
   frame-15 DRAWENV block base back into absolute addresses. Inline, so both
   identical frame-15 tails still cross-jump into one. */
static inline unsigned char *Opaque(unsigned char *p) {
  unsigned char *q;

  OPAQUE_COPY(q, p);
  return q;
}

void RespawnOrGameOver_Draw(void) {
  RECT rect;
  int pos[3];
  int size[3];
  int i;
  int k;
  int t;
  int d;
  short *ramp;
  char *rec;
  int srcY;
  int f;
  void **q;
  int sk;
  int st;
  Actor *sc;
  short *sbase;

  if (g_nRespawnOrGameOverMode == 0) {
    if (g_nRespawnOrGameOverFrame == 0) {
      BuildRenderEntityLists();
      ComposeFrameScene();
      RasterizeEmitList();
      DrawPortalsAndSky();
      SetupFrameOT();
      DrawSync(0);
      VSync(0);
      PutDispEnv(&FRAME_DB(g_pActiveFrameDrawEnv)->disp);
      PutDrawEnv(g_pActiveFrameDrawEnv);
      DrawOTag(LinkOTPrimitives(OT_BIN_COUNT));
      DrawSync(0);
      VSync(0);
      srcY = DRAW_Y;
      rect.x = 0;
      if (g_pActiveFrameDrawEnv != &g_FrameDrawEnv0) {
        srcY = SCREEN_H + DRAW_Y;
      }
      rect.w = SCREEN_W;
      rect.y = srcY;
      rect.h = DRAW_H;
      /* copy this frame's drawing area into the other frame buffer */
      MoveImage(&rect, 0, SCREEN_H + 2 * DRAW_Y - srcY);
      DrawSync(0);
      g_FrameDrawEnv0.isbg = 0;
      g_FrameDrawEnv1.isbg = 0;
    } else if (g_nRespawnOrGameOverFrame < 0x10) {
      if (g_nRespawnOrGameOverFrame == 1) {
        DrawFullscreenTint(2, 0x10, 0x10, 0x10);
      } else {
        DrawFullscreenTint(2, 0x20, 0x20, 0x20);
      }
      DrawSync(0);
      VSync(0);
      PutDispEnv(&FRAME_DB(g_pActiveFrameDrawEnv)->disp);
      PutDrawEnv(g_pActiveFrameDrawEnv);
      DrawOTag(LinkOTPrimitives(OT_BIN_COUNT));
      if (g_nRespawnOrGameOverFrame == 0xF) {
        {
          unsigned char *e = Opaque(g_abFrameDrawEnvIsbgBlock);

          e[0] = 1;
          g_FrameDrawEnv1.isbg = 1;
          g_FrameDrawEnv0.r0 = 0;
          g_FrameDrawEnv0.g0 = 0;
          g_FrameDrawEnv0.b0 = 0;
          e[0x85] = 0;
          e[0x86] = 0;
          e[0x87] = 0;
        }
        DrawSync(0);
        VSync(0);
        rect.w = SCREEN_W;
        rect.x = 0;
        rect.y = 0;
        rect.h = SCREEN_H * 2;
        ClearImage(&rect, 0, 0, 0);
      }
    }
  } else if (g_nRespawnOrGameOverMode == 1) {
    if (g_nRespawnOrGameOverFrame >= 0x3C) {
      i = 0;
      ramp = g_anGameOverRevealScaleRamp;
      k = 0;
      do {
        d = g_abGameOverLetterDelay[i] + 0x3C;
        t = g_nRespawnOrGameOverFrame - d;
        if (t >= 0) {
          rec = (char *)g_pSpriteRecordWriteCursor - ACTOR_SIZE;
          g_pSpriteRecordWriteCursor = (Actor *)rec;
          FillWord(rec, 0, ACTOR_SIZE);
          if (t < 0x18) {
            g_pSpriteRecordWriteCursor->posZ = *(short *)(t * 2 + (int)ramp);
          } else {
            g_pSpriteRecordWriteCursor->posZ = 0x400;
          }
          g_pSpriteRecordWriteCursor->type =
              ACTOR_TYPE_LETTER_GLYPH(g_abGameOverGlyphs[i]);
          g_pSpriteRecordWriteCursor->posX = LETTER_POS(k)->x;
          g_pSpriteRecordWriteCursor->posY = LETTER_POS(k)->y;
          g_pSpriteRecordWriteCursor->rotZ = g_abGameOverLetterAngle[i];
          g_pSpriteRecordWriteCursor->otBias = OT_BIAS_DEFAULT;
          g_pSpriteRecordWriteCursor->tintMode = TINT_HUD;
          g_pSpriteRecordWriteCursor->cullRadius = CULL_RADIUS(0x2000);
        }
        i += 1;
        k += 4;
      } while (i < 8);
      if (g_nCdStreamState >= LSTREAM_READ_PATHS &&
          g_nRespawnOrGameOverFrame >= 0x1A5) {
        size[0] = 0x10;
        size[1] = 1;
        size[2] = TEXT_SMALL_DEPTH;
        pos[0] = 0xAE;
        pos[1] = 0xD0;
        pos[2] = TEXT_DEPTH;
        BuildTextSprites(g_szPressStart, pos, size, 0x12, TINT_HUD);
        i = 0;
        sbase = &g_anSineLut[0x40];
        sk = 0;
        sc = g_pSpriteRecordWriteCursor;
        do {
          i += 1;
          st = sbase[((g_nRespawnOrGameOverFrame * 4) + sk) & 0xFF];
          sk += 0xC;
          sc->rotZ = (st * 3) >> 9;
          sc++;
        } while (i < 0xA);
      }
      q = g_apSpritePrimQueue;
      q[0] = 0;
      EnqueuePendingSpritePrims();
      FillWord((char *)q - FRAME_ARENA_SPRITE_QUEUE, 0, FRAME_ARENA_LIST_WORDS);
      RasterizeSpritePrimQueue();
      if (g_nRespawnOrGameOverFrame >= 0xB5) {
        RasterizeSpyroModel();
      }
    }
    g_anCameraEulerPitchBlock[0] = g_anGameOverCameraEulerU[2];
    g_nCameraEulerYaw = g_anGameOverCameraEulerU[3];
    BuildCameraViewMatrix();
    EmitSkyMesh(-1, (MATRIX *)CAM_VIEW_MTX_VIA_EULER,
                (MATRIX *)CAM_W2C_MTX_VIA_EULER);
    g_anCameraEulerPitchBlock[0] = g_anGameOverCameraEulerU[0];
    g_nCameraEulerYaw = g_anGameOverCameraEulerU[1];
    if (g_nRespawnOrGameOverFrame < 0x20) {
      f = (0x1F - g_nRespawnOrGameOverFrame) * 8;
      DrawFullscreenTint(2, f, f, f);
    }
    DrawSync(0);
    VSync(0);
    PutDispEnv(&FRAME_DB(g_pActiveFrameDrawEnv)->disp);
    PutDrawEnv(g_pActiveFrameDrawEnv);
    DrawOTag(LinkOTPrimitives(OT_BIN_COUNT));
  } else {
    if (g_nRespawnOrGameOverFrame == 0) {
      srcY = DRAW_Y;
      rect.x = 0;
      if (g_pActiveFrameDrawEnv != &g_FrameDrawEnv0) {
        srcY = SCREEN_H + DRAW_Y;
      }
      rect.w = SCREEN_W;
      rect.y = srcY;
      rect.h = DRAW_H;
      /* copy this frame's drawing area into the other frame buffer */
      MoveImage(&rect, 0, SCREEN_H + 2 * DRAW_Y - srcY);
      DrawSync(0);
      g_FrameDrawEnv0.isbg = 0;
      g_FrameDrawEnv1.isbg = 0;
    } else if (g_nRespawnOrGameOverFrame < 0x10) {
      if (g_nRespawnOrGameOverFrame == 1) {
        DrawFullscreenTint(2, 0x10, 0x10, 0x10);
      } else {
        DrawFullscreenTint(2, 0x20, 0x20, 0x20);
      }
      DrawSync(0);
      VSync(0);
      PutDispEnv(&FRAME_DB(g_pActiveFrameDrawEnv)->disp);
      PutDrawEnv(g_pActiveFrameDrawEnv);
      DrawOTag(LinkOTPrimitives(OT_BIN_COUNT));
      if (g_nRespawnOrGameOverFrame == 0xF) {
        {
          unsigned char *e = Opaque(g_abFrameDrawEnvIsbgBlock);

          e[0] = 1;
          g_FrameDrawEnv1.isbg = 1;
          g_FrameDrawEnv0.r0 = 0;
          g_FrameDrawEnv0.g0 = 0;
          g_FrameDrawEnv0.b0 = 0;
          e[0x85] = 0;
          e[0x86] = 0;
          e[0x87] = 0;
        }
        DrawSync(0);
        VSync(0);
        rect.w = SCREEN_W;
        rect.x = 0;
        rect.y = 0;
        rect.h = SCREEN_H * 2;
        ClearImage(&rect, 0, 0, 0);
      }
    }
  }
}

#undef LETTER_POS

/* g_apSpritePrimQueue, reached from the held main-draw-list base. */
#define SPRITE_PRIM_QUEUE (&g_apMainDrawEntityListBlock[2304])

/* Draw for GS_DRAGON_RESCUE (0x8001cfdc, 0x73c).

   The dragon-rescue presentation, one arm per g_nDragonRescueSubstate. Substate
   0 is still an ordinary gameplay frame; 1-3 hand-build the main draw list from
   the rescuing actor (plus the statue once the statue arm is live) and sweep
   the actor pool for the 0xfb marker actors that go into the sprite queue; 4
   does the same around the statue + Spyro mirror pair and shows the name banner
   for the first second; 5 pins the statue and three HUD icon records, scaling
   the draw-list's two vector triples by the timer curve (and by a cosine sweep
   once the timer passes 8); 6 and the first sixteen frames of 7 draw only the
   icon records; the rest of 7 is the return-to-gameplay frame with the
   secondary actor pass. Every arm ends in the shared letterbox check and the
   standard 2-vblank frame submit. */

void DragonRescue_Draw(void) {
  int unused[4];
  unsigned char *icon;

  if (g_anDragonRescueAuraStarBlock[0] != 0) {
    ApplyEulerRotation((unsigned char *)&g_anDragonRescueAuraStarBlock[10],
                       &g_anDragonRescueAuraStarBlock[4],
                       g_anWorldToCameraRotMtx);
    DrawDragonRescueAuraStar();
  }

  if (g_nDragonRescueSubstate == DRAGON_RESCUE_AURA) {
    BuildRenderEntityLists();
    ComposeFrameScene();
    SetupFrameOT();
    DrawPortalsAndSky();
    RasterizeEmitList();
    {
      int tint = g_nDragonRescueFullscreenTintLevel;
      if (tint != 0) {
        DrawFullscreenTint(1, tint, tint, tint);
      }
    }
    if (g_nCinematicActive != 0 || g_nLetterboxBarHeight != 0) {
      DrawCinematicLetterbox();
    }
  } else if (g_nDragonRescueSubstate < DRAGON_RESCUE_CAMERA_PATH) {
    Actor *act;
    unsigned char id;
    void **drawList;
    void **q;
    drawList = g_apMainDrawEntityListBlock;
    if (((Actor *)g_pDragonRescueSourceActor)->mode < ACTOR_MODE_DORMANT) {
      *drawList++ = g_pDragonRescueSourceActor;
    }
    if (g_nDragonRescueSubstate == DRAGON_RESCUE_TURN) {
      *drawList++ = g_pDragonRescueStatueActor;
    }
    *drawList = 0;
    act = g_pActorListBase;
    q = SPRITE_PRIM_QUEUE;
    id = act->mode;
    while (id != ACTOR_MODE_END) {
      if (act->type == ACTOR_TYPE_STATUE_SHARD && id < ACTOR_MODE_DORMANT) {
        *q++ = act;
      }
      act++;
      id = act->mode;
    }
    *q = 0;
    DrawDragonRescuedBanner();
    EnqueuePendingSpritePrims();
    BuildActorDrawList();
    FillWord(&g_apMainDrawEntityList, 0, FRAME_ARENA_LIST_WORDS);
    EmitActorDrawList();
    RasterizeSpritePrimQueue();
    DrawActorDropShadows();
    RasterizeSpyroModel();
    DrawSpyroDropShadow();
    SetupFrameOT();
    DrawPortalsAndSky();
    RasterizeEmitList();
    {
      int tint = g_nDragonRescueFullscreenTintLevel;
      if (tint != 0) {
        DrawFullscreenTint(1, tint, tint, tint);
      }
    }
    if (g_nCinematicActive != 0 || g_nLetterboxBarHeight != 0) {
      DrawCinematicLetterbox();
    }
  } else if (g_nDragonRescueSubstate == DRAGON_RESCUE_CAMERA_PATH) {
    Actor *act;
    unsigned char id;
    void **drawList;
    void **q;
    drawList = g_apMainDrawEntityListBlock;
    *drawList++ = g_pDragonRescueStatueActor;
    *drawList++ = g_pDragonRescueSpyroMirrorActor;
    *drawList = 0;
    q = SPRITE_PRIM_QUEUE;
    act = g_pActorListBase;
    id = act->mode;
    while (id != ACTOR_MODE_END) {
      if (act->type == ACTOR_TYPE_STATUE_SHARD && id < ACTOR_MODE_DORMANT) {
        *q++ = act;
      }
      act++;
      id = act->mode;
    }
    *q = 0;
    if (g_nDragonRescueTimer < RESCUE_BANNER_FRAMES) {
      DrawDragonRescuedBanner();
      EnqueuePendingSpritePrims();
    }
    BuildActorDrawList();
    FillWord(&g_apMainDrawEntityList, 0, FRAME_ARENA_LIST_WORDS);
    EmitActorDrawList();
    RasterizeSpritePrimQueue();
    DrawActorDropShadows();
    DrawSpyroDropShadow();
    SetupFrameOT();
    DrawPortalsAndSky();
    RasterizeEmitList();
    if (g_nCinematicActive != 0 || g_nLetterboxBarHeight != 0) {
      DrawCinematicLetterbox();
    }
  } else if (g_nDragonRescueSubstate == DRAGON_RESCUE_LIGHT_UP) {
    void **drawList;
    void **q;
    int i;
    drawList = g_apMainDrawEntityListBlock;
    i = 0;
    *drawList++ = g_pDragonRescueStatueActor;
    *drawList = 0;
    q = SPRITE_PRIM_QUEUE;
    icon = (unsigned char *)HUD_ICON_SLOT(HUD_SLOT_DRAGON_DIGITS);
  icon5:
    *q++ = icon;
    icon += ACTOR_SIZE;
    i += 1;
    if (i < HUD_DRAGON_SLOTS) {
      goto icon5;
    }
    *q = 0;
    BuildActorDrawList();
    if (g_anActorDrawList[0] != 0) {
      short *dl = (short *)g_anActorDrawList;
      int scale;
      scale =
          g_anDragonRescueScaleCurve[*(volatile int *)&g_nDragonRescueTimer];
      dl[0x0E] = (dl[0x0E] * scale) >> 12;
      dl[0x0F] = (dl[0x0F] * scale) >> 12;
      dl[0x10] = (dl[0x10] * scale) >> 12;
      if (g_nDragonRescueTimer >= 9) {
        scale = LookupCosine((g_nDragonRescueTimer - 8) * 42);
        dl[0x11] = (dl[0x11] * scale) >> 12;
        dl[0x12] = (dl[0x12] * scale) >> 12;
        dl[0x13] = (dl[0x13] * scale) >> 12;
      }
    }
    FillWord(&g_apMainDrawEntityList, 0, FRAME_ARENA_LIST_WORDS);
    EmitActorDrawList();
    RasterizeSpritePrimQueue();
    RasterizeSpyroModel();
    DrawSpyroDropShadow();
    SetupFrameOT();
    DrawPortalsAndSky();
    RasterizeEmitList();
    if (g_nCinematicActive != 0 || g_nLetterboxBarHeight != 0) {
      DrawCinematicLetterbox();
    }
  } else if (g_nDragonRescueSubstate == DRAGON_RESCUE_COUNT) {
    unsigned char *icon;
    void **q;
    int i;
    q = SPRITE_PRIM_QUEUE;
    i = 0;
    icon = (unsigned char *)HUD_ICON_SLOT(HUD_SLOT_DRAGON_DIGITS);
  icon6:
    *q++ = icon;
    icon += ACTOR_SIZE;
    i += 1;
    if (i < HUD_DRAGON_SLOTS) {
      goto icon6;
    }
    *q = 0;
    FillWord(&g_apMainDrawEntityList, 0, FRAME_ARENA_LIST_WORDS);
    RasterizeSpritePrimQueue();
    RasterizeSpyroModel();
    DrawSpyroDropShadow();
    SetupFrameOT();
    DrawPortalsAndSky();
    RasterizeEmitList();
    if (g_nCinematicActive != 0 || g_nLetterboxBarHeight != 0) {
      DrawCinematicLetterbox();
    }
  } else if (g_nDragonRescueSubstate == DRAGON_RESCUE_FADE_OUT) {
    if (g_nDragonRescueTimer < RESCUE_FADE_FRAMES / 2) {
      int i;
      void **q;
      i = 0;
      q = SPRITE_PRIM_QUEUE;
      icon = (unsigned char *)HUD_ICON_SLOT(HUD_SLOT_DRAGON_DIGITS);
    icon7:
      *q++ = icon;
      icon += ACTOR_SIZE;
      i += 1;
      if (i < HUD_DRAGON_SLOTS) {
        goto icon7;
      }
      *q = 0;
      FillWord(&g_apMainDrawEntityList, 0, FRAME_ARENA_LIST_WORDS);
      RasterizeSpritePrimQueue();
    } else {
      BuildRenderEntityLists();
      BuildActorDrawList();
      FillWord(&g_apMainDrawEntityList, 0, FRAME_ARENA_LIST_WORDS);
      EmitActorDrawList();
      BuildSecondaryActorDrawList();
      EmitSecondaryActorPrimitives();
      RasterizeSpritePrimQueue();
      DrawActorDropShadows();
    }
    RasterizeSpyroModel();
    DrawSpyroDropShadow();
    SetupFrameOT();
    DrawPortalsAndSky();
    RasterizeEmitList();
    if (g_nCinematicActive != 0 || g_nLetterboxBarHeight != 0) {
      DrawCinematicLetterbox();
    }
    {
      int tint = g_nDragonRescueFullscreenTintLevel;
      if (tint != 0) {
        DrawFullscreenTint(1, tint, tint, tint);
      }
    }
  }

  DrawSync(0);
  if (g_nDemoActive != 0) {
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
  PutDispEnv(&FRAME_DB(g_pActiveFrameDrawEnv)->disp);
  PutDrawEnv(g_pActiveFrameDrawEnv);
  DrawOTag(LinkOTPrimitives(OT_BIN_COUNT));
}

#undef SPRITE_PRIM_QUEUE

#define SM g_anSaveMenuBlock

/* Rocks the glyphs of the highlighted line on a cosine wave that travels
   along it: each glyph's roll angle (rotZ, +0x46 of its 0x58-byte sprite
   record), which RasterizeSpritePrimQueue turns into the sprite's rotation. */
#define WAVE(rec, n, phase)                                                    \
  for (i = 0; i < (n); i++) {                                                  \
    (rec)[0x46] = (g_anCosineLut[((phase) * 4 + i * 0xC) & 0xFF] * 3) >> 9;    \
    (rec) += ACTOR_SIZE;                                                       \
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
    g_pSpriteRecordBufferTop =
        (Actor *)((char *)g_pPrimBufferWriteCursor + PRIM_BUFFER_BYTES -
                  SAVE_IMAGE_BYTES); /* keep the card buffer clear */
    g_pSpriteRecordWriteCursor = g_pSpriteRecordBufferTop;
  }
  if (g_nSaveMenuMode == SAVE_MODE_MENU) {
    DrawShadedMenuBox(
        g_nSaveMenuXOffset + g_aSaveMenuBoxes[g_nSaveMenuSubstate].x,
        g_nSaveMenuXOffset + g_aSaveMenuBoxes[g_nSaveMenuSubstate].y,
        g_aSaveMenuBoxes[g_nSaveMenuSubstate].w,
        g_aSaveMenuBoxes[g_nSaveMenuSubstate].h);
    size[0] = 0x10;
    size[2] = TEXT_SMALL_DEPTH;
    size[1] = 1;
    pos[2] = TEXT_DEPTH;
    switch (g_nSaveMenuSubstate) {
    case SAVE_SCREEN_MAIN:
      pos[1] = 0x32;
      pos[0] = SM[SM_X_OFFSET] + 0x40;
      BuildTextSprites(g_szHiSpyro, pos, size, 0x12, TINT_HUD);
      pos[1] += 0x18;
      pos[0] = SM[SM_X_OFFSET] + 0x4C;
      BuildTextSprites(g_szSaveGame, pos, size, 0x12, TINT_HUD);
      pos[1] += 0x13;
      if (SM[SM_CURSOR] == SAVE_ROW_SAVE) {
        rec = (unsigned char *)g_pSpriteRecordWriteCursor;
        n = 8;
      }
      pos[0] = SM[SM_X_OFFSET] + 0x4C;
      BuildTextSprites(g_szReplayDragon, pos, size, 0x12, TINT_HUD);
      pos[1] += 0x13;
      if (SM[SM_CURSOR] == SAVE_ROW_REPLAY_DRAGON) {
        rec = (unsigned char *)g_pSpriteRecordWriteCursor;
        n = 0xC;
      }
      pos[0] = SM[SM_X_OFFSET] + 0x4C;
      BuildTextSprites(g_szContinue, pos, size, 0x12, TINT_HUD);
      if (SM[SM_CURSOR] == SAVE_ROW_CONTINUE) {
        rec = (unsigned char *)g_pSpriteRecordWriteCursor;
        n = 8;
      }
      WAVE(rec, n, SM[SM_PHASE]);
      break;
    case SAVE_SCREEN_NO_SESSION:
      pos[1] = 0x32;
      pos[0] = SM[SM_X_OFFSET] + 0x52;
      BuildTextSprites(g_szNoSaveFile, pos, size, 0x12, TINT_HUD);
      rec = (unsigned char *)g_pSpriteRecordWriteCursor;
      pos[1] += 0x1A;
      WAVE(rec, 10, SM[SM_PHASE]);
      size[0] = 0xE;
      size[1] = 1;
      size[2] = TEXT_TINY_DEPTH;
      pos[2] = TEXT_SMALL_DEPTH;
      pos[0] = SM[SM_X_OFFSET] + 0x2D;
      BuildTextSprites(g_szPleaseRestartWith, pos, size, 0x10, TINT_HUD);
      pos[1] += 0x11;
      pos[0] = SM[SM_X_OFFSET] + 0x43;
      BuildTextSprites(g_szAMemoryCardTo, pos, size, 0x10, TINT_HUD);
      pos[1] += 0x11;
      pos[0] = SM[SM_X_OFFSET] + 0x38;
      BuildTextSprites(g_szEnableGameSaves, pos, size, 0x10, TINT_HUD);
      break;
    case SAVE_SCREEN_SAVING:
      pos[1] = 0x52;
      pos[0] = SM[SM_X_OFFSET] + 0x6A;
      BuildTextSprites(g_szSaving, pos, size, 0x12, TINT_HUD);
      rec = (unsigned char *)g_pSpriteRecordWriteCursor;
      WAVE(rec, 9, SM[SM_PHASE]);
      break;
    case SAVE_SCREEN_NO_CARD:
    case SAVE_SCREEN_NO_FILE:
      pos[1] = 0x32;
      if (g_nSaveMenuSubstate == SAVE_SCREEN_NO_CARD) {
        pos[0] = g_nSaveMenuXOffset + 0x42;
        BuildTextSprites(g_szNoMemoryCard, pos, size, 0x12, TINT_HUD);
        n = 0xC;
      } else {
        pos[0] = g_nSaveMenuXOffset + 0x54;
        BuildTextSprites(g_szNoSaveFile, pos, size, 0x12, TINT_HUD);
        n = 0xA;
      }
      rec = (unsigned char *)g_pSpriteRecordWriteCursor;
      pos[1] += 0x1A;
      WAVE(rec, n, SM[SM_PHASE]);
      size[0] = 0xE;
      size[1] = 1;
      size[2] = TEXT_TINY_DEPTH;
      pos[2] = TEXT_SMALL_DEPTH;
      pos[0] = SM[SM_X_OFFSET] + 0x3B;
      BuildTextSprites(g_szPleaseInsertThe, pos, size, 0x10, TINT_HUD);
      pos[1] += 0x11;
      pos[0] = SM[SM_X_OFFSET] + 0x43;
      BuildTextSprites(g_szMemoryCardWith, pos, size, 0x10, TINT_HUD);
      pos[1] += 0x11;
      pos[0] = SM[SM_X_OFFSET] + 0x2D;
      BuildTextSprites(g_szThisGameSaveFile, pos, size, 0x10, TINT_HUD);
      break;
    case SAVE_SCREEN_ERROR:
      pos[1] = 0x32;
      pos[0] = SM[SM_X_OFFSET] + 0x5E;
      BuildTextSprites(g_szSaveError, pos, size, 0x12, TINT_HUD);
      rec = (unsigned char *)g_pSpriteRecordWriteCursor;
      pos[1] += 0x1A;
      WAVE(rec, 9, SM[SM_PHASE]);
      size[0] = 0xE;
      size[1] = 1;
      size[2] = TEXT_TINY_DEPTH;
      pos[2] = TEXT_SMALL_DEPTH;
      pos[0] = SM[SM_X_OFFSET] + 0x3B;
      BuildTextSprites(g_szPleaseCheckThat, pos, size, 0x10, TINT_HUD);
      pos[1] += 0x11;
      pos[0] = SM[SM_X_OFFSET] + 0x49;
      BuildTextSprites(g_szTheMemoryCard, pos, size, 0x10, TINT_HUD);
      pos[1] += 0x11;
      pos[0] = SM[SM_X_OFFSET] + 0x36;
      BuildTextSprites(g_szIsStillInPlace, pos, size, 0x10, TINT_HUD);
      break;
    case SAVE_SCREEN_FAILED:
      pos[1] = 0x35;
      pos[0] = SM[SM_X_OFFSET] + 0x56;
      BuildTextSprites(g_szSaveFailed, pos, size, 0x12, TINT_HUD);
      pos[1] += 0x28;
      pos[0] = SM[SM_X_OFFSET] + 0x87;
      BuildTextSprites(g_szUiRetry, pos, size, 0x12, TINT_HUD);
      pos[1] += 0x13;
      if (SM[SM_CURSOR] == SAVE_FAILED_RETRY) {
        rec = (unsigned char *)g_pSpriteRecordWriteCursor;
        n = 5;
      }
      pos[0] = SM[SM_X_OFFSET] + 0x87;
      BuildTextSprites(g_szUiAbort, pos, size, 0x12, TINT_HUD);
      if (SM[SM_CURSOR] == SAVE_FAILED_ABORT) {
        rec = (unsigned char *)g_pSpriteRecordWriteCursor;
        n = 5;
      }
      WAVE(rec, n, SM[SM_PHASE]);
      break;
    case SAVE_SCREEN_SAVED:
      pos[1] = 0x52;
      pos[0] = SM[SM_X_OFFSET] + 0x5E;
      BuildTextSprites(g_szGameSaved, pos, size, 0x12, TINT_HUD);
      rec = (unsigned char *)g_pSpriteRecordWriteCursor;
      WAVE(rec, 9, SM[SM_PHASE]);
      break;
    }
    if (g_nSaveMenuSubstate >= SAVE_SCREEN_SAVING) {
      size[0] = 0xD;
      size[1] = 1;
      size[2] = 0x1A00;
      pos[1] = 0x1D;
      pos[2] = TEXT_TINY_DEPTH;
      if (g_nSaveSlotSelected == 0) {
        pos[0] = g_nSaveMenuXOffset + 0x30;
        BuildTextSprites(g_szUiSlot1, pos, size, 0xE, TINT_HUD);
      } else {
        pos[0] = 0xE6;
        BuildTextSprites(g_szUiSlot2, pos, size, 0xE, TINT_HUD);
      }
    }
  }
  BuildRenderEntityLists();
  ComposeFrameScene();
  if (g_nSaveMenuMode == SAVE_MODE_MENU) {
    g_apSpritePrimQueue[0] = 0;
    EnqueuePendingSpritePrims();
    RasterizeSpritePrimQueue();
  }
  SetupFrameOT();
  DrawPortalsAndSky();
  RasterizeEmitList();
  DrawCinematicLetterbox();
  DrawSync(0);
  if (g_nDemoActive != 0) {
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
  PutDispEnv(&FRAME_DB(g_pActiveFrameDrawEnv)->disp);
  PutDrawEnv(g_pActiveFrameDrawEnv);
  DrawOTag(LinkOTPrimitives(OT_BIN_COUNT));
}

#undef SM
#undef WAVE

/* Draw handler for GS_ACTOR_CUTSCENE — the level-transition title card
   (0x8001E24C, 0x46c).

   Only substates 4 and 5 draw the card; every other substate hands the frame
   to the handler installed at g_pfnActorCutsceneSubstateDraw. The card text is
   "<world>" for the first five hub arrivals, and "<world> <level>" past that,
   indexed out of g_apWorldNames by g_nNextLevelId / 10 (world); the level
   form indexes [world * 6 + level + 2], which runs past the eight world names
   into g_apLevelNameStrings[(world - 1) * 6 + level]. The string
   is centred by its glyph count (7px each) and handed to BuildTextSprites; the
   emitted glyph records then rock on the cosine LUT (their rotZ), one
   0x58-byte record at a time, walking back down the sprite-record queue.

   Substates below 6 also cross-fade the world fog toward the actor mesh fade
   color: the fade parameter runs g_nActorCutsceneTimer*0x10 on the way in
   (substate 4) and (0x100 - g_nActorCutsceneTimer)*0x10 on the way out, clamped
   to 0..0x1000, and the fogged static mesh is re-emitted until the fade
   completes. Ends with the standard 2-vblank frame submit.

   The main draw list pair is written through an asm-opaque copy of the block
   address so cse keeps it as a held base; the empty volatile asm keeps the
   g_pActorCutsceneRecord load ahead of it.
*/

void ActorCutscene_Draw(void) {
  int pos[3];
  int attr[3];
  char buf[64];
  int color;
  int world;
  int level;
  int phase;
  int variant;
  Actor *rec;

  if (g_nActorCutsceneSubstate >= ACS_STREAM_WAIT) {
    if (g_nActorCutsceneSubstate < ACS_DESCEND) {
      {
        register void **drawList asm("$3");
        register void *h asm("$2");

        h = g_pActorCutsceneRecord;
        KEEP_IN_REG(h);
        OPAQUE_COPY(drawList, g_apMainDrawEntityListBlock);
        drawList[1] = 0;
        drawList[0] = h;
      }
      EBB_BREAK();

      variant = *(volatile int *)&g_nActorCutsceneCardVariant;
      if (variant < 5) {
        sprintf(buf, g_szEnteringWorldFmt, g_apWorldNames[g_nNextLevelId / 10]);
      } else if (variant == 5) {
        sprintf(buf, g_szEnteringFmt, g_apWorldNames[g_nNextLevelId / 10]);
      } else {
        world = g_nNextLevelId / 10;
        level = g_nNextLevelId - world * 10;
        sprintf(buf, g_szEnteringFmt, g_apWorldNames[world * 6 + level + 2]);
      }

      pos[0] = 0x100 - (strlen(buf) - 1) * 7;
      pos[1] = 0xC8;
      pos[2] = TEXT_SMALL_DEPTH;
      attr[0] = 0xE;
      attr[1] = 1;
      attr[2] = TEXT_TINY_DEPTH;
      rec = g_pSpriteRecordWriteCursor;
      BuildTextSprites(buf, pos, attr, 0x10, TINT_GOLD);

      rec--;
      phase = 0;
      if ((int)rec >= (int)g_pSpriteRecordWriteCursor) {
        int *fade = g_anActorCutsceneTimerBlock;
        unsigned short *cos = (unsigned short *)g_anCosineLut;
        do {
          rec->rotZ = cos[(*fade * 2 + phase) & ANGLE8_MASK] >> 7;
          rec--;
          phase += 0xC;
        } while ((int)rec >= (int)g_pSpriteRecordWriteCursor);
      }

      BuildActorDrawList();
      FillWord(g_apMainDrawEntityListBlock, 0, FRAME_ARENA_LIST_WORDS);
      EmitActorDrawList();
      g_apSpritePrimQueue[0] = 0;
      EnqueuePendingSpritePrims();
      RasterizeSpritePrimQueue();
      RasterizeSpyroModel();

      if (g_nActorCutsceneSubstate < ACS_DESCEND) {
        g_dwSkyFadeColor = 0x401010;
        if (g_nActorCutsceneSubstate == ACS_STREAM_WAIT) {
          g_nFadeProgress = g_anActorCutsceneTimerBlock[ACSW_TIMER] * 0x10;
        } else {
          g_nFadeProgress =
              (0x100 - g_anActorCutsceneTimerBlock[ACSW_TIMER]) * 0x10;
        }
        if (g_nFadeProgress < 0) {
          *(volatile int *)&g_nFadeProgress = 0;
        }
        if (g_nFadeProgress > 0x1000) {
          g_nFadeProgress = 0x1000;
        }
        color = ComputeDepthCuedColor(g_dwWorldFogColor, g_dwSkyFadeColor,
                                      g_nFadeProgress);
        g_FrameDrawEnv0.r0 = ((unsigned char *)&color)[0];
        g_FrameDrawEnv0.g0 = ((unsigned char *)&color)[1];
        g_FrameDrawEnv0.b0 = ((unsigned char *)&color)[2];
        g_FrameDrawEnv1.r0 = ((unsigned char *)&color)[0];
        g_FrameDrawEnv1.g0 = ((unsigned char *)&color)[1];
        g_FrameDrawEnv1.b0 = ((unsigned char *)&color)[2];
        if (g_nFadeProgress < 0x1000) {
          EmitSkyMeshFogged();
        }
      }
    } else {
      g_pfnActorCutsceneSubstateDraw();
    }
  } else {
    g_pfnActorCutsceneSubstateDraw();
  }

  DrawCinematicLetterbox();
  DrawSync(0);
  if (g_nDemoActive != 0) {
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
  PutDispEnv(&FRAME_DB(g_pActiveFrameDrawEnv)->disp);
  PutDrawEnv(g_pActiveFrameDrawEnv);
  DrawOTag(LinkOTPrimitives(OT_BIN_COUNT));
}

/* 0x8001e6b8: Draw routine for the story intro (GS_TITLE). While substate
 * 2 is past frame 0x8C it lays out one of four message strings with
 * BuildTextSprites, rewinds the sprite-record cursor by the number of glyph
 * slots the message still has to reveal, then runs a per-glyph wave: each
 * record's rotZ byte (its roll angle) ramps from 0x40 by 8 a frame for the
 * first 0x38 frames of its life, so the glyph spins in and lands upright
 * (0x200), and rocks on the cosine LUT after that. Then the standard sprite
 * flush and the simple (no pace loop) gamestate frame-submit tail. */

/* Held-base view of g_nStoryIntroTimer: the original keeps `&timer` in a
   register across the fade-threshold test and the whole glyph loop, which a
   4-byte `extern int` cannot do (sdata cost 1 wins the tie, §B-i rule 2). */

void StoryIntro_Draw(void) {
  int pos[4];
  int col[3];
  int rec;
  int roll;
  int limit;
  int i;
  int phase;
  int t;
  int age;
  int born;
  int *tp;
  unsigned short *cosp;

  if (g_nStoryIntroSubstate == STORY_INTRO_FLY && g_nStoryIntroTimer >= 0x8C) {
    pos[1] = 0x78;
    pos[2] = TEXT_SMALL_DEPTH;
    rec = (int)g_pSpriteRecordWriteCursor;
    col[0] = 0xE;
    col[1] = 1;
    col[2] = TEXT_TINY_DEPTH;
    if (g_nTitleExitPath == TITLE_EXIT_NEW_GAME) {
      pos[0] = 0x5C;
      BuildTextSprites(g_szInTheWorldOfDragons, pos, col, 0x10, TINT_HUD);
      limit = 0xB8;
    } else if (g_nTitleExitPath == TITLE_EXIT_LOADED_GAME) {
      if (g_abLevelVisitedFlag[LEVEL_SLOT(LEVEL_ARTISANS)] != 0) {
        pos[0] = 0x50;
        BuildTextSprites(g_szTheAdventureContinues, pos, col, 0x10, TINT_HUD);
        limit = 0xBC;
      } else {
        pos[0] = 0x64;
        BuildTextSprites(g_szTheAdventureBegins, pos, col, 0x10, TINT_HUD);
        limit = 0xB6;
      }
    } else {
      pos[0] = 0x68;
      BuildTextSprites(g_szEnteringDemoMode, pos, col, 0x10, TINT_HUD);
      limit = 0xB2;
    }

    if (g_anStoryIntroTimerBlock[0] < limit) {
      g_pSpriteRecordWriteCursor += (limit - g_anStoryIntroTimerBlock[0]) >> 1;
    }

    rec -= ACTOR_SIZE;
    if (rec >= (int)g_pSpriteRecordWriteCursor) {
      i = 0;
      tp = g_anStoryIntroTimerBlock;
      cosp = (unsigned short *)g_anCosineLut;
      roll = rec + 0x46;
      phase = 0;
      do {
        t = tp[0];
        born = i + 0x8C;
        age = t - born;
        if (age < 0x38) {
          *(char *)roll = (char)((age << 3) + 0x40);
        } else {
          *(char *)roll = (char)(cosp[((t << 2) + phase) & 0xFF] >> 7);
        }
        phase += 0xC;
        i += 1;
        roll -= 0x58;
        rec -= ACTOR_SIZE;
      } while (rec >= (int)g_pSpriteRecordWriteCursor);
    }
  }

  g_FrameDrawEnv0.r0 = 0;
  g_FrameDrawEnv0.g0 = 0;
  g_FrameDrawEnv0.b0 = 0;
  g_FrameDrawEnv1.r0 = 0;
  g_FrameDrawEnv1.g0 = 0;
  g_FrameDrawEnv1.b0 = 0;
  g_apSpritePrimQueue[0] = 0;
  EnqueuePendingSpritePrims();
  FillWord(&g_apSpritePrimQueue[-0x900], 0,
           0x900); /* 0x900 bytes from g_apMainDrawEntityList */
  RasterizeSpritePrimQueue();

  if (g_nStoryIntroSubstate == STORY_INTRO_FLY) {
    RasterizeSpyroModel();
    ((int *)g_pOtDepthBinArrayBase)[0x40] = g_pOtActiveDepthSlot[0];
    ((int *)g_pOtDepthBinArrayBase)[0x41] = g_pOtActiveDepthSlot[1];
    g_pOtActiveDepthSlot[0] = 0;
    g_pOtActiveDepthSlot[1] = 0;
  }

  DrawSync(0);
  VSync(0);
  PutDispEnv(&FRAME_DB(g_pActiveFrameDrawEnv)->disp);
  PutDrawEnv(g_pActiveFrameDrawEnv);
  DrawOTag(LinkOTPrimitives(OT_BIN_COUNT));
}

/* Four levers, in the order they paid (A93 is the whole preheader story):
 *  - the sprite-record cursor comparisons are SIGNED (`slt`), so `rec` is an
 *    `int`, not a `char *` — a pointer compare emits `sltu`;
 *  - `age = t - born` with `born = i + 0x8C` computed first: written as
 *    `t - (i + 0x8C)` gcc reassociates to `(t - 0x8C) - i`, which also costs
 *    the load-delay nop the `addiu` should fill;
 *  - the two decrements at the loop bottom go `roll` then `rec`: with `rec`
 *    first the cursor re-load loses its delay-slot filler (+1 insn);
 *  - the preheader is A93 exactly. The original's order is
 *    [i=0][tp copy][cos base][roll][phase], and loop.c-hoisted invariants are
 *    inserted just before loop_start, i.e. AFTER every explicit assignment. So
 *    BOTH bases have to be explicit pre-loop locals declared in that order:
 *    left to loop.c they land last, `roll` is defined while the timer base is
 *    still live, the two allocnos conflict (i.i.greg: `111 conflicts ... 73`),
 *    and the base is pushed off a1 onto t0 — which is the whole 8-insn residue
 *    this park sat on. `tp` also has to be assigned INSIDE the loop's guarded
 *    block: hoisting it above the fade test costs 100 of 196 insns. */

/* Draw GS_CUTSCENE: the scripted in-engine cutscene with
   a g_nScreenFadeLevel fade-to-black. Copies the world fog color into both
   frame DRAWENVs, renders the actor + world passes, tints, then the standard
   2-vblank frame submit (0x8001e9c8). */
void Cutscene_Draw(void) {
  int r;
  int g;
  int b;
  int *ot;
  void *otHead;

  r = ((unsigned char *)&g_dwWorldFogColor)[0];
  g = ((unsigned char *)&g_dwWorldFogColor)[1];
  b = ((unsigned char *)&g_dwWorldFogColor)[2];
  g_FrameDrawEnv0.r0 = r;
  g_FrameDrawEnv0.g0 = g;
  g_FrameDrawEnv0.b0 = b;
  g_FrameDrawEnv1.r0 = r;
  g_FrameDrawEnv1.g0 = g;
  g_FrameDrawEnv1.b0 = b;
  BuildRenderEntityLists();
  BuildActorDrawList();
  otHead = &g_apMainDrawEntityList;
  FillWord(otHead, 0, FRAME_ARENA_LIST_WORDS);
  EmitActorDrawList();
  FillWord(otHead, 0, 0x1c00);
  g_nFarPlaneDrawDistance = 0x14000;
  RenderWorldChunks(-1);
  EmitSkyMesh(-1, (MATRIX *)g_anCameraViewMtx, (MATRIX *)CAM_W2C_MTX_VIA_VIEW);
  if (g_nScreenFadeLevel != 0) {
    int t = g_nScreenFadeLevel << 4;
    DrawFullscreenTint(2, t, t, t);
  }
  DrawSync(0);
  if (g_nDemoActive != 0) {
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
  PutDispEnv(&FRAME_DB(g_pActiveFrameDrawEnv)->disp);
  PutDrawEnv(g_pActiveFrameDrawEnv);
  ot = (int *)LinkOTPrimitives(OT_BIN_COUNT);
  DrawOTag(ot);
}

/* Draw GS_CREDITS's post-credits "RETURNING HOME..." card with the animated
   per-glyph brightness wave (g_anCosineLut stepped by g_nReturningHomePhase),
   then the standard sprite-queue rasterize + frame-flush tail (0x8001eb80). */
void ReturningHome_Draw(void) {
  Actor *first;
  void **queue;
  int phase;
  Actor *glyph;
  int sizeVec[3];
  int posVec[3];

  first = g_pSpriteRecordWriteCursor;
  sizeVec[0] = 0x10;
  sizeVec[1] = 1;
  sizeVec[2] = TEXT_SMALL_DEPTH;
  posVec[0] = 0x76;
  posVec[1] = 0x6e;
  posVec[2] = TEXT_DEPTH;
  g_FrameDrawEnv0.r0 = 0;
  g_FrameDrawEnv0.g0 = 0;
  g_FrameDrawEnv0.b0 = 0;
  g_FrameDrawEnv1.r0 = 0;
  g_FrameDrawEnv1.g0 = 0;
  g_FrameDrawEnv1.b0 = 0;
  BuildTextSprites(g_szReturningHome, posVec, sizeVec, 0x12, TINT_HUD);
  glyph = first - 1;
  phase = 0;
  if ((int)g_pSpriteRecordWriteCursor <= (int)glyph) {
    do {
      glyph->rotZ =
          ((unsigned short *)
               g_anCosineLut)[(g_nReturningHomePhase * 2 + phase) & 0xff] >>
          7;
      glyph--;
      phase += 0xc;
    } while ((int)g_pSpriteRecordWriteCursor <= (int)glyph);
  }
  queue = g_apSpritePrimQueue;
  queue[0] = 0;
  EnqueuePendingSpritePrims();
  FillWord((char *)queue - FRAME_ARENA_SPRITE_QUEUE, 0, FRAME_ARENA_LIST_WORDS);
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
  PutDispEnv(&FRAME_DB(g_pActiveFrameDrawEnv)->disp);
  PutDrawEnv(g_pActiveFrameDrawEnv);
  DrawOTag(LinkOTPrimitives(OT_BIN_COUNT));
}

/* Per-frame top-level draw dispatcher (0x8001ED5C, 0x3fc).

   Flips to the other of the two frame environments (they sit 0x84 apart, so
   the second is derived from the first) and republishes the per-frame buffer
   cursors held in its FrameDB tail: the prim-buffer write cursor
   (primBufferBase) with its PRIM_BUFFER_BYTES limit, the OT depth-bin array
   (otDepthBinBase) and the active depth slot (otMergedChainSlot). The
   sprite-record buffer starts where the prim buffer ends. Then the camera view
   matrix, and the active gamestate's Draw handler.

   State 0 is the gameplay frame: push the world fog color into both frame
   DRAWENVs, build the render entity lists, overlay the loading-screen sprites
   (skipped in flight levels) and the demo-mode overlay, compose the scene,
   then the fullscreen tint while the generic countdown runs and the cinematic
   letterbox while gameplay is blocked. Ends with the standard 2-vblank frame
   submit. */

void GamestateDraw(void) {
  char *env;
  void *otbase;
  int *otslot;
  char *cursor;
  char *limit;
  int r;
  int g;
  int b;
  int tint;

  env = (char *)&g_FrameDrawEnv0;
  if ((char *)g_pActiveFrameDrawEnv == env) {
    env += sizeof(FrameDB);
  }
  otbase = FRAME_DB(env)->otDepthBinBase;
  otslot = FRAME_DB(env)->otMergedChainSlot;
  cursor = FRAME_DB(env)->primBufferBase;
  g_pActiveFrameDrawEnv = (DRAWENV *)env;
  g_nPrimBufferOverflowFlag = 0;
  g_pPrimBufferWriteCursor = cursor;
  limit = cursor + PRIM_BUFFER_BYTES;
  g_pOtDepthBinArrayBase = otbase;
  g_pOtActiveDepthSlot = otslot;
  g_pPrimBufferLimit = limit;
  g_pSpriteRecordBufferTop = (Actor *)limit;
  g_pSpriteRecordWriteCursor = (Actor *)limit;
  BuildCameraViewMatrix();

  if (g_nGamestate != GS_GAMEPLAY) {
    if (g_nGamestate == GS_LEVEL_LOAD) {
      LevelTransition_Draw();
    } else if (g_nGamestate == GS_PAUSE_MENU) {
      PauseMenu_Draw();
    } else if (g_nGamestate == GS_INVENTORY) {
      PauseMenu_Draw();
    } else if (g_nGamestate == GS_RESPAWN) {
      RespawnOrGameOver_Draw();
    } else if (g_nGamestate == GS_GAME_OVER) {
      RespawnOrGameOver_Draw();
    } else if (g_nGamestate == GS_UNUSED_6) {
      PauseMenu_Draw();
    } else if (g_nGamestate == GS_FLIGHT_RESULTS) {
      g_pfnFlightResultsDraw();
    } else if (g_nGamestate == GS_DRAGON_RESCUE) {
      DragonRescue_Draw();
    } else if (g_nGamestate == GS_ARRIVAL_CAMERA) {
      LevelTransition_Draw();
    } else if (g_nGamestate == GS_MENU_TRANSITION) {
      MenuTransition_Draw();
    } else if (g_nGamestate == GS_SAVE_MENU) {
      SaveLoadMenu_Draw();
    } else if (g_nGamestate == GS_ACTOR_CUTSCENE) {
      ActorCutscene_Draw();
    } else if (g_nGamestate == GS_TITLE) {
      if (g_nTitleMode == TITLE_MODE_STORY_INTRO) {
        StoryIntro_Draw();
      } else {
        Titlescreen_Draw();
      }
    } else if (g_nGamestate == GS_CUTSCENE) {
      Cutscene_Draw();
    } else if (g_nGamestate == GS_CREDITS) {
      if (g_nCreditsMode < CREDITS_MODE_RETURN_HOME) {
        Credits_Draw();
      } else {
        ReturningHome_Draw();
      }
    }
  } else {
    r = ((unsigned char *)&g_dwWorldFogColor)[0];
    g = ((unsigned char *)&g_dwWorldFogColor)[1];
    b = ((unsigned char *)&g_dwWorldFogColor)[2];
    g_FrameDrawEnv0.r0 = r;
    g_FrameDrawEnv0.g0 = g;
    g_FrameDrawEnv0.b0 = b;
    g_FrameDrawEnv1.r0 = r;
    g_FrameDrawEnv1.g0 = g;
    g_FrameDrawEnv1.b0 = b;
    BuildRenderEntityLists();
    if (g_nFlightLevelActive == 0) {
      EnqueueHudSprites();
    }
    if (g_nDemoMode != DEMO_OFF) {
      DrawDemoModeOverlay();
    }
    ComposeFrameScene();
    SetupFrameOT();
    DrawPortalsAndSky();
    RasterizeEmitList();
    if (g_nScreenFadeLevel != 0) {
      tint = g_nScreenFadeLevel << 3;
      DrawFullscreenTint(2, tint, tint, tint);
    }
    if (g_nCinematicActive != 0 || g_nLetterboxBarHeight != 0) {
      DrawCinematicLetterbox();
    }
    DrawMotionTrailRibbons();

    DrawSync(0);
    if (g_nDemoActive != 0) {
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
    PutDispEnv(&FRAME_DB(g_pActiveFrameDrawEnv)->disp);
    PutDrawEnv(g_pActiveFrameDrawEnv);
    DrawOTag(LinkOTPrimitives(OT_BIN_COUNT));
  }
}
