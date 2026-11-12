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
  SPYRO_ST_WALK = 1,          /* speed <= SPYRO_JOG_SPEED */
  SPYRO_ST_RUN = 2,           /* speed > SPYRO_RUN_SPEED */
  SPYRO_ST_STOP = 3,          /* braking with no stick input */
  SPYRO_ST_TURN_AROUND = 4,   /* reversing at speed (heading off by > 0x500) */
  SPYRO_ST_JUMP = 5,          /* Cross */
  SPYRO_ST_FALL = 6,          /* airborne >= 4 frames or on a too-steep slope */
  SPYRO_ST_HURT_TINT = 7,     /* hit, SPYRO_EV_HIT_TINT: flashes white/dark (g_abSpyroTintRgba) for 10 frames; exit clears the tint */
  SPYRO_ST_FACE_TARGET = 8,   /* stands still; reached only as g_nSpyroRequestState (the level 14 carrier, ACTOR_TYPE_CARRIER, sets it at its ride path end); after 0x14 frames snaps yaw to g_nSpyroMoveTargetYaw, negates pitch/roll, then idles or walks */
  SPYRO_ST_SIDESTEP_L1 = 9,   /* L1 */
  SPYRO_ST_SIDESTEP_R1 = 10,  /* R1 */
  SPYRO_ST_CHARGE = 11,       /* Square held, on the ground */
  SPYRO_ST_CHARGE_BONK = 12,  /* charge stopped by a wall: dizzy stars + rumble */
  SPYRO_ST_TEETER = 13,       /* idle at a cliff edge (g_nSpyroCliffEdgeHint) */
  SPYRO_ST_HURT = 14,         /* hit, default event */
  SPYRO_ST_GLIDE = 15,        /* Cross again while a jump falls */
  SPYRO_ST_GLIDE_DROP = 16,   /* Triangle while gliding: drop straight down */
  SPYRO_ST_LIFT = 17,         /* carried up while SPYRO_EV_LIFT holds, then glides */
  SPYRO_ST_FIDGET_0 = 18,     /* idle fidget animations: 18 and 36..43 */
  SPYRO_ST_SLIDE = 19,        /* sliding down a steep slope */
  SPYRO_ST_CHARGE_AIR = 20,   /* charge leaving the ground (Square in a jump) */
  SPYRO_ST_JOG = 21,          /* speed between WALK and RUN */
  SPYRO_ST_KNOCKBACK = 22,    /* hit, SPYRO_EV_KNOCKBACK: launched by g_anSpyroRequestVelocity */
  SPYRO_ST_GLIDE_SLOW = 23,   /* gliding below SPYRO_RUN_DROP_SPEED */
  SPYRO_ST_SUPERCHARGE_AIR = 24, /* SUPERCHARGE leaving the ground */
  SPYRO_ST_HURT_PINNED = 25,  /* hit, SPYRO_EV_HIT_PINNED, health left: pushed into the ground */
  SPYRO_ST_SCRIPTED_WALK = 26, /* slow walk steered round g_pScriptedCameraActor */
  SPYRO_ST_HURT_LONG = 27,    /* hit, SPYRO_EV_HIT_LONG: 0x30-frame recovery */
  SPYRO_ST_KNOCKBACK_2 = 28,  /* hit, SPYRO_EV_KNOCKBACK_2: as KNOCKBACK */
  SPYRO_ST_HAZARD = 29,       /* SPYRO_EV_HAZARD: Cross escapes after 0x10 frames, 0x3C kills */
  SPYRO_ST_DEATH = 30,        /* a hurt state ending with g_nSparxHealth < 0 */
  SPYRO_ST_DEATH_PINNED = 31, /* SPYRO_EV_HIT_PINNED with no health left */
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
  SPYRO_ST_SUPERCHARGE = 44,  /* charge above SPYRO_SUPERCHARGE_SPEED */
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

/* Spyro's collision-sphere radius; standing, his position is this far above
 * the floor (probes, contact spheres, spawn and landing heights). */
#define SPYRO_RADIUS 0x164
/* AdvanceSpyroPhysics moves him in substeps of at most COLLIDE_STEP, a
 * collision try correcting him by at most MAX_CORRECTION; each steep-wall
 * contact adds its normal at WALL_NUDGE length to the wall-slide nudge. */
