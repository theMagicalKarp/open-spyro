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

/* g_dwSpyroTriggerEventFlags bits (SpyroObj.triggerEvents): raised by actors
 * and the collision probes during the frame, consumed by
 * DispatchSpyroPadStateChanges / DispatchSpyroChargeStateChanges. The hit
 * bits each pick a hurt state and cost one g_nSparxHealth; the two contact
 * bits only matter while charging (CONTACT_FRONT bonks when the contact actor
 * is ahead, CONTACT bonks anyway, both together hurt). */
#define SPYRO_EV_HIT 0x0001           /* SPYRO_ST_HURT */
#define SPYRO_EV_CONTACT_FRONT 0x0002
#define SPYRO_EV_CONTACT 0x0004
#define SPYRO_EV_CHARGE_HIT 0x0006    /* both contact bits: hurts a charge */
#define SPYRO_EV_HIT_PINNED 0x0010    /* SPYRO_ST_HURT_PINNED / DEATH_PINNED */
#define SPYRO_EV_HIT_TINT 0x0020      /* SPYRO_ST_HURT_TINT */
#define SPYRO_EV_HIT_LONG 0x0040      /* SPYRO_ST_HURT_LONG */
#define SPYRO_EV_KNOCKBACK_2 0x0080   /* SPYRO_ST_KNOCKBACK_2 */
#define SPYRO_EV_KNOCKBACK 0x0100     /* SPYRO_ST_KNOCKBACK */
#define SPYRO_EV_HAZARD 0x0400        /* SPYRO_ST_HAZARD */
#define SPYRO_EV_LIFT 0x0800          /* SPYRO_ST_LIFT while raised */
#define SPYRO_EV_CHARGE_HOLD 0x2000   /* latches g_nSpyroChargeHoldFlag */
#define SPYRO_EV_REBOUND 0x8000       /* bounce off g_anSpyroFirstContactNormal */
#define SPYRO_EV_HITS 0x01F1        /* every hit bit (HIT, PINNED .. KNOCKBACK) */

/* Masks the states pass to DispatchSpyroPadStateChanges: everything but the
 * charge-contact pair; also no hazard; and the supercharge's only two. */
#define SPYRO_EV_MASK_DEFAULT 0xFFF9
#define SPYRO_EV_MASK_NO_HAZARD 0xFBF9
#define SPYRO_EV_MASK_SUPERCHARGE (SPYRO_EV_HAZARD | SPYRO_EV_REBOUND)

/* g_dwSpyroRequestMask bits (SpyroObj.requestMask): a scripted override of
 * Spyro posted by actors and cutscenes, applied by AdvanceSpyroPhysics while
 * SPYRO_RQ_PENDING is set. */
#define SPYRO_RQ_VEL_X 0x00001         /* drive X toward g_anSpyroRequestTargetPos */
#define SPYRO_RQ_VEL_Y 0x00002
#define SPYRO_RQ_VEL_Z 0x00004
#define SPYRO_RQ_PITCH 0x00008         /* take g_abSpyroRequestEuler[0] */
#define SPYRO_RQ_ROLL 0x00010          /* [1] */
#define SPYRO_RQ_YAW 0x00020           /* [2] */
#define SPYRO_RQ_RAW_VELOCITY 0x00040  /* VEL_* take g_anSpyroRequestVelocity */
#define SPYRO_RQ_FORCE_STATE 0x00100   /* g_nSpyroRequestState; no state update */
#define SPYRO_RQ_CAMERA 0x00200        /* CAM_MODE_REQUEST on the request anchor */
#define SPYRO_RQ_PORTAL_PATH_CAMERA 0x00400 /* CAM_MODE_PORTAL_PATH */
#define SPYRO_RQ_LOCK_ON 0x01000       /* face g_pScriptedCameraActor (EnemyLock params) */
#define SPYRO_RQ_FREEZE_INPUT 0x02000  /* replay the pad snapshot */
#define SPYRO_RQ_NO_CLIP 0x04000       /* no ground probes, raw velocity */
#define SPYRO_RQ_TARGET_CAMERA 0x08000 /* CAM_MODE_SCRIPTED_TARGET */
#define SPYRO_RQ_FOLLOW_CAMERA 0x10000 /* CAM_MODE_SCRIPTED_FOLLOW */
#define SPYRO_RQ_SCRIPTED_WALK 0x20000 /* g_nSpyroScriptedMoveMode = 1, walk */
#define SPYRO_RQ_PENDING 0x80000000

/* g_nSpyroSubstate (SpyroObj word 0x1F): a per-state phase that
 * ChangeSpyroState resets on entry; most states use small values of their own
 * (jump: 0 rising with Cross held, 1 descending, 2 Cross released; glide/fly:
 * 1/2 from R1/L1, 3 a flight-level crash check; fall: a frame count that
 * respawns Spyro at 0x79). The values shared across states: */
#define SPYRO_SUB_SUPERCHARGE 0x40    /* charge/charge-air entered from a supercharge */
#define SPYRO_SUB_CHARGE_LATCHED 0x80 /* charge target speed latched (LATCH_CHARGE_SPEED) */
/* Glide/fly substates >= 8 are scripted: flight levels stop the clock and the
 * path tweens while one runs. */
#define SPYRO_SUB_SCRIPTED 8
#define SPYRO_SUB_GLIDE_HOLD 8        /* frozen, turning to g_abSpyroRequestEuler (no-input / forced-jump frames) */
#define SPYRO_SUB_GLIDE_TO_TARGET 9   /* no-clip glide to the hand-off point, then LEVEL_GLIDE_HANDOFF */
#define SPYRO_SUB_GLIDE_ARRIVE 10     /* level-entry glide under GS_ARRIVAL_CAMERA, banks down to the ground */
#define SPYRO_SUB_GLIDE_ENTER 11      /* level-entry glide straight into gameplay (camera mode 12) */

/* Trigger-region record types (byte 0 of a g_pTriggerRegionTable record),
 * dispatched by DispatchSpyroTriggerEvent. */
enum TriggerRegionType {
  TRIGGER_HAZARD = 0,       /* raises SPYRO_EV_HAZARD; data: [0] instant death, [1] g_nSpyroHazardKind */
  TRIGGER_LIFT = 1,         /* raises SPYRO_EV_LIFT (g_pSpyroLiftRegionData) */
  TRIGGER_GROUND_LATCH = 2, /* sets data[0] while Spyro is near the ground */
  TRIGGER_CHARGE_HOLD = 4,  /* raises SPYRO_EV_CHARGE_HOLD */
  TRIGGER_REBOUND = 5,      /* raises SPYRO_EV_REBOUND and spawns type 0xEA */
  TRIGGER_PORTAL = 6,       /* level warp: data[0] level id, [1] portal index */
  TRIGGER_DAMAGE = 7,       /* while its g_anLevelTriggerStateSlots slot is set: a charge-proof hit */
};

#endif /* OPEN_SPYRO_SPYRO_STATE_H */
