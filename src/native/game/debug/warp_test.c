/* src/native/game/debug/warp_test.c — unit tests for warp.c
 * (make native-test).
 *
 * warp.c is included, not linked; the game globals it reads and writes are
 * defined here and the game functions it calls are stubs that count calls.
 * The game headers' wasm32 typedefs that the host's own headers define
 * differently are renamed around the include. funcs.h is kept out (its
 * actor_type.h checks 32-bit struct sizes); the calls warp.c makes are
 * declared here instead. */

#define __ssize_t game___ssize_t
#define ssize_t game_ssize_t
#define __jmp_buf game___jmp_buf
#define OPEN_SPYRO_FUNCS_H
#include "types.h"
#define RESET_SPYRO_SOFT 0
void InitNewGameState(void);
void StopAllSoundExceptMask(int mask);
void InitLightVectorConstants(void);
void ResetSpyroEntity(int full);
void RestartSpyroAnimWithState(int state);
#include "warp.c"
#undef __ssize_t
#undef ssize_t
#undef __jmp_buf

#include "test.h"

int g_nGamestate;
int g_nNextLevelId;
int g_nReturnPortalLevelId;
int g_nEnteredPortalIdx;
int g_nCdStreamState;
int g_nPortalTransitionActive;
int g_nLevelTransitionTallyActive;
int g_nSkipGamestateDraw;
int g_nCameraNextMode;
int g_nActiveLevelId;
int g_nTitleMode;
int g_nStoryIntroSubstate;
int g_nStoryIntroTimer;
int g_nTitleExitPath;
int g_nScreenFadeLevel;
int g_anTitleModeBlock[8];

static int new_game_calls;
static int stop_sound_calls;

void InitNewGameState(void) { new_game_calls++; }
void StopAllSoundExceptMask(int mask) {
  (void)mask;
  stop_sound_calls++;
}
void InitLightVectorConstants(void) {}
void ResetSpyroEntity(int full) { (void)full; }
void RestartSpyroAnimWithState(int state) { (void)state; }

/* The title screen's attract mode with its fly-in started, before any
   start-level handoff; the handoff's outputs hold sentinels. */
static void title_fly_in(void) {
  g_nGamestate = GS_TITLE;
  g_nTitleMode = TITLE_MODE_ATTRACT;
  g_anTitleModeBlock[4] = TITLE_ATTRACT_FLY_IN;
  g_nActiveLevelId = -1;
  g_nStoryIntroSubstate = -1;
  g_nStoryIntroTimer = 0;
  g_nTitleExitPath = -1;
  g_nScreenFadeLevel = 0x55;
  g_nSkipGamestateDraw = 0;
  new_game_calls = 0;
}

/* Nothing the handoff writes has changed since title_fly_in(). */
#define CHECK_UNTOUCHED(title_mode)                                            \
  do {                                                                         \
    CHECK_INT(new_game_calls, 0);                                              \
    CHECK_INT(g_nTitleMode, (title_mode));                                     \
    CHECK_INT(g_nActiveLevelId, -1);                                           \
    CHECK_INT(g_nStoryIntroSubstate, -1);                                      \
    CHECK_INT(g_nTitleExitPath, -1);                                           \
    CHECK_INT(g_nScreenFadeLevel, 0x55);                                       \
    CHECK_INT(g_nSkipGamestateDraw, 0);                                        \
  } while (0)

TEST(start_level_waits_for_title_fly_in) {
  title_fly_in();
  g_nGamestate = GS_GAMEPLAY;
  CHECK_INT(native_start_level(LEVEL_DRY_CANYON), 0);
  CHECK_UNTOUCHED(TITLE_MODE_ATTRACT);

  title_fly_in();
  g_nTitleMode = TITLE_MODE_STORY_INTRO;
  CHECK_INT(native_start_level(LEVEL_DRY_CANYON), 0);
  CHECK_UNTOUCHED(TITLE_MODE_STORY_INTRO);

  title_fly_in();
  g_anTitleModeBlock[4] = TITLE_ATTRACT_FLY_IN - 1;
  CHECK_INT(native_start_level(LEVEL_DRY_CANYON), 0);
  CHECK_UNTOUCHED(TITLE_MODE_ATTRACT);
}

