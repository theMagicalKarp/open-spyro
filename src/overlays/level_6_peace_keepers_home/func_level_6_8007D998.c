/* func_level_6_8007D998 (0x8007D998, level_6_peace_keepers_home overlay, 2568 bytes).
 *
 * Balloonist cutscene DRAW half (gamestate 0xC). Dispatches on the substate
 * word D_800777E8[0]:
 *   < 2  — draw the "<world> THE BALLOONIST" title card, then pulse the alpha
 *          (+0x46) of every sprite record the draw just appended, sampling the
 *          cosine LUT D_8006CC78 with a per-glyph phase step of 0xC.
 *   == 1 — plus one of three sub-menus selected by D_800777E8[3]:
 *            * outside 0x1E..0x20: the dialogue page whose line range is
 *              D_8006F89C[m]..D_8006F89D[m] into D_8006F8C4,
 *            * 0x1E: the greeting + "STAY HERE" + the roster of visited worlds,
 *            * 0x1F/0x20: the confirm prompt + "GO TO <name>".
 *          The highlighted entry is the one whose index equals D_800777F0;
 *          its glyphs get the pulse, the rest stay flat.
 *   >= 4 — append the pending OT node (D_80077850) to the depth-bin list at
 *          D_8006FCF4 instead.
 * Every path finishes with the frame pipeline (func_800521C0 / func_80019698 /
 * func_8002B9CC / func_80050BD0 / func_800573C8).
 *
 * D_80075710 is the sprite-record write cursor; records are 0x58 bytes and it
 * grows downward, so "walk what the draw just added" is
 * `for (rec = saved - 0x58; rec >= D_80075710; rec -= 0x58)`.
 *
 * Word-identical body (branch displacements aside) in all ten hub overlays.
 */

extern void func_80062FD4();  /* sprintf */
extern int func_8006276C();   /* strlen */
extern void func_800181AC();  /* BuildTextSpriteChain */
extern void func_8001844C();  /* EmitRadialShadedLine */
extern void func_8001860C();  /* DrawShadedMenuBox */
extern void func_80018534();  /* EnqueueBlinkingMarkerSprite */
extern void func_80018880();  /* EnqueuePendingSpritePrims */
extern void func_80022A2C();  /* RasterizeSpritePrimQueue */
extern void func_800521C0();  /* BuildRenderEntityLists */
extern void func_80019698();  /* ComposeFrameScene */
extern void func_8002B9CC();  /* SetupFrameOT */
extern void func_80050BD0();  /* DrawActors */
extern void func_800573C8();  /* RasterizeEmitList */

extern int D_800777E8[];            /* [0] substate [1] phase [2] selected [3] submode [4] greeting */
extern int D_80075710;              /* sprite-record write cursor */
extern int D_8007596C;              /* active level id */
extern int D_800758B4;              /* current level id */
extern int D_80077850;              /* pending OT node */
extern int D_800720F4;              /* sprite prim queue head */
extern int D_8006FCF4[];            /* OT depth-bin list */
extern unsigned short D_8006CC78[]; /* cosine LUT */
extern unsigned char D_800758D0[];  /* per-world visited flags */
extern unsigned char D_8006F89C[];  /* dialogue page first line */
extern unsigned char D_8006F89D[];  /* dialogue page end line */
extern char *D_8006F880[];          /* per-world balloonist names */
extern char *D_8006F8C4[];          /* dialogue lines */
extern char *D_8006F7D4[];          /* world names */
extern char *D_8006F7D8[];          /* level names */

static const char g_szGoTo[12];

