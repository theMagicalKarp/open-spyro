/* dragon_rescue.h — GS_DRAGON_RESCUE (BeginDragonRescue, DragonRescue_Update,
 * DragonRescue_Draw). g_nDragonRescueSubstate is word [0] of the rescue
 * record at 0x80077058 (g_anDragonRescueBlock). */

#ifndef OPEN_SPYRO_DRAGON_RESCUE_H
#define OPEN_SPYRO_DRAGON_RESCUE_H

enum DragonRescueSubstate {
  DRAGON_RESCUE_AURA = 0,        /* aura star swells while Spyro keeps playing */
  DRAGON_RESCUE_SWEEP = 1,       /* Spyro swept round the statue, camera eases in */
  DRAGON_RESCUE_LOAD_WAIT = 2,   /* wait for the dragon assets; spawn statue + stand-in */
  DRAGON_RESCUE_TURN = 3,        /* the statue turns to face its heading */
  DRAGON_RESCUE_CAMERA_PATH = 4, /* camera path plays with the dragon's voice line */
  DRAGON_RESCUE_LIGHT_UP = 5,    /* statue lights up, HUD icons roll in */
  DRAGON_RESCUE_COUNT = 6,       /* the dragon counter ticks */
  DRAGON_RESCUE_FADE_OUT = 7,    /* fade back to the world */
};

#endif /* OPEN_SPYRO_DRAGON_RESCUE_H */
