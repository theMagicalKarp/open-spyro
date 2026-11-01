/* actor_cutscene.h — GS_ACTOR_CUTSCENE substates (g_nActorCutsceneSubstate,
 * word [-1] of g_anActorCutsceneTimerBlock). Main's ActorCutscene_Update runs
 * ACS_STREAM_WAIT itself and forwards the rest to the hub overlay's
 * BalloonistCutscene_Update; BeginActorCutscene starts at ACS_APPROACH. */

#ifndef OPEN_SPYRO_ACTOR_CUTSCENE_H
#define OPEN_SPYRO_ACTOR_CUTSCENE_H

enum ActorCutsceneSubstate {
  ACS_APPROACH = 0,    /* swing the camera round to the balloonist */
  ACS_DIALOGUE = 1,    /* dialogue pages / world roster menu */
  ACS_BOARD = 2,       /* fly Spyro to the balloon */
  ACS_LIFT_OFF = 3,    /* raise the balloon, then start the level stream */
  ACS_STREAM_WAIT = 4, /* camera orbits the new level's entry while it streams */
  ACS_CRUISE = 5,      /* camera along the flight path */
  ACS_DESCEND = 6,     /* lower the balloon */
  ACS_LAND = 7,        /* Spyro steps out; back to gameplay */
};

/* g_abWorldBalloonistState[world]: a homeworld balloonist's progress. */
enum BalloonistState {
  BALLOONIST_UNMET = 0,
  BALLOONIST_TALKED = 1,
  BALLOONIST_OPEN = 2, /* travel to this world unlocked (world 0 starts open) */
};

/* g_nActorCutsceneDialogPage: the balloonist dialogue page, an index into
 * g_abBalloonistDialoguePageStarts. Pages 0..0x1D are six per homeworld
 * 0..4, BALLOONIST_PAGE(world, step). The hub's UpdateActors picks the step
 * from that world's travel requirement (world 0: 10 dragons, world 1: 1200
 * treasure, world 2: 5 eggs, ...) and g_abWorldBalloonistState[world + 1]. */
#define BALLOONIST_PAGE(world, step) ((world) * BPAGES_PER_WORLD + (step))

enum BalloonistPageStep {
  BPAGE_LOCKED_FIRST = 0,   /* requirement not met, first talk: then LOCKED_FIRST_2 */
  BPAGE_LOCKED_FIRST_2 = 1, /* then exits (world 0 while world 1 is locked) or ROSTER */
  BPAGE_LOCKED_AGAIN = 2,   /* requirement not met, talked before: same; world 2's
                             * page instead picks an egg-thief hint (BPAGE_HINT_*) */
  BPAGE_UNLOCK_FIRST = 3,   /* requirement met on the first talk: then UNLOCK_OFFER */
  BPAGE_UNLOCK_AGAIN = 4,   /* requirement met after talking before: then UNLOCK_OFFER */
  BPAGE_UNLOCK_OFFER = 5,   /* then BPAGE_CONFIRM_NEXT */
  BPAGES_PER_WORLD = 6,
};

enum BalloonistPage {
  BPAGE_ROSTER = 0x1E,       /* pick an open world (g_nActorCutsceneMenuCursor) and board */
  BPAGE_CONFIRM_NEXT = 0x1F, /* yes/no: fly to the next world (no exits) */
  BPAGE_CONFIRM_HINT = 0x20, /* yes/no: fly to the hinted level (no goes back to ROSTER) */
  /* Egg-thief hints, each for a level whose g_anLevelEggsRecovered is still
   * short. The first three then offer the trip (CONFIRM_HINT, landing card
   * variant page - 0x1B); the rest return to ROSTER. */
  BPAGE_HINT_STONE_HILL = 0x21,
  BPAGE_HINT_DRY_CANYON = 0x22,
  BPAGE_HINT_CLIFF_TOWN = 0x23,
  BPAGE_HINT_MC_HOME = 0x24,       /* level 12 (Magic Crafters home), under 2 eggs */
  BPAGE_HINT_WIZARD_PEAK = 0x25,   /* then WIZARD_PEAK_2 */
  BPAGE_HINT_WIZARD_PEAK_2 = 0x26,
};

#endif /* OPEN_SPYRO_ACTOR_CUTSCENE_H */
