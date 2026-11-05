/* spyro_state.h — Spyro's movement/ability state (g_nSpyroState, 0x80078ad0). */

#ifndef OPEN_SPYRO_SPYRO_STATE_H
#define OPEN_SPYRO_SPYRO_STATE_H

/* g_nSpyroState values. ChangeSpyroState runs a state's entry setup,
 * UpdateSpyroStateBehavior its per-frame exits; the names come from those
 * exits and the pad bits that reach them (pad_buttons.h). Hit states are
 * picked by ApplySpyroTriggerEvents / DispatchSpyroChargeStateChanges
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
  SPYRO_ST_HURT_TINT = 7,     /* hit, event 0x20: flashes white/dark (g_abSpyroTintRgba) for 10 frames; exit clears the tint */
  SPYRO_ST_FACE_TARGET = 8,   /* stands still; reached only as g_nSpyroRequestState (the level 14 carrier, ACTOR_TYPE_CARRIER, sets it at its ride path end); after 0x14 frames snaps yaw to g_nSpyroMoveTargetYaw, negates pitch/roll, then idles or walks */
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
  SPYRO_ST_FLY = 32,          /* flight levels (and Gnasty's Loot) instead of GLIDE */
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
 * ApplySpyroTriggerEvents / DispatchSpyroChargeStateChanges. The hit
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
#define SPYRO_EV_BLOCKS_SUPER 0x4000  /* from the contacted actor's collision shape (DispatchActorContactAtSphere ORs
                                         * each shape record's event word in): the shape stays solid to a supercharge,
                                         * which re-dispatches as a solid contact (AdvanceSpyroPhysics) */
#define SPYRO_EV_REBOUND 0x8000       /* bounce off g_anSpyroFirstContactNormal */
/* Bits above 0xFFFF reach the event word only through RaiseSpyroTriggerInRadius
 * callers, and every dispatcher masks the word with 0xFFFF (or less), so
 * nothing reads them: */
#define SPYRO_EV_UNREAD_20000 0x20000 /* with HIT_TINT: the split-shot projectiles (levels 12/15/16/27) */
#define SPYRO_EV_UNREAD_40000 0x40000 /* with 0x80000: level 31's floating barrel / mine blasts */
#define SPYRO_EV_UNREAD_80000 0x80000
#define SPYRO_EV_HITS 0x01F1        /* every hit bit (HIT, PINNED .. KNOCKBACK) */

/* Masks the states pass to ApplySpyroTriggerEvents: everything but the
 * charge-contact pair; also no hazard; and the supercharge's only two. */
#define SPYRO_EV_MASK_DEFAULT 0xFFF9
#define SPYRO_EV_MASK_NO_HAZARD 0xFBF9
#define SPYRO_EV_MASK_SUPERCHARGE (SPYRO_EV_HAZARD | SPYRO_EV_REBOUND)

/* g_nSpyroGroundSlope: the ground's incline as an 8-bit angle from
 * ProbeSpyroGroundContact (0 flat, 0x40 vertical; 0x400 when the hit faces
 * down, i.e. a ceiling). */
#define SLOPE_FLAT 0x0C       /* below: flat enough to launch a charge jump */
#define SLOPE_STEEP 0x17      /* at or above: too steep to stand; Spyro slides or falls */
#define SLOPE_NUDGE_MAX 0x20  /* below: still floor, so the wall probes may nudge */
#define SLOPE_GROUND_MAX 0x21 /* below: counts as ground (clears the airborne count) */
#define SLOPE_CEILING 0x400   /* the probe hit a downward-facing surface */

/* g_nSpyroAirborneFrames at which a grounded state gives up and falls: a
 * few frames of grace over gaps and bumps. */
#define SPYRO_AIRBORNE_GRACE 4

/* g_nSpyroMotionSpeed bands that pick the ground gait on landing / speed
 * change (SPYRO_ST_WALK / JOG / RUN). */
#define SPYRO_JOG_SPEED 0x780       /* above: jog */
#define SPYRO_RUN_SPEED 0xF00       /* above: run */
#define SPYRO_RUN_DROP_SPEED 0xC80  /* below: a run drops back to a jog; also the
                                     * fast/slow split for glides, leaps and lean */

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
/* SPYRO_ST_JUMP: */
#define SPYRO_SUB_JUMP_RISING 0   /* Cross still held */
#define SPYRO_SUB_JUMP_FALLING 1  /* past the apex */
#define SPYRO_SUB_JUMP_RELEASED 2 /* Cross let go while rising */
/* SPYRO_ST_GLIDE / FLY within 0x200 of the floor reference: the shoulder
 * button held (L1 also stores the height in g_nSpyroMotionAux). */
