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

#endif /* OPEN_SPYRO_ACTOR_CUTSCENE_H */
