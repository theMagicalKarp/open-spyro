/* Level7_BalloonistCutscene_Draw (0x8007D838, level_7_peace_keepers_dry_canyon overlay, 2568 bytes).
 *
 * Balloonist cutscene DRAW half (gamestate 0xC). Dispatches on the substate
 * word g_nActorCutsceneSubstate[0]:
 *   < 2  — draw the "<world> THE BALLOONIST" title card, then rock every glyph
 *          the draw just appended (its rotZ roll angle, +0x46), sampling the
 *          cosine LUT g_anCosineLut with a per-glyph phase step of 0xC.
 *   == 1 — plus one of three sub-menus selected by g_nActorCutsceneSubstate[3]:
 *            * outside 0x1E..0x20: the dialogue page whose line range is
 *              g_abBalloonistDialoguePageStarts[m]..g_abBalloonistDialoguePageEnds[m] into g_apBalloonistDialogueLines,
 *            * 0x1E: the greeting + "STAY HERE" + the roster of visited worlds,
 *            * 0x1F/0x20: the confirm prompt + "GO TO <name>".
 *          The highlighted entry is the one whose index equals g_nActorCutsceneMenuCursor;
 *          its glyphs get the pulse, the rest stay flat.
 *   >= 4 — append the cutscene actor (g_pActorCutsceneRecord) to the main draw list at
 *          g_apMainDrawEntityList instead.
 * Every path finishes with the frame pipeline (BuildRenderEntityLists / ComposeFrameScene /
 * SetupFrameOT / DrawPortalsAndSky / RasterizeEmitList).
 *
 * g_pSpriteRecordWriteCursor is the sprite-record write cursor; records are 0x58 bytes and it
 * grows downward, so "walk what the draw just added" is
 * `for (rec = saved - 0x58; rec >= g_pSpriteRecordWriteCursor; rec -= 0x58)`.
 *
 * Word-identical body (branch displacements aside) in all ten hub overlays.
 */

#include "level_globals.h"
#include "level_funcs.h"
#include "level_balloonist_cutscene.h"

static const char g_szGoTo[12];

