#include "globals.h"

/* Draw for gamestate 8 (GS_GEM_PICKUP, 0x8001cfdc, 0x73c).

   The dragon-rescue presentation, one arm per g_nGemPickupSubstate. Substate 0
   is still an ordinary gameplay frame; 1-3 hand-build the OT head list from the
   rescuing actor (plus the statue once the statue arm is live) and sweep the
   actor pool for the 0xfb marker actors that go into the sprite queue; 4 does
   the same around the statue + Spyro mirror pair and shows the name banner for
   the first second; 5 pins the statue and three HUD icon records, scaling the
   draw-list's two vector triples by the timer curve (and by a cosine sweep once
   the timer passes 8); 6 and the first sixteen frames of 7 draw only the icon
   records; the rest of 7 is the return-to-gameplay frame with the secondary
   actor pass. Every arm ends in the shared letterbox check and the standard
   2-vblank frame submit. */
extern void ApplyEulerRotation(void *euler, void *mtx, void *viewMtx);
extern void DrawGemPickupAuraStar(void);
extern void BuildRenderEntityLists(void);
extern void ComposeFrameScene(void);
extern void SetupFrameOT(void);
extern void DrawActors(void);
extern void RasterizeEmitList(void);
extern void DrawFullscreenTint(int slot, int r, int g, int b);
extern void DrawDragonRescuedBanner(void);
extern void EnqueuePendingSpritePrims(void);
extern void BuildActorDrawList(void);
extern void FillWord(void *dst, unsigned int value, int byte_count);
extern void EmitActorDrawList(void);
extern void RasterizeSpritePrimQueue(void);
extern void DrawEntityDirectionalSpikes(void);
extern void RasterizePairedActor(void);
extern void DrawSpyroDropShadow(void);
extern void BuildSecondaryActorDrawList(void);
extern void EmitSecondaryActorPrimitives(void);
extern void DrawCinematicLetterbox(void);
extern int LookupCosine(unsigned int angle_12_4);
extern void DrawSync(int mode);
extern int VSync(int mode);
extern DISPENV *PutDispEnv(DISPENV *env);
extern void PutDrawEnv(void *env);
extern void *LinkOTPrimitives(int depth_max);
extern void DrawOTag(void *ot);

extern int g_anGemPickupAuraStarBlock[]; /* 0x80076248 aura-star state block */
extern void *g_apOtDepthBinBlock[];      /* 0x8006fcf4: OT head list,
                                            sprite prim queue at [2304] */
extern short D_8006F3C0[];               /* gem-pickup timer scale curve */
extern int g_anVsyncFrameEndBlock[]; /* held-base alias: [-1] = pace anchor */

