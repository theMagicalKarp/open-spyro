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
  if (g_nGamestate0dMode.animTimer > 7) {                                              \
    Titlescreen_EmitSpriteQuad(128, 88, op1,                                        \
                          (g_nGamestate0dMode.selection == 0 &&                 \
                           ((g_nGamestate0dMode.animTimer & 0xF) < 8))                 \
                              ? Color_0x01                                \
                              : Color_0x00);                                \
    Titlescreen_EmitSpriteQuad(256, 88, op2,                                        \
                          (g_nGamestate0dMode.selection == 1 &&                 \
                           ((g_nGamestate0dMode.animTimer & 0xF) < 8))                 \
                              ? Color_0x01                                \
                              : Color_0x00);                                \
  }

void Titlescreen_Draw(void) {

  switch (g_nGamestate0dMode.mode) {
  case Mode_0x00: {

    if (g_nGamestate0dMode.subState == 2) {
      /* spyro landed, logo incoming */
      if (g_pPathTableHead->introFrame >= 1170)
        Titlescreen_EmitSpriteQuad(128, D_8006FA74[g_nGamestate0dMode.animTimer] - 128,
                              Sprite_0x00, Color_0x00);
    } else if (g_nGamestate0dMode.subState == 3) {
      Titlescreen_EmitSpriteQuad(128, 0, Sprite_0x00, 0);
      Titlescreen_EmitSpriteQuad(192, 210, Sprite_0x0B,
                            ((g_nGamestate0dMode.animTimer & 0xF) < 8) * 2);
    } else if (g_nGamestate0dMode.subState == 4) {
      if (g_nGamestate0dMode.animTimer < 16) {
        Titlescreen_EmitSpriteQuad(128, D_8006FA84[g_nGamestate0dMode.animTimer] - 128,
                              Sprite_0x00, Color_0x00);
      } else {
        Titlescreen_EmitSpriteQuad(108, D_8006FA74[g_nGamestate0dMode.animTimer - 16] - 119,
                              Sprite_0x01, Color_0x00);
        Titlescreen_EmitSpriteQuad(255, D_8006FA74[g_nGamestate0dMode.animTimer - 16] - 119,
                              -Sprite_0x01, Color_0x00);
      }
    }
    break;
  }
  case Mode_0x01: {
    /* draw main menu decorative borders (left, then mirror right) */
    Titlescreen_EmitSpriteQuad(108, 9, Sprite_0x01, Color_0x00);
    Titlescreen_EmitSpriteQuad(255, 9, -Sprite_0x01, Color_0x00);

    switch (g_nGamestate0dMode.subState) {
    case SubState_0x00:
      Titlescreen_EmitSpriteQuad(128, 46, Sprite_0x18,
                            Color_0x00); /* accessing mem card */
      break;
    case SubState_0x01: /* memcard slot selected, checking */
      Titlescreen_EmitSpriteQuad(128, 46, Sprite_0x18, Color_0x00);
      if (g_nGamestate0dMode.selection != 0) {
        Titlescreen_EmitSpriteQuad(128, 106,
                              g_nGamestate0dMode.cardPort +
                                  Sprite_0x31,
                              Color_0x00);
      }
      break;
    case SubState_0x02:
      Titlescreen_EmitSpriteQuad(128, 46, Sprite_0x19, Color_0x00);
      DRAW_OPTIONS(Sprite_0x34, Sprite_0x35);
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nGamestate0dMode.cardPort +
                                Sprite_0x31,
                            Color_0x00);
      break;
    case SubState_0x03:
      Titlescreen_EmitSpriteQuad(128, 46, Sprite_0x1A,
                            Color_0x00);
      DRAW_OPTIONS(Sprite_0x34, Sprite_0x35);
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nGamestate0dMode.cardPort +
                                Sprite_0x31,
                            Color_0x00);
      break;
    case SubState_0x04:
      Titlescreen_EmitSpriteQuad(128, 22, Sprite_0x1B, Color_0x00);
      Titlescreen_EmitSpriteQuad(128, 38, Sprite_0x1C,
                            Color_0x00);
      Titlescreen_EmitSpriteQuad(128, 54, Sprite_0x1D,
                            Color_0x00);
      Titlescreen_EmitSpriteQuad(128, 70, Sprite_0x1E,
                            Color_0x00);
      DRAW_OPTIONS(Sprite_0x34, Sprite_0x36);
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nGamestate0dMode.cardPort +
                                Sprite_0x31,
                            Color_0x00);
      break;
    case SubState_0x05:
    case SubState_0x06:
      if (g_nGamestate0dMode.subState == SubState_0x05) {
        Titlescreen_EmitSpriteQuad(128, 30,
                              g_nGamestate0dMode.cardPort +
                                  Sprite_0x1F,
                              Color_0x00);
        Titlescreen_EmitSpriteQuad(128, 46, Sprite_0x21, Color_0x00);
        Titlescreen_EmitSpriteQuad(128, 62, Sprite_0x22, Color_0x00);
      } else {
        Titlescreen_EmitSpriteQuad(128, 30, Sprite_0x1B, Color_0x00);
        Titlescreen_EmitSpriteQuad(128, 46, Sprite_0x23,
                              Color_0x00);
        Titlescreen_EmitSpriteQuad(128, 62, Sprite_0x24,
                              Color_0x00);
      }
      DRAW_OPTIONS(Sprite_0x37, Sprite_0x36);
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nGamestate0dMode.cardPort +
                                Sprite_0x31,
                            Color_0x00);
      break;
    case SubState_0x07:
      Titlescreen_EmitSpriteQuad(128, 46, Sprite_0x25, Color_0x00);
      break;
    case SubState_0x08:
      Titlescreen_EmitSpriteQuad(128, 46, Sprite_0x26, Color_0x00);
      if (g_nGamestate0dMode.animTimer > 7) {
        Titlescreen_EmitSpriteQuad(128, 88, Sprite_0x39,
                              (g_nGamestate0dMode.animTimer & 0xf) < 8
                                  ? Color_0x01
                                  : Color_0x00);
      }
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nGamestate0dMode.cardPort +
                                Sprite_0x31,
                            Color_0x00);
      break;
    case SubState_0x09:
      Titlescreen_EmitSpriteQuad(128, 46, Sprite_0x27, Color_0x00);
      DRAW_OPTIONS(Sprite_0x35, Sprite_0x36);
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nGamestate0dMode.cardPort +
                                Sprite_0x31,
                            Color_0x00);
      break;
    case SubState_0x0A:
      Titlescreen_EmitSpriteQuad(128, 30,
                            g_nGamestate0dMode.cardPort +
                                Sprite_0x1F,
                            Color_0x00);
      Titlescreen_EmitSpriteQuad(128, 46, Sprite_0x28,
                            Color_0x00);
      Titlescreen_EmitSpriteQuad(128, 62, Sprite_0x29,
                            Color_0x00);

      DRAW_OPTIONS(Sprite_0x38, Sprite_0x36);
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nGamestate0dMode.cardPort +
                                Sprite_0x31,
                            Color_0x00);
      break;
    case SubState_0x0B:
      Titlescreen_EmitSpriteQuad(128, 46, Sprite_0x2A, Color_0x00);
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nGamestate0dMode.cardPort +
                                Sprite_0x31,
                            Color_0x00);
      break;
    case SubState_0x0C:
      Titlescreen_EmitSpriteQuad(128, 22,
                            g_nGamestate0dMode.cardPort +
                                Sprite_0x1F,
                            0);
      Titlescreen_EmitSpriteQuad(128, 38, Sprite_0x2B, Color_0x00);
      Titlescreen_EmitSpriteQuad(128, 54, Sprite_0x2C,
                            Color_0x00);
      Titlescreen_EmitSpriteQuad(128, 70, Sprite_0x3F,
                            Color_0x00);
      if (g_nGamestate0dMode.animTimer > 7) {
        Titlescreen_EmitSpriteQuad(128, 88, Sprite_0x39,
                              (g_nGamestate0dMode.animTimer & 0xF) < 8);
      }
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nGamestate0dMode.cardPort +
                                Sprite_0x31,
                            Color_0x00);
      break;
    case SubState_0x0D:
      Titlescreen_EmitSpriteQuad(128, 46, Sprite_0x2D,
                            Color_0x00);
      DRAW_OPTIONS(Sprite_0x35, Sprite_0x36);
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nGamestate0dMode.cardPort +
                                Sprite_0x31,
                            Color_0x00);
      break;
    /* no case 14 */
    case SubState_0x0F:
      Titlescreen_EmitSpriteQuad(128, 46, Sprite_0x33, Color_0x00);
      DRAW_OPTIONS(Sprite_0x3C, Sprite_0x3D);
      break;
    default:
      break;
    }
    break;
  }
  case Mode_0x02: {
    int i;
    int j;
    int dragon_count;
    if (g_nGamestate0dMode.menuState < 5) {
      Titlescreen_EmitSpriteQuad(108, 9, Sprite_0x01, Color_0x00);
      Titlescreen_EmitSpriteQuad(255, 9, -Sprite_0x01, Color_0x00);
      if (g_nGamestate0dMode.menuState > 0) {
        for (i = 0; i < 3; ++i) {
          if (!g_nGamestate0dMode.apSaves[i]->levelVisited[0]) {
            int color = 0;
            if ((g_nGamestate0dMode.menuState < 3 &&
                 g_nGamestate0dMode.selection != i) ||
                g_nGamestate0dMode.menuState - 3u < 2) {
              color = 3;
            }
            Titlescreen_EmitSpriteQuad(140 + 80 * i, 38, Sprite_0x08, color);
          } else {
            int color = 0;
            if ((g_nGamestate0dMode.menuState < 4 &&
                 g_nGamestate0dMode.selection != i) ||
                g_nGamestate0dMode.menuState == 4) {
              color = 3;
            }
            Titlescreen_EmitSpriteQuad(140 + 80 * i, 38,
                                  g_nGamestate0dMode.apSaves[i]->levelId / 10 -
                                      1 + Sprite_0x02,
                                  color);
            Titlescreen_EmitSpriteQuad(184 + 80 * i, 22, Sprite_0x17,
                                  color);

            for (j = 0, dragon_count = 0; j < TOTAL_LEVEL_COUNT; ++j) {
              dragon_count += g_nGamestate0dMode.apSaves[i]->levelDragons[j];
            }

            /* dragon count ones digit */
            Titlescreen_EmitSpriteQuad(168 + 80 * i, 22,
                                  dragon_count % 10 + Sprite_0x0D,
                                  color);
            if (dragon_count > 9) {
              /* dragon count tens digit */
              Titlescreen_EmitSpriteQuad(152 + 80 * i, 22,
                                    dragon_count / 10 + Sprite_0x0D,
                                    color);
            }
          }
        }
      }
    } else {
      Titlescreen_EmitSpriteQuad(108, D_8006FA84[g_nGamestate0dMode.animTimer] - 119,
                            Sprite_0x01, Color_0x00);
      Titlescreen_EmitSpriteQuad(255, D_8006FA84[g_nGamestate0dMode.animTimer] - 119,
                            -Sprite_0x01, Color_0x00);
    }

    if (g_nGamestate0dMode.menuState == 1) {
      if ((g_nGamestate0dMode.animTimer & 0xF) < 8) {
        Titlescreen_EmitSpriteQuad(g_nGamestate0dMode.selection * 80 + 140, 38,
                              Sprite_0x09, Color_0x00);
      }
      Titlescreen_EmitSpriteQuad(128, 88, Sprite_0x3B, Color_0x01);
      Titlescreen_EmitSpriteQuad(256, 88, Sprite_0x3A, Color_0x00);
      Titlescreen_EmitSpriteQuad(128, 106, Sprite_0x2E,
                            Color_0x00);
    } else if (g_nGamestate0dMode.menuState == 2) {
      Titlescreen_EmitSpriteQuad(g_nGamestate0dMode.selection * 80 + 140, 38,
                            Sprite_0x0A, Color_0x00);
      Titlescreen_EmitSpriteQuad(g_nGamestate0dMode.selection * 80 + 140, 38,
                            Sprite_0x09, Color_0x00);
      Titlescreen_EmitSpriteQuad(128, 88, Sprite_0x3E,
                            (g_nGamestate0dMode.unk_0x18 == 0 &&
                             ((g_nGamestate0dMode.animTimer & 0xF) < 8))
                                ? Color_0x01
                                : Color_0x00);
      Titlescreen_EmitSpriteQuad(256, 88, Sprite_0x36,
                            (g_nGamestate0dMode.unk_0x18 == 1 &&
                             ((g_nGamestate0dMode.animTimer & 0xF) < 8))
                                ? Color_0x01
                                : Color_0x00);
    } else if (g_nGamestate0dMode.menuState == 3) {
      if ((g_nGamestate0dMode.animTimer & 0xF) < 8) {
        Titlescreen_EmitSpriteQuad(g_nGamestate0dMode.selection * 80 + 140, 38,
                              Sprite_0x09, Color_0x00);
      }
      Titlescreen_EmitSpriteQuad(128, 88, Sprite_0x3B, Color_0x00);
      Titlescreen_EmitSpriteQuad(256, 88, Sprite_0x3A, Color_0x01);
      Titlescreen_EmitSpriteQuad(128, 106, Sprite_0x2F,
                            Color_0x00);
    } else if (g_nGamestate0dMode.menuState == 4) {
      Titlescreen_EmitSpriteQuad(128, 88, Sprite_0x3B,
                            (g_nGamestate0dMode.selection == 0 &&
                             ((g_nGamestate0dMode.animTimer & 0xF) < 8))
                                ? Color_0x01
                                : Color_0x00);
      Titlescreen_EmitSpriteQuad(256, 88, Sprite_0x3A,
                            (g_nGamestate0dMode.selection == 1 &&
                             ((g_nGamestate0dMode.animTimer & 0xF) < 8))
                                ? Color_0x01
                                : Color_0x00);
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nGamestate0dMode.cardPort +
                                Sprite_0x31,
                            Color_0x00);
    }

    break;
  }
  }

  /* setRGB0 of both draw envs from the cyclorama background color */
  D_80076EF9.r = g_dwWorldFogColor.r, D_80076EF9.g = g_dwWorldFogColor.g,
  D_80076EF9.b = g_dwWorldFogColor.b;
  D_80076F7D.r = g_dwWorldFogColor.r, D_80076F7D.g = g_dwWorldFogColor.g,
  D_80076F7D.b = g_dwWorldFogColor.b;

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
  if (g_nGenericCountdown) {
    DrawFullscreenTint(2, g_nGenericCountdown << 4, g_nGenericCountdown << 4, g_nGenericCountdown << 4);
  }

  DrawSync(0);

  VSync(0);

  g_nVsyncFramePaceAnchor.pre = VSync(-1);

  while (g_nVsyncFramePaceAnchor.pre - g_nVsyncFramePaceAnchor.post < 2) {
    VSync(0);
    g_nVsyncFramePaceAnchor.pre = VSync(-1);
  }

  g_nVsyncFramePaceAnchor.post = VSync(-1);

  PutDispEnv(&g_pActiveFrameDrawEnv->unk_0x5C);
  PutDrawEnv(&g_pActiveFrameDrawEnv->unk_0x00);
  DrawOTag(LinkOTPrimitives(0x800));
}
