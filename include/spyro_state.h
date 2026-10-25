/* spyro_state.h — Spyro's movement/ability state (g_nSpyroState, 0x80078ad0). */

#ifndef OPEN_SPYRO_SPYRO_STATE_H
#define OPEN_SPYRO_SPYRO_STATE_H

/* g_nSpyroState values. ChangeSpyroState runs a state's entry setup,
 * UpdateSpyroStateBehavior its per-frame exits; the names come from those
 * exits and the pad bits that reach them (pad_buttons.h). Hit states are
 * picked by DispatchSpyroPadStateChanges / DispatchSpyroChargeStateChanges
 * from the g_dwSpyroTriggerEventFlags bit noted. g_nSpyroState itself stays an
 * int, like g_nGamestate. */
typedef enum SpyroState
{
  SPYRO_ST_IDLE = 0,          /* standing; cycles the fidgets (CycleSpyroIdleAnim) */
  SPYRO_ST_WALK = 1,          /* speed <= 0x780 */
  SPYRO_ST_RUN = 2,           /* speed > 0xF00 */
  SPYRO_ST_STOP = 3,          /* braking with no stick input */
  SPYRO_ST_TURN_AROUND = 4,   /* reversing at speed (heading off by > 0x500) */
  SPYRO_ST_JUMP = 5,          /* Cross */
  SPYRO_ST_FALL = 6,          /* airborne >= 4 frames or on a too-steep slope */
  SPYRO_ST_HURT_TINT = 7,     /* hit, event 0x20; exit clears g_abSpyroFxParticleRgba alpha */
  SPYRO_ST_08 = 8,            /* no ChangeSpyroState site; flips yaw/pitch/roll after 0x14 frames */
  SPYRO_ST_SIDESTEP_L1 = 9,   /* L1 */
  SPYRO_ST_SIDESTEP_R1 = 10,  /* R1 */
  SPYRO_ST_CHARGE = 11,       /* Square held, on the ground */
  SPYRO_ST_CHARGE_BONK = 12,  /* charge stopped by a wall: dizzy stars + rumble */
  SPYRO_ST_TEETER = 13,       /* idle at a cliff edge (g_nSpyroCliffEdgeHint) */
  SPYRO_ST_HURT = 14,         /* hit, default event */
  SPYRO_ST_GLIDE = 15,        /* Cross again while a jump falls */
  SPYRO_ST_GLIDE_DROP = 16,   /* Triangle while gliding: drop straight down */
  SPYRO_ST_LIFT = 17,         /* carried up while event 0x800 holds, then glides */
  SPYRO_ST_FIDGET_0 = 18,     /* idle fidget animations: 18 and 36..43 */
  SPYRO_ST_SLIDE = 19,        /* sliding down a steep slope */
  SPYRO_ST_CHARGE_AIR = 20,   /* charge leaving the ground (Square in a jump) */
  SPYRO_ST_JOG = 21,          /* speed between WALK and RUN */
  SPYRO_ST_KNOCKBACK = 22,    /* hit, event 0x100: launched by g_anSpyroRequestVelocity */
  SPYRO_ST_GLIDE_SLOW = 23,   /* gliding below speed 0xC80 */
  SPYRO_ST_SUPERCHARGE_AIR = 24, /* SUPERCHARGE leaving the ground */
  SPYRO_ST_HURT_PINNED = 25,  /* hit, event 0x10, health left: pushed into the ground */
  SPYRO_ST_SCRIPTED_WALK = 26, /* slow walk steered round g_pScriptedCameraActor */
  SPYRO_ST_HURT_LONG = 27,    /* hit, event 0x40: 0x30-frame recovery */
  SPYRO_ST_KNOCKBACK_2 = 28,  /* hit, event 0x80: as KNOCKBACK */
  SPYRO_ST_HAZARD = 29,       /* event 0x400 hazard: Cross escapes after 0x10 frames, 0x3C kills */
  SPYRO_ST_DEATH = 30,        /* a hurt state ending with g_nSparxHealth < 0 */
  SPYRO_ST_DEATH_PINNED = 31, /* event 0x10 with no health left */
  SPYRO_ST_FLY = 32,          /* flight levels (and level 0x40) instead of GLIDE */
  SPYRO_ST_FLY_CLIMB = 33,
  SPYRO_ST_FLY_DIVE = 34,
  SPYRO_ST_SCRIPTED_HOLD = 35, /* frozen until g_dwSpyroRequestMask clears */
  SPYRO_ST_FIDGET_1 = 36,
  SPYRO_ST_FIDGET_2 = 37,
  SPYRO_ST_FIDGET_3 = 38,
  SPYRO_ST_FIDGET_4 = 39,
  SPYRO_ST_FIDGET_5 = 40,
  SPYRO_ST_FIDGET_6 = 41,
  SPYRO_ST_FIDGET_7 = 42,
  SPYRO_ST_FIDGET_8 = 43,
  SPYRO_ST_SUPERCHARGE = 44,  /* charge above speed 0x3000 */
} SpyroState;

#define SPYRO_STATE_COUNT 45 /* rows/columns of g_abSpyroStateTransitionAnimMatrix */

#endif /* OPEN_SPYRO_SPYRO_STATE_H */
