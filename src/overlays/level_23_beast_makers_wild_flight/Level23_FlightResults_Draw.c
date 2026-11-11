/* Level23_FlightResults_Draw (0x8007B68C, level_23_beast_makers_wild_flight overlay, 6440 bytes).
 *
 * Flight-level end-of-run results/time-attack screen draw. First frame
 * (g_nMenuDrawFrames == 0): runs the render pipeline once, then freezes the frame
 * as a gray backdrop: each of 4 column strips is read back (StoreImage),
 * packed to 8bpp luma (ConvertRGB15ToLuma8) and reloaded at VRAM x=0x200
 * (LoadImage), followed by the gray CLUT row. Later frames: rebuilds the frozen backdrop as 4 POLY_FT4
 * quads (clut 0x3820) plus a black line at y = 0xE7, then draws either the results
 * board (COMPLETED/QUIT/TIME IS UP/CRASHED headline, four tally rows
 * revealed by g_nSubMenuCursor, bonus column, TOTAL, PRESS-TO-CONTINUE or
 * TRY AGAIN?/YES/NO menu) or, at g_nSubMenuCursor == 0x64, the TIME ATTACK board
 * (times via the formatter g_pfnFlightTimeFormatter, NEW RECORD, TRY AGAIN/YES/NO).
 * Selected glyphs rock on their rotZ (+0x46) from g_anCosineLut. Standard
 * 2-vblank pace tail. Word-identical in all five flight overlays
 * (5/11/17/23/29).
 *
 * Match notes: the two large constants after PutDrawEnv sit behind a
 * do/while(0) barrier so their lui/ori pairs stay adjacent; the quad loop
 * reads the POLY_FT4 back through volatile accesses in source order with
 * block-scoped register pins (x/d v1, a v0, bh/bq a1, ch/cq a2, tpage a3)
 * matching the original's map.
 */

#include "level_globals.h"
#include "level_funcs.h"
#include "main_shared.h"
#include "level_emit_particles.h"
#include "level_spawn_actor.h"
#include "level_flight_results.h"
#include "actor.h"
#include "gpu_prim.h"
#include "frame_db.h"
#include "level_sfx.h"

/* The backdrop quad being built at p, read back through volatile accesses
 * (see the match notes), and the black line under it (y = 0xE7, LINE_F2: the code reuses the 0x40 in b). */
#define QUAD ((volatile PolyFT4 *) p)
#define LINE ((LineF2 *)((PolyFT4 *)p + 1))

static const char g_szTryAgain[12];

