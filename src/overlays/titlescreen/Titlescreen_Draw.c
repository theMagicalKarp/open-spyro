/* Titlescreen_Draw (0x8007CEE4, titlescreen overlay).
 *
 * Per-frame draw for the whole title screen: the mode/substate-driven sprite
 * overlays (logo fly-in, memory-card menus, save-file picker), then the shared
 * frame tail — background color, actor/environment/cyclorama submission, fade,
 * vsync pacing, and the OT flush.
 *
 * Its substate switch owns the jump table at rodata 0x5C (jtbl_8007AA94),
 * emitted by this object's .rodata and slot-placed there.
 *
 * Verified byte-identical inside the relinked titlescreen.ovl.
 */

#include "titlescreen.h"

#define DRAW_OPTIONS(op1, op2)                                                 \
  if (g_nTitleMode.animTimer > 7) {                                              \
    Titlescreen_EmitSpriteQuad(128, 88, op1,                                        \
                          (g_nTitleMode.selection == 0 &&                 \
                           ((g_nTitleMode.animTimer & 0xF) < 8))                 \
                              ? TITLE_COLOR_HIGHLIGHT                                \
                              : TITLE_COLOR_NORMAL);                                \
    Titlescreen_EmitSpriteQuad(256, 88, op2,                                        \
                          (g_nTitleMode.selection == 1 &&                 \
                           ((g_nTitleMode.animTimer & 0xF) < 8))                 \
                              ? TITLE_COLOR_HIGHLIGHT                                \
                              : TITLE_COLOR_NORMAL);                                \
  }

