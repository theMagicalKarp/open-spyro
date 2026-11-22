/* src/native/host/diag_stats_test.c — unit tests for diag_stats.c (make
 * native-test). */

#include <string.h>

#include "diag_stats.h"
#include "test.h"

#define MS 1000000ull

TEST(diag_stats_steady_30fps) {
  DiagStats s = {0};
  uint64_t t = 1000 * MS;

  /* 30 Hz presents for over a window: 33.33 ms apart */
  for (int i = 0; i <= 20; i++) {
    diag_stats_present(&s, t + (uint64_t)i * 100 * MS / 3);
  }
  CHECK(s.fps > 29.9 && s.fps < 30.1);
  CHECK(s.frame_ms > 33.3 && s.frame_ms < 33.4);
  CHECK(s.frame_max_ms > 33.3 && s.frame_max_ms < 33.4);
}

TEST(diag_stats_frame_max) {
  DiagStats s = {0};
  uint64_t t = 1000 * MS;

  diag_stats_present(&s, t);
  for (int i = 1; i <= 9; i++) {
    t += i == 5 ? 100 * MS : 16 * MS;
    diag_stats_present(&s, t);
  }
  t += DIAG_WINDOW_NS;
  diag_stats_present(&s, t);
  CHECK(s.frame_max_ms >= 500);
  /* the next window starts over */
  t += 16 * MS;
  diag_stats_present(&s, t);
  t += DIAG_WINDOW_NS;
  diag_stats_present(&s, t);
  CHECK(s.frame_max_ms >= 500 && s.frame_max_ms < 501);
}

TEST(diag_stats_game_time) {
  DiagStats s = {0};
  uint64_t t = 1000 * MS;

  /* 4 ms of game work, then 29 ms waiting in VSync */
  for (int i = 0; i < 40; i++) {
    diag_stats_vsync(&s, t + 4 * MS, t + 33 * MS);
    t += 33 * MS;
    diag_stats_present(&s, t);
  }
  CHECK(s.game_ms > 3.99 && s.game_ms < 4.01);
}

TEST(diag_stats_game_time_skips_unpaired) {
  DiagStats s = {0};
  uint64_t t = 1000 * MS;

  /* the first VSync has no previous end to measure from */
  diag_stats_vsync(&s, t, t + 10 * MS);
  CHECK(s.games == 0);
  /* a start before the previous end is not counted either */
  diag_stats_vsync(&s, t + 5 * MS, t + 20 * MS);
  CHECK(s.games == 0);
  CHECK(s.game_sum_ms == 0);
  diag_stats_vsync(&s, t + 26 * MS, t + 33 * MS);
  CHECK(s.games == 1);
  CHECK(s.game_sum_ms > 5.99 && s.game_sum_ms < 6.01);
}

TEST(diag_stats_history_wraps) {
  DiagStats s = {0};
  uint64_t t = 1000 * MS;

  diag_stats_present(&s, t);
  CHECK(s.history_len == 0);
  for (int i = 1; i <= DIAG_HISTORY + 10; i++) {
    t += (uint64_t)i * MS;
    diag_stats_present(&s, t);
  }
  CHECK(s.history_len == DIAG_HISTORY);
  CHECK(s.history_pos == 10);
  /* the oldest kept frame is the 11th, 11 ms */
  CHECK(s.history[s.history_pos] > 10.9f && s.history[s.history_pos] < 11.1f);
}

TEST(diag_stats_gamestate_names) {
  CHECK(strcmp(diag_stats_gamestate_name(0), "gameplay") == 0);
  CHECK(strcmp(diag_stats_gamestate_name(15), "credits") == 0);
  CHECK(strcmp(diag_stats_gamestate_name(16), "?") == 0);
  CHECK(strcmp(diag_stats_gamestate_name(-1), "?") == 0);
}
