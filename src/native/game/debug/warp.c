/* Native-only level warps: `spyro --warp N:LEVEL` for headless runs (the
 * teardown HandleLevelSelectInput, src/main/gamestate/cheats.c, runs when the
 * dev level select picks a level, from outside the pad handler), and
 * debug.start.level, which leaves the title screen for a level at boot. */

#include "camera.h"
#include "cd_stream.h"
#include "funcs.h"
#include "gamestate.h"
#include "globals.h"
#include "level_id.h"
#include "main_shared.h"
#include "spyro_state.h"
#include "title_state.h"

/* Called by the host between frames. Returns 1 once the warp has started,
 * 0 while the game is not in a state to leave (the host retries). */
__attribute__((export_name("native_warp"))) int native_warp(int level) {
  if (g_nGamestate != GS_GAMEPLAY) {
    return 0;
  }
  g_nNextLevelId = level;
  StopAllSoundExceptMask(0);
  InitLightVectorConstants();
  g_nReturnPortalLevelId = 0;
  g_nEnteredPortalIdx = -1;
  ResetSpyroEntity(RESET_SPYRO_SOFT);
  RestartSpyroAnimWithState(SPYRO_ST_GLIDE);
  g_nGamestate = GS_LEVEL_LOAD;
  g_nCdStreamState = LSTREAM_FRESH;
  g_nPortalTransitionActive = 0;
  g_nLevelTransitionTallyActive = 0;
  g_nSkipGamestateDraw = 1;
  g_nCameraNextMode = (CAM_HELD | CAM_MODE_LEVEL_TRANSITION);
  return 1;
}

/* Called by the host between frames until it returns 1: once the title
 * screen's scene has loaded (its fly-in has started), takes the title's
 * load-game exit (Titlescreen_Update's fade-out, then StoryIntro_Update) into
 * LEVEL with a fresh game, as the opening cutscene's end does
 * (DispatchSpecialCutscene) for Artisans. */
__attribute__((export_name("native_start_level"))) int
native_start_level(int level) {
  int *title = g_anTitleModeBlock;

  if (g_nGamestate != GS_TITLE || g_nTitleMode != TITLE_MODE_ATTRACT ||
      title[4] /* subState */ < TITLE_ATTRACT_FLY_IN) {
    return 0;
  }
  InitNewGameState();
  g_nActiveLevelId = level;
  g_nTitleMode = TITLE_MODE_STORY_INTRO;
  g_nStoryIntroSubstate = STORY_INTRO_ARM;
  g_nTitleExitPath = TITLE_EXIT_LOADED_GAME;
  g_nScreenFadeLevel = 0;
  g_nSkipGamestateDraw = 1;
  return 1;
}

/* Called by the host each frame while debug.start.level loads: cuts the
 * story intro's scripted fly-over (StoryIntro_Update ends it at timer 0x180,
 * once the level stream has finished) short. */
__attribute__((export_name("native_skip_intro_fly"))) void
native_skip_intro_fly(void) {
  if (g_nGamestate == GS_TITLE && g_nTitleMode == TITLE_MODE_STORY_INTRO &&
      g_nStoryIntroSubstate == STORY_INTRO_FLY && g_nStoryIntroTimer < 0x180) {
    g_nStoryIntroTimer = 0x180;
  }
}

/* Whether the in-level frame is running (the host's fast start ends). */
__attribute__((export_name("native_in_gameplay"))) int
native_in_gameplay(void) {
  return g_nGamestate == GS_GAMEPLAY;
}
