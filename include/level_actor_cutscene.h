/* level_actor_cutscene.h — declarations shared by the *_BeginActorCutscene.c
 * clones (the gamestate-0xC cutscene camera setup), declared once.
 *
 * Each item is spelled identically in every file that includes this header. */

#ifndef OPEN_SPYRO_LEVEL_ACTOR_CUTSCENE_H
#define OPEN_SPYRO_LEVEL_ACTOR_CUTSCENE_H

/* Declared identically by two or more *_BeginActorCutscene.c files; hoisted here. */
extern int g_nActorCutsceneCamYawGoal;
extern int g_nActorCutsceneMenuCursor;
extern int g_nActorCutsceneSubstate;
extern int g_nActorCutsceneTimer;

/* Declared identically (by type) in two or more including files; hoisted here. */
extern struct Actor *g_pActorListBase; /* level actor pool */

#endif /* OPEN_SPYRO_LEVEL_ACTOR_CUTSCENE_H */