#define SPYRO_SUB_GLIDE_R1 1
#define SPYRO_SUB_GLIDE_L1 2
/* SPYRO_ST_FLY on a flight level: the run is live; touching the world or an
 * actor ends it (g_pfnLevelEndFlightRun). */
#define SPYRO_SUB_FLY_RUN 3
/* SPYRO_ST_STOP entered at speed (UpdateSpyroStateBehavior); with the stick
 * idle the speed target drops straight to 0. */
#define SPYRO_SUB_STOP_FROM_SPEED 7
/* SPYRO_ST_SCRIPTED_WALK round the scripted-camera actor: the sign its turn
 * rate is applied with (AdvanceSpyroPhysics picks it from which half of the
 * actor's facing Spyro is in). */
#define SPYRO_SUB_ORBIT_NEG 1
#define SPYRO_SUB_ORBIT_POS 2
#define SPYRO_SUB_SUPERCHARGE 0x40    /* charge/charge-air entered from a supercharge */
#define SPYRO_SUB_CHARGE_LATCHED 0x80 /* charge target speed latched (LATCH_CHARGE_SPEED) */
/* Glide/fly substates >= 8 are scripted: flight levels stop the clock and the
 * path tweens while one runs. */
#define SPYRO_SUB_SCRIPTED 8
#define SPYRO_SUB_GLIDE_HOLD 8        /* frozen, turning to g_abSpyroRequestEuler (no-input / level-intro glide-hold frames) */
#define SPYRO_SUB_GLIDE_TO_TARGET 9   /* no-clip glide to the hand-off point, then LEVEL_GLIDE_HANDOFF */
#define SPYRO_SUB_GLIDE_ARRIVE 10     /* level-entry glide under GS_ARRIVAL_CAMERA, banks down to the ground */
#define SPYRO_SUB_GLIDE_ENTER 11      /* level-entry glide straight into gameplay (g_anCameraParamTransitionGlideIn arrival) */

/* Trigger-region record types (byte 0 of a g_pTriggerRegionTable record),
 * dispatched by DispatchSpyroTriggerEvent. The type-specific data words
 * follow the 4-byte header. */
#define TRIGGER_REGION_DATA(rec) ((int *)((rec) + 4))
enum TriggerRegionType {
  TRIGGER_HAZARD = 0,       /* raises SPYRO_EV_HAZARD; data: [0] instant death, [1] g_nSpyroHazardKind */
  TRIGGER_LIFT = 1,         /* raises SPYRO_EV_LIFT (g_pSpyroLiftRegionData) */
  TRIGGER_GROUND_LATCH = 2, /* sets data[0] while Spyro is near the ground */
  TRIGGER_CHARGE_HOLD = 4,  /* raises SPYRO_EV_CHARGE_HOLD */
  TRIGGER_REBOUND = 5,      /* raises SPYRO_EV_REBOUND and spawns ACTOR_TYPE_REBOUND_FX */
  TRIGGER_PORTAL = 6,       /* level warp: data[0] level id, [1] portal index */
  TRIGGER_DAMAGE = 7,       /* while its g_anLevelTriggerStateSlots slot is set: a charge-proof hit */
};

/* Spyro animation transition styles: the bytes of the state transition
 * matrix (g_abSpyroStateTransitionAnimMatrix[prev][state]), and the matching
 * g_nSpyroAnimMode that ApplySpyroStateTransition leaves for
 * TickSpyroAnimStateMachine. Named from what the two functions do; the anim
 * descriptor bytes are [0] loop start, [1] loop end. */
enum SpyroAnimMode {
  SPYRO_ANIM_PLAY = 0,             /* mode: no transition in progress */
  SPYRO_ANIM_BLEND_IN = 1,         /* new anim from frame 0, sub-frame +2 to 0x10 */
  SPYRO_ANIM_LOOP_WRAP = 2,        /* once past loop end: wrap to loop start, +2 */
  SPYRO_ANIM_FINISH = 3,           /* play the old anim out, then commit */
  SPYRO_ANIM_HOLD_END = 4,         /* hold the loop-end frame, then FINISH (landings) */
  SPYRO_ANIM_BLEND_KEEP_FRAME = 5, /* new anim at the old frame + 1, sub-frame +4 */
  SPYRO_ANIM_BLEND_LOOP_START = 6, /* new anim from its loop start, +2 */
  SPYRO_ANIM_CUT = 7,              /* style only: switch at frame 1, no blend */
  SPYRO_ANIM_BLEND_FAST = 8,       /* new anim from frame 0, one 2-step advance */
  SPYRO_ANIM_FINISH_10 = 10,       /* style treated as FINISH */
  SPYRO_ANIM_CUT_LOOP_START = 11,  /* style only: switch at loop start + 1 */
};