void Titlescreen_Draw(void) {

  switch (g_nTitleMode.mode) {
  case TITLE_MODE_ATTRACT: {

    if (g_nTitleMode.subState == TITLE_ATTRACT_FLY_IN) {
      /* spyro landed, logo incoming */
      if (g_pPathTableHead->frame >= 1170)
        Titlescreen_EmitSpriteQuad(128, g_abTitleSlideInCurve[g_nTitleMode.animTimer] - 128,
                              TITLE_SPR_LOGO, TITLE_COLOR_NORMAL);
    } else if (g_nTitleMode.subState == TITLE_ATTRACT_PRESS_START) {
      Titlescreen_EmitSpriteQuad(128, 0, TITLE_SPR_LOGO, TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(192, 210, TITLE_SPR_PRESS_START,
                            ((g_nTitleMode.animTimer & 0xF) < 8) * TITLE_COLOR_PROMPT);
    } else if (g_nTitleMode.subState == TITLE_ATTRACT_LOGO_OUT) {
      if (g_nTitleMode.animTimer < 16) {
        Titlescreen_EmitSpriteQuad(128, g_abTitleSlideOutCurve[g_nTitleMode.animTimer] - 128,
                              TITLE_SPR_LOGO, TITLE_COLOR_NORMAL);
      } else {
        Titlescreen_EmitSpriteQuad(108, g_abTitleSlideInCurve[g_nTitleMode.animTimer - 16] - 119,
                              TITLE_SPR_BORDER, TITLE_COLOR_NORMAL);
        Titlescreen_EmitSpriteQuad(255, g_abTitleSlideInCurve[g_nTitleMode.animTimer - 16] - 119,
                              -TITLE_SPR_BORDER, TITLE_COLOR_NORMAL);
      }
    }
    break;
  }
  case TITLE_MODE_CARD_CHECK: {
    /* draw main menu decorative borders (left, then mirror right) */
    Titlescreen_EmitSpriteQuad(108, 9, TITLE_SPR_BORDER, TITLE_COLOR_NORMAL);
    Titlescreen_EmitSpriteQuad(255, 9, -TITLE_SPR_BORDER, TITLE_COLOR_NORMAL);

    switch (g_nTitleMode.subState) {
    case TITLE_CARD_PROBE:
      Titlescreen_EmitSpriteQuad(128, 46, TITLE_SPR_MSG_CHECKING,
                            TITLE_COLOR_NORMAL); /* accessing mem card */
      break;
    case TITLE_CARD_READ: /* memcard slot selected, checking */
      Titlescreen_EmitSpriteQuad(128, 46, TITLE_SPR_MSG_CHECKING, TITLE_COLOR_NORMAL);
      if (g_nTitleMode.selection != 0) {
        Titlescreen_EmitSpriteQuad(128, 106,
                              g_nTitleMode.cardPort +
                                  TITLE_SPR_CARD_SLOT_LABEL,
                              TITLE_COLOR_NORMAL);
      }
      break;
    case TITLE_CARD_NONE:
      Titlescreen_EmitSpriteQuad(128, 46, TITLE_SPR_MSG_NO_CARD, TITLE_COLOR_NORMAL);
      DRAW_OPTIONS(TITLE_SPR_OPT_NO_SAVE, TITLE_SPR_OPT_RETRY);
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nTitleMode.cardPort +
                                TITLE_SPR_CARD_SLOT_LABEL,
                            TITLE_COLOR_NORMAL);
      break;
    case TITLE_CARD_UNUSABLE:
      Titlescreen_EmitSpriteQuad(128, 46, TITLE_SPR_MSG_CARD_UNUSABLE,
                            TITLE_COLOR_NORMAL);
      DRAW_OPTIONS(TITLE_SPR_OPT_NO_SAVE, TITLE_SPR_OPT_RETRY);
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nTitleMode.cardPort +
                                TITLE_SPR_CARD_SLOT_LABEL,
                            TITLE_COLOR_NORMAL);
      break;
    case TITLE_CARD_NO_SAVE_WARNING:
      Titlescreen_EmitSpriteQuad(128, 22, TITLE_SPR_MSG_WARNING, TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(128, 38, TITLE_SPR_MSG_NO_SAVE_L2,
                            TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(128, 54, TITLE_SPR_MSG_NO_SAVE_L3,
                            TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(128, 70, TITLE_SPR_MSG_NO_SAVE_L4,
                            TITLE_COLOR_NORMAL);
      DRAW_OPTIONS(TITLE_SPR_OPT_NO_SAVE, TITLE_SPR_OPT_CANCEL);
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nTitleMode.cardPort +
                                TITLE_SPR_CARD_SLOT_LABEL,
                            TITLE_COLOR_NORMAL);
      break;
    case TITLE_CARD_FORMAT_ASK:
    case TITLE_CARD_FORMAT_CONFIRM:
      if (g_nTitleMode.subState == TITLE_CARD_FORMAT_ASK) {
        Titlescreen_EmitSpriteQuad(128, 30,
                              g_nTitleMode.cardPort +
                                  TITLE_SPR_MSG_CARD_IN_SLOT,
                              TITLE_COLOR_NORMAL);
        Titlescreen_EmitSpriteQuad(128, 46, TITLE_SPR_MSG_UNFORMATTED_L1, TITLE_COLOR_NORMAL);
        Titlescreen_EmitSpriteQuad(128, 62, TITLE_SPR_MSG_UNFORMATTED_L2, TITLE_COLOR_NORMAL);
      } else {
        Titlescreen_EmitSpriteQuad(128, 30, TITLE_SPR_MSG_WARNING, TITLE_COLOR_NORMAL);
        Titlescreen_EmitSpriteQuad(128, 46, TITLE_SPR_MSG_FORMAT_SURE_L1,
                              TITLE_COLOR_NORMAL);
        Titlescreen_EmitSpriteQuad(128, 62, TITLE_SPR_MSG_FORMAT_SURE_L2,
                              TITLE_COLOR_NORMAL);
      }
      DRAW_OPTIONS(TITLE_SPR_OPT_FORMAT, TITLE_SPR_OPT_CANCEL);
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nTitleMode.cardPort +
                                TITLE_SPR_CARD_SLOT_LABEL,
                            TITLE_COLOR_NORMAL);
      break;
    case TITLE_CARD_FORMATTING:
      Titlescreen_EmitSpriteQuad(128, 46, TITLE_SPR_MSG_FORMATTING, TITLE_COLOR_NORMAL);
      break;
    case TITLE_CARD_FORMAT_DONE:
      Titlescreen_EmitSpriteQuad(128, 46, TITLE_SPR_MSG_FORMAT_DONE, TITLE_COLOR_NORMAL);
      if (g_nTitleMode.animTimer > 7) {
        Titlescreen_EmitSpriteQuad(128, 88, TITLE_SPR_OPT_CONTINUE,
                              (g_nTitleMode.animTimer & 0xf) < 8
                                  ? TITLE_COLOR_HIGHLIGHT
                                  : TITLE_COLOR_NORMAL);
      }
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nTitleMode.cardPort +
                                TITLE_SPR_CARD_SLOT_LABEL,
                            TITLE_COLOR_NORMAL);
      break;
    case TITLE_CARD_FORMAT_FAILED:
      Titlescreen_EmitSpriteQuad(128, 46, TITLE_SPR_MSG_FORMAT_FAILED, TITLE_COLOR_NORMAL);
      DRAW_OPTIONS(TITLE_SPR_OPT_RETRY, TITLE_SPR_OPT_CANCEL);
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nTitleMode.cardPort +
                                TITLE_SPR_CARD_SLOT_LABEL,
                            TITLE_COLOR_NORMAL);
      break;
    case TITLE_CARD_CREATE_ASK:
      Titlescreen_EmitSpriteQuad(128, 30,
                            g_nTitleMode.cardPort +
                                TITLE_SPR_MSG_CARD_IN_SLOT,
                            TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(128, 46, TITLE_SPR_MSG_NO_FILE_L1,
                            TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(128, 62, TITLE_SPR_MSG_NO_FILE_L2,
                            TITLE_COLOR_NORMAL);

      DRAW_OPTIONS(TITLE_SPR_OPT_CREATE, TITLE_SPR_OPT_CANCEL);
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nTitleMode.cardPort +
                                TITLE_SPR_CARD_SLOT_LABEL,
                            TITLE_COLOR_NORMAL);
      break;
    case TITLE_CARD_CREATING:
      Titlescreen_EmitSpriteQuad(128, 46, TITLE_SPR_MSG_CREATING, TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nTitleMode.cardPort +
                                TITLE_SPR_CARD_SLOT_LABEL,
                            TITLE_COLOR_NORMAL);
      break;
    case TITLE_CARD_FULL:
      Titlescreen_EmitSpriteQuad(128, 22,
                            g_nTitleMode.cardPort +
                                TITLE_SPR_MSG_CARD_IN_SLOT,
                            0);
      Titlescreen_EmitSpriteQuad(128, 38, TITLE_SPR_MSG_CARD_FULL_L1, TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(128, 54, TITLE_SPR_MSG_CARD_FULL_L2,
                            TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(128, 70, TITLE_SPR_MSG_CARD_FULL_L3,
                            TITLE_COLOR_NORMAL);
      if (g_nTitleMode.animTimer > 7) {
        Titlescreen_EmitSpriteQuad(128, 88, TITLE_SPR_OPT_CONTINUE,
                              (g_nTitleMode.animTimer & 0xF) < 8);
      }
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nTitleMode.cardPort +
                                TITLE_SPR_CARD_SLOT_LABEL,
                            TITLE_COLOR_NORMAL);
      break;
    case TITLE_CARD_CREATE_FAILED:
      Titlescreen_EmitSpriteQuad(128, 46, TITLE_SPR_MSG_CREATE_FAILED,
                            TITLE_COLOR_NORMAL);
      DRAW_OPTIONS(TITLE_SPR_OPT_RETRY, TITLE_SPR_OPT_CANCEL);
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nTitleMode.cardPort +
                                TITLE_SPR_CARD_SLOT_LABEL,
                            TITLE_COLOR_NORMAL);
      break;
    /* no case 14 */
    case TITLE_CARD_PICK_SLOT:
      Titlescreen_EmitSpriteQuad(128, 46, TITLE_SPR_MSG_PICK_CARD, TITLE_COLOR_NORMAL);
      DRAW_OPTIONS(TITLE_SPR_OPT_SLOT1, TITLE_SPR_OPT_SLOT2);
      break;
    default:
      break;
    }
    break;
  }
  case TITLE_MODE_SAVE_SELECT: {
    int i;
    int j;
    int dragon_count;
    if (g_nTitleMode.menuState < TITLE_MENU_FADE_OUT) {
      Titlescreen_EmitSpriteQuad(108, 9, TITLE_SPR_BORDER, TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(255, 9, -TITLE_SPR_BORDER, TITLE_COLOR_NORMAL);
      if (g_nTitleMode.menuState > TITLE_MENU_ROUTE) {
        for (i = 0; i < 3; ++i) {
          if (!g_nTitleMode.apSaves[i]->levelVisited[0]) {
            int color = 0;
            if ((g_nTitleMode.menuState < TITLE_MENU_LOAD_SLOT &&
                 g_nTitleMode.selection != i) ||
                g_nTitleMode.menuState - 3u < 2) {
              color = TITLE_COLOR_DIMMED;
            }
            Titlescreen_EmitSpriteQuad(140 + 80 * i, 38, TITLE_SPR_EMPTY_SLOT, color);
          } else {
            int color = 0;
            if ((g_nTitleMode.menuState < TITLE_MENU_NEW_OR_LOAD &&
                 g_nTitleMode.selection != i) ||
                g_nTitleMode.menuState == TITLE_MENU_NEW_OR_LOAD) {
              color = TITLE_COLOR_DIMMED;
            }
            Titlescreen_EmitSpriteQuad(140 + 80 * i, 38,
                                  g_nTitleMode.apSaves[i]->levelId / 10 -
                                      1 + TITLE_SPR_WORLD_NAME,
                                  color);
            Titlescreen_EmitSpriteQuad(184 + 80 * i, 22, TITLE_SPR_DRAGON_ICON,
                                  color);

            for (j = 0, dragon_count = 0; j < TOTAL_LEVEL_COUNT; ++j) {
              dragon_count += g_nTitleMode.apSaves[i]->levelDragons[j];
            }

            /* dragon count ones digit */
            Titlescreen_EmitSpriteQuad(168 + 80 * i, 22,
                                  dragon_count % 10 + TITLE_SPR_DIGIT0,
                                  color);
            if (dragon_count > 9) {
              /* dragon count tens digit */
              Titlescreen_EmitSpriteQuad(152 + 80 * i, 22,
                                    dragon_count / 10 + TITLE_SPR_DIGIT0,
                                    color);
            }
          }
        }
      }
    } else {
      Titlescreen_EmitSpriteQuad(108, g_abTitleSlideOutCurve[g_nTitleMode.animTimer] - 119,
                            TITLE_SPR_BORDER, TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(255, g_abTitleSlideOutCurve[g_nTitleMode.animTimer] - 119,
                            -TITLE_SPR_BORDER, TITLE_COLOR_NORMAL);
    }

    if (g_nTitleMode.menuState == TITLE_MENU_NEW_SLOT) {
      if ((g_nTitleMode.animTimer & 0xF) < 8) {
        Titlescreen_EmitSpriteQuad(g_nTitleMode.selection * 80 + 140, 38,
                              TITLE_SPR_SLOT_CURSOR, TITLE_COLOR_NORMAL);
      }
      Titlescreen_EmitSpriteQuad(128, 88, TITLE_SPR_OPT_NEW_GAME, TITLE_COLOR_HIGHLIGHT);
      Titlescreen_EmitSpriteQuad(256, 88, TITLE_SPR_OPT_LOAD_GAME, TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(128, 106, TITLE_SPR_MSG_PICK_NEW_SLOT,
                            TITLE_COLOR_NORMAL);
    } else if (g_nTitleMode.menuState == TITLE_MENU_OVERWRITE) {
      Titlescreen_EmitSpriteQuad(g_nTitleMode.selection * 80 + 140, 38,
                            TITLE_SPR_SLOT_MARK, TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(g_nTitleMode.selection * 80 + 140, 38,
                            TITLE_SPR_SLOT_CURSOR, TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(128, 88, TITLE_SPR_OPT_OVERWRITE,
                            (g_nTitleMode.yesNoChoice == 0 &&
                             ((g_nTitleMode.animTimer & 0xF) < 8))
                                ? TITLE_COLOR_HIGHLIGHT
                                : TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(256, 88, TITLE_SPR_OPT_CANCEL,
                            (g_nTitleMode.yesNoChoice == 1 &&
                             ((g_nTitleMode.animTimer & 0xF) < 8))
                                ? TITLE_COLOR_HIGHLIGHT
                                : TITLE_COLOR_NORMAL);
    } else if (g_nTitleMode.menuState == TITLE_MENU_LOAD_SLOT) {
      if ((g_nTitleMode.animTimer & 0xF) < 8) {
        Titlescreen_EmitSpriteQuad(g_nTitleMode.selection * 80 + 140, 38,
                              TITLE_SPR_SLOT_CURSOR, TITLE_COLOR_NORMAL);
      }
      Titlescreen_EmitSpriteQuad(128, 88, TITLE_SPR_OPT_NEW_GAME, TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(256, 88, TITLE_SPR_OPT_LOAD_GAME, TITLE_COLOR_HIGHLIGHT);
      Titlescreen_EmitSpriteQuad(128, 106, TITLE_SPR_MSG_PICK_LOAD_SLOT,
                            TITLE_COLOR_NORMAL);
    } else if (g_nTitleMode.menuState == TITLE_MENU_NEW_OR_LOAD) {
      Titlescreen_EmitSpriteQuad(128, 88, TITLE_SPR_OPT_NEW_GAME,
                            (g_nTitleMode.selection == 0 &&
                             ((g_nTitleMode.animTimer & 0xF) < 8))
                                ? TITLE_COLOR_HIGHLIGHT
                                : TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(256, 88, TITLE_SPR_OPT_LOAD_GAME,
                            (g_nTitleMode.selection == 1 &&
                             ((g_nTitleMode.animTimer & 0xF) < 8))
                                ? TITLE_COLOR_HIGHLIGHT
                                : TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nTitleMode.cardPort +
                                TITLE_SPR_CARD_SLOT_LABEL,
                            TITLE_COLOR_NORMAL);
    }

    break;
  }
  }

  /* setRGB0 of both draw envs from the cyclorama background color */
  g_bFrameDrawEnv0BgR.r = g_dwWorldFogColor.r, g_bFrameDrawEnv0BgR.g = g_dwWorldFogColor.g,
  g_bFrameDrawEnv0BgR.b = g_dwWorldFogColor.b;
  g_bFrameDrawEnv1BgR.r = g_dwWorldFogColor.r, g_bFrameDrawEnv1BgR.g = g_dwWorldFogColor.g,
  g_bFrameDrawEnv1BgR.b = g_dwWorldFogColor.b;

  /* draw the mobys */
  BuildRenderEntityLists();
  BuildActorDrawList();
  FillWord(g_pOtDepthBinHead0, 0, 0x900);
  EmitActorDrawList();

  /* draw the environment */
  FillWord(g_pOtDepthBinHead0, 0, 0x1C00);
  g_nFarPlaneDrawDistance = 0x1c000;
  RenderWorldChunks(-1);

  /* draw the cyclorama (view matrix, then projection matrix just below it) */
  EmitStaticActorMeshList(-1, g_anCameraViewMtx, g_anCameraViewMtx - 0x14);

  /* overlay the fade */
  if (g_nScreenFadeLevel) {
    DrawFullscreenTint(2, g_nScreenFadeLevel << 4, g_nScreenFadeLevel << 4, g_nScreenFadeLevel << 4);
  }

  DrawSync(0);

  VSync(0);

  g_nVsyncFramePaceAnchor.pre = VSync(-1);

  while (g_nVsyncFramePaceAnchor.pre - g_nVsyncFramePaceAnchor.post < 2) {
    VSync(0);
    g_nVsyncFramePaceAnchor.pre = VSync(-1);
  }

  g_nVsyncFramePaceAnchor.post = VSync(-1);

  PutDispEnv(&g_pActiveFrameDrawEnv->dispEnv);
  PutDrawEnv(&g_pActiveFrameDrawEnv->drawEnv);
  DrawOTag(LinkOTPrimitives(0x800));
}
