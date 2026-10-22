/* Level30_BalloonistCutscene_Draw (0x8007CF30, level_30_gnastys_world_gnorc_gnexus overlay, 2568 bytes).
 *
 * Balloonist cutscene DRAW half (gamestate 0xC). Dispatches on the substate
 * word g_nGamestate0cSubstate[0]:
 *   < 2  — draw the "<world> THE BALLOONIST" title card, then pulse the alpha
 *          (+0x46) of every sprite record the draw just appended, sampling the
 *          cosine LUT g_anCosineLut with a per-glyph phase step of 0xC.
 *   == 1 — plus one of three sub-menus selected by g_nGamestate0cSubstate[3]:
 *            * outside 0x1E..0x20: the dialogue page whose line range is
 *              g_abBalloonistDialoguePageStarts[m]..g_abBalloonistDialoguePageEnds[m] into g_apBalloonistDialogueLines,
 *            * 0x1E: the greeting + "STAY HERE" + the roster of visited worlds,
 *            * 0x1F/0x20: the confirm prompt + "GO TO <name>".
 *          The highlighted entry is the one whose index equals g_nGamestate0cMenuCursor;
 *          its glyphs get the pulse, the rest stay flat.
 *   >= 4 — append the pending OT node (g_pGamestate0cRecord) to the depth-bin list at
 *          g_pOtDepthBinHead0 instead.
 * Every path finishes with the frame pipeline (BuildRenderEntityLists / ComposeFrameScene /
 * SetupFrameOT / DrawActors / RasterizeEmitList).
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

extern int g_pGamestate0cRecord;              /* pending OT node */

static const char g_szGoTo[12];