/* Spyro's animation ids (g_bSpyroAnimCurrent / Prev, the g_abSpyroAnimDescTable
 * index), each named for the state whose default animation it is in
 * g_abSpyroStateAnimIndexMap (the 0x8006C470 bytes). 46 and 47 are empty. */
enum SpyroAnimId {
  SPYRO_ANIMID_IDLE = 0,
  SPYRO_ANIMID_WALK = 1,
  SPYRO_ANIMID_RUN = 2,
  SPYRO_ANIMID_STOP = 3,
  SPYRO_ANIMID_TURN_AROUND = 4,
  SPYRO_ANIMID_JUMP = 5,
  SPYRO_ANIMID_FALL = 6,
  /* 7: no state plays it by default */
  SPYRO_ANIMID_HURT_TINT = 8,
  SPYRO_ANIMID_FACE_TARGET = 9,
  SPYRO_ANIMID_SIDESTEP_L1 = 10,
  SPYRO_ANIMID_SIDESTEP_R1 = 11,
  SPYRO_ANIMID_CHARGE = 12,
  SPYRO_ANIMID_CHARGE_BONK = 13,
  SPYRO_ANIMID_TEETER = 14,
  SPYRO_ANIMID_HURT = 15,
  SPYRO_ANIMID_GLIDE = 16,
  SPYRO_ANIMID_GLIDE_DROP = 17,
  SPYRO_ANIMID_LIFT = 18,
  SPYRO_ANIMID_FIDGET_0 = 19,
  SPYRO_ANIMID_SLIDE = 20,
  SPYRO_ANIMID_CHARGE_AIR = 21,
  SPYRO_ANIMID_JOG = 22,
  SPYRO_ANIMID_KNOCKBACK = 23,
  SPYRO_ANIMID_GLIDE_SLOW = 24,
  SPYRO_ANIMID_SUPERCHARGE_AIR = 25,
  SPYRO_ANIMID_HURT_PINNED = 26,
  SPYRO_ANIMID_SCRIPTED_WALK = 27,
  SPYRO_ANIMID_HURT_LONG = 28,
  SPYRO_ANIMID_KNOCKBACK_2 = 29,
  SPYRO_ANIMID_HAZARD = 30,
  SPYRO_ANIMID_DEATH = 31,
  SPYRO_ANIMID_DEATH_PINNED = 32,
  SPYRO_ANIMID_FLY = 33,
  SPYRO_ANIMID_FLY_CLIMB = 34,
  SPYRO_ANIMID_FLY_DIVE = 35,
  SPYRO_ANIMID_SCRIPTED_HOLD = 36,
  SPYRO_ANIMID_FIDGET_1 = 37,
  SPYRO_ANIMID_FIDGET_2 = 38,
  SPYRO_ANIMID_FIDGET_3 = 39,
  SPYRO_ANIMID_FIDGET_4 = 40,
  SPYRO_ANIMID_FIDGET_5 = 41,
  SPYRO_ANIMID_FIDGET_6 = 42,
  SPYRO_ANIMID_FIDGET_7 = 43,
  SPYRO_ANIMID_FIDGET_8 = 44,
  SPYRO_ANIMID_SUPERCHARGE = 45,
  SPYRO_ANIMID_COUNT = 46, /* the Model.m_Animations slots InstallSpyroAnimStream fills */
};

/* g_abSpyroAnimDescTable: SPYRO_ANIMID_COUNT 4-byte records, then one byte
 * per animation. */
#define SPYRO_ANIM_DESC_LOOP_START 0 /* frame a looping anim restarts at */
#define SPYRO_ANIM_DESC_LOOP_END 1   /* one past the loop's last frame */
#define SPYRO_ANIM_DESC_FRAMES 2     /* frame count (layer 1 holds the last one) */
#define SPYRO_ANIM_DESC_RATE 3       /* play rate (g_nSpyroFlameAnimRate) */
/* Nonzero: the animation keeps the overlay layers off (the flame breath is
 * cancelled, a layer-2 request is dropped). */
#define SPYRO_ANIM_NO_OVERLAY(anim)                                            \
  g_abSpyroAnimDescTable[SPYRO_ANIMID_COUNT * 4 + (anim)]

/* g_nSpyroFlameState: the flame-breath machine (TickSpyroFlameAttack). */
enum SpyroFlameState {
  SPYRO_FLAME_IDLE = 0,
  SPYRO_FLAME_BREATHING = 1,
};

#endif /* OPEN_SPYRO_SPYRO_STATE_H */
