/* level_sfx.h — slots of the level's sound-id table (g_pLevelSampleBankHeader,
 * one byte per slot, read through LEVEL_SFX). Only slots whose callers agree
 * on one meaning are named; the rest stay numeric. */

#ifndef OPEN_SPYRO_LEVEL_SFX_H
#define OPEN_SPYRO_LEVEL_SFX_H

enum LevelSfx {
  SFX_REWARD = 0,         /* extra life collected, gem-tally burst, cheat accepted */
  SFX_ACTOR_BURST = 0x26, /* BurstActorWithSound */
  SFX_MENU_OPEN = 0x2C,   /* pause / inventory / save menu opens */
  SFX_MENU_MOVE = 0x2D,   /* menu cursor moves */
  SFX_MENU_SELECT = 0x2E, /* menu choice confirmed or menu closed */
  SFX_GEM_BOUNCE = 0x2F,  /* dropped gem bounces; gem-tally particle lands */
  SFX_MENU_PAGE = 0x38,   /* inventory scrolls to another world */
};

#endif /* OPEN_SPYRO_LEVEL_SFX_H */