TEST(start_level_takes_loaded_game_exit) {
  title_fly_in();
  CHECK_INT(native_start_level(LEVEL_DRY_CANYON), 1);
  CHECK_INT(new_game_calls, 1);
  CHECK_INT(g_nActiveLevelId, LEVEL_DRY_CANYON);
  CHECK_INT(g_nTitleMode, TITLE_MODE_STORY_INTRO);
  CHECK_INT(g_nStoryIntroSubstate, STORY_INTRO_ARM);
  CHECK_INT(g_nTitleExitPath, TITLE_EXIT_LOADED_GAME);
  CHECK_INT(g_nScreenFadeLevel, 0);
  CHECK_INT(g_nSkipGamestateDraw, 1);
  CHECK_INT(g_nGamestate, GS_TITLE);

  /* Later attract substates (press start, idle) still hand off. */
  title_fly_in();
  g_anTitleModeBlock[4] = TITLE_ATTRACT_FLY_IN + 1;
  CHECK_INT(native_start_level(LEVEL_DRY_CANYON), 1);
}

TEST(skip_intro_fly_only_in_fly_substate) {
  g_nGamestate = GS_TITLE;
  g_nTitleMode = TITLE_MODE_STORY_INTRO;
  g_nStoryIntroSubstate = STORY_INTRO_FLY;
  g_nStoryIntroTimer = 5;
  native_skip_intro_fly();
  CHECK_INT(g_nStoryIntroTimer, 0x180);

  /* Never lowers the timer. */
  g_nStoryIntroTimer = 0x200;
  native_skip_intro_fly();
  CHECK_INT(g_nStoryIntroTimer, 0x200);

  g_nStoryIntroTimer = 5;
  g_nStoryIntroSubstate = STORY_INTRO_ARM;
  native_skip_intro_fly();
  CHECK_INT(g_nStoryIntroTimer, 5);

  g_nStoryIntroSubstate = STORY_INTRO_FLY;
  g_nTitleMode = TITLE_MODE_ATTRACT;
  native_skip_intro_fly();
  CHECK_INT(g_nStoryIntroTimer, 5);

  g_nTitleMode = TITLE_MODE_STORY_INTRO;
  g_nGamestate = GS_GAMEPLAY;
  native_skip_intro_fly();
  CHECK_INT(g_nStoryIntroTimer, 5);
}

TEST(in_gameplay) {
  g_nGamestate = GS_TITLE;
  CHECK_INT(native_in_gameplay(), 0);
  g_nGamestate = GS_GAMEPLAY;
  CHECK_INT(native_in_gameplay(), 1);
}

TEST(warp_only_from_gameplay) {
  g_nGamestate = GS_TITLE;
  g_nNextLevelId = 0;
  stop_sound_calls = 0;
  CHECK_INT(native_warp(LEVEL_DRY_CANYON), 0);
  CHECK_INT(g_nNextLevelId, 0);
  CHECK_INT(stop_sound_calls, 0);

  g_nGamestate = GS_GAMEPLAY;
  CHECK_INT(native_warp(LEVEL_DRY_CANYON), 1);
  CHECK_INT(g_nNextLevelId, LEVEL_DRY_CANYON);
  CHECK_INT(g_nGamestate, GS_LEVEL_LOAD);
  CHECK_INT(g_nCdStreamState, LSTREAM_FRESH);
  CHECK_INT(g_nEnteredPortalIdx, -1);
  CHECK_INT(g_nSkipGamestateDraw, 1);
  CHECK_INT(stop_sound_calls, 1);
}
