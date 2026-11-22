/* src/native/host/config_test.c — unit tests for config.c (make native-test).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "test.h"

/* Pad bits (config.c button_names). */
enum { B_CROSS = 1u << 6, B_START = 1u << 11, B_UP = 1u << 12 };

/* Captures config_log until log_text() reads it back. */
static char log_buf[1024];

static void log_capture(void) {
  if (config_log != NULL) {
    fclose(config_log);
  }
  config_log = tmpfile();
}

static const char *log_text(void) {
  size_t n;

  rewind(config_log);
  n = fread(log_buf, 1, sizeof(log_buf) - 1, config_log);
  log_buf[n] = '\0';
  return log_buf;
}

static NativeSettings fresh(void) {
  NativeSettings s = NATIVE_SETTINGS_DEFAULT;

  return s;
}

/* --- value formats --- */

TEST(log_level_words_and_letters) {
  CHECK_INT(config_parse_log_level("debug"), 0);
  CHECK_INT(config_parse_log_level("info"), 1);
  CHECK_INT(config_parse_log_level("warn"), 2);
  CHECK_INT(config_parse_log_level("error"), 3);
  CHECK_INT(config_parse_log_level("d"), 0);
  CHECK_INT(config_parse_log_level("e"), 3);
}

TEST(log_level_rejects_others) {
  CHECK_INT(config_parse_log_level(""), -1);
  CHECK_INT(config_parse_log_level("x"), -1);
  CHECK_INT(config_parse_log_level("de"), -1);
  CHECK_INT(config_parse_log_level("DEBUG"), -1);
}

TEST(buttons_single_and_combined) {
  unsigned mask;

  CHECK_INT(config_parse_buttons("start", &mask), 0);
  CHECK_INT(mask, B_START);
  CHECK_INT(config_parse_buttons("cross+up", &mask), 0);
  CHECK_INT(mask, B_CROSS | B_UP);
  CHECK_INT(config_parse_buttons("l2", &mask), 0);
  CHECK_INT(mask, 1);
  CHECK_INT(config_parse_buttons("left", &mask), 0);
  CHECK_INT(mask, 1u << 15);
}

TEST(buttons_stop_at_colon) {
  unsigned mask;

  CHECK_INT(config_parse_buttons("cross:30", &mask), 0);
  CHECK_INT(mask, B_CROSS);
  CHECK_INT(config_parse_buttons("", &mask), 0);
  CHECK_INT(mask, 0);
}

TEST(buttons_unknown_is_reported) {
  unsigned mask;

  log_capture();
  CHECK_INT(config_parse_buttons("cross+jump", &mask), 1);
  CHECK(strstr(log_text(), "unknown button 'jump'") != NULL);
  /* A prefix of a name is not the name. */
  CHECK_INT(config_parse_buttons("st", &mask), 1);
}

TEST(press_with_and_without_hold) {
  int frame, hold;
  unsigned buttons;

  CHECK_INT(config_parse_press("1300:start", &frame, &buttons, &hold), 0);
  CHECK_INT(frame, 1300);
  CHECK_INT(buttons, B_START);
  CHECK_INT(hold, 4);
  CHECK_INT(config_parse_press("50:cross+up:30", &frame, &buttons, &hold), 0);
  CHECK_INT(frame, 50);
  CHECK_INT(buttons, B_CROSS | B_UP);
  CHECK_INT(hold, 30);
}

TEST(press_errors) {
  int frame, hold;
  unsigned buttons;

  log_capture();
  CHECK_INT(config_parse_press("1300", &frame, &buttons, &hold), 1);
  CHECK_INT(config_parse_press("1300:jump", &frame, &buttons, &hold), 2);
}

TEST(warp) {
  int frame, level;

  CHECK_INT(config_parse_warp("3400:21", &frame, &level), 0);
  CHECK_INT(frame, 3400);
  CHECK_INT(level, 21);
  CHECK_INT(config_parse_warp("3400", &frame, &level), 1);
  CHECK_INT(config_parse_warp("0:21", &frame, &level), 1);
  CHECK_INT(config_parse_warp("-5:21", &frame, &level), 1);
}

TEST(takes_value) {
  CHECK(config_takes_value("--disc"));
  CHECK(config_takes_value("--set"));
  CHECK(config_takes_value("--log-level"));
  CHECK(!config_takes_value("--fast"));
  CHECK(!config_takes_value("--stub"));
  CHECK(!config_takes_value("--config")); /* scanned on its own */
  CHECK(!config_takes_value("--dis"));
}