void Level30_BalloonistCutscene_Draw(void) {
  int titlePos[3];
  int titleColor[3];
  char title[32];
  int pagePos[3];
  int pageColor[3];
  int menuPos[3];
  int menuColor[3];
  int askPos[3];
  int askColor[3];
  char dest[40];
  unsigned char *rec;
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

  if (g_nGamestate0cSubstate[0] < 4) {
    if (g_nGamestate0cSubstate[0] < 2) {
      cursor = g_pSpriteRecordWriteCursor;
      sprintf(title, "%s THE BALLOONIST", g_apBalloonistNames[g_nActiveLevelId / 10]);
      titlePos[0] = 0x108 - strlen(title) * 8;
      titlePos[1] = 0xCC;
      titlePos[2] = 0x1100;
      titleColor[0] = 0x10;
      titleColor[1] = 1;
      titleColor[2] = 0x1400;
      BuildTextSprites(title, titlePos, titleColor, 0x12, 0xB);
      for (cursor -= 0x58, titlePhase = 0; cursor >= g_pSpriteRecordWriteCursor;
           cursor -= 0x58, titlePhase += 1) {
        ((unsigned char *)cursor)[0x46] =
            g_anCosineLut[(g_nGamestate0cSubstate[1] * 4 + titlePhase * 0xC) & 0xFF] >> 8;
      }
    }
    if (g_nGamestate0cSubstate[0] == 1) {
      if (g_nGamestate0cSubstate[3] < 0x1E || g_nGamestate0cSubstate[3] > 0x20) {
        DrawShadedMenuBox(0x48, 0x1B8, 0x1C, 0x68);
        pagePos[1] = 0x2C;
        pagePos[2] = 0x1100;
        pageColor[0] = 0x10;
        pageColor[1] = 1;
        pageColor[2] = 0x1400;
        line = g_abBalloonistDialoguePageStarts[g_nGamestate0cSubstate[3]];
        pageCursor = g_pSpriteRecordWriteCursor;
        while (line < g_abBalloonistDialoguePageEnds[g_nGamestate0cSubstate[3]]) {
          pagePos[0] = 0x5E;
          BuildTextSprites(g_apBalloonistDialogueLines[line], pagePos, pageColor, 0x12, 0xB);
          pagePos[1] += 0x12;
          line += 1;
        }
        if (g_nGamestate0cSubstate[1] < 0x50) {
          limit = pageCursor - g_nGamestate0cSubstate[1] * 0x58;
          if (limit >= g_pSpriteRecordWriteCursor) {
            g_pSpriteRecordWriteCursor = limit;
          }
        }
        for (pageCursor -= 0x58, pagePhase = 0; pageCursor >= g_pSpriteRecordWriteCursor;
             pageCursor -= 0x58, pagePhase += 1) {
          ((unsigned char *)pageCursor)[0x46] =
              g_anCosineLut[(g_nGamestate0cSubstate[1] * 4 + pagePhase * 0xC) & 0xFF] >> 8;
        }
        if (g_nGamestate0cSubstate[1] >= 0x50) {
          pagePos[0] = 0x19C;
          pagePos[1] = 0x60;
          pagePos[2] = 0x1100;
          EnqueueBlinkingMarkerSprite(pagePos, g_nGamestate0cSubstate[1], 0);
        }
      } else if (g_nGamestate0cSubstate[3] == 0x1E) {
        menuColor[0] = 0xE;
        menuColor[2] = 0x1600;
        menuPos[2] = 0x1400;
        menuPos[0] = 0xF0;
        menuColor[1] = 1;
        menuPos[1] = 0x24;
        if (g_nGamestate0cSubstate[4] == 0) {
          BuildTextSprites("HOP ON, SPYRO!", menuPos, menuColor, 0x10, 0xB);
        } else if (g_nGamestate0cSubstate[4] == 1) {
          BuildTextSprites("NEED A LIFT?", menuPos, menuColor, 0x10, 0xB);
        } else {
          BuildTextSprites("WHERE TO, SPYRO?", menuPos, menuColor, 0x10, 0xB);
        }
        menuPos[0] = 0x110;
        rec = (unsigned char *)g_pSpriteRecordWriteCursor - 0x58;
        menuPos[1] += 0x14;
        BuildTextSprites("STAY HERE", menuPos, menuColor, 0x10, 0xB);
        menuPos[1] += 0xF;
        if (g_nGamestate0cSubstate[2] == 0) {
          for (greetPhase = 0; rec >= (unsigned char *)g_pSpriteRecordWriteCursor;
               rec -= 0x58, greetPhase += 1) {
            rec[0x46] = g_anCosineLut[(g_nGamestate0cSubstate[1] * 4 + greetPhase * 0xC) & 0xFF] >> 8;
          }
        }
        slot = 1;
        for (world = 0; world < 6; world++) {
          if (g_nActiveLevelId / 10 - 1 != world && g_abWorldBalloonistState[world] == 2) {
            menuPos[0] = 0x110;
            rec = (unsigned char *)g_pSpriteRecordWriteCursor - 0x58;
            BuildTextSprites(g_apWorldNameByLevelTens[world], menuPos, menuColor, 0x10, 0xB);
            menuPos[1] += 0xF;
            if (g_nGamestate0cSubstate[2] == slot) {
              for (rosterPhase = 0; rec >= (unsigned char *)g_pSpriteRecordWriteCursor;
                   rec -= 0x58, rosterPhase += 1) {
                rec[0x46] = g_anCosineLut[(g_nGamestate0cSubstate[1] * 4 + rosterPhase * 0xC) & 0xFF] >> 8;
              }
            }
            slot += 1;
          }
        }
        DrawShadedMenuBox(0xE0, 0x1D8, 0x19, slot * 0xF + 0x33);
        EmitRadialShadedLine(0xF0, 0x2E, 0x1A0, 0x2E);
        menuPos[0] = 0xF4;
        menuPos[2] = 0x1100;
        menuPos[1] = g_nGamestate0cSubstate[2] * 0xF + 0x36;
        EnqueueBlinkingMarkerSprite(menuPos, g_nGamestate0cSubstate[1], 0);
      } else if (g_nGamestate0cSubstate[3] == 0x1F || g_nGamestate0cSubstate[3] == 0x20) {
        DrawShadedMenuBox(0x48, 0x1B8, 0x1C, 0x66);
        EmitRadialShadedLine(0x64, 0x37, 0x184, 0x37);
        askPos[0] = 0x60;
        askPos[2] = 0x1100;
        askPos[1] = g_nGamestate0cSubstate[2] * 0x13 + 0x43;
        EnqueueBlinkingMarkerSprite(askPos, g_nGamestate0cSubstate[1], 0);
        askColor[0] = 0x10;
        askColor[2] = 0x1400;
        askPos[1] = 0x2C;
        askColor[1] = 1;
        askPos[2] = 0x1100;
        askPos[0] = 0x60;
        if (g_nGamestate0cSubstate[3] == 0x1F) {
          BuildTextSprites("ARE YOU READY TO GO?", askPos, askColor, 0x12, 0xB);
        } else {
          BuildTextSprites("WOULD YOU LIKE TO GO?", askPos, askColor, 0x12, 0xB);
        }
        askPos[0] = 0x80;
        rec = (unsigned char *)g_pSpriteRecordWriteCursor - 0x58;
        askPos[1] += 0x19;
        BuildTextSprites("STAY HERE", askPos, askColor, 0x12, 0xB);
        askPos[1] += 0x13;
        if (g_nGamestate0cSubstate[2] == 0) {
          for (promptPhase = 0; rec >= (unsigned char *)g_pSpriteRecordWriteCursor;
               rec -= 0x58, promptPhase += 1) {
            rec[0x46] = g_anCosineLut[(g_nGamestate0cSubstate[1] * 4 + promptPhase * 0xC) & 0xFF] >> 8;
          }
        }
        askPos[0] = 0x80;
        rec = (unsigned char *)g_pSpriteRecordWriteCursor - 0x58;
        if (g_nGamestate0cSubstate[3] == 0x1F) {
          sprintf(dest, g_szGoTo, g_apWorldNameByLevelTens[g_nActiveLevelId / 10]);
        } else {
          sprintf(dest, g_szGoTo,
                        g_apLevelNameByWorldRow[(g_nCurrentLevelId / 10) * 6 + g_nCurrentLevelId % 10]);
        }
        BuildTextSprites(dest, askPos, askColor, 0x12, 0xB);
        if (g_nGamestate0cSubstate[2] == 1) {
          for (namePhase = 0; rec >= (unsigned char *)g_pSpriteRecordWriteCursor;
               rec -= 0x58, namePhase += 1) {
            rec[0x46] = g_anCosineLut[(g_nGamestate0cSubstate[1] * 4 + namePhase * 0xC) & 0xFF] >> 8;
          }
        }
      }
    }
    BuildRenderEntityLists();
    ComposeFrameScene();
    if (g_nGamestate0cSubstate[0] < 2) {
      g_apSpritePrimQueue = 0;
      EnqueuePendingSpritePrims();
      RasterizeSpritePrimQueue();
    }
  } else {
    int *node;

    BuildRenderEntityLists();
    node = g_pOtDepthBinHead0;
    if (*node != 0) {
      do {
        node++;
      } while (*node != 0);
    }
    node[1] = 0;
    node[0] = g_pGamestate0cRecord;
    ComposeFrameScene();
  }
  SetupFrameOT();
  DrawActors();
  RasterizeEmitList();
}

static const char g_szGoTo[12] = {'G',  'O',  ' ',  'T',
                                  'O',  ' ',  '%',  's',
                                  '\0', 0x35, 0x11, 0x3A};