void Level7_BalloonistCutscene_Draw(void) {
  int titlePos[3];
  int titleGlyph[3];
  char title[32];
  int pagePos[3];
  int pageGlyph[3];
  int menuPos[3];
  int menuGlyph[3];
  int askPos[3];
  int askGlyph[3];
  char dest[40];
  Actor *rec;
  int cursor;
  int pageCursor;
  int titlePhase;
  int pagePhase;
  int greetPhase;
  int rosterPhase;
  int promptPhase;
  int namePhase;
  int line;
  int world;
  int limit;
  int slot;

  if (g_nActorCutsceneSubstate[0] < ACS_STREAM_WAIT) {
    if (g_nActorCutsceneSubstate[0] < ACS_BOARD) {
      cursor = g_pSpriteRecordWriteCursor;
      sprintf(title, "%s THE BALLOONIST", g_apBalloonistNames[g_nActiveLevelId / 10]);
      titlePos[0] = 0x108 - strlen(title) * 8;
      titlePos[1] = 0xCC;
      titlePos[2] = TEXT_DEPTH;
      titleGlyph[0] = 0x10;
      titleGlyph[1] = 1;
      titleGlyph[2] = TEXT_SMALL_DEPTH;
      BuildTextSprites(title, titlePos, titleGlyph, 0x12, TINT_HUD);
      for (cursor -= ACTOR_SIZE, titlePhase = 0; cursor >= g_pSpriteRecordWriteCursor;
           cursor -= ACTOR_SIZE, titlePhase += 1) {
        ((Actor *)cursor)->rotZ =
            g_anCosineLut[(g_nActorCutsceneSubstate[1] * 4 + titlePhase * 0xC) & 0xFF] >> 8;
      }
    }
    if (g_nActorCutsceneSubstate[0] == ACS_DIALOGUE) {
      if (g_nActorCutsceneSubstate[3] < BPAGE_ROSTER || g_nActorCutsceneSubstate[3] > BPAGE_CONFIRM_HINT) {
        DrawShadedMenuBox(0x48, 0x1B8, 0x1C, 0x68);
        pagePos[1] = 0x2C;
        pagePos[2] = TEXT_DEPTH;
        pageGlyph[0] = 0x10;
        pageGlyph[1] = 1;
        pageGlyph[2] = TEXT_SMALL_DEPTH;
        line = g_abBalloonistDialoguePageStarts[g_nActorCutsceneSubstate[3]];
        pageCursor = g_pSpriteRecordWriteCursor;
        while (line < g_abBalloonistDialoguePageEnds[g_nActorCutsceneSubstate[3]]) {
          pagePos[0] = 0x5E;
          BuildTextSprites(g_apBalloonistDialogueLines[line], pagePos, pageGlyph, 0x12, TINT_HUD);
          pagePos[1] += 0x12;
          line += 1;
        }
        if (g_nActorCutsceneSubstate[1] < 0x50) {
          limit = pageCursor - g_nActorCutsceneSubstate[1] * ACTOR_SIZE;
          if (limit >= g_pSpriteRecordWriteCursor) {
            g_pSpriteRecordWriteCursor = limit;
          }
        }
        for (pageCursor -= ACTOR_SIZE, pagePhase = 0; pageCursor >= g_pSpriteRecordWriteCursor;
             pageCursor -= ACTOR_SIZE, pagePhase += 1) {
          ((Actor *)pageCursor)->rotZ =
              g_anCosineLut[(g_nActorCutsceneSubstate[1] * 4 + pagePhase * 0xC) & 0xFF] >> 8;
        }
        if (g_nActorCutsceneSubstate[1] >= 0x50) {
          pagePos[0] = 0x19C;
          pagePos[1] = 0x60;
          pagePos[2] = TEXT_DEPTH;
          EnqueueBlinkingMarkerSprite(pagePos, g_nActorCutsceneSubstate[1], 0);
        }
      } else if (g_nActorCutsceneSubstate[3] == BPAGE_ROSTER) {
        menuGlyph[0] = 0xE;
        menuGlyph[2] = 0x1600;
        menuPos[2] = TEXT_SMALL_DEPTH;
        menuPos[0] = 0xF0;
        menuGlyph[1] = 1;
        menuPos[1] = 0x24;
        if (g_nActorCutsceneSubstate[4] == 0) {
          BuildTextSprites("HOP ON, SPYRO!", menuPos, menuGlyph, 0x10, TINT_HUD);
        } else if (g_nActorCutsceneSubstate[4] == 1) {
          BuildTextSprites("NEED A LIFT?", menuPos, menuGlyph, 0x10, TINT_HUD);
        } else {
          BuildTextSprites("WHERE TO, SPYRO?", menuPos, menuGlyph, 0x10, TINT_HUD);
        }
        menuPos[0] = 0x110;
        rec = (Actor *)g_pSpriteRecordWriteCursor - 1;
        menuPos[1] += 0x14;
        BuildTextSprites("STAY HERE", menuPos, menuGlyph, 0x10, TINT_HUD);
        menuPos[1] += 0xF;
        if (g_nActorCutsceneSubstate[2] == 0) {
          for (greetPhase = 0; rec >= (Actor *)g_pSpriteRecordWriteCursor;
               rec--, greetPhase += 1) {
            rec->rotZ = g_anCosineLut[(g_nActorCutsceneSubstate[1] * 4 + greetPhase * 0xC) & 0xFF] >> 8;
          }
        }
        slot = 1;
        for (world = 0; world < 6; world++) {
          if (g_nActiveLevelId / 10 - 1 != world && g_abWorldBalloonistState[world] == BALLOONIST_OPEN) {
            menuPos[0] = 0x110;
            rec = (Actor *)g_pSpriteRecordWriteCursor - 1;
            BuildTextSprites(g_apWorldNameByLevelTens[world], menuPos, menuGlyph, 0x10, TINT_HUD);
            menuPos[1] += 0xF;
            if (g_nActorCutsceneSubstate[2] == slot) {
              for (rosterPhase = 0; rec >= (Actor *)g_pSpriteRecordWriteCursor;
                   rec--, rosterPhase += 1) {
                rec->rotZ = g_anCosineLut[(g_nActorCutsceneSubstate[1] * 4 + rosterPhase * 0xC) & 0xFF] >> 8;
              }
            }
            slot += 1;
          }
        }
        DrawShadedMenuBox(0xE0, 0x1D8, 0x19, slot * 0xF + 0x33);
        EmitRadialShadedLine(0xF0, 0x2E, 0x1A0, 0x2E);
        menuPos[0] = 0xF4;
        menuPos[2] = TEXT_DEPTH;
        menuPos[1] = g_nActorCutsceneSubstate[2] * 0xF + 0x36;
        EnqueueBlinkingMarkerSprite(menuPos, g_nActorCutsceneSubstate[1], 0);
      } else if (g_nActorCutsceneSubstate[3] == BPAGE_CONFIRM_NEXT || g_nActorCutsceneSubstate[3] == BPAGE_CONFIRM_HINT) {
        DrawShadedMenuBox(0x48, 0x1B8, 0x1C, 0x66);
        EmitRadialShadedLine(0x64, 0x37, 0x184, 0x37);
        askPos[0] = 0x60;
        askPos[2] = TEXT_DEPTH;
        askPos[1] = g_nActorCutsceneSubstate[2] * 0x13 + 0x43;
        EnqueueBlinkingMarkerSprite(askPos, g_nActorCutsceneSubstate[1], 0);
        askGlyph[0] = 0x10;
        askGlyph[2] = TEXT_SMALL_DEPTH;
        askPos[1] = 0x2C;
        askGlyph[1] = 1;
        askPos[2] = TEXT_DEPTH;
        askPos[0] = 0x60;
        if (g_nActorCutsceneSubstate[3] == BPAGE_CONFIRM_NEXT) {
          BuildTextSprites("ARE YOU READY TO GO?", askPos, askGlyph, 0x12, TINT_HUD);
        } else {
          BuildTextSprites("WOULD YOU LIKE TO GO?", askPos, askGlyph, 0x12, TINT_HUD);
        }
        askPos[0] = 0x80;
        rec = (Actor *)g_pSpriteRecordWriteCursor - 1;
        askPos[1] += 0x19;
        BuildTextSprites("STAY HERE", askPos, askGlyph, 0x12, TINT_HUD);
        askPos[1] += 0x13;
        if (g_nActorCutsceneSubstate[2] == 0) {
          for (promptPhase = 0; rec >= (Actor *)g_pSpriteRecordWriteCursor;
               rec--, promptPhase += 1) {
            rec->rotZ = g_anCosineLut[(g_nActorCutsceneSubstate[1] * 4 + promptPhase * 0xC) & 0xFF] >> 8;
          }
        }
        askPos[0] = 0x80;
        rec = (Actor *)g_pSpriteRecordWriteCursor - 1;
        if (g_nActorCutsceneSubstate[3] == BPAGE_CONFIRM_NEXT) {
          sprintf(dest, g_szGoTo, g_apWorldNameByLevelTens[g_nActiveLevelId / 10]);
        } else {
          sprintf(dest, g_szGoTo,
                        g_apLevelNameByWorldRow[(g_nNextLevelId / 10) * 6 + g_nNextLevelId % 10]);
        }
        BuildTextSprites(dest, askPos, askGlyph, 0x12, TINT_HUD);
        if (g_nActorCutsceneSubstate[2] == 1) {
          for (namePhase = 0; rec >= (Actor *)g_pSpriteRecordWriteCursor;
               rec--, namePhase += 1) {
            rec->rotZ = g_anCosineLut[(g_nActorCutsceneSubstate[1] * 4 + namePhase * 0xC) & 0xFF] >> 8;
          }
        }
      }
    }
    BuildRenderEntityLists();
    ComposeFrameScene();
    if (g_nActorCutsceneSubstate[0] < ACS_BOARD) {
      g_apSpritePrimQueue = 0;
      EnqueuePendingSpritePrims();
      RasterizeSpritePrimQueue();
    }
  } else {
    int *node;

    BuildRenderEntityLists();
    node = g_apMainDrawEntityList;
    if (*node != 0) {
      do {
        node++;
      } while (*node != 0);
    }
    node[1] = 0;
    node[0] = (int)g_pActorCutsceneRecord;
    ComposeFrameScene();
  }
  SetupFrameOT();
  DrawPortalsAndSky();
  RasterizeEmitList();
}

static const char g_szGoTo[12] = {'G',  'O',  ' ',  'T',
                                  'O',  ' ',  '%',  's',
                                  '\0', 0x3B, 0x0D, 0x0A};
