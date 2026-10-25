/* Credits_Draw (0x8007BFD0, credits overlay).
 *
 * Credits-sequence frame draw. While the sequence gate (g_nGamestate0fMode) is -1,
 * only scrolls the double-height framebuffer: DrawSync/VSync, then MoveImage
 * (MoveImage) of the 512x224 strip at y=8 (front env active) or y=0xF8
 * to the opposite half, and a final DrawSync. Otherwise draws a full frame:
 * copies the fog colour (g_dwWorldFogColor) into both frame DRAWENV backgrounds,
 * builds one 0x58-byte sprite prim per credits line (records at g_pCreditsGlyphPool,
 * 0x1C stride; prim cursor g_pSpriteRecordWriteCursor grows downward; record type 0x115 gets
 * a +0x29 glyph offset and depth 0x1C00), submits the pending sprite queue,
 * clears the OT bins, emits the static world, then runs the standard
 * 2-vblank pace loop and PutDispEnv/PutDrawEnv/DrawOTag submit.
 */

#include "credits.h"
#include "fog_color.h"
#include "level_globals.h"
#include "level_funcs.h"
#include "actor.h"
#include "main_shared.h"
#include "level_emit_particles.h"
#include "level_flight_results.h"
#include "frame_db.h"


/* The sprite record being written (Actor layout). */
#define SREC ((Actor *)g_pSpriteRecordWriteCursor)

void Credits_Draw(void) {
  int i;

  if (g_nGamestate0fMode == -1) {
    struct {
      short x, y, w, h;
    } rect;
    int y;
    DrawSync(0);
    VSync(0);
    y = 8;
    rect.x = 0;
    if (g_pActiveFrameDrawEnv != g_abFrameDrawEnv0) {
      y = 0xF8;
    }
    rect.w = 0x200;
    rect.y = y;
    rect.h = 0xE0;
    MoveImage(&rect, 0, 0x100 - y);
    DrawSync(0);
    return;
  }
  {
    int r = g_dwWorldFogColor.r;
    int g = g_dwWorldFogColor.g;
    int b = g_dwWorldFogColor.b;
    g_bFrameDrawEnv0BgR.r = r;
    g_bFrameDrawEnv0BgR.g = g;
    g_bFrameDrawEnv0BgR.b = b;
    g_bFrameDrawEnv1BgR.r = r;
    g_bFrameDrawEnv1BgR.g = g;
    g_bFrameDrawEnv1BgR.b = b;
  }
  for (i = 0; i < g_nCreditsGlyphCount; i++) {
    {
      CredGlyph *glyph;
      char *prim = g_pSpriteRecordWriteCursor - 0x58;
      g_pSpriteRecordWriteCursor = prim;
      glyph = (CredGlyph *)(g_pCreditsGlyphPool + i * 0x1C);
      FillWord(prim, 0, 0x58);
      SREC->posX = glyph->x;
      SREC->posY = glyph->y;
      SREC->posZ = glyph->z;
      SREC->rotX = glyph->r;
      if (glyph->glyph == 0x115) {
        SREC->rotX += 0x29;
        SREC->posZ = 0x1C00;
      }
      SREC->rotY = glyph->g;
      SREC->rotZ = glyph->b;
      SREC->type = (unsigned short)glyph->glyph;
      SREC->tintMode = 0xB;
      SREC->cullRadius = 0xFF;
      SREC->otBias = 0x7F;
    }
  }
  {
    int n = g_nGenericCountdown;
    if (n != 0) {
      int s = n << 4;
      DrawFullscreenTint(2, s, s, s);
    }
  }
  {
    int *q = g_apSpritePrimQueue;
    char *ot;
    q[0] = 0;
    EnqueuePendingSpritePrims();
    ot = (char *)q - 0x2400;
    FillWord(ot, 0, 0x900);
    RasterizeSpritePrimQueue();
    FillBlock16(ot, 0, 0x1C00);
    g_nFarPlaneDrawDistance = 0x28000;
    RenderWorldChunks(-1);
    EmitStaticActorMeshList(-1, g_anCameraViewMtx, (char *)g_anCameraViewMtx - 0x14);
  }
  DrawSync(0);
  if (g_nDeathRespawnPending != 0) {
    VSync(0);
  }
  {
    int *end;
    *(end = g_nVsyncFrameEndCount) = VSync(-1);
    if (end[0] - g_nVsyncFramePaceAnchor < 2) {
      int *p = end;
      do {
        VSync(0);
        p[0] = VSync(-1);
      } while (p[0] - p[-1] < 2);
    }
  }
  g_nVsyncFramePaceAnchor = VSync(-1);
  PutDispEnv(&FRAME_DB(g_pActiveFrameDrawEnv)->disp);
  PutDrawEnv(g_pActiveFrameDrawEnv);
  DrawOTag(LinkOTPrimitives(0x800));
}
