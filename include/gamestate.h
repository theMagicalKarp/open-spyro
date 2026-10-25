/* gamestate.h — the top-level mode selector (g_nGamestate, 0x800757d8). */

#ifndef OPEN_SPYRO_GAMESTATE_H
#define OPEN_SPYRO_GAMESTATE_H

/* g_nGamestate values, from the GamestateUpdate / GamestateDraw dispatch and
 * the setters (EnterPauseMenu, TriggerRespawnOrGameOver, ...). g_nGamestate
 * itself stays an int: an enum with no negative values is unsigned here. */
typedef enum Gamestate
{
  GS_GAMEPLAY = 0,        /* the in-level frame (render pipeline in GamestateDraw) */
  GS_LEVEL_LOAD = 1,      /* portal / hub return: ticks the world-bundle CD stream */
  GS_PAUSE_MENU = 2,      /* EnterPauseMenu */
  GS_INVENTORY = 3,       /* EnterInventoryMenu (SubMenu_Update) */
  GS_RESPAWN = 4,         /* TriggerRespawnOrGameOver, lives left */
  GS_GAME_OVER = 5,       /* TriggerRespawnOrGameOver, no lives */
  GS_OVERLAY_SCREEN = 6,  /* set only by overlay code; frozen backdrop + overlay sprites */
  GS_OVERLAY_HOOK = 7,    /* g_pfnGamestate7Handler / DrawHook (pause-menu handoff) */
  GS_DRAGON_RESCUE = 8,      /* BeginDragonRescue */
  GS_ARRIVAL_CAMERA = 9,  /* CameraSettle_Update after a level transition */
  GS_MENU_TRANSITION = 10, /* BeginPauseToMenuTransition (pause -> world map) */
  GS_SAVE_MENU = 11,      /* SaveLoadMenu_Draw */
  GS_ACTOR_CUTSCENE = 12, /* BeginActorCutscene: balloonist / fly-in cutscenes */
  GS_TITLE = 13,          /* title screen; g_nGamestate0dMode 3 is the story intro */
  GS_GEM_CUTSCENE = 14,   /* BeginGemCutscene */
  GS_CREDITS = 15,        /* BeginCreditsSequence; mode >= 0x63 is ReturningHome */
} Gamestate;

#endif /* OPEN_SPYRO_GAMESTATE_H */