/* --- argv scan --- */

TEST(scan_args_collects_configs_in_order) {
  char *argv[] = {"spyro",    "--config", "a.toml", "--fast",
                  "--config", "b.toml",   NULL};
  const char *paths[CONFIG_MAX_FILES];
  int n, none;

  CHECK_INT(config_scan_args(6, argv, paths, &n, &none), 0);
  CHECK_INT(n, 2);
  CHECK_INT(none, 0);
  CHECK_STR(paths[0], "a.toml");
  CHECK_STR(paths[1], "b.toml");
}

TEST(scan_args_skips_flag_values) {
  /* "--config" here is --disc's value, not a flag. */
  char *argv[] = {"spyro", "--disc", "--config", "--no-config", NULL};
  const char *paths[CONFIG_MAX_FILES];
  int n, none;

  CHECK_INT(config_scan_args(4, argv, paths, &n, &none), 0);
  CHECK_INT(n, 0);
  CHECK_INT(none, 1);
}

TEST(scan_args_trailing_config_without_value) {
  char *argv[] = {"spyro", "--config", NULL};
  const char *paths[CONFIG_MAX_FILES];
  int n, none;

  CHECK_INT(config_scan_args(2, argv, paths, &n, &none), 0);
  CHECK_INT(n, 0);
}

TEST(scan_args_config_with_no_config_is_an_error) {
  char *argv[] = {"spyro", "--no-config", "--config", "a.toml", NULL};
  const char *paths[CONFIG_MAX_FILES];
  int n, none;

  CHECK(config_scan_args(4, argv, paths, &n, &none) != 0);
}

TEST(scan_args_too_many_configs) {
  char *argv[1 + 2 * (CONFIG_MAX_FILES + 1)];
  const char *paths[CONFIG_MAX_FILES];
  int argc = 1, n, none;

  argv[0] = "spyro";
  for (int i = 0; i < CONFIG_MAX_FILES; i++) {
    argv[argc++] = "--config";
    argv[argc++] = "x.toml";
  }
  CHECK_INT(config_scan_args(argc, argv, paths, &n, &none), 0);
  CHECK_INT(n, CONFIG_MAX_FILES);
  argv[argc++] = "--config";
  argv[argc++] = "x.toml";
  CHECK(config_scan_args(argc, argv, paths, &n, &none) != 0);
}

/* --- --set --- */

TEST(set_each_kind) {
  NativeSettings s = fresh();

  CHECK_INT(config_set(&s, "--set", "disc", "game.cue"), 0);
  CHECK_STR(s.disc, "game.cue");
  CHECK_INT(config_set(&s, "--set", "memcard.port2", "b.mcd"), 0);
  CHECK_STR(s.card[1], "b.mcd");
  CHECK_INT(config_set(&s, "--set", "debug.fast", "true"), 0);
  CHECK_INT(s.fast, 1);
  CHECK_INT(config_set(&s, "--set", "debug.fast", "false"), 0);
  CHECK_INT(s.fast, 0);
  CHECK_INT(config_set(&s, "--set", "debug.log_level", "w"), 0);
  CHECK_INT(s.log_level, 2);
  CHECK_INT(config_set(&s, "--set", "debug.overlay", "true"), 0);
  CHECK_INT(s.overlay, 1);
  CHECK_INT(config_set(&s, "--set", "input.analog", "false"), 0);
  CHECK_INT(s.analog, 0);
  CHECK_INT(config_set(&s, "--set", "input.camera.right_stick", "true"), 0);
  CHECK_INT(s.camera_right_stick, 1);
  CHECK_INT(config_set(&s, "--set", "input.camera.invert", "true"), 0);
  CHECK_INT(s.camera_invert, 1);
  CHECK_INT(config_set(&s, "--set", "input.left_stick.deadzone", "0.1"), 0);
  CHECK(s.stick[0].deadzone == 0.1);
  CHECK_INT(config_set(&s, "--set", "input.camera.speed", "2"), 0);
  CHECK(s.camera_speed == 2.0);
}

