/* Source order of the straight-line block is enforced by gcc 2.7.2
 * true_dependence's "both MEMs volatile" clause: the anchor/display/roll/hold
 * stores and the idx/counter/lives loads are VOL(), the cached-count stores are
 * not. `dragons` is a `$2` register local: as a pseudo it loses v0 to the
 * short-lived `lives` qty and the counter batch rotates. The frame needs an
 * unreferenced 8-byte local (`pad[2]`). */

#include "vecmath.h"

#define VOL(x) (*(volatile __typeof__(x) *)&(x))
#include "funcs.h"
#include "globals.h"
#include "hud.h"

/* HUD init for entering a gameplay-with-HUD state (0x80054600, 0x388).

   Wipes the 12 big-icon actor records, then per slot hides the render record,
   seeds its rest position from g_anHudIconRestPos and writes the default
   render bytes (+0x47 OT bias, +0x49/+0x50 colour, +0x4F flags). Wipes the
   sprite-rect table and copies the 12 digit rest rects into it. Places the
   four fixed HUD anchors, arms all five counter state machines (phase and
   hold timer cleared), snapshots the live gem/dragon/life/egg/butterfly
   counts, and draws the initial digits for the gem, dragon and life counters.

   With `layout_butterflies` set it also clears the world-egg roll request and
   lays the sparx-food butterflies out around icon record 10's screen position
   (records +0x37C/+0x380) using the g_abHudButterflyRingXY ring offsets. */

void InitHudCounters(int layout_butterflies) {
  short *anchor;
  int *icon;
  byte *rec;
  int *pos;
  int i;
  int off;
  int vec;
  int rect;
  int ring;
  int food;
  int white;
  int idx;
  int gems;
  int lives;
  int rectY;
  register int dragons asm("$2");
  int worldEggs;
  int levelEggs;
  int count;
  int size;
  int dst;
  int pad[2];
  FillWord(g_abHudIconActorRecords, 0, 0x420);
  i = 0;
  white = 0xFF;
  pos = &HUD_ICON(0)->posX;
  rec = g_abHudIconActorRecords;
  off = 0;
  vec = 0;
icon_slot:
  HideActorRenderRecord((int)rec);

  CopyVector(pos, HUD_REST_POS(vec));
  pos += sizeof(Actor) / sizeof(int);
  rec += ACTOR_SIZE;
  HUD_ICON(off)->otBias = 0x20;
  HUD_ICON(off)->cullRadius = white;
  HUD_ICON(off)->subMode = white;
  HUD_ICON(off)->tintMode = 0x0B;
  off += ACTOR_SIZE;
  i += 1;
  vec += 0xC;
  if (i < 12) {
    goto icon_slot;
  }
  FillWord(g_anHudSpriteRects, 0, 0x100);
  rect = 0;
  do {
    HUD_RECT(g_anHudSpriteRects, rect)->x =
        HUD_RECT(g_anHudDigitRestRects, rect)->x;
    HUD_RECT(g_anHudSpriteRects, rect)->y =
        HUD_RECT(g_anHudDigitRestRects, rect)->y;
    HUD_RECT(g_anHudSpriteRects, rect)->w =
        HUD_RECT(g_anHudDigitRestRects, rect)->w;
    HUD_RECT(g_anHudSpriteRects, rect)->h =
        HUD_RECT(g_anHudDigitRestRects, rect)->h;
    rect += 8;
  } while (rect < 0x60);
  anchor = &HUD_ICON(4 * ACTOR_SIZE)->type;
  idx = VOL(g_nLevelIntroIndex);
  VOL(anchor[0]) = 0x1D7;
  VOL(HUD_ICON(7 * ACTOR_SIZE)->type) = 0x1FA;
  VOL(HUD_ICON(10 * ACTOR_SIZE)->type) = 0x3A;
  VOL(HUD_ICON(11 * ACTOR_SIZE)->type) = 0xAD;
  VOL(HUD_ICON(11 * ACTOR_SIZE)->rotZ) = 0x40;
  VOL(g_abHudCounterDisplayState[0]) = 1;
  VOL(g_abHudCounterDisplayState[1]) = 1;
  VOL(g_abHudCounterDisplayState[2]) = 1;
  VOL(g_abHudCounterDisplayState[3]) = 1;
  VOL(g_abHudCounterDisplayState[4]) = 1;
  dragons = VOL(g_nWorldDragonsRescued);
  worldEggs = VOL(g_nWorldEggsRecovered);
  levelEggs = VOL(g_nLevelEggCount);
  food = VOL(g_nSparxFoodCount);
  VOL(g_bHudLevelGemRollPhase) = 0;
  VOL(g_bHudDragonRollPhase) = 0;
  VOL(g_bHudLivesRollPhase) = 0;
  VOL(g_bHudWorldEggRollPhase) = 0;
  VOL(g_bHudLevelEggRollPhase) = 0;
  VOL(g_nHudLevelGemHoldTimer) = 0;
  VOL(g_nHudDragonHoldTimer) = 0;
  VOL(g_nHudLivesHoldTimer) = 0;
  VOL(g_nHudWorldEggHoldTimer) = 0;
  VOL(g_nHudLevelEggHoldTimer) = 0;
  gems = g_anLevelGemsCollected[idx];
  lives = VOL(g_nExtraLives);
  g_nHudDragonCachedCount = dragons;
  g_nHudWorldEggCachedCount = worldEggs;
  g_nHudLevelEggCachedCount = levelEggs;
  g_nHudButterflyCachedCount = food;
  g_nHudLivesCachedCount = lives;
  g_nHudLevelGemCachedCount = gems;
  SetHudCounterDigits(0, 4, gems, 1);
  SetHudCounterDigits(5, 2, g_nHudDragonCachedCount, 1);
  SetHudCounterDigits(8, 2, g_nHudLivesCachedCount, 1);
  if (layout_butterflies != 0) {
    g_nHudWorldEggRollRequest = 0;
    i = 0;
    count = g_nSparxFoodCount;
    if (count > 0) {
      icon = &HUD_ICON(10 * ACTOR_SIZE)->posX;
      size = 8;
      ring = 0;
      dst = 0x60;
      do {
        int x = icon[0];
        i += 1;
        HUD_RECT(g_anHudSpriteRects, dst)->x =
            (g_abHudButterflyRingXY[ring] + x) - 0x1D;
        x = icon[1];
        rectY = (g_abHudButterflyRingXY[ring + 1] + x) - 0x1C;
        HUD_RECT(g_anHudSpriteRects, dst)->w = size;
        HUD_RECT(g_anHudSpriteRects, dst)->h = size;
        HUD_RECT(g_anHudSpriteRects, dst)->y = rectY;
        ring += 2;
        dst += 8;
      } while (i < count);
    }
  }
}
