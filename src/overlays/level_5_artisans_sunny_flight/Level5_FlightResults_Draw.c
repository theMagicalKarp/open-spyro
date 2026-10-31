/* Level5_FlightResults_Draw (0x8007B68C, level_5_artisans_sunny_flight overlay, 6440 bytes).
 *
 * Flight-level end-of-run results/time-attack screen draw. First frame
 * (g_nPauseMenuTransitionFrames == 0): runs the render pipeline once, then captures the frame
 * into VRAM strips (4x MoveImage columns + LoadImage of the backdrop strip
 * and CLUT row). Later frames: rebuilds the frozen backdrop as 4 POLY_FT4
 * quads (clut 0x3820) plus a black TILE bar, then draws either the results
 * board (COMPLETED/QUIT/TIME IS UP/CRASHED headline, four tally rows
 * revealed by g_nSubMenuCursor, bonus column, TOTAL, PRESS-TO-CONTINUE or
 * TRY AGAIN?/YES/NO menu) or, at g_nSubMenuCursor == 0x64, the TIME ATTACK board
 * (times via the formatter g_pfnFlightTimeFormatter, NEW RECORD, TRY AGAIN/YES/NO).
 * Selected records get a sine glow on +0x46 from g_anCosineLut. Standard
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

/* The backdrop quad being built at p, read back through volatile accesses
 * (see the match notes), and the TILE bar that follows the last quad. */
#define QUAD ((volatile PolyFT4 *) p)
#define TILE ((Tile *) ((PolyFT4 *) p + 1))



