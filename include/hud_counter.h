/* hud_counter.h — the five HUD counter roll machines UpdateHudCounters runs
 * out of g_abHudCounterDisplayState[] (one byte per counter). */

#ifndef OPEN_SPYRO_HUD_COUNTER_H
#define OPEN_SPYRO_HUD_COUNTER_H

/* g_abHudCounterDisplayState index */
enum HudCounter {
  HUD_CTR_GEMS = 0,    /* level gems */
  HUD_CTR_DRAGONS = 1,
  HUD_CTR_LIVES = 2,   /* lives + the sparx-food butterfly ring */
  HUD_CTR_EGGS = 3,    /* world eggs */
  HUD_CTR_KEY = 4,     /* the level key */
};

/* g_abHudCounterDisplayState value */
enum HudCounterState {
  HUD_CTR_HIDDEN = 0,   /* idle: watch the live value */
  HUD_CTR_ROLL_IN = 1,  /* 14 steps off g_anHudRollInStepTable */
  HUD_CTR_SHOWN = 2,    /* hold; ratchet the cached count toward the live one */
  HUD_CTR_ROLL_OUT = 3, /* 14 steps off g_anHudRollOutStepTable */
  HUD_CTR_ALL_GEMS = 4, /* gems only: level complete, hold 0xF0 frames */
};

#endif /* OPEN_SPYRO_HUD_COUNTER_H */