void Level23_FlightResults_Draw(void)
{
  struct 
  {
    short x;
    short y;
    short w;
    short h;
  } rect;
  int buf[8]; /* the status text; int-typed as in the match */
  int pos[3];
  if (g_nMenuDrawFrames == 0)
  {
    int i;
    int h;
    int strip;
    int y;
    char **img;
    char **img2;
    char *im;
    char *envBase;
    i = 0;
    BuildRenderEntityLists();
    h = DRAW_H;
    ComposeFrameScene();
    do {
    strip = MENU_BACKDROP_X;
    RasterizeEmitList();
    DrawPortalsAndSky();
    SetupFrameOT();
    DrawSync(0);
    VSync(0);
    envBase = g_FrameDrawEnv0;
    } while (0);
    do {
    PutDispEnv(&FRAME_DB(g_pActiveFrameDrawEnv)->disp);
    img = g_pRenderScratchPrimTop;
    } while (0);
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
    StoreImage(&rect, img[0] - MENU_SNAPSHOT_BYTES);
    y = DRAW_Y;
    do
    {
      rect.x = i << 7;
      if (g_pActiveFrameDrawEnv != envBase)
      {
        y = SCREEN_H + DRAW_Y;
      }
      rect.y = y;
      rect.w = MENU_BACKDROP_STRIP_W;
      rect.h = h;
      StoreImage(&rect, img2[0], y);
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
    }
    while (i < MENU_BACKDROP_STRIPS);
    rect.x = MENU_BACKDROP_X;
    rect.y = MENU_BACKDROP_CLUT_Y;
    rect.w = 0x20;
    rect.h = 1;
    LoadImage(&rect, g_anMenuBackdropGrayClut);
    DrawSync(0);
    g_nVsyncFramePaceAnchor = VSync(-1);
  }
  else
  {
    char *env;
    char *cur;
    char *p;
    Actor *fp;
    char *next;
    char *tv;
    int allclear;
    int bonus;
    int i;
    int x;
    int tpage;
    int b;
    int a;
    int d;
    int snd; /* a reveal's sound slot (LevelSfx), then the run time's tint */
    unsigned short bh;
    unsigned short ch;
    unsigned char bq;
    unsigned char cq;
    cur = g_pActiveFrameDrawEnv;
    allclear = 0;
    env = g_FrameDrawEnv0;
    if (cur == env)
    {
      env += 0x84;
    }
    i = 0;
    PutDrawEnv(env);
    {
    int off, bias;
    do { off = PRIM_BUFFER_BYTES; bias = -MENU_SNAPSHOT_BYTES; } while (0);
    {
    register char *e8 asm("$3");
    register char *f0 asm("$2");
    e8 = g_pRenderScratchRegionBase;
    f0 = g_pRenderScratchPrimTop[0];
    g_nPrimBufferOverflowFlag = 0;
    g_pPrimBufferWriteCursor = e8;
    g_pPrimBufferLimit = e8 + off;
    tv = f0 + bias;
    }
    }
    g_pSpriteRecordBufferTop = (Actor *)tv;
    g_pSpriteRecordWriteCursor = (Actor *)tv;
    quad:
    {
    register int xx asm("$3");
    register int tp asm("$7");
    xx = i << 7;

    tp = i + MENU_BACKDROP_TPAGE;
    p = g_pPrimBufferWriteCursor;
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
    ((PolyFT4 *) p)->clut = MENU_BACKDROP_CLUT;
    ((PolyFT4 *) p)->tpage = tp;
    AddPrimToOT(p);
    }
    }
    next = p + 0x28;
    g_pPrimBufferWriteCursor = next;
    if (i < 4)
    {
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
    AddPrimToOT(next);
    g_pPrimBufferWriteCursor = p + 0x38;
    if (g_nSubMenuCursor < FLIGHT_REVEAL_TIME_BOARD)
    {
      bonus = 0;
      if ((((g_anFlightTargetTally + g_nFlightTargetTally1) + g_nFlightTargetTally2) + g_nFlightTargetTally3) == FLIGHT_TARGET_TOTAL)
      {
        memcpy(buf, "COMPLETED", 10);
        allclear = 1;
      }
      else
        if (g_nFlightQuitFlag != 0)
      {
        memcpy(buf, "QUIT", 5);
      }
      else
        if (g_nFlightTimeBonus < 0)
      {
        memcpy(buf, "TIME IS UP", 11);
      }
      else
      {
        memcpy(buf, "CRASHED", 8);
      }
      pos[0] = 0x10C - (strlen((char *)buf) * 0xC);
      pos[1] = 0x1E;
      pos[2] = 0xC00;
      BuildTextSpriteChain(buf, pos, 0x18, TINT_HUD);
      fp = g_pSpriteRecordWriteCursor - 1;
      g_pSpriteRecordWriteCursor = fp;
      FillWord(fp, 0, ACTOR_SIZE);
      {
        char *r = (char *)g_pSpriteRecordWriteCursor;
        ACTOR_RAW(r, short, type) = ACTOR_TYPE_HUD_GEM_ICON;
        ACTOR_RAW(r, int, posX) = 0x1CC;
        ACTOR_RAW(r, int, posY) = 0x28;
        ACTOR_RAW(r, int, posZ) = 0xC00;
        ((Actor *)r)->rotY = g_anCosineLut[g_dwHudIconSpinAngle] >> 9;
      }
      g_pSpriteRecordWriteCursor->rotZ = g_dwHudIconSpinAngle;
      g_pSpriteRecordWriteCursor->otBias = OT_BIAS_SPRITE;
      g_pSpriteRecordWriteCursor->tintMode = TINT_HUD;
      g_pSpriteRecordWriteCursor->cullRadius = ACTOR_CULL_ALWAYS;
      EmitRadialShadedLine(0x3C, 0x2E, 0x1C4, 0x2E);
      pos[0] = 0x64;
      pos[1] = 0x3C;
      pos[2] = TEXT_DEPTH;
      BuildTextSpriteChain(g_apFlightRowTitles[g_nActiveWorldIndex * 4], pos, 0x12, TINT_HUD);
      if (g_nSubMenuCursor > 0)
      {
        sprintf(buf, "%d/8", g_anFlightTargetTally);
        pos[0] = 0x140;
        pos[1] = 0x3C;
        pos[2] = TEXT_DEPTH;
        BuildTextSpriteChain(buf, pos, 0x12, TINT_HUD);
      }
      if (g_nSubMenuCursor >= 2)
      {
        if (g_anFlightTargetTally == FLIGHT_CLASS_TARGETS)
        {
          int k = g_nActiveWorldIndex * 5;
          if (g_abFlightBonusPaid[k] != 0)
          {
            sprintf(buf, "^");
          }
          else
          {
            sprintf(buf, "%d", g_anFlightCourseBonusTable[k]);
            bonus += g_anFlightCourseBonusTable[g_nActiveWorldIndex * 5];
          }
          snd = SFX_REWARD;
        }
        else
        {
          sprintf(buf, "0");
          snd = SFX_FLIGHT_CLOCK;
        }
        pos[0] = 0x1E0 - (strlen((char *)buf) * 0x12);
        pos[1] = 0x3C;
        pos[2] = TEXT_DEPTH;
        BuildTextSpriteChain(buf, pos, 0x12, TINT_HUD);
        if (g_nFlightResultsRevealStep < 2)
        {
          PlaySoundEffect(g_pLevelSampleBankHeader[snd], (Actor *)g_anSpyroWorldPos, SFX_MODE_PANNED, 0);
          g_nFlightResultsRevealStep = 2;
        }
      }
      pos[0] = 0x64;
      pos[1] = 0x50;
      pos[2] = TEXT_DEPTH;
      BuildTextSpriteChain(g_apFlightRowTitles_r1[g_nActiveWorldIndex * 4], pos, 0x12, TINT_HUD);
      if (g_nSubMenuCursor >= 3)
      {
        sprintf(buf, "%d/8", g_nFlightTargetTally1);
        pos[0] = 0x140;
        pos[1] = 0x50;
        pos[2] = TEXT_DEPTH;
        BuildTextSpriteChain(buf, pos, 0x12, TINT_HUD);
      }
      if (g_nSubMenuCursor >= 4)
      {
        if (g_nFlightTargetTally1 == FLIGHT_CLASS_TARGETS)
        {
          int k = g_nActiveWorldIndex * 5;
          if (g_abFlightBonusPaid_r1[k] != 0)
          {
            sprintf(buf, "^");
          }
          else
          {
            sprintf(buf, "%d", g_anFlightCourseBonusTable_r1[k]);
            bonus += g_anFlightCourseBonusTable[g_nActiveWorldIndex * 5 + 1];
          }
          snd = SFX_REWARD;
        }
        else
        {
          sprintf(buf, "0");
          snd = SFX_FLIGHT_CLOCK;
        }
        pos[0] = 0x1E0 - (strlen((char *)buf) * 0x12);
        pos[1] = 0x50;
        pos[2] = TEXT_DEPTH;
        BuildTextSpriteChain(buf, pos, 0x12, TINT_HUD);
        if (g_nFlightResultsRevealStep < 4)
        {
          PlaySoundEffect(g_pLevelSampleBankHeader[snd], (Actor *)g_anSpyroWorldPos, SFX_MODE_PANNED, 0);
          g_nFlightResultsRevealStep = 4;
        }
      }
      pos[0] = 0x64;
      pos[1] = 0x64;
      pos[2] = TEXT_DEPTH;
      BuildTextSpriteChain(g_apFlightRowTitles_r2[g_nActiveWorldIndex * 4], pos, 0x12, TINT_HUD);
      if (g_nSubMenuCursor >= 5)
      {
        sprintf(buf, "%d/8", g_nFlightTargetTally2);
        pos[0] = 0x140;
        pos[1] = 0x64;
        pos[2] = TEXT_DEPTH;
        BuildTextSpriteChain(buf, pos, 0x12, TINT_HUD);
      }
      if (g_nSubMenuCursor >= 6)
      {
        if (g_nFlightTargetTally2 == FLIGHT_CLASS_TARGETS)
        {
          int k = g_nActiveWorldIndex * 5;
          if (g_abFlightBonusPaid_r2[k] != 0)
          {
            sprintf(buf, "^");
          }
          else
          {
            sprintf(buf, "%d", g_anFlightCourseBonusTable_r2[k]);
            bonus += g_anFlightCourseBonusTable_r2[g_nActiveWorldIndex * 5];
          }
          snd = SFX_REWARD;
        }
        else
        {
          sprintf(buf, "0");
          snd = SFX_FLIGHT_CLOCK;
        }
        pos[0] = 0x1E0 - (strlen((char *)buf) * 0x12);
        pos[1] = 0x64;
        pos[2] = TEXT_DEPTH;
        BuildTextSpriteChain(buf, pos, 0x12, TINT_HUD);
        if (g_nFlightResultsRevealStep < 6)
        {
          PlaySoundEffect(g_pLevelSampleBankHeader[snd], (Actor *)g_anSpyroWorldPos, SFX_MODE_PANNED, 0);
          g_nFlightResultsRevealStep = 6;
        }
      }
      pos[0] = 0x64;
      pos[1] = 0x78;
      pos[2] = TEXT_DEPTH;
      BuildTextSpriteChain(g_apFlightRowTitles_r3[g_nActiveWorldIndex * 4], pos, 0x12, TINT_HUD);
      if (g_nSubMenuCursor >= 7)
      {
        sprintf(buf, "%d/8", g_nFlightTargetTally3);
        pos[0] = 0x140;
        pos[1] = 0x78;
        pos[2] = TEXT_DEPTH;
        BuildTextSpriteChain(buf, pos, 0x12, TINT_HUD);
      }
      if (g_nSubMenuCursor >= 8)
      {
        if (g_nFlightTargetTally3 == FLIGHT_CLASS_TARGETS)
        {
          int k = g_nActiveWorldIndex * 5;
          if (g_abFlightBonusPaid_r3[k] != 0)
          {
            sprintf(buf, "^");
          }
          else
          {
            sprintf(buf, "%d", g_anFlightCourseBonusTable_r3[k]);
            bonus += g_anFlightCourseBonusTable_r3[g_nActiveWorldIndex * 5];
          }
          snd = SFX_REWARD;
        }
        else
        {
          sprintf(buf, "0");
          snd = SFX_FLIGHT_CLOCK;
        }
        pos[0] = 0x1E0 - (strlen((char *)buf) * 0x12);
        pos[1] = 0x78;
        pos[2] = TEXT_DEPTH;
        BuildTextSpriteChain(buf, pos, 0x12, TINT_HUD);
        if (g_nFlightResultsRevealStep < 8)
        {
          PlaySoundEffect(g_pLevelSampleBankHeader[snd], (Actor *)g_anSpyroWorldPos, SFX_MODE_PANNED, 0);
          g_nFlightResultsRevealStep = 8;
        }
      }
      pos[0] = 0x64;
      pos[1] = 0x8C;
      pos[2] = TEXT_DEPTH;
      BuildTextSpriteChain("ALL IN ONE", pos, 0x12, TINT_HUD);
      if (g_nSubMenuCursor >= FLIGHT_REVEAL_ALL)
      {
        if ((((g_anFlightTargetTally + g_nFlightTargetTally1) + g_nFlightTargetTally2) + g_nFlightTargetTally3) == FLIGHT_TARGET_TOTAL)
        {
          int k = g_nActiveWorldIndex * 5;
          if (g_abFlightBonusPaid_r4[k] != 0)
          {
            sprintf(buf, "^");
          }
          else
          {
            sprintf(buf, "%d", g_anFlightCourseBonusTable_r4[k]);
            bonus += g_anFlightCourseBonusTable_r4[g_nActiveWorldIndex * 5];
          }
          snd = SFX_REWARD;
        }
        else
        {
          sprintf(buf, "0");
          snd = SFX_FLIGHT_CLOCK;
        }
        pos[0] = 0x1E0 - (strlen((char *)buf) * 0x12);
        pos[1] = 0x8C;
        pos[2] = TEXT_DEPTH;
        BuildTextSpriteChain(buf, pos, 0x12, TINT_HUD);
        if (g_nFlightResultsRevealStep < 9)
        {
          PlaySoundEffect(g_pLevelSampleBankHeader[snd], (Actor *)g_anSpyroWorldPos, SFX_MODE_PANNED, 0);
          g_nFlightResultsRevealStep = 9;
        }
      }
      EmitRadialShadedLine(0x1A0, 0x96, 0x1D6, 0x96);
      pos[0] = 0x64;
      pos[1] = 0xA0;
      pos[2] = TEXT_DEPTH;
      BuildTextSpriteChain("TOTAL", pos, 0x12, TINT_HUD);
      sprintf(buf, "%d", bonus);
      pos[0] = 0x1E0 - (strlen((char *)buf) * 0x12);
      pos[1] = 0xA0;
      pos[2] = TEXT_DEPTH;
      BuildTextSpriteChain(buf, pos, 0x12, TINT_HUD);
      EmitRadialShadedLine(0x3C, 0xB0, 0x1C4, 0xB0);
      if (g_nSubMenuCursor >= FLIGHT_REVEAL_ALL)
      {
        if ((allclear != 0) || (g_anFlightClearTimes[g_nActiveWorldIndex] != 0))
        {
          pos[0] = 0x50;
          pos[1] = 0xD2;
          pos[2] = TEXT_DEPTH;
          BuildTextSpriteChain("PRESS   TO CONTINUE", pos, 0x12, TINT_HUD);
          pos[0] = 0xBC;
          BuildTextSpriteChain("X", pos, 0x12, TINT_BLUE);
        }
        else
        {
          Actor *rec;
          int count;
          int blink;
          int *pp;
          int *q3;
          pos[0] = 0x8C;
          pos[1] = 0xBE;
          pos[2] = TEXT_DEPTH;
          BuildTextSpriteChain("TRY AGAIN?", pos, 0x12, TINT_HUD);
          blink = 0;
          pos[0] = 0x15C;
          pos[1] = 0xBE;
          pos[2] = TEXT_DEPTH;
          pp = pos;
          q3 = pp;
          if (g_nPauseMenuCursor == FLIGHT_RETRY_YES)
          {
            if ((g_nMenuTickFrames % 0x10) < 6)
            {
              blink = 1;
            }
          }
          BuildTextSpriteChain("YES", q3, 0x12, (blink * 2) + 0xA);
          count = 3;
          blink = 0;
          pos[0] = 0x15C;
          pos[1] = 0xD2;
          pos[2] = TEXT_DEPTH;
          rec = g_pSpriteRecordWriteCursor;
          pp = pos;
          if (g_nPauseMenuCursor == FLIGHT_RETRY_NO)
          {
            if ((g_nMenuTickFrames % 0x10) < 6)
            {
              blink = 1;
            }
          }
          BuildTextSpriteChain("NO", pp, 0x12, (blink * 2) + 0xA);
          if (g_nPauseMenuCursor != FLIGHT_RETRY_YES)
          {
            rec = g_pSpriteRecordWriteCursor;
            count = 2;
          }
          i = 0;
          if (count > 0)
          {
            short *lut = g_anCosineLut;
            int ph = 0;
            do
            {
              i += 1;
              rec->rotZ = (lut[((g_nMenuDrawFrames * 8) + ph) & 0xFF] * 3) >> 9;
              ph += 0xC;
              rec++;
            }
            while (i < count);
          }
        }
      }
      {
        char **slot;
        char *r;
        slot = g_apSpritePrimQueue;
        i = 0;
        r = g_aHudIconActorRecords;
        pub:
        *slot = r;

        do { do { slot += 1; } while (0); } while (0);
        i += 1;
        r += ACTOR_SIZE;
        if (i < 4)
        {
          goto pub;
        }
        *slot = 0;
      }
    }
    else
    {
      Actor *rec;
      int count;
      int blink;
      int *pp;
      int *qq;
      int *q2;
      pos[0] = 0x88;
      pos[1] = 0x28;
      pos[2] = 0xC00;
      BuildTextSpriteChain("TIME ATTACK", pos, 0x18, TINT_HUD);
      EmitRadialShadedLine(0x3C, 0x3D, 0x1C4, 0x3D);
      pos[0] = 0x140;
      pos[1] = 0x50;
      pos[2] = TEXT_DEPTH;
      BuildTextSpriteChain("BEST TIME", pos, 0x12, TINT_HUD);
      pos[0] = 0x191;
      pos[1] = 0x64;
      pos[2] = TEXT_DEPTH;
      g_pfnFlightTimeFormatter(g_anFlightClearTimes[g_nActiveWorldIndex], pos, TINT_HUD);
      pos[0] = 0x38;
      pos[1] = 0x50;
      pos[2] = TEXT_DEPTH;
      BuildTextSpriteChain("YOUR TIME", pos, 0x12, TINT_HUD);
      pos[0] = 0x89;
      pos[1] = 0x64;
      pos[2] = TEXT_DEPTH;
      snd = TINT_HUD;
      if (g_anFlightClearTimes[g_nActiveWorldIndex] >= g_nFlightRunClock)
      {
        snd = TINT_RED;
      }
      if ((((g_anFlightTargetTally + g_nFlightTargetTally1) + g_nFlightTargetTally2) + g_nFlightTargetTally3) == FLIGHT_TARGET_TOTAL)
      {
        count = g_pfnFlightTimeFormatter(g_nFlightRunClock, pos, snd);
        rec = g_pSpriteRecordWriteCursor;
        i = 0;
        if (count > 0)
        {
          short *lut = g_anCosineLut;
          int ph = 0;
          do
          {
            i += 1;
            rec->rotZ = (lut[((g_nMenuDrawFrames * 8) + ph) & 0xFF] * 3) >> 9;
            ph += 0xC;
            rec++;
          }
          while (i < count);
        }
      }
      else
      {
        g_pfnFlightTimeFormatter(-1, pos, TINT_HUD);
      }
      if ((((g_anFlightTargetTally + g_nFlightTargetTally1) + g_nFlightTargetTally2) + g_nFlightTargetTally3) < FLIGHT_TARGET_TOTAL)
      {
        if (g_nFlightQuitFlag != 0)
        {
          memcpy(buf, "QUIT", 5);
        }
        else
        {
          memcpy(buf, "CRASHED", 8);
        }
        count = strlen((char *)buf);
        pos[0] = 0x10C - (count * 0xC);
        pos[1] = 0x96;
        pos[2] = 0xC00;
        BuildTextSpriteChain(buf, pos, 0x18, TINT_HUD);
        rec = g_pSpriteRecordWriteCursor;
        i = 0;
        if (count > 0)
        {
          short *lut = g_anCosineLut;
          int ph = 0;
          do
          {
            i += 1;
            rec->rotZ = (lut[((g_nMenuDrawFrames * 8) + ph) & 0xFF] * 3) >> 9;
            ph += 0xC;
            rec++;
          }
          while (i < count);
        }
      }
      else
        if (g_anFlightClearTimes[g_nActiveWorldIndex] >= g_nFlightRunClock)
      {
        int ph;
        short *lut;
        pos[0] = 0x94;
        pos[1] = 0x96;
        pos[2] = 0xC00;
        BuildTextSpriteChain("NEW RECORD", pos, 0x18, TINT_RED);
        count = 0xA;
        i = 0;
        lut = g_anCosineLut;
        ph = 0;
        rec = g_pSpriteRecordWriteCursor;
        do
        {
          i += 1;
          rec->rotZ = (lut[((g_nMenuDrawFrames * 8) + ph) & 0xFF] * 3) >> 9;
          ph += 0xC;
          rec++;
        }
        while (i < count);
      }
      EmitRadialShadedLine(0x3C, 0xAB, 0x1C4, 0xAB);
      pos[0] = 0x8C;
      pos[1] = 0xBE;
      pos[2] = TEXT_DEPTH;
      pp = pos;
      BuildTextSpriteChain(g_szTryAgain, pp, 0x12, TINT_HUD);
      blink = 0;
      pos[0] = 0x154;
      pos[1] = 0xBE;
      pos[2] = TEXT_DEPTH;
      q2 = pp;
      if (g_nPauseMenuCursor == FLIGHT_RETRY_YES)
      {
        if ((g_nMenuTickFrames % 0x10) < 6)
        {
          blink = 1;
        }
      }
      BuildTextSpriteChain("YES", q2, 0x12, (blink * 2) + 0xA);
      count = 3;
      blink = 0;
      pos[0] = 0x154;
      pos[1] = 0xD2;
      pos[2] = TEXT_DEPTH;
      rec = g_pSpriteRecordWriteCursor;
      qq = pos;
      if (g_nPauseMenuCursor == FLIGHT_RETRY_NO)
      {
        if ((g_nMenuTickFrames % 0x10) < 6)
        {
          blink = 1;
        }
      }
      BuildTextSpriteChain("NO", qq, 0x12, (blink * 2) + 0xA);
      if (g_nPauseMenuCursor != FLIGHT_RETRY_YES)
      {
        rec = g_pSpriteRecordWriteCursor;
        count = 2;
      }
      i = 0;
      if (count > 0)
      {
        short *lut = g_anCosineLut;
        int ph = 0;
        do
        {
          i += 1;
          rec->rotZ = (lut[((g_nMenuDrawFrames * 8) + ph) & 0xFF] * 3) >> 9;
          ph += 0xC;
          rec++;
        }
        while (i < count);
      }
      g_apSpritePrimQueue[0] = 0;
    }
  }
  if (g_nMenuDrawFrames > 0)
  {
    EnqueuePendingSpritePrims();
    FillWord(g_apMainDrawEntityList, 0, FRAME_ARENA_LIST_WORDS);
    RasterizeSpritePrimQueue();
  }
  DrawOTag(LinkOTPrimitives(0));
  DrawSync(0);
  if (g_nDemoActive != 0)
  {
    VSync(0);
  }
  {
    int *end;
    *(end = g_nVsyncFrameEndCount) = VSync(-1);
    if ((end[0] - g_nVsyncFramePaceAnchor) < 2)
    {
      int *p2 = end;
      do
      {
        VSync(0);
        p2[0] = VSync(-1);
      }
      while ((p2[0] - p2[-1]) < 2);
    }
  }
  g_nVsyncFramePaceAnchor = VSync(-1);
  PutDispEnv(&FRAME_DB(g_pActiveFrameDrawEnv)->disp);
  g_nMenuDrawFrames += 1;
}

static const char g_szTryAgain[12] = {'T', 'R', 'Y', ' ', 'A', 'G', 'A', 'I', 'N', '\0', 't', ' '};