#define SPYRO_COLLIDE_STEP 340
#define SPYRO_MAX_CORRECTION 0x140
#define SPYRO_WALL_NUDGE 0x400

/* g_nSpyroAirborneFrames at which a grounded state gives up and falls: a
 * few frames of grace over gaps and bumps. */
#define SPYRO_AIRBORNE_GRACE 4

/* g_nSpyroMotionSpeed bands that pick the ground gait on landing / speed
 * change (SPYRO_ST_WALK / JOG / RUN). */
#define SPYRO_JOG_SPEED 0x780       /* above: jog */
#define SPYRO_RUN_SPEED 0xF00       /* above: run */
#define SPYRO_RUN_DROP_SPEED 0xC80  /* below: a run drops back to a jog; also the
                                     * fast/slow split for glides, leaps and lean */

/* Speed limits (g_nSpyroMotionSpeed / g_nSpyroSpeedTarget, and |velocity| in
 * the same units), from IntegrateSpyroMotionForSubstep's per-state clamps. */
#define SPYRO_TERMINAL_SPEED 0x2300 /* fastest fall (-velocity Z); also caps
                                     * slide and charge-wind-down speed */
#define SPYRO_MAX_SPEED 0x5900      /* cap for charge, supercharge and flight */
#define SPYRO_SUPERCHARGE_SPEED 0x3000 /* a held charge (SPYRO_EV_CHARGE_HOLD) past this
                                        * becomes SPYRO_ST_SUPERCHARGE */
#define SPYRO_JUMP_SPEED 0xDC0      /* upward velocity a jump (and a charge or
                                     * supercharge leaving the ground) adds */
#define SPYRO_CHARGE_SPEED 0x1F80   /* least speed a charge (ground or air) starts at */
#define SPYRO_CHARGE_MIN_SPEED 0x1400 /* charge / supercharge floor: slower
                                       * resets the speed target to it */
#define SPYRO_GLIDE_SPEED 0x1900    /* glide target speed (stick back slows it);
                                     * a glide starts no faster */
#define SPYRO_GLIDE_MIN_SPEED 0x780 /* glide and flight floor */
/* g_nSpyroStateTimer limits (substeps since the state began): */
#define SPYRO_LATE_JUMP_TICKS 4        /* FALL (g_nSpyroLateJumpAllowed) / SLIDE:
                                        * Cross this early still jumps */
#define SPYRO_GLIDE_DROP_DELAY 0x10    /* GLIDE / FLY*: Triangle drops out of the
                                        * glide (GLIDE_DROP) only after this */
#define SPYRO_FLY_PITCH_MIN_TICKS 0x1F /* FLY_CLIMB / FLY_DIVE: least time before
                                        * levelling back out to FLY */
#define SPYRO_STUCK_RESPAWN_TICKS 0x12C /* FALL / SLIDE held this long: respawn,
                                         * refunding the life (or end a flight run) */
/* TickSpyroFlameAttack's breath, by g_nSpyroFlameTimer: the flame anchor
 * drops at FLAME_ANCHOR_DROP_FRAME; every FLAME_EMIT_PERIOD frames from
 * FLAME_EMIT_FIRST to FLAME_EMIT_LAST two flame particles spawn; a fresh
 * Circle from FLAME_REARM_FRAME starts a second breath, and at
 * FLAME_END_FRAME the breath ends. */
#define FLAME_ANCHOR_DROP_FRAME 0x10
#define FLAME_EMIT_FIRST 0xC
#define FLAME_EMIT_LAST 0x1C
#define FLAME_EMIT_PERIOD 4
#define FLAME_REARM_FRAME 0x2C
#define FLAME_END_FRAME 0x30

/* TickSpyroAttackEffects. A superflame tints Spyro SUPERFLAME_TINT_R/G/B
 * with a sine alpha (minus TINT_ALPHA_BIAS); under WARN_FRAMES left the tint
 * blinks off from BLINK_OFF of every BLINK_PERIOD frames. Each flame-breath
 * slot (four short arcs, four long) walks its arc path's ARC_POINTS points
 * and frees itself at ARC_LIFE frames, probing actors at FLAME_HIT_RADIUS;
 * a slot arms with end age FLAME_SLOT_END_AGE (its burst frame if it hits). */
