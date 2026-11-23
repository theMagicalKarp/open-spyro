/* src/native/host/stick_test.c — unit tests for stick.c (make native-test). */

#include <stdlib.h>

#include "stick.h"
#include "test.h"

/* The game's per-axis calibration (MapPadAxisToCalibratedRange after
   FinalizePadCalibration) with the 0x80 centre the shaped report rests at:
   returns the deflection the game sees, -127..127. */
static int game_axis(unsigned char raw) {
  const int neutral = 0x80, hi = neutral + 0x30, lo = neutral - 0x30;

  if (raw > hi) {
    int v = ((raw - hi) * (0x4000 / (0xFF - hi))) >> 7;
    return v > 0x80 ? 0x80 : v;
  }
  if (raw < lo) {
    int v = ((lo - raw) * (0x3F80 / lo)) >> 7;
    return v > 0x7F ? -0x7F : -v;
  }
  return 0;
}

static const StickShape linear = {STICK_MODE_RADIAL, {0.0, 1.0}, 1.0};

TEST(stick_centre_stays_centred) {
  unsigned char x = 0x80, y = 0x80;

  stick_shape(&x, &y, &linear);
  CHECK_INT(x, 0x80);
  CHECK_INT(y, 0x80);
}

TEST(stick_deadzone_is_radial) {
  StickShape s = {STICK_MODE_RADIAL, {0.2, 1.0}, 1.0};
  unsigned char x = 0x80 + 20, y = 0x80 + 20; /* |(0.157, 0.157)| = 0.22 */

  stick_shape(&x, &y, &s);
  CHECK(game_axis(x) > 0 && game_axis(y) > 0);
  x = 0x80 + 17, y = 0x80 + 17; /* 0.19: inside */
  stick_shape(&x, &y, &s);
  CHECK_INT(x, 0x80);
  CHECK_INT(y, 0x80);
}

/* With no dead zone, every deflection reaches the game within one step,
   past the game's own 0x30 dead zone. */
TEST(stick_linear_passes_through_game_calibration) {
  for (int raw = 0; raw <= 0xFF; raw++) {
    unsigned char x = (unsigned char)raw, y = 0x80;
    int want = raw - 0x80;

    if (want < -127) {
      want = -127;
    }
    stick_shape(&x, &y, &linear);
    CHECK(abs(game_axis(x) - want) <= 2);
    CHECK_INT(game_axis(y), 0);
  }
}

TEST(stick_full_deflection_reaches_full) {
  unsigned char x = 0xFF, y = 0x80;

  stick_shape(&x, &y, &(StickShape){STICK_MODE_RADIAL, {0.375, 1.0}, 1.0});
  CHECK(game_axis(x) >= 126);
  x = 0x00, y = 0x80;
  stick_shape(&x, &y, &(StickShape){STICK_MODE_RADIAL, {0.375, 1.0}, 1.0});
  CHECK(game_axis(x) <= -126);
}

TEST(stick_range_outer_saturates) {
  StickShape s = {STICK_MODE_RADIAL, {0.0, 0.8}, 1.0};
  unsigned char x = 0x80 + 0x66, y = 0x80; /* 0.80 */

  stick_shape(&x, &y, &s);
  CHECK(game_axis(x) >= 126);
}

TEST(stick_curve_softens_small_deflections) {
  StickShape s = {STICK_MODE_RADIAL, {0.0, 1.0}, 2.0};
  unsigned char x = 0x80 + 64, y = 0x80; /* 0.5 -> 0.25 */

  stick_shape(&x, &y, &s);
  CHECK(abs(game_axis(x) - 32) <= 2);
}

TEST(stick_report_only_shapes_analog_pads) {
  unsigned char digital[8] = {0, 0x41, 0xFF, 0xFF, 0x90, 0x90, 0x90, 0x90};
  unsigned char analog[8] = {0, 0x73, 0xFF, 0xFF, 0x90, 0x80, 0x80, 0x90};
  StickShape big = {STICK_MODE_RADIAL, {0.5, 1.0}, 1.0};

  stick_shape_report(digital, 8, &big, &big);
  CHECK_INT(digital[4], 0x90);
  stick_shape_report(analog, 8, &big, &linear);
  CHECK_INT(analog[7], 0x80);      /* left stick: inside 0.5 */
  CHECK(game_axis(analog[4]) > 0); /* right stick: linear */
}

/* The radial shaping scales both axes alike, so the direction survives. */
TEST(stick_diagonal_keeps_direction) {
  StickShape s = {STICK_MODE_RADIAL, {0.2, 1.0}, 1.0};
  unsigned char x = 0x80 + 90, y = 0x80 - 30;

  stick_shape(&x, &y, &s);
  CHECK(game_axis(x) > 0 && game_axis(y) < 0);
  CHECK(abs(game_axis(x) + 3 * game_axis(y)) <= 6);
}

TEST(stick_y_axis_shaped_like_x) {
  StickShape s = {STICK_MODE_RADIAL, {0.375, 1.0}, 1.0};
  unsigned char x = 0x80, y = 0xFF;

  stick_shape(&x, &y, &s);
  CHECK_INT(game_axis(x), 0);
  CHECK(game_axis(y) >= 126);
  x = 0x80, y = 0x80 - 40; /* 0.31: inside */
  stick_shape(&x, &y, &s);
  CHECK_INT(y, 0x80);
}

TEST(stick_report_ignores_short_and_bad_reports) {
  unsigned char short_[8] = {0, 0x73, 0xFF, 0xFF, 0x90, 0x90, 0x90, 0x90};
  unsigned char failed[8] = {0xFF, 0x73, 0xFF, 0xFF, 0x90, 0x90, 0x90, 0x90};
  StickShape big = {STICK_MODE_RADIAL, {0.5, 1.0}, 1.0};

  stick_shape_report(short_, 7, &big, &big);
  CHECK_INT(short_[4], 0x90);
  CHECK_INT(short_[6], 0x90);
  stick_shape_report(failed, 8, &big, &big);
  CHECK_INT(failed[4], 0x90);
  CHECK_INT(failed[6], 0x90);
}

/* The default leaves every byte to the game's own per-axis calibration. */
TEST(stick_original_mode_passes_bytes_through) {
  const StickShape original = STICK_SHAPE_ORIGINAL;

  for (int raw = 0; raw <= 0xFF; raw += 5) {
    unsigned char x = (unsigned char)raw, y = (unsigned char)(0xFF - raw);

    stick_shape(&x, &y, &original);
    CHECK_INT(x, raw);
    CHECK_INT(y, 0xFF - raw);
  }
}

TEST(stick_report_original_mode_unchanged) {
  unsigned char report[8] = {0, 0x73, 0xFF, 0xFF, 0x7A, 0x93, 0xA0, 0x60};
  const StickShape original = STICK_SHAPE_ORIGINAL;

  stick_shape_report(report, 8, &original, &original);
  CHECK_INT(report[4], 0x7A);
  CHECK_INT(report[5], 0x93);
  CHECK_INT(report[6], 0xA0);
  CHECK_INT(report[7], 0x60);
}
