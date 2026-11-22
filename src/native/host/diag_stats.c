/* src/native/host/diag_stats.c — the diagnostics overlay's numbers
 * (diag_stats.h). */

#include "diag_stats.h"

static const char *const gamestate_names[] = {
    "gameplay",       "level load",  "pause",           "inventory",
    "respawn",        "game over",   "unused 6",        "flight results",
    "dragon rescue",  "arrival cam", "menu transition", "save menu",
    "actor cutscene", "title",       "cutscene",        "credits",
};

static void finish_window(DiagStats *s, uint64_t now) {
  double secs = (double)(now - s->window_start) / 1e9;
  int frames = s->presents > 0 ? s->presents : 1;

  s->fps = secs > 0 ? s->presents / secs : 0;
  s->frame_ms = s->frame_sum_ms / frames;
  s->frame_max_ms = s->frame_max_sum_ms;
  s->game_ms = s->games > 0 ? s->game_sum_ms / s->games : 0;
  s->window_start = now;
  s->presents = 0;
  s->frame_sum_ms = s->frame_max_sum_ms = s->game_sum_ms = 0;
  s->games = 0;
}

void diag_stats_present(DiagStats *s, uint64_t now) {
  if (s->window_start == 0) {
    s->window_start = s->last_present = now;
    return;
  }
  {
    double ms = (double)(now - s->last_present) / 1e6;

    s->frame_sum_ms += ms;
    if (ms > s->frame_max_sum_ms) {
      s->frame_max_sum_ms = ms;
    }
    s->history[s->history_pos] = (float)ms;
    s->history_pos = (s->history_pos + 1) % DIAG_HISTORY;
    if (s->history_len < DIAG_HISTORY) {
      s->history_len++;
    }
  }
  s->last_present = now;
  s->presents++;
  if (now - s->window_start >= DIAG_WINDOW_NS) {
    finish_window(s, now);
  }
}

void diag_stats_vsync(DiagStats *s, uint64_t start, uint64_t end) {
  if (s->last_vsync_end != 0 && start >= s->last_vsync_end) {
    s->game_sum_ms += (double)(start - s->last_vsync_end) / 1e6;
    s->games++;
  }
  s->last_vsync_end = end;
}

const char *diag_stats_gamestate_name(int gamestate) {
  int n = (int)(sizeof(gamestate_names) / sizeof(gamestate_names[0]));

  return gamestate >= 0 && gamestate < n ? gamestate_names[gamestate] : "?";
}