TEST(stick_mode) {
  NativeSettings s = fresh();

  CHECK_INT(s.stick[0].mode, STICK_MODE_ORIGINAL);
  CHECK_INT(s.stick[1].mode, STICK_MODE_RADIAL);
  CHECK_INT(config_set(&s, "--set", "input.left_stick.mode", "radial"), 0);
  CHECK_INT(s.stick[0].mode, STICK_MODE_RADIAL);
  CHECK_INT(config_set(&s, "--set", "input.right_stick.mode", "original"), 0);
  CHECK_INT(s.stick[1].mode, STICK_MODE_ORIGINAL);
  log_capture();
  CHECK(config_set(&s, "--set", "input.left_stick.mode", "round") != 0);
  CHECK(strstr(log_text(), "input.left_stick.mode must be \"original\" or "
                           "\"radial\"") != NULL);
  CHECK_INT(s.stick[0].mode, STICK_MODE_RADIAL);
  CHECK_INT(config_load_string(&s, "x.toml",
                               "[input.left_stick]\nmode = \"original\"\n"),
            0);
  CHECK_INT(s.stick[0].mode, STICK_MODE_ORIGINAL);
  CHECK(config_load_string(&s, "y.toml", "[input.left_stick]\nmode = 1\n") !=
        0);
}

TEST(set_number_range) {
  NativeSettings s = fresh();

  log_capture();
  CHECK(config_set(&s, "--set", "input.right_stick.deadzone", "0.95") != 0);
  CHECK(strstr(log_text(), "input.right_stick.deadzone must be a number from "
                           "0 to 0.9") != NULL);
  CHECK(config_set(&s, "--set", "input.camera.speed", "fast") != 0);
  CHECK(config_set(&s, "--set", "input.camera.speed", "2x") != 0);
  CHECK(config_set(&s, "--set", "input.camera.speed", "") != 0);
  CHECK(config_set(&s, "--set", "input.camera.speed", "nan") != 0);
  CHECK(config_set(&s, "--set", "input.camera.boost", "0.5") != 0);
  CHECK(s.stick[1].deadzone == 0.15);
  CHECK(s.camera_speed == 1.0);
  CHECK(s.camera_boost == 1.0);
}

/* A range's ends are in it. */
TEST(set_number_range_inclusive) {
  NativeSettings s = fresh();

  CHECK_INT(config_set(&s, "--set", "input.right_stick.deadzone", "0"), 0);
  CHECK(s.stick[1].deadzone == 0.0);
  CHECK_INT(config_set(&s, "--set", "input.right_stick.deadzone", "0.9"), 0);
  CHECK(s.stick[1].deadzone == 0.9);
  CHECK_INT(config_set(&s, "--set", "input.left_stick.curve", "0.1"), 0);
  CHECK(s.stick[0].curve == 0.1);
  CHECK_INT(config_set(&s, "--set", "input.camera.boost_time", "10"), 0);
  CHECK(s.camera_boost_time == 10.0);
}

TEST(load_numbers) {
  NativeSettings s = fresh();

  log_capture();
  CHECK_INT(config_load_string(&s, "x.toml",
                               "[input.left_stick]\ndeadzone = 0.2\n"
                               "curve = 2\n[input.camera]\nboost = 2.5\n"),
            0);
  CHECK(s.stick[0].deadzone == 0.2);
  CHECK(s.stick[0].curve == 2.0);
  CHECK(s.camera_boost == 2.5);
  CHECK(config_load_string(&s, "y.toml", "[input.camera]\nspeed = \"2\"\n") !=
        0);
  CHECK(config_load_string(&s, "z.toml", "[input.camera]\nspeed = true\n") !=
        0);
  CHECK(config_load_string(&s, "w.toml", "[input.camera]\nboost = 9\n") != 0);
  CHECK(s.camera_boost == 2.5);
}

TEST(set_rejects_bad_values) {
  NativeSettings s = fresh();

  log_capture();
  CHECK(config_set(&s, "--fast", "debug.fast", "yes") != 0);
  CHECK(strstr(log_text(), "--fast: debug.fast must be true or false") != NULL);
  CHECK_INT(s.fast, 0);
  CHECK(config_set(&s, "--set", "debug.log_level", "loud") != 0);
  CHECK_INT(s.log_level, 1);
}

TEST(set_unknown_key) {
  NativeSettings s = fresh();

  log_capture();
  CHECK(config_set(&s, "--set", "debug.speed", "2") != 0);
  CHECK(strstr(log_text(), "unknown setting debug.speed") != NULL);
  /* Old top-level names are not --set keys. */
  CHECK(config_set(&s, "--set", "fast", "true") != 0);
}

TEST(set_disc_forgets_the_config_that_set_it) {
  NativeSettings s = fresh();

  s.disc_file = "spyro.toml";
  CHECK_INT(config_set(&s, "--disc", "disc", "other.cue"), 0);
  CHECK(s.disc_file == NULL);
}