void func_level_6_8007D998(void) {
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

  if (D_800777E8[0] < 4) {
    if (D_800777E8[0] < 2) {
      cursor = D_80075710;
      func_80062FD4(title, "%s THE BALLOONIST", D_8006F880[D_8007596C / 10]);
      titlePos[0] = 0x108 - func_8006276C(title) * 8;
      titlePos[1] = 0xCC;
      titlePos[2] = 0x1100;
      titleColor[0] = 0x10;
      titleColor[1] = 1;
      titleColor[2] = 0x1400;
      func_800181AC(title, titlePos, titleColor, 0x12, 0xB);
      for (cursor -= 0x58, titlePhase = 0; cursor >= D_80075710;
           cursor -= 0x58, titlePhase += 1) {
        ((unsigned char *)cursor)[0x46] =
            D_8006CC78[(D_800777E8[1] * 4 + titlePhase * 0xC) & 0xFF] >> 8;
      }
    }
    if (D_800777E8[0] == 1) {
      if (D_800777E8[3] < 0x1E || D_800777E8[3] > 0x20) {
        func_8001860C(0x48, 0x1B8, 0x1C, 0x68);
        pagePos[1] = 0x2C;
        pagePos[2] = 0x1100;
        pageColor[0] = 0x10;
        pageColor[1] = 1;
        pageColor[2] = 0x1400;
        line = D_8006F89C[D_800777E8[3]];
        pageCursor = D_80075710;
        while (line < D_8006F89D[D_800777E8[3]]) {
          pagePos[0] = 0x5E;
          func_800181AC(D_8006F8C4[line], pagePos, pageColor, 0x12, 0xB);
          pagePos[1] += 0x12;
          line += 1;
        }
        if (D_800777E8[1] < 0x50) {
          limit = pageCursor - D_800777E8[1] * 0x58;
          if (limit >= D_80075710) {
            D_80075710 = limit;
          }
        }
        for (pageCursor -= 0x58, pagePhase = 0; pageCursor >= D_80075710;
             pageCursor -= 0x58, pagePhase += 1) {
          ((unsigned char *)pageCursor)[0x46] =
              D_8006CC78[(D_800777E8[1] * 4 + pagePhase * 0xC) & 0xFF] >> 8;
        }
        if (D_800777E8[1] >= 0x50) {
          pagePos[0] = 0x19C;
          pagePos[1] = 0x60;
          pagePos[2] = 0x1100;
          func_80018534(pagePos, D_800777E8[1], 0);
        }
      } else if (D_800777E8[3] == 0x1E) {
        menuColor[0] = 0xE;
        menuColor[2] = 0x1600;
        menuPos[2] = 0x1400;
        menuPos[0] = 0xF0;
        menuColor[1] = 1;
        menuPos[1] = 0x24;
        if (D_800777E8[4] == 0) {
          func_800181AC("HOP ON, SPYRO!", menuPos, menuColor, 0x10, 0xB);
        } else if (D_800777E8[4] == 1) {
          func_800181AC("NEED A LIFT?", menuPos, menuColor, 0x10, 0xB);
        } else {
          func_800181AC("WHERE TO, SPYRO?", menuPos, menuColor, 0x10, 0xB);
        }
        menuPos[0] = 0x110;
        rec = (unsigned char *)D_80075710 - 0x58;
        menuPos[1] += 0x14;
        func_800181AC("STAY HERE", menuPos, menuColor, 0x10, 0xB);
        menuPos[1] += 0xF;
        if (D_800777E8[2] == 0) {
          for (greetPhase = 0; rec >= (unsigned char *)D_80075710;
               rec -= 0x58, greetPhase += 1) {
            rec[0x46] = D_8006CC78[(D_800777E8[1] * 4 + greetPhase * 0xC) & 0xFF] >> 8;
          }
        }
        slot = 1;
        for (world = 0; world < 6; world++) {
          if (D_8007596C / 10 - 1 != world && D_800758D0[world] == 2) {
            menuPos[0] = 0x110;
            rec = (unsigned char *)D_80075710 - 0x58;
            func_800181AC(D_8006F7D4[world], menuPos, menuColor, 0x10, 0xB);
            menuPos[1] += 0xF;
            if (D_800777E8[2] == slot) {
              for (rosterPhase = 0; rec >= (unsigned char *)D_80075710;
                   rec -= 0x58, rosterPhase += 1) {
                rec[0x46] = D_8006CC78[(D_800777E8[1] * 4 + rosterPhase * 0xC) & 0xFF] >> 8;
              }
            }
            slot += 1;
          }
        }
        func_8001860C(0xE0, 0x1D8, 0x19, slot * 0xF + 0x33);
        func_8001844C(0xF0, 0x2E, 0x1A0, 0x2E);
        menuPos[0] = 0xF4;
        menuPos[2] = 0x1100;
        menuPos[1] = D_800777E8[2] * 0xF + 0x36;
        func_80018534(menuPos, D_800777E8[1], 0);
      } else if (D_800777E8[3] == 0x1F || D_800777E8[3] == 0x20) {
        func_8001860C(0x48, 0x1B8, 0x1C, 0x66);
        func_8001844C(0x64, 0x37, 0x184, 0x37);
        askPos[0] = 0x60;
        askPos[2] = 0x1100;
        askPos[1] = D_800777E8[2] * 0x13 + 0x43;
        func_80018534(askPos, D_800777E8[1], 0);
        askColor[0] = 0x10;
        askColor[2] = 0x1400;
        askPos[1] = 0x2C;
        askColor[1] = 1;
        askPos[2] = 0x1100;
        askPos[0] = 0x60;
        if (D_800777E8[3] == 0x1F) {
          func_800181AC("ARE YOU READY TO GO?", askPos, askColor, 0x12, 0xB);
        } else {
          func_800181AC("WOULD YOU LIKE TO GO?", askPos, askColor, 0x12, 0xB);
        }
        askPos[0] = 0x80;
        rec = (unsigned char *)D_80075710 - 0x58;
        askPos[1] += 0x19;
        func_800181AC("STAY HERE", askPos, askColor, 0x12, 0xB);
        askPos[1] += 0x13;
        if (D_800777E8[2] == 0) {
          for (promptPhase = 0; rec >= (unsigned char *)D_80075710;
               rec -= 0x58, promptPhase += 1) {
            rec[0x46] = D_8006CC78[(D_800777E8[1] * 4 + promptPhase * 0xC) & 0xFF] >> 8;
          }
        }
        askPos[0] = 0x80;
        rec = (unsigned char *)D_80075710 - 0x58;
        if (D_800777E8[3] == 0x1F) {
          func_80062FD4(dest, g_szGoTo, D_8006F7D4[D_8007596C / 10]);
        } else {
          func_80062FD4(dest, g_szGoTo,
                        D_8006F7D8[(D_800758B4 / 10) * 6 + D_800758B4 % 10]);
        }
        func_800181AC(dest, askPos, askColor, 0x12, 0xB);
        if (D_800777E8[2] == 1) {
          for (namePhase = 0; rec >= (unsigned char *)D_80075710;
               rec -= 0x58, namePhase += 1) {
            rec[0x46] = D_8006CC78[(D_800777E8[1] * 4 + namePhase * 0xC) & 0xFF] >> 8;
          }
        }
      }
    }
    func_800521C0();
    func_80019698();
    if (D_800777E8[0] < 2) {
      D_800720F4 = 0;
      func_80018880();
      func_80022A2C();
    }
  } else {
    int *node;

    func_800521C0();
    node = D_8006FCF4;
    if (*node != 0) {
      do {
        node++;
      } while (*node != 0);
    }
    node[1] = 0;
    node[0] = D_80077850;
    func_80019698();
  }
  func_8002B9CC();
  func_80050BD0();
  func_800573C8();
}

static const char g_szGoTo[12] = {'G',  'O',  ' ',  'T',
                                  'O',  ' ',  '%',  's',
                                  '\0', 0x09, 0x53, 0x70};
