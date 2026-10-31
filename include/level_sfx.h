/* level_sfx.h — slots of the level's sound-id table (g_pLevelSampleBankHeader,
 * one byte per slot, read through LEVEL_SFX). Only slots whose callers agree
 * on one meaning are named; the rest stay numeric. */

#ifndef OPEN_SPYRO_LEVEL_SFX_H
#define OPEN_SPYRO_LEVEL_SFX_H

enum LevelSfx {
  SFX_REWARD = 0,         /* extra life collected, gem-tally burst, cheat accepted */
  SFX_CHARGE = 4,         /* charge entry; the loop StopSoundVoicesByOwner ends on exit */
  SFX_SUPERCHARGE = 5,    /* supercharge entry */
  SFX_SPYRO_HURT_FLASH = 0x1F, /* SPYRO_EV_HIT_TINT hit (DispatchSpyroPadStateChanges) */
  SFX_SPYRO_PINNED_EXIT = 0x20, /* leaving SPYRO_ST_HURT_PINNED */
  SFX_SPLASH = 0x1D,      /* Spyro enters a hazard; actors touch surface-0 ground (with a ripple spawn) */
  /* Flight levels (types 0x154 targets, 0x159..0x15B time bonuses, 0x166 HUD). */
  SFX_FLIGHT_CLOCK = 0x21,      /* the clock passes a whole unit under 10 */
  SFX_FLIGHT_TIME_ADDED = 0x22, /* a time bonus reaches the HUD clock */
  SFX_FLIGHT_COLLECT = 0x23,    /* target flown through / time bonus taken */
  SFX_FLIGHT_TALLY = 0x24,      /* a collected target moves into the HUD tally */
  SFX_FLIGHT_HURRY = 0x25,      /* per-tenth tick once the clock is low */
  SFX_ACTOR_BURST = 0x26, /* BurstActorWithSound */
  SFX_MENU_OPEN = 0x2C,   /* pause / inventory / save menu opens */
  SFX_MENU_MOVE = 0x2D,   /* menu cursor moves */
  SFX_MENU_SELECT = 0x2E, /* menu choice confirmed or menu closed */
  SFX_ATTACK_HIT = 0x28,  /* a TickSpyroAttackEffects arc tip hits an actor or the world */
  SFX_GEM_BOUNCE = 0x2F,  /* dropped gem bounces; gem-tally particle lands; Sparx hands over a pickup */
  /* The dragon rescue (DragonRescue_Update substates in brackets). */
  SFX_RESCUE_START = 0x30,  /* statue touched (BeginDragonRescue) */
  SFX_RESCUE_STATUE = 0x31, /* [2] the statue and Spyro's stand-in spawn */
  SFX_RESCUE_SWEEP = 0x32,  /* [0 -> 1] Spyro is swept round the statue */
  SFX_RESCUE_FADE = 0x33,   /* [6 -> 7] fade back to the world */
  SFX_RESCUE_DEPART = 0x34, /* [5 -> 6] the dragon leaves in a particle burst */
  SFX_RESCUE_COUNT = 0x35,  /* [6] the HUD dragon counter rolls one */
  SFX_WHIRLWIND = 0x36,     /* type 300 whirlwind every frame it is awake; level 14's type 109 carrier */
  SFX_SEQUENCE_TONE = 0x37, /* pitched up per step: type 13 followers released in turn;
                             * level 0's type 18 switches take its pitch */
  SFX_MENU_PAGE = 0x38,   /* inventory scrolls to another world */
  SFX_SPYRO_HURT_PINNED = 0x3A, /* SPYRO_ST_HURT_PINNED entry */
  SFX_SPYRO_HURT = 0x3B,        /* SPYRO_ST_HURT / KNOCKBACK entry */
  SFX_SPYRO_HURT_EXIT = 0x1B,   /* leaving SPYRO_ST_HURT / HURT_TINT / HURT_LONG / KNOCKBACK(_2),
                                 * the end of the HURT_PINNED animation, and SPYRO_ST_HAZARD
                                 * entry (which has no exit sound of its own) */
};

#endif /* OPEN_SPYRO_LEVEL_SFX_H */