void Gamestate08_Draw(void) {
  int unused[4];
  unsigned char *icon;

  if (g_anGemPickupAuraStarBlock[0] != 0) {
    ApplyEulerRotation(&g_anGemPickupAuraStarBlock[10],
                       &g_anGemPickupAuraStarBlock[4], g_anWorldToCameraRotMtx);
    DrawGemPickupAuraStar();
  }

  if (g_nGemPickupSubstate == 0) {
    BuildRenderEntityLists();
    ComposeFrameScene();
    SetupFrameOT();
    DrawActors();
    RasterizeEmitList();
    {
      int tint = g_nGemPickupFullscreenTintLevel;
      if (tint != 0) {
        DrawFullscreenTint(1, tint, tint, tint);
      }
    }
    if (g_nGameplayBlocked != 0 || g_nLetterboxBarHeight != 0) {
      DrawCinematicLetterbox();
    }
  } else if (g_nGemPickupSubstate < 4) {
    unsigned char *act;
    unsigned char id;
    void **ot;
    void **q;
    ot = g_apOtDepthBinBlock;
    if (((unsigned char *)g_pGemPickupSourceActor)[0x48] < 0x80) {
      *ot++ = g_pGemPickupSourceActor;
    }
    if (g_nGemPickupSubstate == 3) {
      *ot++ = g_pGemPickupStatueActor;
    }
    *ot = 0;
    act = (unsigned char *)g_pActorListBase;
    q = &g_apOtDepthBinBlock[2304];
    id = act[0x48];
    while (id != 0xFF) {
      if (*(short *)(act + 0x36) == 0xFB && id < 0x80) {
        *q++ = act;
      }
      act += 0x58;
      id = act[0x48];
    }
    *q = 0;
    DrawDragonRescuedBanner();
    EnqueuePendingSpritePrims();
    BuildActorDrawList();
    FillWord(&g_pOtDepthBinHead0, 0, 0x900);
    EmitActorDrawList();
    RasterizeSpritePrimQueue();
    DrawEntityDirectionalSpikes();
    RasterizePairedActor();
    DrawSpyroDropShadow();
    SetupFrameOT();
    DrawActors();
    RasterizeEmitList();
    {
      int tint = g_nGemPickupFullscreenTintLevel;
      if (tint != 0) {
        DrawFullscreenTint(1, tint, tint, tint);
      }
    }
    if (g_nGameplayBlocked != 0 || g_nLetterboxBarHeight != 0) {
      DrawCinematicLetterbox();
    }
  } else if (g_nGemPickupSubstate == 4) {
    unsigned char *act;
    unsigned char id;
    void **ot;
    void **q;
    ot = g_apOtDepthBinBlock;
    *ot++ = g_pGemPickupStatueActor;
    *ot++ = g_pGemPickupSpyroMirrorActor;
    *ot = 0;
    q = &g_apOtDepthBinBlock[2304];
    act = (unsigned char *)g_pActorListBase;
    id = act[0x48];
    while (id != 0xFF) {
      if (*(short *)(act + 0x36) == 0xFB && id < 0x80) {
        *q++ = act;
      }
      act += 0x58;
      id = act[0x48];
    }
    *q = 0;
    if (g_nGemPickupTimer < 0x3C) {
      DrawDragonRescuedBanner();
      EnqueuePendingSpritePrims();
    }
    BuildActorDrawList();
    FillWord(&g_pOtDepthBinHead0, 0, 0x900);
    EmitActorDrawList();
    RasterizeSpritePrimQueue();
    DrawEntityDirectionalSpikes();
    DrawSpyroDropShadow();
    SetupFrameOT();
    DrawActors();
    RasterizeEmitList();
    if (g_nGameplayBlocked != 0 || g_nLetterboxBarHeight != 0) {
      DrawCinematicLetterbox();
    }
  } else if (g_nGemPickupSubstate == 5) {
    void **ot;
    void **q;
    int i;
    ot = g_apOtDepthBinBlock;
    i = 0;
    *ot++ = g_pGemPickupStatueActor;
    *ot = 0;
    q = &g_apOtDepthBinBlock[2304];
    icon = &g_abHudIconActorRecords[0x1B8];
  icon5:
    *q++ = icon;
    icon += 0x58;
    i += 1;
    if (i < 3) {
      goto icon5;
    }
    *q = 0;
    BuildActorDrawList();
    if (g_anActorDrawList[0] != 0) {
      short *dl = (short *)g_anActorDrawList;
      int scale;
      scale = D_8006F3C0[*(volatile int *)&g_nGemPickupTimer];
      dl[0x0E] = (dl[0x0E] * scale) >> 12;
      dl[0x0F] = (dl[0x0F] * scale) >> 12;
      dl[0x10] = (dl[0x10] * scale) >> 12;
      if (g_nGemPickupTimer >= 9) {
        scale = LookupCosine((g_nGemPickupTimer - 8) * 42);
        dl[0x11] = (dl[0x11] * scale) >> 12;
        dl[0x12] = (dl[0x12] * scale) >> 12;
        dl[0x13] = (dl[0x13] * scale) >> 12;
      }
    }
    FillWord(&g_pOtDepthBinHead0, 0, 0x900);
    EmitActorDrawList();
    RasterizeSpritePrimQueue();
    RasterizePairedActor();
    DrawSpyroDropShadow();
    SetupFrameOT();
    DrawActors();
    RasterizeEmitList();
    if (g_nGameplayBlocked != 0 || g_nLetterboxBarHeight != 0) {
      DrawCinematicLetterbox();
    }
  } else if (g_nGemPickupSubstate == 6) {
    unsigned char *icon;
    void **q;
    int i;
    q = &g_apOtDepthBinBlock[2304];
    i = 0;
    icon = &g_abHudIconActorRecords[0x1B8];
  icon6:
    *q++ = icon;
    icon += 0x58;
    i += 1;
    if (i < 3) {
      goto icon6;
    }
    *q = 0;
    FillWord(&g_pOtDepthBinHead0, 0, 0x900);
    RasterizeSpritePrimQueue();
    RasterizePairedActor();
    DrawSpyroDropShadow();
    SetupFrameOT();
    DrawActors();
    RasterizeEmitList();
    if (g_nGameplayBlocked != 0 || g_nLetterboxBarHeight != 0) {
      DrawCinematicLetterbox();
    }
  } else if (g_nGemPickupSubstate == 7) {
    if (g_nGemPickupTimer < 0x10) {
      int i;
      void **q;
      i = 0;
      q = &g_apOtDepthBinBlock[2304];
      icon = &g_abHudIconActorRecords[0x1B8];
    icon7:
      *q++ = icon;
      icon += 0x58;
      i += 1;
      if (i < 3) {
        goto icon7;
      }
      *q = 0;
      FillWord(&g_pOtDepthBinHead0, 0, 0x900);
      RasterizeSpritePrimQueue();
    } else {
      BuildRenderEntityLists();
      BuildActorDrawList();
      FillWord(&g_pOtDepthBinHead0, 0, 0x900);
      EmitActorDrawList();
      BuildSecondaryActorDrawList();
      EmitSecondaryActorPrimitives();
      RasterizeSpritePrimQueue();
      DrawEntityDirectionalSpikes();
    }
    RasterizePairedActor();
    DrawSpyroDropShadow();
    SetupFrameOT();
    DrawActors();
    RasterizeEmitList();
    if (g_nGameplayBlocked != 0 || g_nLetterboxBarHeight != 0) {
      DrawCinematicLetterbox();
    }
    {
      int tint = g_nGemPickupFullscreenTintLevel;
      if (tint != 0) {
        DrawFullscreenTint(1, tint, tint, tint);
      }
    }
  }

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