/* --- paths --- */

TEST(relative_path) {
  CHECK_STR(config_relative_path("dir/spyro.toml", "game.cue"), "dir/game.cue");
  CHECK_STR(config_relative_path("/a/b/c.toml", "sub/game.cue"),
            "/a/b/sub/game.cue");
  CHECK_STR(config_relative_path("dir/spyro.toml", "/abs/game.cue"),
            "/abs/game.cue");
  CHECK_STR(config_relative_path("dir/spyro.toml", "none"), "none");
  CHECK_STR(config_relative_path("spyro.toml", "game.cue"), "game.cue");
}

/* --- config files --- */

TEST(load_tables_and_keys) {
  NativeSettings s = fresh();

  CHECK_INT(config_load_string(&s, "cfg/spyro.toml",
                               "disc = \"Spyro.cue\"\n"
                               "[memcard]\n"
                               "port1 = \"card1.mcd\"\n"
                               "port2 = \"none\"\n"
                               "[debug]\n"
                               "fast = true\n"
                               "log_level = \"debug\"\n"),
            0);
  CHECK_STR(s.disc, "cfg/Spyro.cue");
  CHECK_STR(s.disc_file, "cfg/spyro.toml");
  CHECK_STR(s.card[0], "cfg/card1.mcd");
  CHECK_STR(s.card[1], "none");
  CHECK_INT(s.fast, 1);
  CHECK_INT(s.log_level, 0);
}

TEST(load_dotted_keys) {
  NativeSettings s = fresh();

  CHECK_INT(config_load_string(&s, "x.toml", "debug.fast = true\n"), 0);
  CHECK_INT(s.fast, 1);
}

TEST(load_later_file_overrides) {
  NativeSettings s = fresh();

  CHECK_INT(config_load_string(&s, "base.toml",
                               "disc = \"/a.cue\"\n[debug]\nfast = true\n"),
            0);
  CHECK_INT(config_load_string(&s, "over.toml", "[debug]\nfast = false\n"), 0);
  CHECK_STR(s.disc, "/a.cue");
  CHECK_STR(s.disc_file, "base.toml");
  CHECK_INT(s.fast, 0);
}

TEST(load_unknown_and_renamed_keys_warn) {
  NativeSettings s = fresh();
  const char *log;

  log_capture();
  CHECK_INT(config_load_string(&s, "x.toml",
                               "fast = true\n"
                               "[debug]\n"
                               "speed = 2\n"),
            0);
  CHECK_INT(s.fast, 0);
  log = log_text();
  CHECK(strstr(log, "x.toml:1: fast is now debug.fast (ignored)") != NULL);
  CHECK(strstr(log, "x.toml:3: unknown key debug.speed (ignored)") != NULL);
}

TEST(load_wrong_types_fail) {
  NativeSettings s = fresh();

  log_capture();
  CHECK(config_load_string(&s, "x.toml", "[debug]\nfast = \"true\"\n") != 0);
  CHECK(strstr(log_text(), "x.toml:2: debug.fast must be true or false") !=
        NULL);
  CHECK(config_load_string(&s, "x.toml", "disc = 3\n") != 0);
  CHECK(config_load_string(&s, "x.toml", "[debug]\nlog_level = \"v\"\n") != 0);
  CHECK(s.disc == NULL);
  CHECK_INT(s.log_level, 1);
}

TEST(load_parse_error) {
  NativeSettings s = fresh();

  log_capture();
  CHECK(config_load_string(&s, "bad.toml", "disc = \n") != 0);
  CHECK(strstr(log_text(), "native: bad.toml: ") != NULL);
}

TEST(load_file) {
  NativeSettings s = fresh();
  const char *path = TEST_TMPDIR "/load_file.toml";
  FILE *f = fopen(path, "w");

  CHECK(f != NULL);
  fputs("disc = \"game.cue\"\n", f);
  fclose(f);
  CHECK_INT(config_load_file(&s, path, 0), 0);
  CHECK_STR(s.disc, TEST_TMPDIR "/game.cue");
  CHECK_STR(s.disc_file, path);
  remove(path);
}

TEST(load_missing_file) {
  NativeSettings s = fresh();

  log_capture();
  CHECK_INT(config_load_file(&s, TEST_TMPDIR "/missing.toml", 1), 0);
  CHECK(config_load_file(&s, TEST_TMPDIR "/missing.toml", 0) != 0);
  CHECK(strstr(log_text(), "cannot read config") != NULL);
}
