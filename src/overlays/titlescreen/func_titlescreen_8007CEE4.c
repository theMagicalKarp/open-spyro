/* func_titlescreen_8007CEE4 (0x8007CEE4, titlescreen overlay).
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
  if (D_80078D78.unk_0x0C > 7) {                                              \
    func_titlescreen_8007CD38(128, 88, op1,                                        \
                          (D_80078D78.unk_0x14 == 0 &&                 \
                           ((D_80078D78.unk_0x0C & 0xF) < 8))                 \
                              ? Color_0x01                                \
                              : Color_0x00);                                \
    func_titlescreen_8007CD38(256, 88, op2,                                        \
                          (D_80078D78.unk_0x14 == 1 &&                 \
                           ((D_80078D78.unk_0x0C & 0xF) < 8))                 \
                              ? Color_0x01                                \
                              : Color_0x00);                                \
  }

void func_titlescreen_8007CEE4(void) {

  switch (D_80078D78.unk_0x00) {
  case Mode_0x00: {

    if (D_80078D78.unk_0x10 == 2) {
      /* spyro landed, logo incoming */
      if (D_80075680->unk_0x00 >= 1170)
        func_titlescreen_8007CD38(128, D_8006FA74[D_80078D78.unk_0x0C] - 128,
                              Sprite_0x00, Color_0x00);
    } else if (D_80078D78.unk_0x10 == 3) {
      func_titlescreen_8007CD38(128, 0, Sprite_0x00, 0);
      func_titlescreen_8007CD38(192, 210, Sprite_0x0B,
                            ((D_80078D78.unk_0x0C & 0xF) < 8) * 2);
    } else if (D_80078D78.unk_0x10 == 4) {
      if (D_80078D78.unk_0x0C < 16) {
        func_titlescreen_8007CD38(128, D_8006FA84[D_80078D78.unk_0x0C] - 128,
                              Sprite_0x00, Color_0x00);
      } else {
        func_titlescreen_8007CD38(108, D_8006FA74[D_80078D78.unk_0x0C - 16] - 119,
                              Sprite_0x01, Color_0x00);
        func_titlescreen_8007CD38(255, D_8006FA74[D_80078D78.unk_0x0C - 16] - 119,
                              -Sprite_0x01, Color_0x00);
      }
    }
    break;
  }
  case Mode_0x01: {
    /* draw main menu decorative borders (left, then mirror right) */
    func_titlescreen_8007CD38(108, 9, Sprite_0x01, Color_0x00);
    func_titlescreen_8007CD38(255, 9, -Sprite_0x01, Color_0x00);

    switch (D_80078D78.unk_0x10) {
    case SubState_0x00:
      func_titlescreen_8007CD38(128, 46, Sprite_0x18,
                            Color_0x00); /* accessing mem card */
      break;
    case SubState_0x01: /* memcard slot selected, checking */
      func_titlescreen_8007CD38(128, 46, Sprite_0x18, Color_0x00);
      if (D_80078D78.unk_0x14 != 0) {
        func_titlescreen_8007CD38(128, 106,
                              D_80078D78.unk_0x28 +
                                  Sprite_0x31,
                              Color_0x00);
      }
      break;
    case SubState_0x02:
      func_titlescreen_8007CD38(128, 46, Sprite_0x19, Color_0x00);
      DRAW_OPTIONS(Sprite_0x34, Sprite_0x35);
      func_titlescreen_8007CD38(128, 106,
                            D_80078D78.unk_0x28 +
                                Sprite_0x31,
                            Color_0x00);
      break;
    case SubState_0x03:
      func_titlescreen_8007CD38(128, 46, Sprite_0x1A,
                            Color_0x00);
      DRAW_OPTIONS(Sprite_0x34, Sprite_0x35);
      func_titlescreen_8007CD38(128, 106,
                            D_80078D78.unk_0x28 +
                                Sprite_0x31,
                            Color_0x00);
      break;
    case SubState_0x04:
      func_titlescreen_8007CD38(128, 22, Sprite_0x1B, Color_0x00);
      func_titlescreen_8007CD38(128, 38, Sprite_0x1C,
                            Color_0x00);
      func_titlescreen_8007CD38(128, 54, Sprite_0x1D,
                            Color_0x00);
      func_titlescreen_8007CD38(128, 70, Sprite_0x1E,
                            Color_0x00);
      DRAW_OPTIONS(Sprite_0x34, Sprite_0x36);
      func_titlescreen_8007CD38(128, 106,
                            D_80078D78.unk_0x28 +
                                Sprite_0x31,
                            Color_0x00);
      break;
    case SubState_0x05:
    case SubState_0x06:
      if (D_80078D78.unk_0x10 == SubState_0x05) {
        func_titlescreen_8007CD38(128, 30,
                              D_80078D78.unk_0x28 +
                                  Sprite_0x1F,
                              Color_0x00);
        func_titlescreen_8007CD38(128, 46, Sprite_0x21, Color_0x00);
        func_titlescreen_8007CD38(128, 62, Sprite_0x22, Color_0x00);
      } else {
        func_titlescreen_8007CD38(128, 30, Sprite_0x1B, Color_0x00);
        func_titlescreen_8007CD38(128, 46, Sprite_0x23,
                              Color_0x00);
        func_titlescreen_8007CD38(128, 62, Sprite_0x24,
                              Color_0x00);
      }
      DRAW_OPTIONS(Sprite_0x37, Sprite_0x36);
      func_titlescreen_8007CD38(128, 106,
                            D_80078D78.unk_0x28 +
                                Sprite_0x31,
                            Color_0x00);
      break;
    case SubState_0x07:
      func_titlescreen_8007CD38(128, 46, Sprite_0x25, Color_0x00);
      break;
    case SubState_0x08:
      func_titlescreen_8007CD38(128, 46, Sprite_0x26, Color_0x00);
      if (D_80078D78.unk_0x0C > 7) {
        func_titlescreen_8007CD38(128, 88, Sprite_0x39,
                              (D_80078D78.unk_0x0C & 0xf) < 8
                                  ? Color_0x01
                                  : Color_0x00);
      }
      func_titlescreen_8007CD38(128, 106,
                            D_80078D78.unk_0x28 +
                                Sprite_0x31,
                            Color_0x00);
      break;
    case SubState_0x09:
      func_titlescreen_8007CD38(128, 46, Sprite_0x27, Color_0x00);
      DRAW_OPTIONS(Sprite_0x35, Sprite_0x36);
      func_titlescreen_8007CD38(128, 106,
                            D_80078D78.unk_0x28 +
                                Sprite_0x31,
                            Color_0x00);
      break;
    case SubState_0x0A:
      func_titlescreen_8007CD38(128, 30,
                            D_80078D78.unk_0x28 +
                                Sprite_0x1F,
                            Color_0x00);
      func_titlescreen_8007CD38(128, 46, Sprite_0x28,
                            Color_0x00);
      func_titlescreen_8007CD38(128, 62, Sprite_0x29,
                            Color_0x00);

      DRAW_OPTIONS(Sprite_0x38, Sprite_0x36);
      func_titlescreen_8007CD38(128, 106,
                            D_80078D78.unk_0x28 +
                                Sprite_0x31,
                            Color_0x00);
      break;
    case SubState_0x0B:
      func_titlescreen_8007CD38(128, 46, Sprite_0x2A, Color_0x00);
      func_titlescreen_8007CD38(128, 106,
                            D_80078D78.unk_0x28 +
                                Sprite_0x31,
                            Color_0x00);
      break;
    case SubState_0x0C:
      func_titlescreen_8007CD38(128, 22,
                            D_80078D78.unk_0x28 +
                                Sprite_0x1F,
                            0);
      func_titlescreen_8007CD38(128, 38, Sprite_0x2B, Color_0x00);
      func_titlescreen_8007CD38(128, 54, Sprite_0x2C,
                            Color_0x00);
      func_titlescreen_8007CD38(128, 70, Sprite_0x3F,
                            Color_0x00);
      if (D_80078D78.unk_0x0C > 7) {
        func_titlescreen_8007CD38(128, 88, Sprite_0x39,
                              (D_80078D78.unk_0x0C & 0xF) < 8);
      }
      func_titlescreen_8007CD38(128, 106,
                            D_80078D78.unk_0x28 +
                                Sprite_0x31,
                            Color_0x00);
      break;
    case SubState_0x0D:
      func_titlescreen_8007CD38(128, 46, Sprite_0x2D,
                            Color_0x00);
      DRAW_OPTIONS(Sprite_0x35, Sprite_0x36);
      func_titlescreen_8007CD38(128, 106,
                            D_80078D78.unk_0x28 +
                                Sprite_0x31,
                            Color_0x00);
      break;
    /* no case 14 */
    case SubState_0x0F:
      func_titlescreen_8007CD38(128, 46, Sprite_0x33, Color_0x00);
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
    if (D_80078D78.unk_0x04 < 5) {
      func_titlescreen_8007CD38(108, 9, Sprite_0x01, Color_0x00);
      func_titlescreen_8007CD38(255, 9, -Sprite_0x01, Color_0x00);
      if (D_80078D78.unk_0x04 > 0) {
        for (i = 0; i < 3; ++i) {
          if (!D_80078D78.unk_0x50[i]->unk_0x40[0]) {
            int color = 0;
            if ((D_80078D78.unk_0x04 < 3 &&
                 D_80078D78.unk_0x14 != i) ||
                D_80078D78.unk_0x04 - 3u < 2) {
              color = 3;
            }
            func_titlescreen_8007CD38(140 + 80 * i, 38, Sprite_0x08, color);
          } else {
            int color = 0;
            if ((D_80078D78.unk_0x04 < 4 &&
                 D_80078D78.unk_0x14 != i) ||
                D_80078D78.unk_0x04 == 4) {
              color = 3;
            }
            func_titlescreen_8007CD38(140 + 80 * i, 38,
                                  D_80078D78.unk_0x50[i]->unk_0x00 / 10 -
                                      1 + Sprite_0x02,
                                  color);
            func_titlescreen_8007CD38(184 + 80 * i, 22, Sprite_0x17,
                                  color);

            for (j = 0, dragon_count = 0; j < TOTAL_LEVEL_COUNT; ++j) {
              dragon_count += D_80078D78.unk_0x50[i]->unk_0x88[j];
            }

            /* dragon count ones digit */
            func_titlescreen_8007CD38(168 + 80 * i, 22,
                                  dragon_count % 10 + Sprite_0x0D,
                                  color);
            if (dragon_count > 9) {
              /* dragon count tens digit */
              func_titlescreen_8007CD38(152 + 80 * i, 22,
                                    dragon_count / 10 + Sprite_0x0D,
                                    color);
            }
          }
        }
      }
    } else {
      func_titlescreen_8007CD38(108, D_8006FA84[D_80078D78.unk_0x0C] - 119,
                            Sprite_0x01, Color_0x00);
      func_titlescreen_8007CD38(255, D_8006FA84[D_80078D78.unk_0x0C] - 119,
                            -Sprite_0x01, Color_0x00);
    }

    if (D_80078D78.unk_0x04 == 1) {
      if ((D_80078D78.unk_0x0C & 0xF) < 8) {
        func_titlescreen_8007CD38(D_80078D78.unk_0x14 * 80 + 140, 38,
                              Sprite_0x09, Color_0x00);
      }
      func_titlescreen_8007CD38(128, 88, Sprite_0x3B, Color_0x01);
      func_titlescreen_8007CD38(256, 88, Sprite_0x3A, Color_0x00);
      func_titlescreen_8007CD38(128, 106, Sprite_0x2E,
                            Color_0x00);
    } else if (D_80078D78.unk_0x04 == 2) {
      func_titlescreen_8007CD38(D_80078D78.unk_0x14 * 80 + 140, 38,
                            Sprite_0x0A, Color_0x00);
      func_titlescreen_8007CD38(D_80078D78.unk_0x14 * 80 + 140, 38,
                            Sprite_0x09, Color_0x00);
      func_titlescreen_8007CD38(128, 88, Sprite_0x3E,
                            (D_80078D78.unk_0x18 == 0 &&
                             ((D_80078D78.unk_0x0C & 0xF) < 8))
                                ? Color_0x01
                                : Color_0x00);
      func_titlescreen_8007CD38(256, 88, Sprite_0x36,
                            (D_80078D78.unk_0x18 == 1 &&
                             ((D_80078D78.unk_0x0C & 0xF) < 8))
                                ? Color_0x01
                                : Color_0x00);
    } else if (D_80078D78.unk_0x04 == 3) {
      if ((D_80078D78.unk_0x0C & 0xF) < 8) {
        func_titlescreen_8007CD38(D_80078D78.unk_0x14 * 80 + 140, 38,
                              Sprite_0x09, Color_0x00);
      }
      func_titlescreen_8007CD38(128, 88, Sprite_0x3B, Color_0x00);
      func_titlescreen_8007CD38(256, 88, Sprite_0x3A, Color_0x01);
      func_titlescreen_8007CD38(128, 106, Sprite_0x2F,
                            Color_0x00);
    } else if (D_80078D78.unk_0x04 == 4) {
      func_titlescreen_8007CD38(128, 88, Sprite_0x3B,
                            (D_80078D78.unk_0x14 == 0 &&
                             ((D_80078D78.unk_0x0C & 0xF) < 8))
                                ? Color_0x01
                                : Color_0x00);
      func_titlescreen_8007CD38(256, 88, Sprite_0x3A,
                            (D_80078D78.unk_0x14 == 1 &&
                             ((D_80078D78.unk_0x0C & 0xF) < 8))
                                ? Color_0x01
                                : Color_0x00);
      func_titlescreen_8007CD38(128, 106,
                            D_80078D78.unk_0x28 +
                                Sprite_0x31,
                            Color_0x00);
    }

    break;
  }
  }

  /* setRGB0 of both draw envs from the cyclorama background color */
  D_80076EF9.r = D_80078A50.r, D_80076EF9.g = D_80078A50.g,
  D_80076EF9.b = D_80078A50.b;
  D_80076F7D.r = D_80078A50.r, D_80076F7D.g = D_80078A50.g,
  D_80076F7D.b = D_80078A50.b;

  /* draw the mobys */
  func_800521C0();
  func_8001F158();
  func_80016914(D_8006FCF4, 0, 0x900);
  func_8001F798();

  /* draw the environment */
  func_80016914(D_8006FCF4, 0, 0x1C00);
  D_800785D0 = 0x1c000;
  func_800258F0(-1);

  /* draw the cyclorama (view matrix, then projection matrix just below it) */
  func_8004EBA8(-1, D_80076DE4, D_80076DE4 - 0x14);

  /* overlay the fade */
  if (D_80075918) {
    func_800190D4(2, D_80075918 << 4, D_80075918 << 4, D_80075918 << 4);
  }

  func_8005F764(0);

  func_8005DBC4(0);

  D_80075950.pre = func_8005DBC4(-1);

  while (D_80075950.pre - D_80075950.post < 2) {
    func_8005DBC4(0);
    D_80075950.pre = func_8005DBC4(-1);
  }

  D_80075950.post = func_8005DBC4(-1);

  func_80060030(&D_80075888->unk_0x5C);
  func_8005FDD8(&D_80075888->unk_0x00);
  func_8005FD64(func_80016784(0x800));
}
