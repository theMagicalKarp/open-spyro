#include "globals.h"

/* Draw handler for gamestate 0xC — the level-transition title card
   (0x8001E24C, 0x46c).

   Only substates 4 and 5 draw the card; every other substate hands the frame
   to the handler installed at D_800758D8. The card text is "<world>" for the
   first five hub arrivals, and "<world> <level>" past that, indexed out of
   g_apTitleCardNames by g_nCurrentLevelId / 10 (world) and % 10 (level, six
   levels per world starting two entries into the table). The string is
   centred by its glyph count (7px each) and handed to BuildTextSprites; the
   emitted glyph records are then alpha-pulsed from the cosine LUT, one
   0x58-byte record at a time, walking back down the sprite-record queue.

   Substates below 6 also cross-fade the world fog toward the actor mesh fade
   color: the fade parameter runs D_800777EC*0x10 on the way in (substate 4)
   and (0x100 - D_800777EC)*0x10 on the way out, clamped to 0..0x1000, and the
   fogged static mesh is re-emitted until the fade completes. Ends with the
   standard 2-vblank frame submit.

   The OT bin-head pair is written through an asm-opaque copy of the block
   address so cse keeps it as a held base; the empty volatile asm keeps the
   D_80077850 load ahead of it.
*/
extern int sprintf();
extern int strlen(const char *s);
extern void *BuildTextSprites(char *text, int *pos, int *attr, int spacing,
                              int flags);
extern void BuildActorDrawList(void);
extern void FillWord(void *dst, unsigned int value, int byte_count);
extern void EmitActorDrawList(void);
extern void EnqueuePendingSpritePrims(void);
extern void RasterizeSpritePrimQueue(void);
extern void RasterizePairedActor(void);
extern int ComputeDepthCuedColor(unsigned int fog, unsigned int color, int t);
extern void EmitStaticActorMeshListFogged(void);
extern void DrawCinematicLetterbox(void);
extern void DrawSync(int mode);
extern int VSync(int mode);
extern DISPENV *PutDispEnv(DISPENV *env);
extern void PutDrawEnv(void *env);
extern void *LinkOTPrimitives(int depth_max);
extern void DrawOTag(void *ot);

extern char D_80010CD0[]; /* "%s" */
extern char D_80010CE4[]; /* "%s" (level form) */
extern void *D_80077850;  /* OT bin head seed for the title card */
extern int D_800777EC[];  /* transition fade counter */
extern int D_800777FC;    /* transition card variant */
extern void *D_800758D8;  /* substate draw handler */
extern unsigned short g_anCosineLut[];
extern void *g_apOtDepthBinBlock[];  /* alias @0x8006fcf4: head/tail pair */
extern int g_anVsyncFrameEndBlock[]; /* held-base alias: [-1] = pace anchor */

void Gamestate0C_Draw(void) {
  int pos[3];
  int attr[3];
  char buf[64];
  int color;
  int world;
  int level;
  int phase;
  int variant;
  char *rec;

  if (g_nGamestate0cSubstate >= 4) {
    if (g_nGamestate0cSubstate < 6) {
      {
        register void **ot asm("$3");
        register void *h asm("$2");

        h = D_80077850;
        __asm__ volatile("" : : "r"(h));
        __asm__("" : "=r"(ot) : "0"(g_apOtDepthBinBlock));
        ot[1] = 0;
        ot[0] = h;
      }
      do {
      } while (0);

      variant = *(volatile int *)&D_800777FC;
      if (variant < 5) {
        sprintf(buf, D_80010CD0, g_apTitleCardNames[g_nCurrentLevelId / 10]);
      } else if (variant == 5) {
        sprintf(buf, D_80010CE4, g_apTitleCardNames[g_nCurrentLevelId / 10]);
      } else {
        world = g_nCurrentLevelId / 10;
        level = g_nCurrentLevelId - world * 10;
        sprintf(buf, D_80010CE4, g_apTitleCardNames[world * 6 + level + 2]);
      }

      pos[0] = 0x100 - (strlen(buf) - 1) * 7;
      pos[1] = 0xC8;
      pos[2] = 0x1400;
      attr[0] = 0xE;
      attr[1] = 1;
      attr[2] = 0x1600;
      rec = (char *)g_pSpriteRecordWriteCursor;
      BuildTextSprites(buf, pos, attr, 0x10, 2);

      rec -= 0x58;
      phase = 0;
      if ((int)rec >= (int)g_pSpriteRecordWriteCursor) {
        int *fade = D_800777EC;
        unsigned short *cos = g_anCosineLut;
        do {
          rec[0x46] = cos[(*fade * 2 + phase) & 0xFF] >> 7;
          rec -= 0x58;
          phase += 0xC;
        } while ((int)rec >= (int)g_pSpriteRecordWriteCursor);
      }

      BuildActorDrawList();
      FillWord(g_apOtDepthBinBlock, 0, 0x900);
      EmitActorDrawList();
      g_apSpritePrimQueue[0] = 0;
      EnqueuePendingSpritePrims();
      RasterizeSpritePrimQueue();
      RasterizePairedActor();

      if (g_nGamestate0cSubstate < 6) {
        g_dwActorMeshFadeColor = 0x401010;
        if (g_nGamestate0cSubstate == 4) {
          g_nFadeProgress = D_800777EC[0] * 0x10;
        } else {
          g_nFadeProgress = (0x100 - D_800777EC[0]) * 0x10;
        }
        if (g_nFadeProgress < 0) {
          *(volatile int *)&g_nFadeProgress = 0;
        }
        if (g_nFadeProgress > 0x1000) {
          g_nFadeProgress = 0x1000;
        }
        color = ComputeDepthCuedColor(g_dwWorldFogColor, g_dwActorMeshFadeColor,
                                      g_nFadeProgress);
        g_abFrameDrawEnv0.r0 = ((unsigned char *)&color)[0];
        g_abFrameDrawEnv0.g0 = ((unsigned char *)&color)[1];
        g_abFrameDrawEnv0.b0 = ((unsigned char *)&color)[2];
        g_abFrameDrawEnv1.r0 = ((unsigned char *)&color)[0];
        g_abFrameDrawEnv1.g0 = ((unsigned char *)&color)[1];
        g_abFrameDrawEnv1.b0 = ((unsigned char *)&color)[2];
        if (g_nFadeProgress < 0x1000) {
          EmitStaticActorMeshListFogged();
        }
      }
    } else {
      (*(void (*)())D_800758D8)();
    }
  } else {
    (*(void (*)())D_800758D8)();
  }

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