#define SUPERFLAME_TINT_R 0x90
#define SUPERFLAME_TINT_G 0x20
#define SUPERFLAME_TINT_B 0x10
#define SUPERFLAME_TINT_ALPHA_BIAS 0x28
/* SPYRO_ST_HURT_TINT (DispatchSpyroPhysicsByState): for its first
 * HURT_FLASH_FRAMES animation frames Spyro's tint alternates white and dark
 * grey (odd / even frames). */
#define HURT_FLASH_FRAMES 10
#define HURT_FLASH_LIGHT 0xFF
#define HURT_FLASH_DARK 0x40
#define SUPERFLAME_WARN_FRAMES 0x78
#define SUPERFLAME_BLINK_PERIOD 24
#define SUPERFLAME_BLINK_OFF 0xE
#define FLAME_SHORT_ARC_POINTS 0x18
#define FLAME_SHORT_ARC_LIFE 0x20
#define FLAME_LONG_ARC_POINTS 0x20
#define FLAME_LONG_ARC_LIFE 0x28
#define FLAME_HIT_RADIUS 0xA0
#define FLAME_SLOT_END_AGE 0x20

/* UpdateSpyroStateBehavior's other g_nSpyroStateTimer thresholds: */
#define SPYRO_WALK_MIN_TICKS 0x10      /* WALK / SCRIPTED_WALK: least time before
                                        * stopping back to IDLE */
#define SPYRO_STOP_TURN_TICKS 0x12     /* STOP: reversing this early turns around */
#define SPYRO_TURN_AROUND_TICKS 0x25   /* TURN_AROUND's length */
#define SPYRO_JUMP_LAND_TICKS 0x10     /* JUMP: least time before it can land */
#define SPYRO_JUMP_FALL_TICKS 0x78     /* JUMP: longer than this becomes FALL */
#define SPYRO_REGLIDE_TICKS 0x10       /* glide already spent: falling this long
                                        * re-allows FLY (flight) / GLIDE (glide drop) */
#define SPYRO_REGLIDE_FALL_TICKS 0x1F  /* ...and GLIDE from a plain FALL */
#define SPYRO_FACE_TARGET_SNAP_TICKS 0xD /* FACE_TARGET: interrupted after this, it
                                          * still snaps to the target heading */
#define SPYRO_FACE_TARGET_TICKS 0x14   /* FACE_TARGET's length */
#define SPYRO_SIDESTEP_MIN_TICKS 0x15  /* SIDESTEP: least length once released */
#define SPYRO_BONK_FALL_TICKS 0x10     /* CHARGE_BONK: airborne after this: FALL */
#define SPYRO_BONK_TICKS 0x19          /* CHARGE_BONK's length */
#define SPYRO_TEETER_MIN_TICKS 0x1F    /* TEETER: least length off the edge hint */
#define SPYRO_HURT_TICKS 0x18          /* HURT / HURT_TINT / HURT_PINNED / KNOCKBACK */
#define SPYRO_HURT_LONG_TICKS 0x30     /* HURT_LONG */
#define SPYRO_GLIDE_SLOW_TICKS 0x10    /* GLIDE: slowing below RUN_DROP_SPEED after
                                        * this becomes GLIDE_SLOW */
