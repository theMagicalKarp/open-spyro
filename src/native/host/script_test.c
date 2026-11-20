/* src/native/host/script_test.c — unit tests for script.c (make native-test).
 */

#include <string.h>

#include "config.h"
#include "script.h"
#include "test.h"

/* Pad bits (config.c button_names). */
enum { B_CROSS = 1u << 6, B_START = 1u << 11, B_UP = 1u << 12 };

/* --- held buttons -------------------------------------------------------- */

TEST(script_buttons_hold_window) {
  ScriptPress p[] = {{.frame = 10, .hold = 4, .buttons = B_START}};

  CHECK_INT(script_buttons(p, 1, 9), 0);
  CHECK_INT(script_buttons(p, 1, 10), B_START);
  CHECK_INT(script_buttons(p, 1, 13), B_START);
  CHECK_INT(script_buttons(p, 1, 14), 0); /* hold VSyncs, then released */
}

TEST(script_buttons_overlapping_presses) {
  ScriptPress p[] = {
      {.frame = 0, .hold = 10, .buttons = B_UP},
      {.frame = 5, .hold = 2, .buttons = B_CROSS},
      {.frame = 6, .hold = 1, .buttons = B_UP}, /* same button again */
  };

  CHECK_INT(script_buttons(p, 3, 4), B_UP);
  CHECK_INT(script_buttons(p, 3, 5), B_UP | B_CROSS);
  CHECK_INT(script_buttons(p, 3, 6), B_UP | B_CROSS);
  CHECK_INT(script_buttons(p, 3, 7), B_UP);
  CHECK_INT(script_buttons(p, 3, 10), 0);
  CHECK_INT(script_buttons(p, 0, 5), 0);
}

TEST(script_buttons_from_press_flag) {
  ScriptPress p;

  CHECK_INT(config_parse_press("30:cross+start", &p.frame, &p.buttons, &p.hold),
            0);
  CHECK_INT(script_buttons(&p, 1, 30), B_CROSS | B_START);
  CHECK_INT(script_buttons(&p, 1, 30 + p.hold), 0);
}

/* --- pad report ---------------------------------------------------------- */

TEST(script_pad_clears_active_low_bits) {
  /* a digital pad with nothing held: buttons read 0xFF 0xFF */
  unsigned char r[4] = {0x00, 0x41, 0xFF, 0xFF};

  script_apply_pad(r, 4, B_CROSS | B_START | B_UP);
  CHECK_INT(r[0], 0x00);
  CHECK_INT(r[1], 0x41);
  CHECK_INT(r[2], 0xFF & ~((B_START | B_UP) >> 8));
  CHECK_INT(r[3], 0xFF & ~B_CROSS);
}

TEST(script_pad_adds_to_held_buttons) {
  unsigned char r[4] = {0x00, 0x41, 0xFE, 0x7F}; /* a host button held */

  script_apply_pad(r, 4, B_CROSS);
  CHECK_INT(r[2], 0xFE);
  CHECK_INT(r[3], 0x7F & ~B_CROSS);
}

TEST(script_pad_plugs_digital_pad) {
  unsigned char r[8];

  memset(r, 0xFF, sizeof(r)); /* no device on the port */
  script_apply_pad(r, 8, B_START);
  CHECK_INT(r[0], 0x00);
  CHECK_INT(r[1], 0x41);
  CHECK_INT(r[2], 0xFF & ~(B_START >> 8));
  CHECK_INT(r[3], 0xFF);
  CHECK_INT(r[4], 0x00);
  CHECK_INT(r[7], 0x00);
}

TEST(script_pad_untouched_without_buttons) {
  unsigned char r[4] = {0xFF, 0xFF, 0xFF, 0xFF};
  unsigned char s[3] = {0x00, 0x41, 0xFF};

  script_apply_pad(r, 4, 0); /* nothing scripted: no pad is plugged */
  CHECK_INT(r[0], 0xFF);
  CHECK_INT(r[1], 0xFF);
  script_apply_pad(s, 3, B_CROSS); /* too short for the button bytes */
  CHECK_INT(s[2], 0xFF);
}

/* --- shot paths ---------------------------------------------------------- */

TEST(script_shot_path_replaces_extension) {
  char buf[64];

  script_shot_path(buf, sizeof(buf), "out.bmp", 120);
  CHECK_STR(buf, "out-120.bmp");
  script_shot_path(buf, sizeof(buf), "dir.v2/run.final.bmp", 7);
  CHECK_STR(buf, "dir.v2/run.final-7.bmp");
  script_shot_path(buf, sizeof(buf), "shot", 3);
  CHECK_STR(buf, "shot-3.bmp");
}

TEST(script_shot_path_truncates) {
  char buf[8];

  script_shot_path(buf, sizeof(buf), "longname.bmp", 1);
  CHECK_STR(buf, "longnam");
}