void Level5_FlightResults_Draw(void)
{
  struct 
  {
    short x;
    short y;
    short w;
    short h;
  } rect;
  int buf[8];
  int pos[3];
  if (g_nPauseMenuTransitionFrames == 0)
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
    h = 0xE0;
    ComposeFrameScene();
    do {
    strip = 0x200;
    RasterizeEmitList();
    DrawPortalsAndStaticActors();
    SetupFrameOT();
    DrawSync(0);
    VSync(0);
    envBase = g_abFrameDrawEnv0;
    } while (0);
    do {
    PutDispEnv(&FRAME_DB(g_pActiveFrameDrawEnv)->disp);
    img = g_pRenderScratchPrimTop;
    } while (0);
    img2 = img - 2;
    PutDrawEnv(g_pActiveFrameDrawEnv);
    DrawOTag(LinkOTPrimitives(0x800));
    DrawSync(0);
    VSync(0);
    PutDispEnv(&FRAME_DB(g_pActiveFrameDrawEnv)->disp);
    rect.x = 0x200;
    rect.y = 0;
    rect.w = 0x100;
    rect.h = 0xE1;
    StoreImage(&rect, img[0] + 0xFFFE3E00);
    y = 8;
    do
    {
      rect.x = i << 7;
      if (g_pActiveFrameDrawEnv != envBase)
      {
        y = 0xF8;
      }
      rect.y = y;
      rect.w = 0x80;
      rect.h = h;
      StoreImage(&rect, img2[0], y);
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
    }
    while (i < 4);
    rect.x = 0x200;
    rect.y = 0xE0;
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
    char *fp;
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
    int snd;
    unsigned short bh;
    unsigned short ch;
    unsigned char bq;
    unsigned char cq;
    cur = g_pActiveFrameDrawEnv;
    allclear = 0;
    env = g_abFrameDrawEnv0;
    if (cur == env)
    {
      env += 0x84;
    }
    i = 0;
    PutDrawEnv(env);
    {
    int off, bias;
    do { off = 0x1C000; bias = 0xFFFE3E00; } while (0);
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
    g_pSpriteRecordBufferTop = tv;
    g_pSpriteRecordWriteCursor = tv;
    quad:
    {
    register int xx asm("$3");
    register int tp asm("$7");
    xx = i << 7;

    tp = i + 0x88;
    p = g_pPrimBufferWriteCursor;
    {
    register int a asm("$2");
    register int d asm("$3");
    register unsigned short bh asm("$5");
    register unsigned short ch asm("$6");
    register unsigned char bq asm("$5");
    register unsigned char cq asm("$6");
    QUAD->tag = 0x9000000;
    QUAD->code = 0x2C;
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
    ((PolyFT4 *) p)->clut = 0x3820;
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
    TILE->tag = 0x3000000;
    TILE->code = b;
    TILE->x0 = 0;
    TILE->y0 = 0xE7;
    TILE->w = 0x200;
    TILE->h = 0xE7;
    TILE->r0 = 0;
    TILE->g0 = 0;
    TILE->b0 = 0;
    AddPrimToOT(next);
    g_pPrimBufferWriteCursor = p + 0x38;
    if (g_nSubMenuCursor < 0x64)
    {
      bonus = 0;
      if ((((g_anFlightTargetTally + g_nFlightTargetTally1) + g_nFlightTargetTally2) + g_nFlightTargetTally3) == 0x20)
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
      pos[0] = 0x10C - (strlen(buf) * 0xC);
      pos[1] = 0x1E;
      pos[2] = 0xC00;
      BuildTextSpriteChain(buf, pos, 0x18, 0xB);
      fp = g_pSpriteRecordWriteCursor - ACTOR_SIZE;
      g_pSpriteRecordWriteCursor = fp;
      FillWord(fp, 0, 0x58);
      {
        char *r = g_pSpriteRecordWriteCursor;
        ACTOR_RAW(r, short, type) = ACTOR_TYPE_HUD_GEM_ICON;
        ACTOR_RAW(r, int, posX) = 0x1CC;
        ACTOR_RAW(r, int, posY) = 0x28;
        ACTOR_RAW(r, int, posZ) = 0xC00;
        ((Actor *)r)->rotY = ((short) g_anCosineLut[g_dwHudBobSinePhase]) >> 9;
      }
      ((Actor *) g_pSpriteRecordWriteCursor)->rotZ = g_dwHudBobSinePhase;
      ((Actor *) g_pSpriteRecordWriteCursor)->otBias = 0x7F;
      ((Actor *) g_pSpriteRecordWriteCursor)->tintMode = 0xB;
      ((Actor *) g_pSpriteRecordWriteCursor)->cullRadius = 0xFF;
      EmitRadialShadedLine(0x3C, 0x2E, 0x1C4, 0x2E);
      pos[0] = 0x64;
      pos[1] = 0x3C;
      pos[2] = 0x1100;
      BuildTextSpriteChain(g_apFlightRowTitles[g_nActiveWorldIndex * 4], pos, 0x12, 0xB);
      if (g_nSubMenuCursor > 0)
      {
        sprintf(buf, "%d/8", g_anFlightTargetTally);
        pos[0] = 0x140;
        pos[1] = 0x3C;
        pos[2] = 0x1100;
        BuildTextSpriteChain(buf, pos, 0x12, 0xB);
      }
      if (g_nSubMenuCursor >= 2)
      {
        if (g_anFlightTargetTally == 8)
        {
          int k = g_nActiveWorldIndex * 5;
          if (g_abWorldPerLevelByteTable[k] != 0)
          {
            sprintf(buf, "^");
          }
          else
          {
            sprintf(buf, "%d", g_anFlightCourseBonusTable[k]);
            bonus += g_anFlightCourseBonusTable[g_nActiveWorldIndex * 5];
          }
          snd = 0;
        }
        else
        {
          sprintf(buf, "0");
          snd = 0x21;
        }
        pos[0] = 0x1E0 - (strlen(buf) * 0x12);
        pos[1] = 0x3C;
        pos[2] = 0x1100;
        BuildTextSpriteChain(buf, pos, 0x12, 0xB);
        if (g_nFlightResultsRevealStep < 2)
        {
          PlaySoundEffect(g_pLevelSampleBankHeader[snd], g_anSpyroWorldPos, SFX_MODE_PANNED, 0);
          g_nFlightResultsRevealStep = 2;
        }
      }
      pos[0] = 0x64;
      pos[1] = 0x50;
      pos[2] = 0x1100;
      BuildTextSpriteChain(g_apFlightRowTitles_r1[g_nActiveWorldIndex * 4], pos, 0x12, 0xB);
      if (g_nSubMenuCursor >= 3)
      {
        sprintf(buf, "%d/8", g_nFlightTargetTally1);
        pos[0] = 0x140;
        pos[1] = 0x50;
        pos[2] = 0x1100;
        BuildTextSpriteChain(buf, pos, 0x12, 0xB);
      }
      if (g_nSubMenuCursor >= 4)
      {
        if (g_nFlightTargetTally1 == 8)
        {
          int k = g_nActiveWorldIndex * 5;
          if (g_abWorldPerLevelByteTable_r1[k] != 0)
          {
            sprintf(buf, "^");
          }
          else
          {
            sprintf(buf, "%d", g_anFlightCourseBonusTable_r1[k]);
            bonus += g_anFlightCourseBonusTable[g_nActiveWorldIndex * 5 + 1];
          }
          snd = 0;
        }
        else
        {
          sprintf(buf, "0");
          snd = 0x21;
        }
        pos[0] = 0x1E0 - (strlen(buf) * 0x12);
        pos[1] = 0x50;
        pos[2] = 0x1100;
        BuildTextSpriteChain(buf, pos, 0x12, 0xB);
        if (g_nFlightResultsRevealStep < 4)
        {
          PlaySoundEffect(g_pLevelSampleBankHeader[snd], g_anSpyroWorldPos, SFX_MODE_PANNED, 0);
          g_nFlightResultsRevealStep = 4;
        }
      }
      pos[0] = 0x64;
      pos[1] = 0x64;
      pos[2] = 0x1100;
      BuildTextSpriteChain(g_apFlightRowTitles_r2[g_nActiveWorldIndex * 4], pos, 0x12, 0xB);
      if (g_nSubMenuCursor >= 5)
      {
        sprintf(buf, "%d/8", g_nFlightTargetTally2);
        pos[0] = 0x140;
        pos[1] = 0x64;
        pos[2] = 0x1100;
        BuildTextSpriteChain(buf, pos, 0x12, 0xB);
      }
      if (g_nSubMenuCursor >= 6)
      {
        if (g_nFlightTargetTally2 == 8)
        {
          int k = g_nActiveWorldIndex * 5;
          if (g_abWorldPerLevelByteTable_r2[k] != 0)
          {
            sprintf(buf, "^");
          }
          else
          {
            sprintf(buf, "%d", g_anFlightCourseBonusTable_r2[k]);
            bonus += g_anFlightCourseBonusTable_r2[g_nActiveWorldIndex * 5];
          }
          snd = 0;
        }
        else
        {
          sprintf(buf, "0");
          snd = 0x21;
        }
        pos[0] = 0x1E0 - (strlen(buf) * 0x12);
        pos[1] = 0x64;
        pos[2] = 0x1100;
        BuildTextSpriteChain(buf, pos, 0x12, 0xB);
        if (g_nFlightResultsRevealStep < 6)
        {
          PlaySoundEffect(g_pLevelSampleBankHeader[snd], g_anSpyroWorldPos, SFX_MODE_PANNED, 0);
          g_nFlightResultsRevealStep = 6;
        }
      }
      pos[0] = 0x64;
      pos[1] = 0x78;
      pos[2] = 0x1100;
      BuildTextSpriteChain(g_apFlightRowTitles_r3[g_nActiveWorldIndex * 4], pos, 0x12, 0xB);
      if (g_nSubMenuCursor >= 7)
      {
        sprintf(buf, "%d/8", g_nFlightTargetTally3);
        pos[0] = 0x140;
        pos[1] = 0x78;
        pos[2] = 0x1100;
        BuildTextSpriteChain(buf, pos, 0x12, 0xB);
      }
      if (g_nSubMenuCursor >= 8)
      {
        if (g_nFlightTargetTally3 == 8)
        {
          int k = g_nActiveWorldIndex * 5;
          if (g_abWorldPerLevelByteTable_r3[k] != 0)
          {
            sprintf(buf, "^");
          }
          else
          {
            sprintf(buf, "%d", g_anFlightCourseBonusTable_r3[k]);
            bonus += g_anFlightCourseBonusTable_r3[g_nActiveWorldIndex * 5];
          }
          snd = 0;
        }
        else
        {
          sprintf(buf, "0");
          snd = 0x21;
        }
        pos[0] = 0x1E0 - (strlen(buf) * 0x12);
        pos[1] = 0x78;
        pos[2] = 0x1100;
        BuildTextSpriteChain(buf, pos, 0x12, 0xB);
        if (g_nFlightResultsRevealStep < 8)
        {
          PlaySoundEffect(g_pLevelSampleBankHeader[snd], g_anSpyroWorldPos, SFX_MODE_PANNED, 0);
          g_nFlightResultsRevealStep = 8;
        }
      }
      pos[0] = 0x64;
      pos[1] = 0x8C;
      pos[2] = 0x1100;
      BuildTextSpriteChain("ALL IN ONE", pos, 0x12, 0xB);
      if (g_nSubMenuCursor >= 9)
      {
        if ((((g_anFlightTargetTally + g_nFlightTargetTally1) + g_nFlightTargetTally2) + g_nFlightTargetTally3) == 0x20)
        {
          int k = g_nActiveWorldIndex * 5;
          if (g_abWorldPerLevelByteTable_r4[k] != 0)
          {
            sprintf(buf, "^");
          }
          else
          {
            sprintf(buf, "%d", g_anFlightCourseBonusTable_r4[k]);
            bonus += g_anFlightCourseBonusTable_r4[g_nActiveWorldIndex * 5];
          }
          snd = 0;
        }
        else
        {
          sprintf(buf, "0");
          snd = 0x21;
        }
        pos[0] = 0x1E0 - (strlen(buf) * 0x12);
        pos[1] = 0x8C;
        pos[2] = 0x1100;
        BuildTextSpriteChain(buf, pos, 0x12, 0xB);
        if (g_nFlightResultsRevealStep < 9)
        {
          PlaySoundEffect(g_pLevelSampleBankHeader[snd], g_anSpyroWorldPos, SFX_MODE_PANNED, 0);
          g_nFlightResultsRevealStep = 9;
        }
      }
      EmitRadialShadedLine(0x1A0, 0x96, 0x1D6, 0x96);
      pos[0] = 0x64;
      pos[1] = 0xA0;
      pos[2] = 0x1100;
      BuildTextSpriteChain("TOTAL", pos, 0x12, 0xB);
      sprintf(buf, "%d", bonus);
      pos[0] = 0x1E0 - (strlen(buf) * 0x12);
      pos[1] = 0xA0;
      pos[2] = 0x1100;
      BuildTextSpriteChain(buf, pos, 0x12, 0xB);
      EmitRadialShadedLine(0x3C, 0xB0, 0x1C4, 0xB0);
      if (g_nSubMenuCursor >= 9)
      {
        if ((allclear != 0) || (g_anWorldPersistentFlags[g_nActiveWorldIndex] != 0))
        {
          pos[0] = 0x50;
          pos[1] = 0xD2;
          pos[2] = 0x1100;
          BuildTextSpriteChain("PRESS   TO CONTINUE", pos, 0x12, 0xB);
          pos[0] = 0xBC;
          BuildTextSpriteChain("X", pos, 0x12, 0xA);
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
          pos[2] = 0x1100;
          BuildTextSpriteChain("TRY AGAIN?", pos, 0x12, 0xB);
          blink = 0;
          pos[0] = 0x15C;
          pos[1] = 0xBE;
          pos[2] = 0x1100;
          pp = pos;
          q3 = pp;
          if (g_nPauseMenuCursor == 0)
          {
            if ((g_nPauseMenuIdleFrames % 0x10) < 6)
            {
              blink = 1;
            }
          }
          BuildTextSpriteChain("YES", q3, 0x12, (blink * 2) + 0xA);
          count = 3;
          blink = 0;
          pos[0] = 0x15C;
          pos[1] = 0xD2;
          pos[2] = 0x1100;
          rec = (Actor *)g_pSpriteRecordWriteCursor;
          pp = pos;
          if (g_nPauseMenuCursor == 1)
          {
            if ((g_nPauseMenuIdleFrames % 0x10) < 6)
            {
              blink = 1;
            }
          }
          BuildTextSpriteChain("NO", pp, 0x12, (blink * 2) + 0xA);
          if (g_nPauseMenuCursor != 0)
          {
            rec = (Actor *)g_pSpriteRecordWriteCursor;
            count = 2;
          }
          i = 0;
          if (count > 0)
          {
            short *lut = (short *) g_anCosineLut;
            int ph = 0;
            do
            {
              i += 1;
              rec->rotZ = (lut[((g_nPauseMenuTransitionFrames * 8) + ph) & 0xFF] * 3) >> 9;
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
        r = g_abHudIconActorRecords;
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
      BuildTextSpriteChain("TIME ATTACK", pos, 0x18, 0xB);
      EmitRadialShadedLine(0x3C, 0x3D, 0x1C4, 0x3D);
      pos[0] = 0x140;
      pos[1] = 0x50;
      pos[2] = 0x1100;
      BuildTextSpriteChain("BEST TIME", pos, 0x12, 0xB);
      pos[0] = 0x191;
      pos[1] = 0x64;
      pos[2] = 0x1100;
      g_pfnFlightTimeFormatter(g_anWorldPersistentFlags[g_nActiveWorldIndex], pos, 0xB);
      pos[0] = 0x38;
      pos[1] = 0x50;
      pos[2] = 0x1100;
      BuildTextSpriteChain("YOUR TIME", pos, 0x12, 0xB);
      pos[0] = 0x89;
      pos[1] = 0x64;
      pos[2] = 0x1100;
      snd = 0xB;
      if (g_anWorldPersistentFlags[g_nActiveWorldIndex] >= g_nFlightRunClock)
      {
        snd = 8;
      }
      if ((((g_anFlightTargetTally + g_nFlightTargetTally1) + g_nFlightTargetTally2) + g_nFlightTargetTally3) == 0x20)
      {
        count = g_pfnFlightTimeFormatter(g_nFlightRunClock, pos, snd);
        rec = (Actor *)g_pSpriteRecordWriteCursor;
        i = 0;
        if (count > 0)
        {
          short *lut = (short *) g_anCosineLut;
          int ph = 0;
          do
          {
            i += 1;
            rec->rotZ = (lut[((g_nPauseMenuTransitionFrames * 8) + ph) & 0xFF] * 3) >> 9;
            ph += 0xC;
            rec++;
          }
          while (i < count);
        }
      }
      else
      {
        g_pfnFlightTimeFormatter(-1, pos, 0xB);
      }
      if ((((g_anFlightTargetTally + g_nFlightTargetTally1) + g_nFlightTargetTally2) + g_nFlightTargetTally3) < 0x20)
      {
        if (g_nFlightQuitFlag != 0)
        {
          memcpy(buf, "QUIT", 5);
        }
        else
        {
          memcpy(buf, "CRASHED", 8);
        }
        count = strlen(buf);
        pos[0] = 0x10C - (count * 0xC);
        pos[1] = 0x96;
        pos[2] = 0xC00;
        BuildTextSpriteChain(buf, pos, 0x18, 0xB);
        rec = (Actor *)g_pSpriteRecordWriteCursor;
        i = 0;
        if (count > 0)
        {
          short *lut = (short *) g_anCosineLut;
          int ph = 0;
          do
          {
            i += 1;
            rec->rotZ = (lut[((g_nPauseMenuTransitionFrames * 8) + ph) & 0xFF] * 3) >> 9;
            ph += 0xC;
            rec++;
          }
          while (i < count);
        }
      }
      else
        if (g_anWorldPersistentFlags[g_nActiveWorldIndex] >= g_nFlightRunClock)
      {
        int ph;
        short *lut;
        pos[0] = 0x94;
        pos[1] = 0x96;
        pos[2] = 0xC00;
        BuildTextSpriteChain("NEW RECORD", pos, 0x18, 8);
        count = 0xA;
        i = 0;
        lut = (short *) g_anCosineLut;
        ph = 0;
        rec = (Actor *)g_pSpriteRecordWriteCursor;
        do
        {
          i += 1;
          rec->rotZ = (lut[((g_nPauseMenuTransitionFrames * 8) + ph) & 0xFF] * 3) >> 9;
          ph += 0xC;
          rec++;
        }
        while (i < count);
      }
      EmitRadialShadedLine(0x3C, 0xAB, 0x1C4, 0xAB);
      pos[0] = 0x8C;
      pos[1] = 0xBE;
      pos[2] = 0x1100;
      pp = pos;
      BuildTextSpriteChain("TRY AGAIN", pp, 0x12, 0xB);
      blink = 0;
      pos[0] = 0x154;
      pos[1] = 0xBE;
      pos[2] = 0x1100;
      q2 = pp;
      if (g_nPauseMenuCursor == 0)
      {
        if ((g_nPauseMenuIdleFrames % 0x10) < 6)
        {
          blink = 1;
        }
      }
      BuildTextSpriteChain("YES", q2, 0x12, (blink * 2) + 0xA);
      count = 3;
      blink = 0;
      pos[0] = 0x154;
      pos[1] = 0xD2;
      pos[2] = 0x1100;
      rec = (Actor *)g_pSpriteRecordWriteCursor;
      qq = pos;
      if (g_nPauseMenuCursor == 1)
      {
        if ((g_nPauseMenuIdleFrames % 0x10) < 6)
        {
          blink = 1;
        }
      }
      BuildTextSpriteChain("NO", qq, 0x12, (blink * 2) + 0xA);
      if (g_nPauseMenuCursor != 0)
      {
        rec = (Actor *)g_pSpriteRecordWriteCursor;
        count = 2;
      }
      i = 0;
      if (count > 0)
      {
        short *lut = (short *) g_anCosineLut;
        int ph = 0;
        do
        {
          i += 1;
          rec->rotZ = (lut[((g_nPauseMenuTransitionFrames * 8) + ph) & 0xFF] * 3) >> 9;
          ph += 0xC;
          rec++;
        }
        while (i < count);
      }
      g_apSpritePrimQueue[0] = 0;
    }
  }
  if (g_nPauseMenuTransitionFrames > 0)
  {
    EnqueuePendingSpritePrims();
    FillWord(g_pOtDepthBinHead0, 0, 0x900);
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
  g_nPauseMenuTransitionFrames += 1;
}