#define SPYRO_GLIDE_DROP_FALL_TICKS 0x18 /* GLIDE_DROP: landing later than this
                                          * falls on (keeping the fall's timer) */
#define SPYRO_HAZARD_TICKS 0x3C        /* HAZARD: longer and the hazard claims him */
#define SPYRO_HAZARD_JUMP_TICKS 0x10   /* HAZARD: Cross jumps out after this */
#define SPYRO_DEATH_TICKS 0x65         /* DEATH: respawn after this */
#define SPYRO_DEATH_PINNED_TICKS 0x7D  /* DEATH_PINNED: respawn after this */
/* HAZARD: once sinking starts, the depth grows SPYRO_HAZARD_SINK_STEP a
 * substep, up to SPYRO_HAZARD_SINK_SURVIVE while Sparx still has health, or
 * to SPYRO_HAZARD_SINK_FATAL on the fatal hit, where Spyro respawns (or a
 * flight level ends the run). */
#define SPYRO_HAZARD_SINK_STEP 8
#define SPYRO_HAZARD_SINK_SURVIVE 0x96
#define SPYRO_HAZARD_SINK_FATAL 0x240
/* The level box's X / Y edge: Spyro closer than this to 0 falls out. */
#define SPYRO_WORLD_EDGE_XY 0x800
#define SPYRO_SUPERCHARGE_MIN_TICKS 0xF /* SUPERCHARGE: letting go of Square
                                         * this early stays supercharged */
#define SPYRO_SUPERCHARGE_JUMP_HOLD 10 /* SUPERCHARGE: frames Cross is held before
                                        * it jumps on its own */
/* g_nSpyroInputBlankCountdown floors on landing. */
#define SPYRO_DROP_LAND_BLANK 8        /* out of a glide drop */
#define SPYRO_HURT_LAND_BLANK 0xC      /* out of a hurt state */

/* UpdateSpyroStateBehavior speed / motion / angle thresholds. */
#define SPYRO_WALK_DROP_SPEED 0x640    /* JOG / RUN slower than this: WALK */
#define SPYRO_REVERSE_ANGLE 0x500      /* RUN / STOP: a move target this far
                                        * (12-bit) off his heading reverses */
#define SPYRO_TURN_AROUND_STICK 0x30   /* RUN reversing: stick Y past this turns
                                        * around, else stops */
#define SPYRO_TURN_AROUND_EXIT_SPEED 0xA00 /* TURN_AROUND -> WALK speed */
#define SPYRO_FACE_TARGET_EXIT_SPEED 0x15  /* FACE_TARGET -> WALK speed */
#define SPYRO_SHOVED_MOTION 0x400      /* IDLE / FIDGET moved this far in a frame
                                        * (pushed): STOP */
#define SPYRO_STOP_SETTLE_MOTION 0x100 /* STOP at rest and moving less: IDLE */
#define SPYRO_GLIDE_STALL_MOTION 0x400 /* glide / fly moving less a frame: FALL */
#define SPYRO_JUMP_FALL_SPEED 0x1900   /* JUMP falling faster: FALL */
#define SPYRO_FLY_CEILING 0x200        /* GLIDE / FLY*: this far over the floor
                                        * reference: FALL */
#define SPYRO_GLIDE_RISE_MAX 0x18      /* GLIDE / GLIDE_SLOW risen this far over
                                        * the floor reference: FALL */
#define SPYRO_SLIDE_CHARGE_SPEED 0x1F00 /* SLIDE: Square faster than this charges */
#define SPYRO_SLIDE_AIRBORNE_GRACE 8   /* SLIDE: airborne this long: FALL */
#define SPYRO_SLIDE_LAND_MOTION 0x900  /* SLIDE: on a walkable slope at the anim's
                                        * end and moving less: lands */
#define SPYRO_SUPERCHARGE_BONK_RATIO 0x800 /* SUPERCHARGE_AIR: frame motion under
                                            * this / 4096 of |velocity|: bonk */
#define SPYRO_FLY_CLIMB_VZ 0x800       /* FLY rising faster: FLY_CLIMB */
#define SPYRO_FLY_DIVE_VZ 0x1000       /* FLY falling faster: FLY_DIVE */
#define SPYRO_FLY_CLIMB_LEVEL_VZ 0x600 /* FLY_CLIMB rising slower: back to FLY */
#define SPYRO_FLY_DIVE_LEVEL_VZ 0xC00  /* FLY_DIVE falling slower: back to FLY */

/* Four separate 0x1E00 limits: */
#define SPYRO_HURT_KNOCK_MAX_SPEED 0x1E00 /* HURT / HURT_TINT: cap on the knock-back
                                           * velocity (the reversed motion) */
#define SPYRO_BANK_MIN_SPEED 0x1E00      /* glide / fly with R1 or L1 held: slower,
                                          * and not banking, ends the substate */
#define SPYRO_CHARGE_AIR_SPEED 0x1E00    /* CHARGE_AIR speed target, unless entered
                                          * from a supercharge */
#define SPYRO_SUPERCHARGE_EXIT_SPEED 0x1E00 /* SUPERCHARGE: slower drops to CHARGE */
#define SPYRO_FLIGHT_CRUISE_SPEED 0x1040 /* level flight: falling below the
                                          * floor reference adds speed, rising
                                          * above it takes it away */

/* Ground and air motion per substep (IntegrateSpyroMotionForSubstep). */
#define SPYRO_GROUND_ACCEL 0x140     /* AdvanceSpyroSpeedTowardTarget accel: walk /
                                      * jog / run / stop, jump, sidestep */
#define SPYRO_GROUND_DECEL 0x0C0     /* its decel (also hurt), and the brake while
                                      * a gait substate is set */
#define SPYRO_PAD_TURN_SPEED 0x400   /* slower: the pad turns his body directly
                                      * (ApplyPadInputToSpyroBodyYaw); faster:
                                      * TurnSpyroTowardMoveTargetWithLean */
#define SPYRO_SHARP_TURN 0x200       /* heading error (12-bit) past which a gait
                                      * stops accelerating */
#define SPYRO_TURN_AROUND_BRAKE 0x100 /* TURN_AROUND: speed lost per substep */
#define SPYRO_JUMP_MOVE_SPEED 0xC80  /* JUMP: the stick's full-tilt speed target */
#define SPYRO_JUMP_STEER_MAX 0x280   /* JUMP: most the stick bends his air velocity */
#define SPYRO_JUMP_HOLD_TICKS 0xD    /* JUMP: Cross held this early in the rise... */
#define SPYRO_JUMP_HOLD_LIFT 0xD7    /* ...adds this lift (on top of gravity) */
#define SPYRO_FALL_STEER_MAX 0x40    /* FALL: most air drag removes per substep */
#define SPYRO_SIDESTEP_SPEED 0x1000  /* SIDESTEP_L1 / _R1 speed target */
#define SPYRO_SCRIPTED_FLAME_DELAY 0x2C /* idle under a scripted move: breathe
                                         * flame after this many substeps */
/* CHARGE: a homing target (g_pSpyroHomingTargetActor) pulls his heading for
 * HOMING_TICKS while his heading is at least HOMING_CONE off the camera's and it is
 * within SPYRO_HOMING_CONE of his heading and HOMING_RANGE of him
 * (ChangeSpyroState picks it on entering CHARGE: the lowest-scoring live
 * colliding actor in that cone and range). Off a
 * charge-hold strip his carried motion loses CHARGE_DRAG / 4096 a substep
 * and gains CHARGE_THRUST along his heading; letting go of the charge slows
 * him by CHARGE_STOP_DECEL. */
#define SPYRO_HOMING_TICKS 0x78
#define SPYRO_HOMING_CONE 0x200
#define SPYRO_HOMING_RANGE 0x1800
#define SPYRO_HOMING_QUICK_RANGE 0x4000 /* ChangeSpyroState: |dx|+|dy|+|dz| cull */
#define SPYRO_HOMING_ANGLE_WEIGHT 4     /* score = distance + angle off * this */
#define SPYRO_HOMING_NO_SCORE 0x7530    /* worse than any candidate's score */
#define SPYRO_CHARGE_DRAG 300
#define SPYRO_CHARGE_THRUST 0x240
#define SPYRO_CHARGE_STOP_DECEL 0x180
#define SPYRO_PINNED_FALL_ACCEL 0x600 /* HURT_PINNED / HURT_LONG airborne */
#define SPYRO_HURT_LONG_SPIN 0x55     /* HURT_LONG: body yaw added per substep */
#define SPYRO_PINNED_PRESS 0xC00      /* HURT_PINNED: downward velocity on entry */
#define SPYRO_STOP_MAX_SPEED 0x1400   /* entering STOP caps the speed here */
#define SPYRO_SCRIPTED_WALK_SPEED 0x280 /* SCRIPTED_WALK's move-target speed */
/* Glide / flight attitude (12-bit angles). With the stick idle the d-pad
 * stands in for it: each held direction moves the virtual stick
 * FLY_DPAD_STEP a substep (to +-0x7F), released it recentres by up to
 * FLY_DPAD_RECENTRE. Roll is held within +-FLY_MAX_ROLL and pitch within
 * +-FLY_MAX_PITCH; the R1 / L1 rolls and the levelling turn the body at most
 * ROLL_RATE a substep, a roll past ROLL_QUARTER counting as banked. Outside a
 * flight level (Gnasty's Loot's free flight) the speed gets
 * FREE_FLIGHT_BOOST on top of the cruise speed. */
#define SPYRO_FLY_DPAD_STEP 0x10
#define SPYRO_FLY_DPAD_RECENTRE 0x20
#define SPYRO_FLY_MAX_ROLL 0x320
#define SPYRO_FLY_MAX_PITCH 0x300
#define SPYRO_ROLL_RATE 0x1E
#define SPYRO_ROLL_QUARTER 0x400
#define SPYRO_FREE_FLIGHT_BOOST 0x41E

/* Gravity per substep: ChangeSpyroState sets g_nSpyroGravityStep to minus
 * one of these on entering a state (LIFT rises at +0x80 instead). */
#define SPYRO_FALL_ACCEL 0x0C0       /* jump / fall / glide drop; also the pull
                                      * added at a cliff edge or when airborne */
#define SPYRO_LIGHT_FALL_ACCEL 0x080 /* idle, glide and flight */
#define SPYRO_SLIDE_FALL_ACCEL 0x200
#define SPYRO_CHARGE_FALL_ACCEL 0x240
#define SPYRO_HURT_FALL_ACCEL 0x300  /* hurt states and the charge bonk */
#define SPYRO_GROUND_PRESS 0xC00     /* walking / standing states: pins Spyro to
                                      * the floor, projected along the slope */

/* g_bSpyroOtBinBias (SpyroObj.otBinBias): ordering-table bins Spyro's model
 * is pulled toward the camera. SampleSpyroShadowRingHeights resets it to
 * SPYRO_OT_BIAS_DEFAULT every tick unless it is SPYRO_OT_BIAS_PINNED, which
 * the portal guides set while Spyro jumps and glides into a portal. */
#define SPYRO_OT_BIAS_DEFAULT 5
#define SPYRO_OT_BIAS_PINNED 0x7F
/* UpdateGameplayCamera, line of sight to Spyro blocked: a smaller pull (and
 * no shadow bias) for the frame. */
#define SPYRO_OT_BIAS_OCCLUDED 2
/* SampleSpyroShadowRingHeights: the drop shadow's OT bias (one bin pair
 * deeper once Spyro is SHADOW_HIGH_ABOVE above the ground), and its 8-vertex
 * ring probed one vertex a tick from SHADOW_PROBE_LIFT above, keeping ground
 * within SHADOW_PROBE_WINDOW of the vertex; the stored height is scaled by
 * SHADOW_HEIGHT_SCALE / 0x200. */
#define SHADOW_OT_BIAS 3
#define SHADOW_OT_BIAS_HIGH 5
#define SHADOW_HIGH_ABOVE 0x365
#define SHADOW_RING_VERTS 8
#define SHADOW_PROBE_LIFT 0x200
#define SHADOW_PROBE_DEPTH 0x400
#define SHADOW_PROBE_WINDOW 0x80
#define SHADOW_HEIGHT_SCALE 0xC2

/* g_dwSpyroRequestMask bits (SpyroObj.requestMask): a scripted override of
 * Spyro posted by actors and cutscenes, applied by AdvanceSpyroPhysics while
 * SPYRO_RQ_PENDING is set. */
#define SPYRO_RQ_VEL_X 0x00001         /* drive X toward g_anSpyroRequestTargetPos */
#define SPYRO_RQ_VEL_Y 0x00002
#define SPYRO_RQ_VEL_Z 0x00004
#define SPYRO_RQ_PITCH 0x00008         /* take g_abSpyroRequestEuler[0]; PITCH and ROLL are
                                        * handled but no caller requests them, and
                                        * nothing stores euler [0] / [1] (xref audit) */
#define SPYRO_RQ_ROLL 0x00010          /* [1] */
#define SPYRO_RQ_YAW 0x00020           /* [2] */
#define SPYRO_RQ_RAW_VELOCITY 0x00040  /* VEL_* take g_anSpyroRequestVelocity */
#define SPYRO_RQ_FORCE_STATE 0x00100   /* g_nSpyroRequestState; no state update */
#define SPYRO_RQ_CAMERA 0x00200        /* CAM_MODE_REQUEST on the request anchor */
#define SPYRO_RQ_PORTAL_PATH_CAMERA 0x00400 /* CAM_MODE_PORTAL_PATH */
#define SPYRO_RQ_LOCK_ON 0x01000       /* face g_pScriptedCameraActor (EnemyLock params) */
/* SPYRO_RQ_LOCK_ON, manning a fireball cannon (levels 6 / 24;
 * AdvanceSpyroPhysics): once the camera has been within LOCK_ON_ALIGN_ANGLE
 * (8-bit) of the cannon's facing for LOCK_ON_SETTLE_FRAMES, a pad move with
 * Spyro inside the cone behind the cannon (LOCK_ON_CONE, or _WALK while
 * already circling; narrower in Peace Keepers) and heading within
 * LOCK_ON_ORBIT_MATCH of a quarter turn off its facing starts
 * SPYRO_ST_SCRIPTED_WALK round it. The camera anchors LOCK_ON_ANCHOR_LIFT
 * above the cannon. */
#define LOCK_ON_ALIGN_ANGLE 0x10
#define LOCK_ON_SETTLE_FRAMES 0x1E
#define LOCK_ON_CONE 0x30
#define LOCK_ON_CONE_WALK 0x38
#define LOCK_ON_CONE_PEACE_KEEPERS 0x28
#define LOCK_ON_CONE_WALK_PEACE_KEEPERS 0x32
#define LOCK_ON_ORBIT_TURN 0x40
#define LOCK_ON_ORBIT_MATCH 0x200
#define LOCK_ON_ANCHOR_LIFT 0x200
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
/* SPYRO_ST_IDLE under a scripted move: the scripted flame breath was pressed;
 * the next tick hands control back. */
#define SPYRO_SUB_IDLE_FLAMED 1
/* SPYRO_ST_WALK / RUN / JOG: cleared every substep; ApplyPadInputToSpyroBodyYaw
 * sets it when a light push asks for a turn sharper than SPYRO_PIVOT_TURN, and
 * Spyro brakes (SPYRO_GROUND_DECEL) instead of accelerating while he pivots. */
#define SPYRO_SUB_WALK_PIVOT 1
#define SPYRO_PIVOT_TURN 0x100
/* ...and only while the stick is pushed less than SPYRO_PIVOT_STICK (of
 * PAD_STICK_MAX): a light push turns Spyro toward it at a rate scaled by the
 * push. */
#define SPYRO_PIVOT_STICK 0x60
/* SPYRO_ST_CHARGE: ChangeSpyroState sets bit 0 on every ground-charge entry
 * and nothing clears it while charging, so the stick-steer / homing branch it
 * gates always runs (SUPERCHARGE, which shares the latch bits, never sets it). */
#define SPYRO_SUB_CHARGE_STEER 1
/* SPYRO_ST_FALL out of the level (below its death plane or off the box's
 * edge): the substate counts frames, and at SPYRO_DEATH_FALL_FRAMES Spyro
 * respawns (TickSpyroGameplayFrame). */
#define SPYRO_DEATH_FALL_FRAMES 0x79
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
/* g_nGroundSurfaceType / g_nSpyroFirstContactSurfaceType: the low bits index
 * g_pTriggerRegionTable, TRIGGER_REGION_NONE meaning no region; the first
 * contact is SURFACE_NO_CONTACT until something is hit this frame. */
#define TRIGGER_REGION_MASK 0x3F
#define TRIGGER_REGION_NONE 0x3F
#define SURFACE_NO_CONTACT 0xFF
/* DispatchSpyroTriggerEvent's event (AdvanceSpyroFrameMotion): the region
 * Spyro first touched this frame, the one on the ground below him, or the
 * one he stands on (grounded, within TRIGGER_ON_HEIGHT). A ground latch arms
 * for touch / over within GROUND_LATCH_HEIGHT. */
#define TRIGGER_EV_TOUCH 0
#define TRIGGER_EV_OVER 1
#define TRIGGER_EV_ON 2
#define TRIGGER_ON_HEIGHT 0x200
#define GROUND_LATCH_HEIGHT 0x190
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
/* Field `f` (SPYRO_ANIM_DESC_*) of animation `anim`'s record. */
#define SPYRO_ANIM_DESC(anim, f) g_abSpyroAnimDescTable[(anim) * 4 + (f)]
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
