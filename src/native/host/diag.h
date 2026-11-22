/* src/native/host/diag.h — the diagnostics overlay (not in the original): FPS,
 * frame times and the game's state, drawn over the top-left of the window.
 *
 * Backtick (`), or L3 + R3 on a gamepad (one stick clicked while the other is
 * held), shows and hides it; debug.overlay starts it shown. A Dear ImGui panel
 * drawn through PsyZ's SDL_GPU overlay hooks; if ImGui can't start, the
 * numbers go in the window title. */

#ifndef OPEN_SPYRO_NATIVE_DIAG_H
#define OPEN_SPYRO_NATIVE_DIAG_H

#include <stdint.h>

/* Registers the overlay with PsyZ; before the backend initializes. */
void diag_init(int shown);

/* The game called a blocking VSync at START (SDL ticks, ns), which returned
   at END. */
void diag_vsync(uint64_t start, uint64_t end);

#endif
