/* src/native/host/diag_stats.h — the diagnostics overlay's numbers (diag.c):
 * frame rate and frame times, averaged over a short window.
 *
 * Kept free of SDL and the wasm module so it builds into the host unit tests
 * (native/tests). Times are in nanoseconds from any fixed origin. */

#ifndef OPEN_SPYRO_NATIVE_DIAG_STATS_H
#define OPEN_SPYRO_NATIVE_DIAG_STATS_H

#include <stdint.h>

/* How long each average covers; the shown values change this often. */
#define DIAG_WINDOW_NS 500000000ull
/* How many of the latest frame times the graph keeps. */
#define DIAG_HISTORY 240

typedef struct DiagStats {
  /* The last finished window's results. */
  double fps;          /* presented frames per second */
  double frame_ms;     /* mean time between presents */
  double frame_max_ms; /* longest time between presents */
  double game_ms;      /* mean time the game ran between blocking VSyncs */
  /* The window being collected. */
  uint64_t window_start, last_present, last_vsync_end;
  int presents;
  double frame_sum_ms, frame_max_sum_ms, game_sum_ms;
  int games;
  /* The latest DIAG_HISTORY times between presents (ms), oldest at
     history[history_pos] once full. */
  float history[DIAG_HISTORY];
  int history_pos, history_len;
} DiagStats;

/* A frame was presented at NOW. */
void diag_stats_present(DiagStats *s, uint64_t now);

/* The game called a blocking VSync at START, which returned at END. */
void diag_stats_vsync(DiagStats *s, uint64_t start, uint64_t end);

/* A short name for the game's g_nGamestate value, or "?". */
const char *diag_stats_gamestate_name(int gamestate);

#endif
