#include "dragon_rescue.h"
#include "frame_db.h"
#include "funcs.h"
#include "globals.h"
#include "hud.h"
#include "main_shared.h"

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
