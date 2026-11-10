/* actor_modes.h — named ->mode / ->subMode values for the actor classes the
 * level overlays share (each level's UpdateActors carries its own copy of
 * these state machines). Values are decoded from the clone bodies; included
 * by actor_type.h. */

#ifndef OPEN_SPYRO_ACTOR_MODES_H
#define OPEN_SPYRO_ACTOR_MODES_H

/* ->subMode of a collectible (extra life, Sparx food, gems; CollectibleState),
 * as the level UpdateActors clones step it. */
enum CollectSubMode {
  COLLECT_SETTLE = 0, /* fresh: sample the floor normal, face along it, then rest */
  COLLECT_REST = 1,   /* on the ground; calls Sparx over when Spyro is near */
  COLLECT_BOUNCE = 2, /* knocked loose: arcs and bounces until it settles */
  COLLECT_HOMING = 3, /* pulled toward Spyro (fetched, or a DROP_TUMBLE), then collected */
  COLLECT_INERT = 4,  /* skipped: a seed pod's displayed prize (the pod moves it) */
};

/* Spyro collects a pickup within TOUCH_RANGE (2D) and from TOUCH_BELOW under
 * it to TOUCH_ABOVE over it, once its clock (CollectibleState.timer) is past
 * ARM_FRAMES; a homing one is collected once the clock passes
 * HOMING_COLLECT_TICK. An undrawn one resting still beyond IDLE_RANGE skips
 * its update. */
#define COLLECT_TOUCH_RANGE 0x200
#define COLLECT_TOUCH_BELOW 0x17F
#define COLLECT_TOUCH_ABOVE 0x200
#define COLLECT_ARM_FRAMES 0x20
#define COLLECT_HOMING_COLLECT_TICK 0x40
/* COLLECT_BOUNCE: a knocked-loose pickup moves at most BOUNCE_MAX_STEP a
 * frame, falls BOUNCE_GRAVITY faster a frame down to BOUNCE_MAX_FALL, and
 * collides as a COLLECT_RADIUS sphere centred that far above its base. On its
 * last bounce it rests once within REST_DROP of the floor on a slope under
 * REST_MAX_SLOPE (8-bit). */
#define COLLECT_BOUNCE_MAX_STEP 0xDC
#define COLLECT_BOUNCE_GRAVITY 5
#define COLLECT_BOUNCE_MAX_FALL 0xDC
#define COLLECT_RADIUS 0xF0
#define COLLECT_REST_DROP 0x190
#define COLLECT_REST_MAX_SLOPE 0x18
#define COLLECT_IDLE_RANGE 0x2000
/* COLLECT_HOMING: the pickup sits st->timer / 32 of the way from where the
 * pull began (the state's first vector) to Spyro, on a sine arc; it snaps
 * onto Spyro (timer = DONE) once a 1/32 step of that gap exceeds SNAP_STEP,
 * and stays on him from DONE on. */
#define COLLECT_HOMING_SNAP_STEP 480
#define COLLECT_HOMING_DONE 0x20
/* A knocked-loose pickup's bounce chime volume (g_nSfxOverrideVolL/R), shifted
 * right by 3 - bounces left so each bounce is half as loud as the last. */
#define COLLECT_BOUNCE_VOLUME 0x3CCC

/* CollectibleState.mode: how a COLLECT_REST collectible holds its place. */
enum CollectRestMode {
  COLLREST_STILL = 0,    /* stays put */
  COLLREST_SNAP = 1,     /* snaps to the floor every frame (gems lean along it) */
  COLLREST_ON_TRI = 2,   /* rides the centroid of its ground triangle (a moving chunk) */
  COLLREST_FIND_TRI = 3, /* looks for the triangle below it, then ON_TRI */
};

/* Sparx's ->subMode while he has a target (SparxState.target); with none he
 * hovers beside Spyro. */
enum SparxSubMode {
  SPARX_SUB_CHASE = 0,    /* closing in on a butterfly (set by the butterfly) */
  SPARX_SUB_EAT = 1,      /* caught it: +1 health, despawn the butterfly */
  SPARX_SUB_EAT_DONE = 2, /* back to the hover animation, drop the target */
  SPARX_SUB_FETCH = 4,    /* flying to a pickup, which he sends COLLECT_HOMING */
  SPARX_SUB_PARKED = 99,  /* takes no targets; never set (no 0x63 store to subMode in main or any
                           * overlay), so the ~100 tests of it are dead */
};

/* A resting collectible calls Sparx over (SPARX_SUB_FETCH) once Spyro, on the
 * ground, is within this 2D distance (ApproxDist2D) and height of it. */
#define SPARX_FETCH_RANGE 0x59A
#define SPARX_FETCH_DZ 0x200

/* Sparx drops his target once Spyro is farther than TARGET_LEASH (2D) from
 * him, and reaches it within CATCH_RANGE (3D, Sparx to target). */
#define SPARX_TARGET_LEASH 0x2000
#define SPARX_CATCH_RANGE 0x200
/* SparxState.timer when a butterfly calls him (RandomIntInRange): he steps
 * to the catch animation when it runs out, or once it is under
 * CATCH_EARLY_FRAMES and the butterfly's heading lines up with the camera. */
#define SPARX_CHASE_MIN_FRAMES 150
#define SPARX_CHASE_MAX_FRAMES 210
#define SPARX_CATCH_EARLY_FRAMES 80
/* With no target Sparx hovers at a random drift HOVER_BEHIND behind Spyro
 * (his body frame), FREE_LOOK_PULLBACK further back under the free-look
 * camera, and at least CHARGE_HOVER_MIN_Z up during a charge; his pitch
 * follows his climb within +-MAX_PITCH. A fetch moves him FETCH_STEP_FAR a
 * frame toward the pickup (FETCH_STEP_NEAR once within that). */
#define SPARX_HOVER_BEHIND 0x258
#define SPARX_FREE_LOOK_PULLBACK 0x200
#define SPARX_CHARGE_HOVER_MIN_Z 0x64
#define SPARX_MAX_PITCH 0x20
#define SPARX_FETCH_STEP_FAR 0x136
#define SPARX_FETCH_STEP_NEAR 0x122
/* Hovering Sparx is pushed out of the world as a sphere of this radius. */
#define SPARX_COLLIDE_RADIUS 0x100
/* Blue or better he twinkles from both wingtips, WING_SPAN to either side;
 * gold adds a glow that starts at GLOW_START_SIZE and grows GLOW_GROW a frame
 * to SPARX_GLOW_MAX_SIZE. */
#define SPARX_WING_SPAN 0x64
#define SPARX_GLOW_START_SIZE 0x40
#define SPARX_GLOW_GROW 0x20

/* g_nSfxOverrideVolL/R (of SPU_VOLUME_MAX) for Sparx's buzz when he
 * sets off to fetch, and for the chime when he hands the pickup over. */
#define SPARX_BUZZ_VOLUME 0x2000
#define SPARX_HANDOVER_VOLUME 0x3600

/* Sparx's hover animation per health colour; the odd index after each is the
 * same colour's catch animation (the chase steps up to it, EAT_DONE back). */
enum SparxAnim {
  SPARX_ANIM_GREEN = 0,
  SPARX_ANIM_BLUE = 2,
  SPARX_ANIM_GOLD = 4,
};

/* ->mode of a dragon egg (ACTOR_TYPE_DRAGON_EGG, EggRiseState). */
enum EggMode {
  EGG_CARRIED = 0,      /* held by its egg thief */
  EGG_RELEASED = 1,     /* the thief was caught: freeze Spyro, start the rise */
  EGG_SPIRAL_UP = 2,    /* spirals up off its spot (Cross skips to EGG_COLLECTED) */
  EGG_TO_SPYRO = 3,     /* flies to the apex above Spyro */
  EGG_SPIRAL_DOWN = 4,  /* spirals down into Spyro */
  EGG_COLLECTED = 5,    /* counts the egg and despawns */
};

/* EMIT_ACTOR_SPARK colour of the sparks a flying egg trails
 * (EGG_SPIRAL_UP .. EGG_SPIRAL_DOWN). */
#define EGG_SPARK_COLOR 0x602080
/* The rise (EggRiseState.phase, frames): SPIRAL_UP and SPIRAL_DOWN each last
 * SPIRAL_FRAMES, EGG_TO_SPYRO FLIGHT_FRAMES; it flies to APEX_HEIGHT above
 * Spyro and spirals down into him from SPIRAL_RADIUS. */
#define EGG_SPIRAL_FRAMES 0x80
#define EGG_FLIGHT_FRAMES 0x40
#define EGG_APEX_HEIGHT 0x300
#define EGG_SPIRAL_RADIUS 0x200
/* The apex sits 0x400 out from Spyro at 45 degrees to his heading: each
 * axis takes APEX_DIAG (0x400 / sqrt 2) times the sum / difference of his
 * heading's cosine and sine (some clones spell it APEX_DIAG_4 * 4). */
#define EGG_APEX_DIAG 724
#define EGG_APEX_DIAG_4 0xB5

/* ->mode of the save-prompt host (ACTOR_TYPE_SAVE_HOST, SaveHostState). */
enum SaveHostMode {
  SAVEHOST_WAIT_AWAY = 0,  /* until Spyro is away from the anchor */
  SAVEHOST_WAIT_ARMED = 1, /* until the arming dragon goes dormant (rescued) */
  SAVEHOST_HIDDEN = 2,     /* hidden; pops in once Spyro comes near */
  SAVEHOST_POP_IN = 3,     /* spins and shrinks in */
  SAVEHOST_HOVER = 4,      /* bobs; opens the save menu when Spyro idles at the anchor */
  SAVEHOST_WANDER = 5,     /* glides to a new hover point near the anchor */
  SAVEHOST_POP_OUT = 6,    /* spins and grows out, back to SAVEHOST_HIDDEN */
};

/* Save-host ranges to Spyro (2D ApproxDist2D / height difference): it re-arms
 * once Spyro leaves REARM of the anchor, pops in within APPEAR, prompts when
 * Spyro idles within PROMPT of the anchor, and pops out past a mode's LEASH. */
#define SAVEHOST_REARM_RANGE 0xA00
#define SAVEHOST_APPEAR_RANGE 0x2000
#define SAVEHOST_APPEAR_DZ 0x800
#define SAVEHOST_PROMPT_RANGE 0x400
#define SAVEHOST_PROMPT_DZ 0x200
#define SAVEHOST_HOVER_LEASH 0x2400
#define SAVEHOST_WANDER_LEASH 0x2800
#define SAVEHOST_LEASH_DZ 0xC00
/* Frame counts: a hidden host waits APPEAR_DELAY before it may pop in (after
 * arming, and again after popping out); it hovers WANDER_DELAY (then
 * WANDER_DELAY + 0..0x1F) between glides, each WANDER_FRAMES long. */
#define SAVEHOST_APPEAR_DELAY 0x10
#define SAVEHOST_WANDER_DELAY 0x1E
#define SAVEHOST_WANDER_FRAMES 0x10
/* Popping in it appears POP_RAISE above its base at POP_SCALE (drawn
 * within CULL_RADIUS), shrinking SCALE_STEP and spinning SPIN_STEP a frame
 * to ACTOR_SCALE_UNIT (popping out grows back to POP_SCALE); it snaps to the
 * floor found within PROBE_DEPTH below. A glide aims WANDER_DIST
 * (+0..0x3F) from the anchor toward Spyro. */
#define SAVEHOST_POP_RAISE 0x80
#define SAVEHOST_POP_SCALE 0x60
#define SAVEHOST_CULL_RADIUS 0x20
#define SAVEHOST_SCALE_STEP 4
#define SAVEHOST_SPIN_STEP 8
#define SAVEHOST_PROBE_DEPTH 0x1000
#define SAVEHOST_WANDER_DIST 0x140

/* ->mode of a crystal dragon statue (ACTOR_TYPE_DRAGON_STATUE,
 * DragonStatueState). */
enum DragonStatueMode {
  STATUE_INIT = 0,      /* snapshot the rest pose, hold its checkpoint (CHECKPT_HELD) */
  STATUE_IDLE = 1,      /* shakes now and then; Spyro touching it frees the dragon */
  STATUE_RESCUING = 2,  /* BeginDragonRescue ran (also the save menu's replay) */
};

/* A statue is freed when Spyro comes within TOUCH_RADIUS of it (3D, height
 * scaled by 3/4), tested once he is within NEAR_RANGE (2D). */
#define STATUE_NEAR_RANGE 0x800
#define STATUE_TOUCH_RADIUS 0x440
/* DragonStatueState.phase, an idle statue's clock (frames, wraps past
 * CYCLE_TICKS): glints may spawn on the way, at SETTLE_TICK it snapshots its
 * rest pose and from SHAKE_TICK it rattles. A hit jumps it to SETTLE_TICK. */
#define STATUE_SETTLE_TICK 188
#define STATUE_SHAKE_TICK 192
#define STATUE_CYCLE_TICKS 256

/* ->mode of a butterfly (ACTOR_TYPE_BUTTERFLY, WanderState). */
enum ButterflyMode {
  BUTTERFLY_FLUTTER = 0, /* drifts about its spawn point */
  BUTTERFLY_CIRCLE = 1,  /* Sparx is coming to eat it: circles Spyro */
};

/* A butterfly calls Sparx (spawning him if Spyro has none) while Spyro is
 * below full health and within CALL_RANGE (2D) / CALL_DZ (height) of it; a
 * fluttering one turns back once LEASH from its spawn point. */
#define BUTTERFLY_CALL_RANGE 2400
#define BUTTERFLY_CALL_DZ 1800
#define BUTTERFLY_LEASH 2000
/* A butterfly Sparx is chasing (BUTTERFLY_CIRCLE) orbits Spyro at this radius. */
#define BUTTERFLY_CIRCLE_RADIUS 0x320
/* Height bands over the ground (ProbeActorGroundHeight): a fluttering
 * butterfly climbs when below FLUTTER_MIN_HEIGHT and sinks when above
 * FLUTTER_MAX_HEIGHT, a circling one uses CIRCLE_MIN / MAX; at DROP_HEIGHT or
 * more it is over a drop and keeps its height. */
#define BUTTERFLY_FLUTTER_MIN_HEIGHT 300
#define BUTTERFLY_FLUTTER_MAX_HEIGHT 1024
#define BUTTERFLY_CIRCLE_MIN_HEIGHT 200
#define BUTTERFLY_CIRCLE_MAX_HEIGHT 1000
#define BUTTERFLY_DROP_HEIGHT 2048
/* ButterflyState.offscreenTimer seed: frames it keeps fluttering undrawn. */
#define BUTTERFLY_OFFSCREEN_FRAMES 0x708
/* Flutter random ranges (RandomIntInRange lo, hi): every VEER_MIN..MAX_FRAMES
 * the heading veers by VEER_MIN..MAX either way; every CLIMB_MIN..MAX_FRAMES
 * vz takes a new VZ_MIN..MAX (either sign, forced up / down at the height
 * band edges). A circling butterfly bobs with the CIRCLE_ ranges instead. */
#define BUTTERFLY_VEER_MIN 30
#define BUTTERFLY_VEER_MAX 90
#define BUTTERFLY_VEER_MIN_FRAMES 60
#define BUTTERFLY_VEER_MAX_FRAMES 140
#define BUTTERFLY_FLUTTER_VZ_MIN 10
#define BUTTERFLY_FLUTTER_VZ_MAX 25
#define BUTTERFLY_FLUTTER_CLIMB_MIN_FRAMES 80
#define BUTTERFLY_FLUTTER_CLIMB_MAX_FRAMES 140
#define BUTTERFLY_CIRCLE_VZ_MIN 5
#define BUTTERFLY_CIRCLE_VZ_MAX 20
#define BUTTERFLY_CIRCLE_CLIMB_MIN_FRAMES 40
#define BUTTERFLY_CIRCLE_CLIMB_MAX_FRAMES 80
/* Lined up on its heading but blocked: it turns back by BOUNCE_MIN..MAX
 * (about 0x80, a half turn) and holds that for BOUNCE_MIN..MAX_FRAMES; past
 * BUTTERFLY_LEASH it heads home for LEASH_TURN_FRAMES. */
#define BUTTERFLY_BOUNCE_MIN 98
#define BUTTERFLY_BOUNCE_MAX 158
#define BUTTERFLY_BOUNCE_MIN_FRAMES 30
#define BUTTERFLY_BOUNCE_MAX_FRAMES 80
#define BUTTERFLY_LEASH_TURN_FRAMES 40
/* Flutter step: MoveActorAlongHeading speed / radius. */
#define BUTTERFLY_SPEED 30
#define BUTTERFLY_RADIUS 50

/* ->mode of a seed pod (ACTOR_TYPE_SEED_POD, SeedPodState). */
enum SeedPodMode {
  SEEDPOD_CLOSED = 0, /* struck: spawns its prize (COLLECT_INERT) and opens */
  SEEDPOD_OPEN = 1,   /* shakes with the prize floating above; closes after a while */
};
/* ->subMode of a closed seed pod: its first tick takes the drop-kind tint. */
enum SeedPodSubMode {
  SEEDPOD_SUB_UNTINTED = 0,
  SEEDPOD_SUB_TINTED = 1,
};

/* An open pod's prize is taken once Spyro is within this 2D distance of it
 * and his height is within PRIZE_DZ_BELOW below it (a PRIZE_DZ_SPAN band:
 * -0xDF..+0x1DF), or at once by a supercharge. */
#define SEEDPOD_PRIZE_RANGE 0x1E0
#define SEEDPOD_PRIZE_DZ_BELOW 0xDF
#define SEEDPOD_PRIZE_DZ_SPAN 0x2BF
/* A closed pod updates within IDLE_UPDATE_DIST, an open one within
 * OPEN_UPDATE_DIST. Open, its prize bobs PRIZE_HOVER above it (drawn within
 * PRIZE_CULL), the pod shakes for SHAKE_TICKS and sparks for SPARK_TICKS,
 * and it closes after OPEN_TICKS with a RING_SPARKS spark ring. Bursting, it
 * throws SHARD_PAIRS shard pairs and CHIPS chips. */
#define SEEDPOD_IDLE_UPDATE_DIST 0x10
#define SEEDPOD_OPEN_UPDATE_DIST 0x40
#define SEEDPOD_PRIZE_HOVER 0x1A4
#define SEEDPOD_PRIZE_CULL 0x18
#define SEEDPOD_SHAKE_TICKS 0x40
#define SEEDPOD_SPARK_TICKS 0x20
#define SEEDPOD_OPEN_TICKS 0x56
#define SEEDPOD_RING_SPARKS 6
#define SEEDPOD_SHARD_PAIRS 2
#define SEEDPOD_CHIPS 6

/* A seed pod's voices (PlayActorMeshVoice). */
enum SeedPodVoice {
  SEEDPOD_VOICE_OPEN = 0,
  SEEDPOD_VOICE_CLOSE = 1,
  SEEDPOD_VOICE_BURST = 2, /* the prize collected or a supercharge hit */
};

/* ->mode of a life chest (ACTOR_TYPE_LIFE_CHEST, LifeChestState). */
enum LifeChestMode {
  LIFECHEST_CLOSED = 0, /* shakes on a timer, then lifts its lid */
  LIFECHEST_OPEN = 1,   /* lid up, its rider showing; closes after LIFECHEST_OPEN_FRAMES */
};
/* LifeChestState.phase while closed (frames): at SETTLE_TICK it snapshots its
 * rest pose (a COLLREST_SNAP chest restarts the clock there instead), from
 * SHAKE_TICK it rattles, and past OPEN_TICK it opens; it stays open for
 * OPEN_FRAMES of the open animation, then closes with a random phase. */
#define LIFECHEST_SETTLE_TICK 0x19C
#define LIFECHEST_SHAKE_TICK 0x1A0
#define LIFECHEST_OPEN_TICK 0x1E0
#define LIFECHEST_OPEN_FRAMES 0xE6
/* Broken, it throws SHARD_PAIRS shard pairs, CHIPS chips and a SPARK_RING
 * spark ring. */
#define LIFECHEST_SHARD_PAIRS 2
#define LIFECHEST_CHIPS 6
#define LIFECHEST_SPARK_RING 8

/* A life chest's voices (PlayActorMeshVoice, or m_Sounds[] for BURST). */
enum LifeChestVoice {
  LIFECHEST_VOICE_OPEN = 0,
  LIFECHEST_VOICE_RATTLE = 1, /* the shake before it opens */
  LIFECHEST_VOICE_BURST = 2,  /* as it despawns, on its own voice row */
  LIFECHEST_VOICE_CLOSE = 3,
  LIFECHEST_VOICE_BREAK = 4,  /* when struck */
};

/* ->mode of a level-exit whirlwind (ACTOR_TYPE_LEVEL_EXIT, ExitWhirlState). */
enum LevelExitMode {
  LEVELEXIT_FAR = 0,  /* camera out of range */
  LEVELEXIT_NEAR = 1, /* camera close: its name letters were spawned */
};

/* Level-exit ranges (2D ApproxDist2D): its name letters appear once the
 * camera is within SHOW and go once it leaves HIDE; it emits the full spiral
 * while Spyro is within SPARKLE and lifts him out within ENTER (its name
 * letters spin while he is that close). */
#define LEVELEXIT_SHOW_RANGE 0x2400
#define LEVELEXIT_HIDE_RANGE 0x2800
#define LEVELEXIT_SPARKLE_RANGE 0x4000
#define LEVELEXIT_ENTER_RANGE 0x400
/* Inside ENTER, Spyro LIFT_BOTTOM..LIFT_TOP above it is lifted toward
 * LIFT_TOP while he is lifting already or more than TOP_MARGIN under its
 * depth (ExitWhirlState.depth, else DEFAULT_DEPTH); above the depth with the
 * camera pitched past LEAVE_PITCH (12-bit) he leaves the level. Its name
 * letters spawn NAME_HEIGHT above it. */
#define LEVELEXIT_LIFT_BOTTOM 0x201
#define LEVELEXIT_LIFT_TOP 0x4000
#define LEVELEXIT_DEFAULT_DEPTH 0x2000
#define LEVELEXIT_TOP_MARGIN 0xC00
#define LEVELEXIT_LEAVE_PITCH 0x200
#define LEVELEXIT_NAME_HEIGHT 0x600

/* g_nLevelKeyState: the level key's progress (HUD counter 4). */
enum LevelKeyState {
  KEYSTATE_NONE = 0,         /* key not picked up */
  KEYSTATE_CARRIED = 1,      /* Spyro has the key (ACTOR_TYPE_KEY touched) */
  KEYSTATE_CHEST_OPENED = 2, /* the locked chest burst and dropped its reward */
  KEYSTATE_REWARD_TAKEN = 3, /* that reward was collected (CollectPickupActor) */
};

/* ->subMode of the level key (ACTOR_TYPE_KEY, KeyState). */
enum KeySubMode {
  KEY_WAITING = 0, /* rocks and sparkles until Spyro touches it */
  KEY_TAKEN = 2,   /* picked up (hidden); the locked chest flies it to its lock */
};

/* Spyro picks the key up within this 2D distance and height of it. */
#define KEY_TOUCH_RANGE 0x200
#define KEY_TOUCH_DZ 0x200

/* ->subMode of the locked chest (ACTOR_TYPE_LOCKED_CHEST, LockedChestState). */
enum LockedChestSubMode {
  LOCKCHEST_NO_KEY = 0,    /* waits for KEYSTATE_CARRIED */
  LOCKCHEST_KEY_READY = 1, /* waits for Spyro to face it up close */
  LOCKCHEST_OPENING = 2,   /* the key flies into the lock, then it bursts
                            * (a superflame hit skips straight to the burst) */
};

/* How close (2D / height) Spyro, carrying the key, must stand to the locked
 * chest for LOCKCHEST_KEY_READY to open it. */
#define LOCKCHEST_KEY_RANGE 0x600
#define LOCKCHEST_KEY_DZ 0x200
/* ...and face it within FACING_ANGLE (8-bit), with it facing him as well.
 * LOCKCHEST_OPENING runs on LockedChestState.shake as a clock: the key flies
 * round to the lock until INSERT_TICK, slides in until BURST_TICK, then the
 * chest bursts. A superflame hit or Cross sets the clock to SKIP_TICK. */
#define LOCKCHEST_FACING_ANGLE 0x30
#define LOCKCHEST_INSERT_TICK 0xC0
#define LOCKCHEST_BURST_TICK 0xD0
#define LOCKCHEST_SKIP_TICK 0xD2
/* The lock sits LOCK_FWD ahead of and LOCK_UP above the chest's origin (chest
 * space); the key flies there from Spyro along a sine arc, turning to face
 * the chest over KEY_SPIN_TICK..KEY_TURN_TICK..INSERT_TICK, then slides
 * LOCK_FWD back in, 8 a tick. */
#define LOCKCHEST_LOCK_FWD 0x200
#define LOCKCHEST_LOCK_UP 0x140
#define LOCKCHEST_KEY_SPIN_TICK 0x40
#define LOCKCHEST_KEY_TURN_TICK 0x80

/* ->subMode of a cracking boulder (ACTOR_TYPE_CRACKING_BOULDER,
 * CrackingBoulderState). */
enum CrackingBoulderSubMode {
  BOULDER_INIT = 0,     /* take the drop-kind tint */
  BOULDER_INTACT = 1,   /* a charge bursts it at once, a flame or blast cracks it */
  BOULDER_CRACKING = 2, /* shakes and smokes for BOULDER_CRACK_FRAMES */
  BOULDER_BURST = 3,    /* debris rings, the drop, and a push on Spyro */
};
/* CrackingBoulderState.timer while BOULDER_CRACKING: it bursts at
 * CRACK_FRAMES; a blast (intact or cracking) jumps the clock to BLAST_TICK. */
#define BOULDER_CRACK_FRAMES 0x78
#define BOULDER_BLAST_TICK 0x5A
/* Its last SHAKE_TICKS of cracking the boulder rattles (g_aShakeRotOffsets
 * indexed by the frames left). BOULDER_BURST lasts BURST_TICKS, its blast
 * sphere growing 0x100 a tick; Spyro caught in it is pushed off at
 * (PUSH_RANGE - distance) / 48 with PUSH_VZ up. */
#define BOULDER_BURST_TICKS 8
#define BOULDER_PUSH_RANGE 0x1000
#define BOULDER_PUSH_VZ 0x46

/* ->subMode of a spin chest (ACTOR_TYPE_SPIN_CHEST, SpinChestState). */
enum SpinChestSubMode {
  SPINCHEST_INIT = 0,     /* spawn the lid */
  SPINCHEST_SPINNING = 1, /* flames spin it up; fast enough (or a superflame) bursts it */
};
/* SpinChestState.speed: a flame adds FLAME_SPIN at most every FLAME_COOLDOWN
 * frames; it spins down by 1 a frame to MIN_SPEED (silent there) and bursts
 * past BURST_SPEED. */
#define SPINCHEST_FLAME_COOLDOWN 0x10
#define SPINCHEST_FLAME_SPIN 0x64
#define SPINCHEST_MIN_SPEED 0x18
#define SPINCHEST_BURST_SPEED 0xE0

/* ->subMode of the spin chest's lid (ACTOR_TYPE_SPIN_CHEST_LID,
 * SpinChestLidState). */
enum SpinChestLidSubMode {
  LID_ON_CHEST = 0, /* spun by its chest */
  LID_LAUNCH = 1,   /* the chest burst: aim at an enemy (or away from Spyro) */
  LID_FLYING = 2,   /* in flight; breaks on what it hits */
};
/* A launching lid aims at the live enemy (negative collision) best lined up
 * with Spyro's line through the lid, within MAX_ANGLE (12-bit) of it, at
 * MIN_RANGE..MAX_RANGE (2D) from the lid and within DZ of its height;
 * PREFILTER bounds |dx| + |dy| before the length is taken. */
#define LID_TARGET_MAX_ANGLE 0x180
#define LID_TARGET_MIN_RANGE 0x1000
#define LID_TARGET_MAX_RANGE 0x3000
#define LID_TARGET_DZ 0x1000
#define LID_TARGET_PREFILTER 0x6000
/* At a target the lid flies 1/128 of the gap a frame, aimed AIM_RAISE above
 * it; with none it flies away from Spyro at MISS_SPEED. Either way it
 * launches LAUNCH_VZ up, spinning SPIN_STEP a frame, and falls 2.5 a frame
 * faster down to -MAX_FALL. After ARM_TICKS it bursts on a wall (a
 * HIT_RADIUS sphere HIT_RAISE up) or an actor within CONTACT_RADIUS,
 * blasting actors within BLAST_RADIUS, into SHARD_PAIRS wood-shard pairs
 * and SPARKLES sparkle shards. */
#define LID_AIM_RAISE 0x200
#define LID_MISS_SPEED 0x50
#define LID_LAUNCH_VZ 0xA0
#define LID_SPIN_STEP 8
#define LID_MAX_FALL 0xA0
#define LID_ARM_TICKS 5
#define LID_HIT_RAISE 0xB4
#define LID_HIT_RADIUS 0xB4
#define LID_CONTACT_RADIUS 0x100
#define LID_BLAST_RADIUS 0x400
#define LID_SHARD_PAIRS 2
#define LID_SPARKLES 6

/* ->mode of a portal guide (ACTOR_TYPE_PORTAL_GUIDE, PortalState); set to
 * PORTALGUIDE_ENTER by DispatchSpyroTriggerEvent when Spyro enters a
 * portal. */
enum PortalGuideMode {
  PORTALGUIDE_IDLE = 0,  /* inactive; set back once the camera reaches the portal and the load starts */
  PORTALGUIDE_ENTER = 1, /* Spyro jumps toward the portal's midpoint */
  PORTALGUIDE_GLIDE = 2, /* Spyro glides along the portal camera path */
};
/* ENTER flies Spyro at up to JUMP_SPEED a frame toward the portal's
 * midpoint (plus its offset); the portal camera path steps at most
 * PATH_MAX_STEP a frame. The load starts once a CAM_PROBE_RADIUS sphere at
 * the camera touches the portal's trigger region. */
#define PORTALGUIDE_JUMP_SPEED 0x80
#define PORTALGUIDE_PATH_MAX_STEP 0x60
#define PORTALGUIDE_CAM_PROBE_RADIUS 0x300

/* ->subMode of a portal guide: the side of the portal plane Spyro entered
 * from (g_nPortalCamPathReverseFlag). */
enum PortalSide {
  PORTAL_SIDE_FORWARD = 0,  /* walk the camera path forward */
  PORTAL_SIDE_REVERSED = 1, /* walk it backward, offset negated */
};

/* ->mode of a portal name sign (ACTOR_TYPE_PORTAL_NAME). */
enum PortalNameMode {
  PORTALNAME_FAR = 0,  /* camera out of range (or side-on) */
  PORTALNAME_NEAR = 1, /* camera close and facing one side: letters shown */
};

/* The owner mode that removes a floating name's letters (NameLetterState.sign
 * is a portal name or a level exit; PORTALNAME_FAR and LEVELEXIT_FAR are
 * both 0). */
#define ORBIT_OWNER_FAR 0

/* ->subMode of a portal name sign: the side the camera reads it from;
 * SpawnNameLetterActors mirrors the letter arc for NAMESIDE_BACK. */
enum NameSignSide {
  NAMESIDE_FRONT = 0,
  NAMESIDE_BACK = 1,
};

/* ThiefState.mode: an egg thief's behaviour, from the level data; THIEF_IDLE
 * runs it until Spyro triggers the matching routine. Path node numbers are
 * the thief's own route (ThiefState.path). */
enum ThiefKind {
  THIEFKIND_HOP = 0,    /* Spyro near: THIEF_RUN, leap to a perch, leap home */
  THIEFKIND_GUARD = 1,  /* keeps between Spyro and path node 0 */
  THIEFKIND_LEDGE = 2,  /* Spyro near: the THIEF_LEDGE_* routine */
  THIEFKIND_FLEE = 3,   /* Spyro in range (and sight): THIEF_FLEE */
  THIEFKIND_CLIMB = 4,  /* Spyro in its area box: the THIEF_CLIMB_* routine */
  THIEFKIND_CIRCLE = 5, /* Spyro near: THIEF_CIRCLE */
  THIEFKIND_STAND = 6,  /* THIEF_STAND */
};

/* ->mode of an egg thief (ACTOR_TYPE_EGG_THIEF, ThiefState). */
enum ThiefMode {
  THIEF_IDLE = 0,           /* runs its ThiefKind */
  THIEF_RUN = 1,            /* runs the path to node 4 */
  THIEF_LEAP = 2,           /* leaps toward the current node */
  THIEF_PERCH = 3,          /* lands, waits at node 4 or 5 until Spyro nears 6/7 */
  THIEF_LEAP_HOME = 4,      /* leaps back to node 0 */
  THIEF_CAUGHT = 10,        /* hit: releases the egg (EGG_RELEASED) and bursts */
  THIEF_LEDGE_RUN = 0x14,   /* runs the path to node 3 */
  THIEF_LEDGE_WAIT = 0x15,  /* waits for Spyro */
  THIEF_LEDGE_RUN_ON = 0x16, /* runs on to node 1 */
  THIEF_LEDGE_TAUNT = 0x17, /* waits until Spyro is close and level */
  THIEF_LEDGE_JUMP = 0x18,  /* jumps across to node 2, then waits again */
  THIEF_LEDGE_RESET = 0x19, /* Spyro is at node 6: warps to node 0 */
  THIEF_FLEE = 0x1E,        /* runs its path ring away from Spyro */
  THIEF_CLIMB_RUN = 0x28,   /* runs the path to node 11 */
  THIEF_CLIMB_HOP = 0x29,   /* hops node to node up to node 13 */
  THIEF_CLIMB_TOP = 0x2A,   /* waits on top until Spyro is well below */
  THIEF_CLIMB_HOP_DOWN = 0x2B, /* hops down to node 9 */
  THIEF_CLIMB_RUN_BACK = 0x2C, /* runs the path back to its start */
  THIEF_CIRCLE = 0x32,      /* circles the current node away from Spyro */
  THIEF_STAND = 0x3C,       /* stands facing Spyro */
};

/* Egg-thief ranges to Spyro (2D): a HOP or LEDGE thief sets off once he is
 * within NOTICE; a LEDGE thief warps home (THIEF_LEDGE_RESET) once he is
 * within LEDGE_RESET of its path node 6. */
#define THIEF_NOTICE_RANGE 0x1C00
#define THIEF_LEDGE_RESET_RANGE 0x1800
/* Spyro within NEAR (2D): a CIRCLE thief starts circling, a LEDGE thief runs
 * on from its wait node or jumps across from its taunt node. */
#define THIEF_NEAR_RANGE 0x2000
/* A GUARD thief: inside HOME of node 0 it backs away from Spyro; otherwise,
 * with Spyro inside STEER, it orbits node 0 at ORBIT_RADIUS to keep node 0
 * between them, and a broken-off chase returns to orbiting past ORBIT_RADIUS. */
#define THIEF_GUARD_HOME_RANGE 0x1000
#define THIEF_GUARD_STEER_RANGE 0x3000
#define THIEF_GUARD_ORBIT_RADIUS 0x1800
/* A perched HOP thief leaps home once Spyro is this close to node 6 or 7. */
#define THIEF_PERCH_LEAVE_RANGE 0x800
/* A path node counts as reached inside this 2D distance (leaps, hops). */
#define THIEF_NODE_REACHED_RANGE 0x100
/* A CLIMB thief below this world Z has fallen off: it is put back on its node. */
#define THIEF_FELL_BELOW_Z 0x800
/* A FLEE thief only reconsiders its ring direction while this far from both
 * the node it is heading for and the one after. */
#define THIEF_FLEE_TURN_CLEARANCE 0x190
/* A FLEE thief notices Spyro within its level-data range (st->range << 10, 2D)
 * and this height of its feet. */
#define THIEF_FLEE_NOTICE_DZ 0x708
/* A CLIMB thief on top hops back down once Spyro is CLIMB_DOWN_DZ below node
 * 10 and within CLIMB_DOWN_RANGE (2D) of the thief. */
#define THIEF_CLIMB_DOWN_DZ 0x5DC
#define THIEF_CLIMB_DOWN_RANGE 0x1800
/* A CIRCLE thief starts circling once Spyro, within THIEF_NEAR_RANGE, is more
 * than this (8-bit) off the far side of its node from it. */
#define THIEF_CIRCLE_ENTER_ANGLE 0x30
/* Hit (THIEF_CAUGHT), a thief slides from KNOCK_SPEED (CHARGE_KNOCK_SPEED
 * when charged), losing KNOCK_DRAG a frame, and lets go of its egg on
 * EGG_RELEASE_FRAME of THIEF_ANIM_CAUGHT. */
#define THIEF_KNOCK_SPEED 0x96
#define THIEF_CHARGE_KNOCK_SPEED 0xE1
#define THIEF_KNOCK_DRAG 0x10
#define THIEF_EGG_RELEASE_FRAME 5
/* THIEF_ANIM_TAUNT may voice a taunt on these frames. */
#define THIEF_TAUNT_EARLY_FRAME 2
#define THIEF_TAUNT_LATE_FRAME 0xC
/* GUARD and RUN speeds: BASE_SPEED less 1/32 of Spyro's 2D distance, clamped
 * to GUARD_MIN..MAX_SPEED / RUN_MIN..MAX_SPEED (the closer he is, the
 * faster). */
#define THIEF_BASE_SPEED 0x154
#define THIEF_GUARD_MIN_SPEED 0x8C
#define THIEF_GUARD_MAX_SPEED 0xF0
#define THIEF_RUN_MIN_SPEED 0x46
#define THIEF_RUN_MAX_SPEED 0xDC
/* Leaps and hops: vz loses GRAVITY a frame down to -MAX_FALL. LEAP_VZ is the
 * RUN thief's leap to its perch, HOP_VZ every other hop, CLIMB_DOWN_VZ the
 * climber's hop down; airborne thieves move HOP_SPEED (LEAP_HOME_SPEED on the
 * way home). */
#define THIEF_GRAVITY 0x14
#define THIEF_MAX_FALL 0xC8
#define THIEF_LEAP_VZ 0x12C
#define THIEF_HOP_VZ 0x96
#define THIEF_CLIMB_DOWN_VZ 0x1A4
#define THIEF_HOP_SPEED 0x96
#define THIEF_LEAP_HOME_SPEED 0x82
/* LEDGE: runs to its wait node at LEDGE_RUN_SPEED, on at LEDGE_RUN_ON_SPEED,
 * and jumps across at LEDGE_JUMP_SPEED. */
#define THIEF_LEDGE_RUN_SPEED 0xF0
#define THIEF_LEDGE_RUN_ON_SPEED 0xDA
#define THIEF_LEDGE_JUMP_SPEED 0xFA
/* FLEE: runs toward its node's speed (FLEE_SPEED when the node has none),
 * changing FLEE_ACCEL a frame. */
#define THIEF_FLEE_SPEED 0xCD
#define THIEF_FLEE_ACCEL 0x1E
/* CLIMB: runs CLIMB_SPEED up (CLIMB_BACK_SPEED back), hopping from node 11
 * up to CLIMB_TOP_NODE. */
#define THIEF_CLIMB_SPEED 0xB4
#define THIEF_CLIMB_BACK_SPEED 0xC8
#define THIEF_CLIMB_TOP_NODE 0xD
#define THIEF_BURST_SPARK_LIFE 0x18 /* BurstActorWithSound spark life */

/* An egg thief's animations (nextAnim), from what each mode plays. */
enum ThiefAnim {
  THIEF_ANIM_IDLE = 0,   /* back on its start node */
  THIEF_ANIM_RUN = 1,    /* every run/hop mode */
  THIEF_ANIM_CAUGHT = 3, /* hit: the egg drops from frame 5, then it bursts */
  THIEF_ANIM_TAUNT = 4,  /* stands facing Spyro; voices 6/5 on frames 2/0xC */
};

/* An egg thief's voices (PlayActorMeshVoice): 0..4 are its running chatter,
 * picked at random without repeats; the taunt plays the other two. */
enum ThiefVoice {
  THIEF_VOICE_TAUNT_LATE = 5,  /* on THIEF_ANIM_TAUNT frame 0xC */
  THIEF_VOICE_TAUNT_EARLY = 6, /* on THIEF_ANIM_TAUNT frame 2 */
};

/* ->subMode of an egg thief that keeps away around its home node. */
enum ThiefSubMode {
  THIEFSUB_ORBIT = 0, /* circles path node 0 keeping it between itself and Spyro */
  THIEFSUB_BREAK = 1, /* stuck or cornered: runs straight past, then ORBIT */
};

/* ->mode of a roaming ram (ACTOR_TYPE_ROAMING_RAM, RoamingRamState). */
enum RoamingRamMode {
  RAM_GRAZE = 0,     /* calls; Spyro near: picks the node facing away from him */
  RAM_WANDER = 1,    /* walks to that node */
  RAM_QUIET_KNOCKED = 2, /* the quiet kind (ACTOR_TYPE_ROAMING_RAM_QUIET)'s KNOCKED */
  RAM_KNOCKED = 3,   /* hit: tumbles away and bursts */
  RAM_TRAPPED = 4,   /* inside its trigger box while the trigger is set */
  RAM_RUN_PATH = 10, /* path rammer (RAMKIND_PATH): runs its path */
  RAM_PATH_END = 11, /* at the end: wakes its partner, then idles calling */
};

/* RoamingRamState.mode: the ram's kind (spawn record). */
enum RoamingRamKind {
  RAMKIND_ROAMER = 0,
  RAMKIND_PATH = 1, /* RAM_GRAZE with Spyro close: RAM_RUN_PATH */
};
/* A grazing ram: a PATH ram sets off within PATH_RANGE (2D) of Spyro; any ram
 * within FLEE_RANGE picks the path node facing away from him. */
#define RAM_PATH_RANGE 0x1800
#define RAM_FLEE_RANGE 0x1388
/* The quiet ram's PATH kind sets off within QUIET_PATH_RANGE instead. A
 * fleeing ram only moves on when the best node is FLEE_MIN_TURN off Spyro's
 * bearing; it walks at WANDER_SPEED (moving with MOVE_RADIUS) and reaches it
 * within NODE_RANGE. A path ram runs at RUN_SPEED (nodes within
 * PIVOT_THRESH). It calls every CALL_MIN.. ticks (END_CALL_MIN.. once at the
 * path end). Hit, it slides from KNOCK_SPEED and KNOCK_VZ up (a charge adds
 * CHARGE_KNOCK_BONUS / CHARGE_KNOCK_VZ_BONUS; a blast knocks it off its
 * target at BLAST_KNOCK_SPEED / BLAST_KNOCK_VZ), losing KNOCK_DRAG and
 * KNOCK_GRAVITY a frame, then bursts with BURST_SPARK_LIFE sparks. Trapped,
 * its struggle loops back before frame TRAPPED_LOOP_FRAME. */
#define RAM_QUIET_PATH_RANGE 0x1000
#define RAM_FLEE_MIN_TURN 0x40
#define RAM_WANDER_SPEED 0x50
#define RAM_MOVE_RADIUS 0x12C
#define RAM_NODE_RANGE 0x100
#define RAM_RUN_SPEED 0x78
#define RAM_PIVOT_THRESH 0x100
#define RAM_CALL_MIN 0x50
#define RAM_END_CALL_MIN 0x78
#define RAM_KNOCK_SPEED 0xC8
#define RAM_KNOCK_VZ 0x32
#define RAM_CHARGE_KNOCK_BONUS 0x46
#define RAM_CHARGE_KNOCK_VZ_BONUS 0x32
#define RAM_BLAST_KNOCK_SPEED 0x8C
#define RAM_BLAST_KNOCK_VZ 0x23
#define RAM_KNOCK_DRAG 0xC
#define RAM_KNOCK_GRAVITY 0x10
#define RAM_BURST_SPARK_LIFE 0x10
#define RAM_TRAPPED_LOOP_FRAME 0xF

/* ->mode of a path walker (ACTOR_TYPE_PATH_WALKER, PathState). */
enum PathWalkerMode {
  PATHWALK_WAIT = 0,  /* until Spyro is near and level with it */
  PATHWALK_PACE = 1,  /* paces its path, heading for the node away from Spyro */
  PATHWALK_DYING = 2, /* hit: plays out its death animation */
};
/* "Near and level": Spyro within NOTICE_RANGE (2D) and NOTICE_DZ of its feet.
 * A pacing walker drops back to WAIT at a node once he is not. */
#define PATHWALK_NOTICE_RANGE 0x1800
#define PATHWALK_NOTICE_DZ 0x800
/* PACE walks at SPEED (moving with MOVE_RADIUS) and reaches a node within
 * NODE_RANGE, then picks the neighbour leading away from Spyro. */
#define PATHWALK_SPEED 0x60
#define PATHWALK_MOVE_RADIUS 0x200
#define PATHWALK_NODE_RANGE 0x400
/* A floor marker (ACTOR_TYPE_FLOOR_MARKER) probes from FLOOR_MARKER_RAISE
 * above itself and lands on a floor within FLOOR_MARKER_SNAP_RANGE, then
 * sits at the centroid of the triangle it landed on. */
#define FLOOR_MARKER_RAISE 0x200
#define FLOOR_MARKER_SNAP_RANGE 0x400

/* ->mode of a wandering critter (ACTOR_TYPE_WANDERING_CRITTER / _B / _C,
 * CritterKnockState / WanderKnockState). */
enum WanderingCritterMode {
  CRITTER_WANDER = 0,  /* UpdateActorWander; the _C kind rests on a timer */
  CRITTER_KNOCKED = 1, /* hit: slides away and despawns */
  CRITTER_REST = 2,    /* the _C kind's pause, then WANDER */
};
/* A hit wandering critter slides off from KNOCK_SPEED (FLAME_KNOCK_SPEED
 * when flamed), losing KNOCK_DRAG a frame while at least KNOCK_MIN_SPEED,
 * moving with MOVE_RADIUS. */
#define CRITTER_KNOCK_SPEED 0xFA
#define CRITTER_FLAME_KNOCK_SPEED 0x8C
#define CRITTER_KNOCK_DRAG 0xF
#define CRITTER_KNOCK_MIN_SPEED 0x10
#define CRITTER_MOVE_RADIUS 0x2BC
/* The _B and _C kinds slide from STRONG_KNOCK_SPEED (STRONG_FLAME_KNOCK_SPEED
 * when flamed); _C moves with C_MOVE_RADIUS, rests C_REST_MIN..C_REST_MAX ticks
 * between wanders and bursts with C_BURST_SPARK_LIFE sparks. */
#define CRITTER_STRONG_KNOCK_SPEED 0x190
#define CRITTER_STRONG_FLAME_KNOCK_SPEED 0xC8
#define CRITTER_C_MOVE_RADIUS 0x1F4
#define CRITTER_C_REST_MIN 0x5A
#define CRITTER_C_REST_MAX 0x12C
#define CRITTER_C_BURST_SPARK_LIFE 0x10

/* ->mode of Gnasty Gnorc (ACTOR_TYPE_GNASTY_GNORC, GnastyState). GnastyState
 * .phase counts the fight: flamed near the end of his phase-2 path he swaps
 * to the escape path (GNASTY_FLAMED), flamed near the end of that one he is
 * beaten (GNASTY_DEFEATED). */
enum GnastyMode {
  GNASTY_WAIT = 0,         /* faces Spyro until he is seen near home */
  GNASTY_THROW = 2,        /* throws trail bombs (ACTOR_TYPE_TRAIL_BOMB) at Spyro */
  GNASTY_SWIPE = 3,        /* close swipe, then runs */
  GNASTY_LOOKOUT = 4,      /* waits on his path for Spyro to come near */
  GNASTY_RUN = 5,          /* runs his path away from Spyro */
  GNASTY_ESCAPE = 6,       /* runs the escape path, jumping at flagged nodes */
  GNASTY_FLAMED = 7,       /* flamed in phase 2: recoils, then GNASTY_ESCAPE */
  GNASTY_DEFEATED = 8,     /* knocked out: drops the keys, freezes Spyro */
  GNASTY_FADE_OUT = 9,     /* fades the screen into the ending cutscene */
  GNASTY_TO_ESCAPE = 10,   /* walks to the escape path's start (phase 2) */
  GNASTY_AT_DOOR = 11,     /* at escape node 6: waits for Spyro, opens the way */
  GNASTY_STAND = 12,       /* at the path end: throws when Spyro is far */
  GNASTY_STAND_SWIPE = 13, /* swipe from GNASTY_STAND */
  GNASTY_PAUSE = 20,       /* waits out stateTimer, then runs */
  GNASTY_PATH_END = 21,    /* at the path end until Spyro comes near from below */
};

/* GnastyState.phase: the fight's progress (0 and 1 before he reaches his
 * main path). */
enum GnastyPhase {
  GNASTY_PHASE_MAIN = 2,   /* reached path point 1: runs and throws along his main path */
  GNASTY_PHASE_ESCAPE = 3, /* flamed near its end: the escape path */
  GNASTY_PHASE_BEATEN = 4, /* flamed near the escape path's end */
};

/* Gnasty Gnorc runs to animation 1 (THROW plays its own index, 2). */
enum GnastyAnim {
  GNASTY_ANIM_RUN = 1,
};

/* Gnasty's fight tuning. He is flamed into the next phase only within
 * FLAME_END_RANGE of his current path's end. Before his main path the follow
 * camera holds on him (CAM_HOLD_FRAMES after a charge) within CAM_RANGE, in
 * sight or within CAM_NEAR_RANGE. Phase 2 starts once Spyro, facing him
 * within ESCAPE_FACING, comes within ESCAPE_RANGE (or he has thrown
 * THROW_VOLLEY bombs). */
#define GNASTY_FLAME_END_RANGE 0xBB8
#define GNASTY_CAM_HOLD_FRAMES 0x3C
#define GNASTY_CAM_RANGE 0x4000
#define GNASTY_CAM_NEAR_RANGE 0x3000
/* The scripted boss shots (Gnasty's path change and defeat, Doctor
 * Shemp's retreat) look down BOSS_SHOT_PITCH with BOSS_SHOT_PITCH_BIAS from
 * a boss-specific distance: GNASTY_SHOT_DIST (DEFEAT_SHOT_DIST once
 * beaten). */
#define BOSS_SHOT_PITCH 0x19
#define BOSS_SHOT_PITCH_BIAS (-0x32)
#define GNASTY_SHOT_DIST 0x1770
#define GNASTY_DEFEAT_SHOT_DIST 0xFA0
#define GNASTY_ESCAPE_FACING 0x28
#define GNASTY_ESCAPE_RANGE 0x1B58
/* Trail bombs (GNASTY_THROW): THROW_VOLLEY of them, released on frame
 * THROW_FRAME every THROW_GAP frames, fly BOMB_SPEED (climbing at most
 * BOMB_MAX_DROP a step down) and live BOMB_LIFE frames. */
#define GNASTY_THROW_VOLLEY 4
#define GNASTY_THROW_FRAME 9
#define GNASTY_THROW_GAP 0x50
#define GNASTY_BOMB_SPEED 0x190
#define GNASTY_BOMB_MAX_DROP 0x1F4
#define GNASTY_BOMB_LIFE 0xA0
/* Gravity (GNASTY_FALL, ESCAPE, STAND): vSpeed loses GRAVITY a frame down to
 * -MAX_FALL, landing on the floor found FLOOR_PROBE_RAISE above him. Escape
 * jumps launch at JUMP_LOW / MID / HIGH (PATH_JUMP_*). */
#define GNASTY_GRAVITY 0x19
#define GNASTY_MAX_FALL 0x190
#define GNASTY_FLOOR_PROBE_RAISE 0xBB8
#define GNASTY_JUMP_LOW 0xDC
#define GNASTY_JUMP_MID 0x10E
#define GNASTY_JUMP_HIGH 0x140
/* GNASTY_RUN speed: RUN_SPEED (or the node's), from node 0xE LATE_SPEED
 * (0x1D on: LAST_SPEED), CHASED_SPEED while Spyro is within CHASE_RANGE
 * (CHASE_RANGE_FAST once already fast) and CHASE_DZ. Escape: ESCAPE_SPEED;
 * to the escape start: TO_ESCAPE_SPEED. Beaten: slides from KNOCK_SPEED. */
#define GNASTY_RUN_SPEED 0x13B
#define GNASTY_LATE_SPEED 0xEB
#define GNASTY_LAST_SPEED 0xD7
#define GNASTY_CHASED_SPEED 0x122
#define GNASTY_CHASE_RANGE 0x1964
#define GNASTY_CHASE_RANGE_FAST 0x1F40
#define GNASTY_CHASE_DZ 0x4B0
#define GNASTY_ESCAPE_SPEED 0x104
#define GNASTY_TO_ESCAPE_SPEED 0xF0
#define GNASTY_KNOCK_SPEED 0xD2
/* Within SWIPE_RANGE he swipes instead of throwing. */
#define GNASTY_SWIPE_RANGE 0xE10
#define GNASTY_BURST_SPARK_LIFE 0x40 /* BurstActorWithSound spark life */
#define GNASTY_KNOCK_DRAG 0xC /* DriveActorTrackedMove friction */

/* ->mode of Blowhard, the level 16 boss (ACTOR_TYPE_BLOWHARD,
 * BlowhardState). Each of its three phases rises (floorZ + 0x578), hovers
 * or patrols its path, and sinks to drop an ACTOR_TYPE_RIM_RIDER on Spyro;
 * a flame hit in any of them goes to BLOWHARD_HIT, then the next phase. */
enum BlowhardMode {
  BLOWHARD_START_PHASE = 0,     /* enter the current phase's first state */
  BLOWHARD_HIT = 2,             /* hit: once its chunk animation halts, next phase */
  BLOWHARD_DYING = 7,           /* the third hit: drops its loot and is removed */
  BLOWHARD_P1_RISE = 0x46,
  BLOWHARD_P1_HOVER = 0x47,     /* faces Spyro; close: sinks to drop a rider */
  BLOWHARD_P1_DROP_RIDER = 0x48,
  BLOWHARD_P2_TRAVEL = 0x50,    /* tweens to the phase-2 path */
  BLOWHARD_P2_PATROL = 0x51,
  BLOWHARD_P2_DROP_RIDER = 0x52,
  BLOWHARD_P2_WAIT = 0x53,
  BLOWHARD_P3_TRAVEL = 0x5A,    /* tweens to the phase-3 path */
  BLOWHARD_P3_PATROL = 0x5B,
  BLOWHARD_P3_DROP_RIDER = 0x5C,
  BLOWHARD_P3_WAIT = 0x5D,
  BLOWHARD_NEXT_PHASE = 0x63,   /* phase + 1, next path, back to START_PHASE */
};
/* BlowhardState.phase: one hit ends each (BLOWHARD_HIT); the third is fatal. */
enum BlowhardPhase {
  BLOWHARD_PHASE_1 = 0, /* paths[0]: BLOWHARD_P1_* */
  BLOWHARD_PHASE_2 = 1, /* paths[1] (paths[3] restored from a checkpoint): BLOWHARD_P2_* */
  BLOWHARD_PHASE_3 = 2, /* paths[2] (paths[4] restored): BLOWHARD_P3_* */
};

/* Blowhard's animations: it hovers between rider drops. */
enum BlowhardAnim {
  BLOWHARD_ANIM_HOVER = 0,
  BLOWHARD_ANIM_DROP_RIDER = 5, /* the rim rider leaves it from frame 6 */
  BLOWHARD_ANIM_DROP_END = 6,
};

/* Blowhard's tuning. The follow camera holds on him with Spyro in sight
 * within CAM_DZ and NEAR_RANGE (except while he travels between phases);
 * NEAR_RANGE also wakes his patrols, and he drops a rider once Spyro is past
 * DROP_MIN_RANGE (in phases 2/3, facing him within DROP_FACING). Hit checks
 * and rider drops need a clear line from EYE_HEIGHT over him to Spyro. He
 * bobs BOB_HEIGHT up and down (st->floorZ, eased 1/8 a frame until within
 * BOB_SETTLE), patrols at PATROL_SPEED, tweens TRAVEL_STEP a frame between
 * phases, and waits WAIT_FRAMES after each drop. */
#define BLOWHARD_CAM_DZ 0x578
#define BLOWHARD_NEAR_RANGE 0x2800
#define BLOWHARD_DROP_MIN_RANGE 0x1000
#define BLOWHARD_DROP_FACING 0x30
#define BLOWHARD_EYE_HEIGHT 0x400
#define BLOWHARD_BOB_HEIGHT 0x578
#define BLOWHARD_BOB_SETTLE 0x20
#define BLOWHARD_PATROL_SPEED 0xB4
#define BLOWHARD_TRAVEL_STEP 0x20
#define BLOWHARD_WAIT_FRAMES 0x6E

/* ->mode of Doctor Shemp, the level 10 boss (ACTOR_TYPE_DOCTOR_SHEMP,
 * ShempState). SHEMP_IDLE starts the routine for ShempState.phase 0..2;
 * a flame or charge from behind (or into the phase-2 slam) stuns him and he
 * retreats along his path to the next phase's node; stunned in phase 3 he
 * is beaten. */
enum ShempMode {
  SHEMP_IDLE = 0,
  SHEMP_P2_TAUNT = 1,    /* above Spyro: taunts until he comes close */
  SHEMP_P2_SLAM = 2,     /* staff slam; pins Spyro when it lands near him */
  SHEMP_P1_START = 11,
  SHEMP_P1_CIRCLE = 12,  /* circles his node to face Spyro */
  SHEMP_P1_WINDUP = 13,
  SHEMP_P1_CHARGE = 14,  /* runs at Spyro */
  SHEMP_P1_STRIKE = 15,  /* staff strike; pins Spyro on contact */
  SHEMP_P1_RETURN = 16,  /* back to his node */
  SHEMP_P3_TAUNT = 21,
  SHEMP_P3_TURN = 22,    /* turns to line up the sweep */
  SHEMP_P3_SWEEP = 23,   /* spinning staff sweep; knocks Spyro back */
  SHEMP_STUNNED = 40,    /* next phase, release that phase's chunk animation */
  SHEMP_RETREAT = 41,    /* walks his path to the next node, camera on him */
  SHEMP_ARRIVE = 42,     /* turns to Spyro, back to SHEMP_IDLE */
  SHEMP_DYING = 45,      /* stunned in phase 3: bursts and drops his loot */
};

/* ShempState.phase: which of Doctor Shemp's three fights is running; also
 * indexes his path nodes, camera anchors and the world chunk anim a stun
 * resumes. */
enum ShempPhase {
  SHEMP_PHASE_1 = 0, /* close: SHEMP_P1_START */
  SHEMP_PHASE_2 = 1, /* Spyro near and above: SHEMP_P2_TAUNT */
  SHEMP_PHASE_3 = 2, /* same, SHEMP_P3_TAUNT; stunned here he is beaten */
};

/* Doctor Shemp's animations, from the modes that play them. */
enum ShempAnim {
  SHEMP_ANIM_IDLE = 0,
  SHEMP_ANIM_DYING = 3,
  SHEMP_ANIM_ROAR = 7,     /* opens the phase-2 taunt */
  SHEMP_ANIM_WALK = 9,     /* circle, charge, return */
  SHEMP_ANIM_WINDUP = 0xA, /* phase-1 windup, opens the phase-3 taunt */
  SHEMP_ANIM_TURN = 0xB,
  SHEMP_ANIM_STUNNED = 0xE,
  SHEMP_ANIM_STRIKE = 0xF,
  SHEMP_ANIM_STAND = 0x12, /* between gestures; the sweep blends back to it */
  SHEMP_ANIM_GESTURE = 0x13, /* starts each taunt round of 1..2 stands */
};

/* Doctor Shemp's tuning. Within CAM_RANGE the follow camera holds on him.
 * Flame or superflame hurts him only from behind (Spyro outside BACK_ARC of
 * his facing) or during frames SLAM_OPEN..SLAM_CLOSE of his slam. Phase 1
 * starts within P1_WAKE_RANGE, phases 2/3 within WAKE_RANGE with Spyro
 * above. His blows (slam, strike, sweep) land on Spyro within their range,
 * at least HIT_ABOVE over his base and within HIT_DZ of it. */
#define SHEMP_CAM_RANGE 0x2000
#define SHEMP_RETREAT_SHOT_DIST 0x1770 /* SHEMP_RETREAT's shot (BOSS_SHOT_PITCH) */
#define SHEMP_BACK_ARC 0x55
#define SHEMP_SLAM_OPEN_FRAME 0x16
#define SHEMP_SLAM_CLOSE_FRAME 0x30
#define SHEMP_P1_WAKE_RANGE 0x2328
#define SHEMP_WAKE_RANGE 0x1964
#define SHEMP_HIT_ABOVE 300
#define SHEMP_HIT_DZ 0x5DC
/* Phase 2: slams (animation 16) within P2_SLAM_RANGE, hitting on frames
 * SLAM_HIT..SLAM_HIT_END within SLAM_HIT_RANGE; then rests SLAM_REST. */
#define SHEMP_P2_SLAM_RANGE 0x16A8
#define SHEMP_SLAM_HIT_FRAME 0x34
#define SHEMP_SLAM_HIT_END 0x38
#define SHEMP_SLAM_HIT_RANGE 0x189C
#define SHEMP_SLAM_REST 0xA0
/* Phase 1: after P1_START_DELAY he circles his node (CIRCLE_RADIUS, at
 * CIRCLE_SPEED) for CIRCLE_FRAMES, winds up and charges at CHARGE_SPEED for
 * up to CHARGE_FRAMES (or until within STRIKE_RANGE, lined up), strikes on
 * frames STRIKE_HIT..STRIKE_HIT_END within STRIKE_HIT_RANGE, and walks back
 * (RETURN_SPEED, within RETURN_ARRIVE). */
#define SHEMP_P1_START_DELAY 0x5A
#define SHEMP_CIRCLE_RADIUS 0xC80
#define SHEMP_CIRCLE_SPEED 0xB4
#define SHEMP_CIRCLE_FRAMES 0x8C
#define SHEMP_CHARGE_SPEED 0xAA
#define SHEMP_CHARGE_FRAMES 0x32
#define SHEMP_STRIKE_RANGE 0x9C4
#define SHEMP_STRIKE_HIT_FRAME 4
#define SHEMP_STRIKE_HIT_END 0xA
#define SHEMP_STRIKE_HIT_RANGE 0x1194
#define SHEMP_RETURN_SPEED 0x5A
#define SHEMP_RETURN_ARRIVE 0x100
/* Phase 3: within P3_SWEEP_RANGE he sweeps his staff round SWEEP_ARC (8-bit,
 * at SWEEP_RATE / 16 a frame), knocking Spyro back within SWEEP_HIT_RANGE
 * unless he is SWEEP_HIT_CLEARANCE or more off the ground; then rests
 * SWEEP_REST. */
#define SHEMP_P3_SWEEP_RANGE 0x1388
#define SHEMP_SWEEP_RATE 0x38
#define SHEMP_SWEEP_ARC 0x105
#define SHEMP_SWEEP_HIT_RANGE 0x13EC
#define SHEMP_SWEEP_HIT_CLEARANCE 0x320
#define SHEMP_SWEEP_REST 0x3C
/* Stunned, the camera shows his retreat for STUN_FRAMES while he walks to
 * the next node at RETREAT_SPEED. */
#define SHEMP_STUN_FRAMES 0xC8
#define SHEMP_RETREAT_SPEED 0xA0

/* ->mode of a shepherd (ACTOR_TYPE_SHEPHERD, ShepherdState; levels 12/13).
 * SHEPHERD_IDLE runs ShepherdState.post's waiting behaviour; shepherds
 * work in pairs (ShepherdState.partner), one riding the other's shoulders. */
enum ShepherdMode {
  SHEPHERD_IDLE = 0,
  SHEPHERD_CHASE = 2,     /* heads for Spyro's side; too far from home: GO_HOME */
  SHEPHERD_KNOCKED = 3,   /* charged: tumbles away and bursts */
  SHEPHERD_SWING = 4,     /* crook swing (nudges the camera look) */
  SHEPHERD_WATCH = 6,     /* faces Spyro; the partner was knocked: RECOVER */
  SHEPHERD_RECOVER = 7,   /* plays out its animation, back to SHEPHERD_IDLE */
  SHEPHERD_ROUSE = 10,    /* turns to Spyro and shouts, then CHASE */
  SHEPHERD_ALERTED = 11,  /* straight to CHASE */
  SHEPHERD_LEAP_ON = 16,  /* jumps onto the partner's attach point */
  SHEPHERD_RIDE = 17,     /* rides the partner (which goes ALERTED) */
  SHEPHERD_THROWN = 18,   /* the partner was knocked: falls off */
  SHEPHERD_GO_HOME = 100, /* walks back to its home point */
};

/* ShepherdState.sub on SHEPHERD_POST_PAIR_PATROL (1 is written through
 * the function's pinned `one`). */
enum ShepherdPairStep {
  SHEPHERD_PAIR_WALK = 0, /* patrols its path; near its partner on the timer: TURN */
  SHEPHERD_PAIR_TURN = 1, /* turns to the partner */
  SHEPHERD_PAIR_FACE = 2, /* SHEPHERD_ANIM_FACE_PARTNER, then WALK */
};

/* ShepherdState.post: what a shepherd does while SHEPHERD_IDLE, and how it
 * chases in SHEPHERD_CHASE (the spawn record sets it; 8 is never used). Like
 * CaptainPost. */
enum ShepherdPost {
  SHEPHERD_POST_GUARD = 0,       /* faces Spyro; swings when he is close and just above */
  SHEPHERD_POST_RUN_PATH = 1,    /* Spyro near: clears its done flag and rouses the group; CHASE runs
                                  * its path once, sets the flag and drops to POST_GUARD */
  SHEPHERD_POST_LOITER = 2,      /* holds its heading; Spyro near: rouses the group. CHASE flanks him
                                  * and gives up when he turns away or it strays from home */
  SHEPHERD_POST_GUARD_LEVEL = 3, /* POST_GUARD on the ground, Spyro at its own height */
  SHEPHERD_POST_THROWER = 4,     /* throws crooks; Spyro near: sends the partner to CHASE, then WATCH */
  SHEPHERD_POST_LEAPER = 5,      /* throws crooks; Spyro above its path: LEAP_ON the partner (ROUSE) */
  SHEPHERD_POST_PAIR_PATROL = 6, /* patrols, stopping to face the partner; Spyro near: the pair split
                                  * sides (ShepherdState.side) and go ALERTED */
  SHEPHERD_POST_PATROL = 7,      /* patrols its path; Spyro near: CHASE, re-homing at the nearest point */
  SHEPHERD_POST_WATCH = 9,       /* faces Spyro; he lands near: CHASE */
  SHEPHERD_POST_COVER = 10,      /* throws toward its partners, guards when Spyro is close; a
                                  * partner knocked: POST_WATCH and RECOVER */
};

/* A shepherd's animations, from the modes that play them. */
enum ShepherdAnim {
  SHEPHERD_ANIM_RUN = 1,
  SHEPHERD_ANIM_WATCH = 2,      /* the leaper tracking Spyro above it */
  SHEPHERD_ANIM_THROW = 5,      /* a crook leaves its hand on frame 0xE */
  SHEPHERD_ANIM_LOITER = 6,
  SHEPHERD_ANIM_RIDE = 0xC,
  SHEPHERD_ANIM_SIGNAL = 0xD,   /* sends its partner after Spyro */
  SHEPHERD_ANIM_FACE_PARTNER = 0xE, /* the pair patrol's stop */
};

/* Shepherd ranges (2D, from Spyro). A guard swings within SWING_RANGE of him
 * and SWING_DZ of his height (POST_GUARD: GUARD_SWING_RANGE with him up to
 * GUARD_SWING_ABOVE over it); a chaser swings only once he faces it
 * (SPY_FACING under FACING_BAND), he is under SWING_ABOVE above it and
 * within SWING_CHASE_DZ. The waiting posts wake within WAKE_RANGE
 * (RUN_PATH_WAKE / THROWER_WAKE / WATCH_WAKE for those posts, FLANK_RANGE
 * for the pair split) and NOTICE_DZ of his height. */
#define SHEPHERD_SWING_RANGE 0x4B0
#define SHEPHERD_SWING_DZ 0x320
#define SHEPHERD_GUARD_SWING_RANGE 0x898
#define SHEPHERD_GUARD_SWING_ABOVE 0x7D0
#define SHEPHERD_FACING_BAND 0x32
#define SHEPHERD_SWING_ABOVE 0xC8
#define SHEPHERD_SWING_CHASE_DZ 0x3E8
#define SHEPHERD_WAKE_RANGE 0x1C00
#define SHEPHERD_RUN_PATH_WAKE_RANGE 0x3800
#define SHEPHERD_THROWER_WAKE_RANGE 0x3000
#define SHEPHERD_WATCH_WAKE_RANGE 0x2000
#define SHEPHERD_FLANK_RANGE 0x1000
#define SHEPHERD_NOTICE_DZ 0x320
/* POST_COVER guards (stops throwing) once Spyro is within COVER_GUARD_RANGE
 * and goes back to throwing past COVER_RELEASE_RANGE. */
#define SHEPHERD_COVER_GUARD_RANGE 0x1800
#define SHEPHERD_COVER_RELEASE_RANGE 0x2000
/* SHEPHERD_CHASE: steps CHASE_SPEED a frame; Spyro turning away (SPY_FACING
 * past TURNED_AWAY) swaps the flank side and sends a loiterer home. A chaser
 * gives up past its leash: LOITER_LEASH / WATCH_LEASH from home, LEAPER_LEASH
 * for the leaper, PATH_LEASH from its nearest path point. */
#define SHEPHERD_CHASE_SPEED 0x32
#define SHEPHERD_TURNED_AWAY 0x40
#define SHEPHERD_LOITER_LEASH 0x1800
#define SHEPHERD_WATCH_LEASH 0x1800
#define SHEPHERD_LEAPER_LEASH 0x2000
#define SHEPHERD_PATH_LEASH 0x1000
/* SHEPHERD_SWING ends at SWING_END_FRAME and rests SWING_COOLDOWN frames. */
#define SHEPHERD_SWING_END_FRAME 0x1C
#define SHEPHERD_SWING_COOLDOWN 0xA0
/* SHEPHERD_ANIM_THROW lets go of a crook (ACTOR_TYPE_SHEPHERD_THROW) on
 * THROW_FRAME: it flies CROOK_SPEED for CROOK_LIFE frames (the thrower's
 * CROOK_LIFE_SHORT). */
#define SHEPHERD_THROW_FRAME 0xE
#define SHEPHERD_CROOK_SPEED 0x50
#define SHEPHERD_CROOK_LIFE 0x1E
#define SHEPHERD_CROOK_LIFE_SHORT 0x14
/* SHEPHERD_KNOCKED launch speeds (DriveActorTrackedMove). */
#define SHEPHERD_KNOCK_SPEED 0x1C2
#define SHEPHERD_KNOCK_VZ 0x8C
/* LEAP_ON / THROWN: vz loses GRAVITY a frame (down to -LEAP_MAX_FALL /
 * -THROWN_MAX_FALL); LEAP_VZ is the leap, THROWN_VZ / THROWN_SPEED the throw
 * off a knocked partner (the speed loses THROWN_DRAG a frame). */
#define SHEPHERD_GRAVITY 0x14
#define SHEPHERD_LEAP_VZ 0xD2
#define SHEPHERD_LEAP_MAX_FALL 0x12C
#define SHEPHERD_THROWN_VZ 0xC8
#define SHEPHERD_THROWN_MAX_FALL 0xC8
#define SHEPHERD_THROWN_SPEED 0x190
#define SHEPHERD_THROWN_DRAG 0x14
/* SHEPHERD_GO_HOME walks HOME_SPEED, arriving within HOME_ARRIVE, and rests
 * HOME_REST frames. */
#define SHEPHERD_HOME_SPEED 0x5A
#define SHEPHERD_HOME_ARRIVE 0x80
#define SHEPHERD_HOME_REST 0x3C
#define SHEPHERD_BURST_SPARK_LIFE 0x18 /* BurstActorWithSound spark life */
#define SHEPHERD_KNOCK_DRAG 0x12 /* DriveActorTrackedMove friction */
#define SHEPHERD_KNOCK_GRAVITY 0xC /* DriveActorTrackedMove gravity */

/* ->mode of a singing path walker (ACTOR_TYPE_SINGING_PATH_WALKER,
 * PathWalkerState; levels 24/28). The mode doubles as the pose's animation
 * index (ENTER_POSE). PathWalkerState.mode is its SingerRole. */
enum SingerMode {
  SINGER_IDLE = 0,      /* waits; Spyro near: TURN, then WALK (close) or WATCH */
  SINGER_TURN = 1,      /* plays the turn, then enters PathWalkerState.next */
  SINGER_REST = 2,      /* faces Spyro; while carried, rides (rideMode 1) or flies (2) */
  SINGER_WALK = 3,      /* walks its path, calling while Spyro is close; then REST */
  SINGER_WATCH = 4,     /* faces Spyro; close: WALK, now and then: REST */
  SINGER_LANDED = 7,    /* a thrown walker hit something: plays out, then RETURN */
  SINGER_STRUCK = 8,    /* flamed or charged: releases its role's chunk animation */
  SINGER_SING = 9,      /* sings in place until its timer runs out */
  SINGER_SING_END = 10, /* last verse; a paired walker cues a singing partner to stop */
  SINGER_SETTLE = 12,   /* stops its voices, back to REST */
  SINGER_RETURN = 99,   /* walks its path back and frees the rider */
  SINGER_CUE_STOP = 99, /* PathWalkerState.next: the partner says stop singing */
};

/* A singing path walker's animations besides the poses equal to its mode. */
enum SingerAnim {
  SINGER_ANIM_THROWN = 6, /* flying after its rider throws it */
};

/* PathWalkerState.mode: the walker's role, fixed at spawn. */
enum SingerRole {
  SINGER_ROLE_PLAIN = 0,
  SINGER_ROLE_DROPPED = 1, /* was CARRIED; struck off its rider */
  SINGER_ROLE_PAIRED = 2,  /* sings with PathWalkerState.partner */
  SINGER_ROLE_CARRIED = 3, /* rides PathWalkerState.rider */
  SINGER_ROLE_TWO_GATE = 4, /* releases chunk animation 1, else 2 */
  SINGER_ROLE_ABSENT = 6,  /* despawns at init */
};
/* Singing-walker tuning. Spyro within WALK_RANGE sets it walking (calling
 * every CALL_MIN.. ticks while he stays close, else tweening at FAST_TWEEN
 * back to REST); within WATCH_RANGE it watches. A thrown walker lands on
 * hitting anything within THROWN_RADIUS. Struck, it sinks STRUCK_SINK a
 * frame while over a floor gap, and a CARRIED one drops DROPPED_PATH_TRIM
 * nodes off its path. A PAIRED / TWO_GATE walker sings PAIRED_SING_TICKS
 * (RESUME_SING_TICKS once its partner's gate halts), then its last verse
 * SING_END_TICKS; a paired walker rewinds a halted gate below
 * GATE_REWIND_BELOW to step GATE_REWIND_STEP. */
#define SINGER_WALK_RANGE 0x1000
#define SINGER_WATCH_RANGE 0x2000
#define SINGER_CALL_MIN 100
#define SINGER_FAST_TWEEN 0x16
#define SINGER_THROWN_RADIUS 0x100
#define SINGER_STRUCK_SINK 0x40
#define SINGER_DROPPED_PATH_TRIM 4
#define SINGER_PAIRED_SING_TICKS 0x1C2
#define SINGER_RESUME_SING_TICKS 0x14A
#define SINGER_SING_END_TICKS 0x50
#define SINGER_GATE_REWIND_BELOW 0x14
#define SINGER_GATE_REWIND_STEP 0x1E

/* ->mode of a charging guard (ACTOR_TYPE_CHARGING_GUARD, ChargingGuardState;
 * levels 18/20). ChargingGuardState.flag is shared by the guards of one
 * group: only one charges at a time. */
enum ChargerMode {
  CHARGER_IDLE = 0,      /* runs its ChargerRole until Spyro is in sight range */
  CHARGER_CHARGE = 1,    /* aims at Spyro and runs at him; close and lined up: RAM */
  CHARGER_KNOCKED = 2,   /* flamed: knocked back, then bursts */
  CHARGER_RAM = 3,       /* the horn lunge; knocks Spyro back, camera pulls in */
  CHARGER_TURN_BACK = 4, /* turns about after the ram */
  CHARGER_GO_HOME = 100, /* back to its home point, then IDLE */
};

/* ChargingGuardState.mode: what a charging guard does while CHARGER_IDLE. */
enum ChargerRole {
  CHARGER_ROLE_STAND = 0,  /* faces Spyro */
  CHARGER_ROLE_PATROL = 1, /* walks to random path nodes */
  CHARGER_ROLE_POST = 2,   /* holds node 0; charges once Spyro reaches node 1 or 2 */
  CHARGER_ROLE_FOLLOW = 3, /* follows ChargingGuardState.target (PATROL once alerted) */
};

/* A lurking guard (ChargingGuardState.lurk) Spyro is already within
 * LURK_RANGE of on its first update is removed, its drop scattered. */
#define CHARGER_LURK_RANGE 0x2000
/* While camHold runs the camera frames the guard from CAM_PULLBACK past it. */
#define CHARGER_CAM_PULLBACK 0x1000
/* Roles: a patroller walks PATROL_MIN_SPEED or more, picking a new node and a
 * PATROL_MIN..MAX_SPEED within NODE_ARRIVE of one; a follower walks
 * FOLLOW_SPEED, FOLLOW_NEAR_SPEED within FOLLOW_NEAR of its target and
 * FOLLOW_FAR_SPEED past FOLLOW_FAR; a post guard charges once Spyro is within
 * POST_TRIGGER_RANGE of path node 1 or 2. The others charge with Spyro in
 * sight range and within SIGHT_DZ of their height. */
#define CHARGER_PATROL_MIN_SPEED 0x96
#define CHARGER_PATROL_SPEED_MIN 0xB4
#define CHARGER_PATROL_SPEED_MAX 0xE6
#define CHARGER_NODE_ARRIVE 0x100
#define CHARGER_FOLLOW_SPEED 0xA0
#define CHARGER_FOLLOW_NEAR 0xFA0
#define CHARGER_FOLLOW_NEAR_SPEED 0x8C
#define CHARGER_FOLLOW_FAR 0x1770
#define CHARGER_FOLLOW_FAR_SPEED 0xB4
#define CHARGER_POST_TRIGGER_RANGE 0x5DC
#define CHARGER_SIGHT_DZ 0x320
/* CHARGER_CHARGE runs CHARGE_SPEED (or speedOverride), re-reading Spyro's
 * heading every REAIM_FRAMES; it rams (RAM_SPEED, POST_RAM_SPEED for a post
 * guard) within RAM_RANGE once lined up within RAM_CONE. It gives up (rest
 * REST_FRAMES, GO_HOME) when blocked, when Spyro turns away (SPY facing delta
 * GIVE_UP_FACING or more) or past LEASH from where it set off. A guard with a
 * `filter` type super-hits those (and lurkers) within TRAMPLE_RANGE and
 * TRAMPLE_CONE of its heading. */
#define CHARGER_CHARGE_SPEED 0x15E
#define CHARGER_REAIM_FRAMES 0x1E
#define CHARGER_RAM_RANGE 0xED8
#define CHARGER_RAM_CONE 0x14
#define CHARGER_RAM_SPEED 0xFA
#define CHARGER_POST_RAM_SPEED 0xE6
#define CHARGER_REST_FRAMES 0x78
#define CHARGER_GIVE_UP_FACING 0x3D
#define CHARGER_LEASH 0x4000
#define CHARGER_TRAMPLE_BOX 0xBB8
#define CHARGER_TRAMPLE_RANGE 0x7D0
#define CHARGER_TRAMPLE_CONE 0x19
/* CHARGER_RAM slows by RAM_DRAG a frame from frame 4 down to RAM_MIN_SPEED,
 * and hits Spyro within HIT_RANGE / HIT_DZ: he is knocked back at
 * HIT_KNOCK_SPEED (HIT_KNOCK_VZ up), the guard rests HIT_REST_FRAMES and the
 * camera holds on it for CAM_HOLD_FRAMES from CAM_HIT_PULLBACK past Spyro. */
#define CHARGER_RAM_DRAG 0x1E
#define CHARGER_RAM_MIN_SPEED 0x28
#define CHARGER_HIT_RANGE 0x5DC
#define CHARGER_HIT_DZ 0x384
#define CHARGER_HIT_KNOCK_SPEED 0x87
#define CHARGER_HIT_KNOCK_VZ 0x32
#define CHARGER_HIT_REST_FRAMES 0xDC
#define CHARGER_CAM_HOLD_FRAMES 0x1E
#define CHARGER_CAM_HIT_PULLBACK 0xC00
/* CHARGER_KNOCKED slides from KNOCK_SPEED, losing KNOCK_DRAG a frame. */
#define CHARGER_KNOCK_SPEED 0xFA
#define CHARGER_KNOCK_DRAG 0xF
/* CHARGER_GO_HOME walks HOME_SPEED (a post guard orbits back at
 * POST_HOME_SPEED) and is home within HOME_ARRIVE (POST_HOME_ARRIVE). */
#define CHARGER_HOME_SPEED 0xA0
#define CHARGER_HOME_ARRIVE 0xB4
#define CHARGER_POST_HOME_SPEED 0xC8
#define CHARGER_POST_HOME_ARRIVE 0x200
#define CHARGER_BURST_SPARK_LIFE 0x28 /* BurstActorWithSound spark life */

/* ->mode of a path guard (ACTOR_TYPE_PATH_GUARD, PathGuardState; levels
 * 12/15/16). */
enum PathGuardMode {
  PATHGUARD_WALK = 0,    /* walks its ring with its partner; Spyro near: THROW */
  PATHGUARD_THROW = 1,   /* winds up and throws ACTOR_TYPE_SPLIT_SHOTs */
  PATHGUARD_KNOCKED = 2, /* flamed or charged: knocked away, then bursts */
};

/* A path guard throws once Spyro is within THROW_RANGE (2D) and THROW_DZ of
 * its height; a held guard waits within NODE_ARRIVE of its node. It walks
 * WALK_SPEED (the ring pair steers at RING_SPEED). Hit, it slides from
 * KNOCK_SPEED (+ CHARGE_KNOCK_BONUS when charged). Its THROW pose releases a
 * split shot at once, on frame 2 and on frame 6, then the timer holds
 * THROW_COOLDOWN. */
#define PATHGUARD_THROW_RANGE 0x1000
#define PATHGUARD_THROW_DZ 0x800
#define PATHGUARD_NODE_ARRIVE 0x100
#define PATHGUARD_WALK_SPEED 0x60
#define PATHGUARD_RING_SPEED 0x40
#define PATHGUARD_KNOCK_SPEED 0xDC
#define PATHGUARD_CHARGE_KNOCK_BONUS 0x8C
#define PATHGUARD_THROW_COOLDOWN 0x3C
/* It walks (and rings) with MOVE_RADIUS, the ring's wall probe WALL_RADIUS;
 * hit, it slides along Spyro's heading bent KNOCK_BEND_STEP_WIDE /
 * KNOCK_BEND_FRAC_WIDE toward his yaw, losing KNOCK_DRAG a frame, and
 * bursts with BURST_SPARK_LIFE sparks. */
#define PATHGUARD_MOVE_RADIUS 0x180
#define PATHGUARD_WALL_RADIUS 0x15C
#define PATHGUARD_KNOCK_BEND_STEP_WIDE 0x20
#define PATHGUARD_KNOCK_BEND_FRAC_WIDE 0x60
#define PATHGUARD_KNOCK_DRAG 0xC
#define PATHGUARD_BURST_SPARK_LIFE 0x18

/* ->mode of a split shot (ACTOR_TYPE_SPLIT_SHOT), set at spawn from the
 * thrower's animation frame; the mode doubles as the animation index. */
enum SplitShotMode {
  SPLITSHOT_GATHER = 0,   /* before the thrower's frame 2: glows in its hand */
  SPLITSHOT_RAISE = 1,    /* frames 2..5: held overhead */
  SPLITSHOT_FLY = 2,      /* frame 6+: launched; splits on reaching Spyro */
  SPLITSHOT_FRAGMENT = 3, /* one of the four a split leaves behind */
};
/* A flying split shot moves at SPEED (radius RADIUS) until its timer, a
 * wall or leaving the level floor (any axis under MIN_COORD); within
 * HIT_RADIUS of Spyro it splits into FRAGMENTS a quarter turn apart. */
#define SPLITSHOT_SPEED 0x80
#define SPLITSHOT_RADIUS 0x100
#define SPLITSHOT_MIN_COORD 0x400
#define SPLITSHOT_HIT_RADIUS 0x80
#define SPLITSHOT_FRAGMENTS 4

/* ->subMode of a cannon (ACTOR_TYPE_CANNON, CannonState; levels 8/19/25/34). */
enum CannonSubMode {
  CANNON_UNLIT = 0,  /* waits for a flame; despawns once CannonState.link is dormant */
  CANNON_FUSE = 1,   /* smokes, then sparks and recoils, then fires */
  CANNON_FLIGHT = 2, /* flies the shot along its path; at the end strikes the link */
};
/* A lit cannon smokes from its muzzle (MUZZLE_LEN along the barrel) until
 * SMOKE_TICKS, then sparks and recoils until FIRE_TICK, when it fires; the
 * shot takes LEG_TICKS a path leg. Barrels pitched within LOW_PITCH_SPAN
 * above flat aim at LOW_PITCH. */
#define CANNON_MUZZLE_LEN 0x14C
#define CANNON_SMOKE_TICKS 0x5A
#define CANNON_FIRE_TICK 0xB4
#define CANNON_LEG_TICKS 4
#define CANNON_LOW_PITCH_SPAN 0x60
#define CANNON_LOW_PITCH 0xE0

/* ACTOR_TYPE_FLIGHT_TARGET is flown through when Spyro, within NEAR_RANGE (2D)
 * and NEAR_DZ, is within HIT_RANGE (3D, height weighted) of it and
 * PLANE_DIST of its plane. */
#define FLIGHT_TARGET_NEAR_RANGE 3000
#define FLIGHT_TARGET_NEAR_DZ 4000
#define FLIGHT_TARGET_HIT_RANGE 0x578
#define FLIGHT_TARGET_PLANE_DIST 0x200
/* Spyro's height is taken from FLIGHT_TARGET_CENTRE_DROP above the target's
 * centre; flown through, it bursts into FLIGHT_TARGET_SHARDS shards living
 * FLIGHT_TARGET_SHARD_LIFE ticks. */
#define FLIGHT_TARGET_CENTRE_DROP 0xC0
#define FLIGHT_TARGET_SHARDS 13
#define FLIGHT_TARGET_SHARD_LIFE 0x3C

/* ->mode of a banking flight target (ACTOR_TYPE_BANKING_FLIGHT_TARGET,
 * RollerState; flight levels 5/17/23). */
enum BankerMode {
  BANKER_FLY = 0,    /* tweens its path, banking into turns */
  BANKER_STRUCK = 1, /* flamed: pays out a time bonus, tallies, flees */
  BANKER_FLEE = 2,   /* keeps running until it splashes down */
};
/* A banking flight target flies its path (tween speed TWEEN_SPEED; FLAT_TWEEN
 * on levels 5 and 23, which fly it with TWEEN_FLY), banking with its turns
 * up to MAX_ROLL (12-bit). Struck, it is knocked from KNOCK_SPEED along
 * Spyro's heading bent KNOCK_BEND_STEP_WIDE toward his yaw, losing
 * KNOCK_DRAG and KNOCK_GRAVITY a frame, throwing SPARK_TRIPLES spark-debris
 * triples. */
#define BANKER_TWEEN_SPEED 0x16
#define BANKER_FLAT_TWEEN 0xC
#define BANKER_MAX_ROLL 0x200
#define BANKER_KNOCK_SPEED 0x15E
#define BANKER_KNOCK_BEND_STEP_WIDE 0x28
#define BANKER_KNOCK_DRAG 0xC
#define BANKER_KNOCK_GRAVITY 0xC
#define BANKER_SPARK_TRIPLES 3

/* ->mode of a flight time bonus (ACTOR_TYPE_TIME_BONUS..+2), set to
 * TIMEBONUS_COLLECT by the target that pays it out. */
enum TimeBonusMode {
  TIMEBONUS_WAIT = 0,    /* faces Spyro beside its target */
  TIMEBONUS_COLLECT = 1, /* banks the bonus and moves to screen space */
  TIMEBONUS_TO_HUD = 2,  /* flies to the HUD clock */
  TIMEBONUS_ARRIVED = 3, /* adds the seconds after a short hold */
};
/* A collected time bonus (TIME_BONUS_SECONDS(type) seconds of 60 frames)
 * moves to screen space (rolled SCREEN_ROLL, OT bias OT_BIAS) and flies at
 * FLY_SPEED to the HUD clock at HUD_X / HUD_Y / HUD_DEPTH, arriving within
 * ARRIVE_RANGE; it adds its seconds after HOLD_TICKS there. */
#define TIMEBONUS_FRAMES_PER_SECOND 60
#define TIMEBONUS_SCREEN_ROLL 0xC0
#define TIMEBONUS_OT_BIAS 0x20
#define TIMEBONUS_FLY_SPEED 0x80
#define TIMEBONUS_HUD_X 0x1A4
#define TIMEBONUS_HUD_Y 0x30
#define TIMEBONUS_HUD_DEPTH 0x157C
#define TIMEBONUS_ARRIVE_RANGE 0x20
#define TIMEBONUS_HOLD_TICKS 0x1E

/* ->mode of a Peace Keeper soldier (ACTOR_TYPE_SOLDIER / _B, SoldierState;
 * level 6). SOLDIER_IDLE runs SoldierState.kind's patrol/guard behaviour;
 * whole groups are switched with ReplaceActorGroupState. */
enum SoldierMode {
  SOLDIER_IDLE = 0,
  SOLDIER_CHASE = 1,       /* walks at Spyro (or runs to its cannon); too far: GO_HOME */
  SOLDIER_SHOVE = 2,       /* close: shoves Spyro back */
  SOLDIER_KNOCKED = 3,     /* flamed or charged: knocked away, then bursts */
  SOLDIER_ALERT = 4,       /* turns to Spyro and shouts, then CHASE */
  SOLDIER_GUNNER = 8,      /* works SoldierState.link's cannon, lobbing fireballs at Spyro */
  SOLDIER_DUEL = 9,        /* fires its cannon at the rival cannon; Spyro near: GUNNER */
  SOLDIER_TO_POST = 10,    /* shouts, then runs to SoldierState.link's post */
  SOLDIER_IN_POST = 11,    /* hides in the post and pops out at Spyro */
  SOLDIER_ENTER_POST = 16, /* climbs in, then IN_POST */
  SOLDIER_GO_HOME = 100,   /* walks back to SoldierState.home, then IDLE */
};

/* SoldierState.phase during SOLDIER_TO_POST. */
enum SoldierPostPhase {
  SOLDIER_POST_START = 0, /* rolls SoldierState.bold and a random shout delay */
  SOLDIER_POST_SHOUT = 1, /* turns to Spyro and shouts (SOLDIER_ANIM_SHOUT) */
  SOLDIER_POST_RUN = 2,   /* runs to its post, then ENTER_POST */
};

/* A soldier's animations besides the poses equal to its mode (CHASE 1,
 * KNOCKED 3, ALERT 4, IN_POST 0xB, ENTER_POST 0x10). */
enum SoldierAnim {
  SOLDIER_ANIM_LOOK_AROUND = 6, /* a patrol's pause at a path point */
  SOLDIER_ANIM_SHOUT = 9,       /* TO_POST's call before it runs */
  SOLDIER_ANIM_HIDE = 0xC,      /* down in its post, facing Spyro */
  SOLDIER_ANIM_ALERT_SHORT = 0xF, /* the bounded sentry's ALERT */
  SOLDIER_ANIM_POP_OUT = 0x11,  /* up out of the post while Spyro looks away */
};

/* Soldier tuning. Patrols walk WALK_SPEED, pausing LOOK_MIN..MAX_FRAMES at
 * each point. Kinds notice Spyro within NOTICE_RANGE (a sentry within
 * SENTRY_RANGE, a short-leash one SHORT_LEASH_RANGE) and shove him within
 * SHOVE_RANGE; chasers give up past LEASH from home (SHORT_LEASH,
 * BOUNDED_LEASH). Knocked, it slides from KNOCK_SPEED, losing KNOCK_DRAG a
 * frame. GO_HOME walks HOME_SPEED, home within HOME_ARRIVE. */
#define SOLDIER_WALK_SPEED 0x46
#define SOLDIER_LOOK_MIN_FRAMES 0x78
#define SOLDIER_LOOK_MAX_FRAMES 0xB4
#define SOLDIER_NOTICE_RANGE 0x2000
#define SOLDIER_SENTRY_RANGE 0x2800
#define SOLDIER_SHORT_LEASH_RANGE 0x1800
#define SOLDIER_SHOVE_RANGE 0x44C
#define SOLDIER_LEASH 0x1800
#define SOLDIER_SHORT_LEASH 0x1000
#define SOLDIER_BOUNDED_LEASH 0x3000
#define SOLDIER_KNOCK_SPEED 0xFA
#define SOLDIER_KNOCK_DRAG 0x14
#define SOLDIER_HOME_SPEED 0x5A
#define SOLDIER_HOME_ARRIVE 0x80
/* A cannon runner runs from RUN_START_SPEED up to RUN_SPEED (RUN_ACCEL a
 * frame) and mans the cannon within CANNON_REACH, orbiting it at that
 * radius. A gunner fires once Spyro is FIRE_MIN..MAX_RANGE away and it has
 * been drawn FIRE_SEEN_FRAMES: fireballs fly FIREBALL_SPEED and live
 * FIREBALL_LIFE; it then waits FIRE_GAP (its partner half that). TO_POST
 * runs at up to POST_RUN_SPEED (POST_RUN_ACCEL a frame), arriving within
 * POST_ARRIVE of the post's front. */
#define SOLDIER_RUN_START_SPEED 0x32
#define SOLDIER_RUN_SPEED 0xAA
#define SOLDIER_RUN_ACCEL 0x1E
#define SOLDIER_CANNON_REACH 0x5DC
#define SOLDIER_FIRE_MIN_RANGE 0x1400
#define SOLDIER_FIRE_MAX_RANGE 0x3C00
#define SOLDIER_FIRE_SEEN_FRAMES 0x50
#define SOLDIER_FIREBALL_SPEED 0xB4
#define SOLDIER_FIREBALL_LIFE 0xB4
#define SOLDIER_FIRE_GAP 0xB4
#define SOLDIER_POST_RUN_SPEED 0x82
#define SOLDIER_POST_RUN_ACCEL 0x14
#define SOLDIER_POST_ARRIVE 0x200
#define SOLDIER_BURST_SPARK_LIFE 0x18 /* BurstActorWithSound spark life */

/* ->mode of a level 6 fireball cannon (ACTOR_TYPE_FIREBALL_CANNON,
 * FireballCannonState). While its crew (FireballCannonState.link) stands, the
 * crew fires it; once the crew is down, Spyro can flame the fuse. */
enum FireballCannonMode {
  FBCANNON_IDLE = 0,
  FBCANNON_LIT = 1,  /* flamed from behind: starts the fuse */
  FBCANNON_FUSE = 2, /* smokes for 0x3C ticks */
  FBCANNON_FIRE = 3, /* lobs a fireball at the best target ahead, back to IDLE */
};

/* A fireball cannon's animations; its gunner (or a dueling soldier) plays
 * them on its link. */
enum FireballCannonAnim {
  FBCANNON_ANIM_REST = 0,
  FBCANNON_ANIM_TURN_NEG = 1, /* the gunner's aim is 0xB+ off, negative yaw delta */
  FBCANNON_ANIM_TURN_POS = 2, /* the same, positive */
  FBCANNON_ANIM_FIRE = 3,     /* the lob; back to REST from frame 8 */
};

/* Fireball cannon tuning (the level 6 cousin of the comet cannon). With
 * Spyro beside it (within LOCK_RANGE, LOCK_RANGE_HELD once locked) the
 * camera locks on it; flamed from behind it lights a FUSE_FRAMES fuse and
 * fires a fireball from MUZZLE_FWD ahead and MUZZLE_UP above at BALL_SPEED,
 * re-aimed at the best target within SEEK_CONE (under SEEK_QUICK Manhattan
 * and SEEK_RANGE exact, scored by distance outside SEEK_NEAR..SEEK_FAR plus
 * 4096 per unit of angle, a reward burst's score quartered) and lobbed when
 * over LOB_MIN_DROP below the target's level. */
#define FBCANNON_LOCK_RANGE 0x4E2
#define FBCANNON_LOCK_RANGE_HELD 0x578
#define FBCANNON_FUSE_FRAMES 0x3C
#define FBCANNON_MUZZLE_FWD 0x546
#define FBCANNON_MUZZLE_UP 0x47E
#define FBCANNON_BALL_SPEED 0xF0
#define FBCANNON_SEEK_QUICK 0xBFFF
#define FBCANNON_SEEK_CONE 7
#define FBCANNON_SEEK_RANGE 0x6800
#define FBCANNON_SEEK_NEAR 0x3800
#define FBCANNON_SEEK_FAR 0x4800
#define FBCANNON_LOB_MIN_DROP 0x400

/* ->mode of a guard captain (ACTOR_TYPE_GUARD_CAPTAIN, CaptainState;
 * level 13). CAPTAIN_IDLE runs CaptainState.mode's post (partnered sentry,
 * group lookout, ledge guard, wanderer...). */
enum CaptainMode {
  CAPTAIN_IDLE = 0,
  CAPTAIN_ADVANCE = 1,       /* walks at Spyro (or on its path); close: BASH or FIGHT */
  CAPTAIN_ROUSE = 2,         /* waits out its timer, then ADVANCE */
  CAPTAIN_KNOCKED = 3,       /* flamed: knocked away, then bursts */
  CAPTAIN_FIGHT = 6,         /* sword fight, CaptainState.step sequencing the swings */
  CAPTAIN_BASH = 7,          /* shoulder-bashes Spyro away, then GO_HOME */
  CAPTAIN_SET_HOME = 10,     /* takes path node 0 as home */
  CAPTAIN_STALK = 11,        /* the ledge guard closes on Spyro */
  CAPTAIN_DRAW = 20,         /* draws its sword, then FIGHT */
  CAPTAIN_ALERTED = 21,      /* set group-wide by a lookout: waits, then draws */
  CAPTAIN_DRAW_ADVANCE = 22, /* draws its sword, then ADVANCE */
  CAPTAIN_GO_HOME = 100,     /* walks back to CaptainState.home, then IDLE */
};

/* CaptainState.mode: the post a guard captain keeps while CAPTAIN_IDLE (from
 * its spawn record). */
enum CaptainPost {
  CAPTAIN_POST_SENTRY = 0,       /* holds its heading beside a partner; fights when close or the partner falls */
  CAPTAIN_POST_LOOKOUT = 1,      /* within range: alerts its group (CAPTAIN_ALERTED) */
  CAPTAIN_POST_LEDGE = 2,        /* once armed (step), stalks Spyro when he is above its path */
  CAPTAIN_POST_FIGHTER = 3,      /* fights straight away */
  CAPTAIN_POST_PATROL = 4,       /* walks between path points once its shepherd partner is knocked */
  CAPTAIN_POST_LEDGE_WAIT = 5,   /* faces Spyro until cued, then arms as POST_LEDGE */
};

/* CaptainState.step during CAPTAIN_FIGHT: the sword-fight sequence. */
enum CaptainFightStep {
  CAPTAIN_STEP_GUARD = 0,    /* waits for Spyro within CAPTAIN_NOTICE_RANGE */
  CAPTAIN_STEP_DRAW = 1,     /* draw animation */
  CAPTAIN_STEP_SPAR = 2,     /* close: STRIKE; far: SHEATHE_RESET; timer2 out: SHEATHE */
  CAPTAIN_STEP_STRIKE = 3,   /* the swing; hits on frames 0x25..0x28 */
  CAPTAIN_STEP_SHEATHE_RESET = 4, /* puts the sword away, back to GUARD */
  CAPTAIN_STEP_SHEATHE = 5,  /* puts the sword away, then REST */
  CAPTAIN_STEP_REST = 6,     /* close: STRIKE; rested: DRAW again */
};

/* A guard captain's animations, from the modes that play them. */
enum CaptainAnim {
  CAPTAIN_ANIM_STAND = 0,    /* sentry; held on its frame 0x14/0x15 pose */
  CAPTAIN_ANIM_GUARD = 2,
  CAPTAIN_ANIM_FIGHT_DRAW = 6, /* the fight's draw step */
  CAPTAIN_ANIM_WALK = 7,
  CAPTAIN_ANIM_SHEATHE = 8,
  CAPTAIN_ANIM_ROUSE = 9,
  CAPTAIN_ANIM_BASH = 0xA,
  CAPTAIN_ANIM_START = 0xB,  /* level-data start pose (the lookouts and one patrol captain); no
                              * C writer, so it holds until another anim replaces it. A patrol
                              * captain still in it draws (DRAW) before fighting */
  CAPTAIN_ANIM_DRAW = 0xC,   /* alerted: draws before DRAW / DRAW_ADVANCE */
};

/* Guard captain tuning. Flamed it is knocked off at KNOCK_SPEED. A sentry
 * fights within SENTRY_RANGE; a patroller draws within NOTICE_RANGE /
 * NOTICE_DZ and walks at PATROL_SPEED (new point within NODE_ARRIVE); a
 * lookout walks LOOKOUT_SPEED and fights within LOOKOUT_RANGE; a ledge guard
 * stalks within LEDGE_RANGE at STALK_SPEED. Advancing (ADVANCE_SPEED) it
 * bashes within BASH_RANGE / NOTICE_DZ, Spyro flung at BASH_SPEED (BASH_VZ
 * up), and gives up past LEASH (STALK_LEASH stalking; STALK_REST after a
 * blocked stalk). Fighting: it draws within NOTICE_RANGE, spars
 * SPAR_MIN..MAX_FRAMES, strikes within STRIKE_RANGE / STRIKE_DZ (rest
 * STRIKE_REST, the blow landing within STRIKE_HIT_RANGE on frames
 * 0x25..0x28), rests REST_MIN..MAX_FRAMES and sheathes past FIGHT_LEAVE.
 * GO_HOME walks HOME_SPEED, home within HOME_ARRIVE. */
#define CAPTAIN_KNOCK_SPEED 0x96
#define CAPTAIN_SENTRY_RANGE 0x1400
#define CAPTAIN_NOTICE_RANGE 0x1800
#define CAPTAIN_NOTICE_DZ 0x320
#define CAPTAIN_PATROL_SPEED 0x78
#define CAPTAIN_NODE_ARRIVE 0x100
#define CAPTAIN_LOOKOUT_SPEED 0x5A
#define CAPTAIN_LOOKOUT_RANGE 0xC00
#define CAPTAIN_LEDGE_RANGE 0x2800
#define CAPTAIN_ADVANCE_SPEED 0x96
#define CAPTAIN_LEASH 0x2800
#define CAPTAIN_BASH_RANGE 0x7D0
#define CAPTAIN_BASH_SPEED 0x8C
#define CAPTAIN_BASH_VZ 0x19
#define CAPTAIN_STALK_SPEED 0x6E
#define CAPTAIN_STALK_LEASH 0x2000
#define CAPTAIN_STALK_REST 0xF0
#define CAPTAIN_SPAR_MIN_FRAMES 0x96
#define CAPTAIN_SPAR_MAX_FRAMES 0x15E
#define CAPTAIN_STRIKE_RANGE 0x1000
#define CAPTAIN_STRIKE_DZ 0x384
#define CAPTAIN_STRIKE_REST 0xB4
#define CAPTAIN_STRIKE_HIT_RANGE 0xED8
#define CAPTAIN_REST_MIN_FRAMES 0x3C
#define CAPTAIN_REST_MAX_FRAMES 0x78
#define CAPTAIN_FIGHT_LEAVE 0x1C00
#define CAPTAIN_HOME_SPEED 0x6E
#define CAPTAIN_HOME_ARRIVE 0x80
/* CaptainState.step on a ledge post instead: nonzero once armed. */
#define CAPTAIN_LEDGE_ARMED 1
#define CAPTAIN_BURST_SPARK_LIFE 0x46 /* BurstActorWithSound spark life */

/* ->mode of Doctor Shemp's guard (ACTOR_TYPE_SHEMP_GUARD, ShempGuardState;
 * level 10). A charge, or a flame from outside its front, knocks it away. */
enum ShempGuardMode {
  SGUARD_IDLE = 0,       /* faces Spyro; in range (or cued): ROUSE / CHASE */
  SGUARD_CHASE = 1,      /* runs at Spyro; lined up and close: RUSH */
  SGUARD_ROUSE = 2,      /* plays the alert, then CHASE */
  SGUARD_KNOCKED = 3,    /* knocked away, then bursts */
  SGUARD_RUSH = 4,       /* straight-line rush; butts Spyro back, then BRAKE_AIM */
  SGUARD_BRAKE_AIM = 5,  /* aims at the nearest path point */
  SGUARD_BRAKE = 6,      /* skids to a stop, turning to it */
  SGUARD_RECOVER = 7,    /* catches its breath, then GUARD */
  SGUARD_GUARD = 8,      /* holds its ground facing Spyro; close: SWIPE */
  SGUARD_SWIPE = 9,      /* swipe, back to GUARD */
  SGUARD_FALL = 10,      /* ran off a ledge: drops its loot on the path, despawns */
  SGUARD_PATH_CHASE = 20, /* runs ShempGuardState.target's path, then CHASE */
  SGUARD_PATH_RUSH = 30, /* rushes along ShempGuardState.target's path, then BRAKE_AIM */
};

/* A Shemp guard's animations besides the poses equal to its mode (CHASE 1,
 * KNOCKED 3). */
enum ShempGuardAnim {
  SGUARD_ANIM_IDLE = 0,   /* SGUARD_RECOVER catches its breath on it */
  SGUARD_ANIM_STANCE = 4, /* SGUARD_GUARD */
  SGUARD_ANIM_SWIPE = 5,
  SGUARD_ANIM_FALL = 6,
};

/* A Shemp guard is hurt by a charge, or a flame from behind (Spyro outside
 * BACK_ARC of its facing) unless it is chasing; it is knocked off at
 * KNOCK_SPEED / KNOCK_VZ. It wakes for Spyro in its range and within
 * NOTICE_DZ; a holder rushes its path once Spyro, in its HOLD_CONE, is within
 * HOLD_RANGE of path node 3. */
#define SGUARD_BACK_ARC 0x55
#define SGUARD_KNOCK_SPEED 0x15E
#define SGUARD_KNOCK_VZ 0xA0
#define SGUARD_NOTICE_DZ 0x800
#define SGUARD_HOLD_CONE 0x40
#define SGUARD_HOLD_RANGE 0x898
/* Chasing it gains RAMP_ACCEL a frame up to RAMP_SPEED and rushes once
 * lined up within RUSH_RANGE. A rush butts Spyro back (BUTT_VZ up) within
 * BUTT_RANGE, BUTT_DZ and BUTT_CONE, and brakes once he is off its heading
 * by BRAKE_ANGLE. Braking loses BRAKE_DRAG a frame (steering only after the
 * first BRAKE_AIM_DELAY frames), then it rests RECOVER_FRAMES. */
#define SGUARD_RAMP_ACCEL 0x14
#define SGUARD_RAMP_SPEED 0xC8
#define SGUARD_RUSH_RANGE 0xC00
#define SGUARD_BUTT_RANGE 0x4B0
#define SGUARD_BUTT_DZ 0x1F4
#define SGUARD_BUTT_CONE 0x14
#define SGUARD_BUTT_VZ 0x6E
#define SGUARD_BRAKE_ANGLE 0x41
#define SGUARD_BRAKE_FRAMES 0xB4
#define SGUARD_BRAKE_AIM_DELAY 0xF
#define SGUARD_BRAKE_DRAG 0x10
#define SGUARD_RECOVER_FRAMES 0x3C
/* Over a drop of LEDGE_DROP (BRAKE: BRAKE_LEDGE_DROP or unflat ground) it
 * falls: at least FALL_MIN_SPEED, FALL_VZ up, despawning after FALL_FRAMES
 * and leaving its drop on the nearest path point. */
#define SGUARD_LEDGE_DROP 0x1F4
#define SGUARD_BRAKE_LEDGE_DROP 0x320
#define SGUARD_FALL_MIN_SPEED 0x50
#define SGUARD_FALL_VZ 0x28
#define SGUARD_FALL_FRAMES 0x3C
/* GUARD swipes within SWIPE_RANGE, then waits SWIPE_REST. Path runs go at
 * PATH_CHASE_SPEED / PATH_RUSH_SPEED. */
#define SGUARD_SWIPE_RANGE 0x578
#define SGUARD_SWIPE_REST 0x78
#define SGUARD_PATH_CHASE_SPEED 0xA0
#define SGUARD_PATH_RUSH_SPEED 0xB4
#define SGUARD_BURST_SPARK_LIFE 0x18 /* BurstActorWithSound spark life */
#define SGUARD_KNOCK_DRAG 0xC /* DriveActorTrackedMove friction */
#define SGUARD_KNOCK_GRAVITY 0xC /* DriveActorTrackedMove gravity */

/* ->mode of a path runner (ACTOR_TYPE_PATH_RUNNER..+1, PathRunnerState;
 * level 21). PathRunnerState.mode picks which of the two runs it makes. */
enum RunnerMode {
  RUNNER_RUN = 0,      /* runs its path ahead of Spyro; raises its goal flag near the end */
  RUNNER_PROD = 1,     /* turns to PathRunnerState.target and wakes it, then RUN */
  RUNNER_KNOCKED = 2,  /* flamed or charged: knocked away, then bursts */
  RUNNER_WAIT = 3,     /* at its goal, facing Spyro; Spyro closes in facing it: FLEE */
  RUNNER_FLEE = 4,     /* runs its home path, kicking up sparks at speed */
  RUNNER_DODGE = 5,    /* circles its path point away from Spyro */
  RUNNER_CREEP = 6,    /* edges back toward Spyro's side */
  RUNNER_TAUNT = 7,    /* faces Spyro; close or charging: DODGE */
  RUNNER_LOOKOUT = 10, /* second run done: watches until Spyro climbs up */
  RUNNER_RETURN = 11,  /* walks to home node 0, then WAIT */
  RUNNER_LURK = 20,    /* waits for Spyro to enter PathRunnerState.volA, then RUN */
};

/* A path runner's animations besides the poses equal to its mode. */
enum RunnerAnim {
  RUNNER_ANIM_STAND = 4, /* WAIT, TAUNT, LOOKOUT and LURK face Spyro on it; voices 6/5 on frames 1/0xA */
};

/* ->mode of a swooping guard (ACTOR_TYPE_SWOOPER, SwoopState; levels 7/8).
 * SwoopState.alert is shared: one swoops at a time. */
enum SwoopMode {
  SWOOP_PERCH = 0,   /* hovers at home; Spyro in range, in sight, facing it: SWOOP */
  SWOOP_DIVE = 1,    /* weaves in at Spyro; a hit, a miss or losing him: RETURN */
  SWOOP_RETURN = 2,  /* flies back home, then PERCH */
  SWOOP_KNOCKED = 4, /* flamed or charged: drifts down and bursts */
};

/* A swooper's animations. */
enum SwoopAnim {
  SWOOP_ANIM_LAUNCH = 1,  /* starts the dive; it holds still until frame 0xB */
  SWOOP_ANIM_FLY = 2,     /* after the launch, and on the way home */
  SWOOP_ANIM_KNOCKED = 4,
};

/* SwoopState.kind: how a dive is flown. 0 and 1 are fixed weave sides and
 * are copied straight into SwoopState.side (0 sways the heading negative,
 * 1 positive), so they stay literal. */
enum SwoopKind {
  SWOOP_KIND_RANDOM = -1, /* each dive picks side 0 or 1 at random */
  SWOOP_KIND_STRAIGHT = 2, /* no weave; dives to Spyro's height and turns back sharply */
};

/* A perched swooper dives at Spyro once he is in its range but at least
 * MIN_RANGE away, within SIGHT_DZ of its height, facing it (within
 * SPY_FACING_CONE) with it facing him (FACING_CONE) and in sight, for
 * SIGHT_FRAMES in a row (any check failing restarts the count). */
#define SWOOP_MIN_RANGE 0x7D1
#define SWOOP_SIGHT_DZ 0xBB8
#define SWOOP_SPY_FACING_CONE 0x19
#define SWOOP_FACING_CONE 0x30
#define SWOOP_SIGHT_FRAMES 0x28
/* SWOOP_DIVE: speeds up by DIVE_ACCEL a frame to DIVE_SPEED, flying
 * DIVE_CLEARANCE over the ground (a STRAIGHT one STRAIGHT_CLEARANCE over
 * Spyro) and changing height CLIMB_STEP a frame; it weaves within
 * WEAVE_RANGE. It hits Spyro within HIT_RANGE, HIT_CONE of its heading and
 * HIT_DZ of his height, and turns back past range + LEASH_SLACK from home. */
#define SWOOP_DIVE_SPEED 0xAA
#define SWOOP_DIVE_ACCEL 0xA
#define SWOOP_DIVE_CLEARANCE 0x226
#define SWOOP_STRAIGHT_CLEARANCE 0x12C
#define SWOOP_CLIMB_STEP 0x78
#define SWOOP_WEAVE_RANGE 0x4000
#define SWOOP_HIT_RANGE 0x578
#define SWOOP_HIT_CONE 0x28
#define SWOOP_HIT_DZ 0x258
#define SWOOP_LEASH_SLACK 0x800
/* SWOOP_RETURN / PERCH change height RETURN_CLIMB_STEP a frame; home within
 * HOME_ARRIVE. Knocked, it slides from KNOCK_SPEED (losing KNOCK_DRAG a
 * frame), sinking KNOCK_SINK a frame. */
#define SWOOP_RETURN_CLIMB_STEP 0x64
#define SWOOP_HOME_ARRIVE 0x80
#define SWOOP_KNOCK_SPEED 0x12C
#define SWOOP_KNOCK_DRAG 0x10
#define SWOOP_KNOCK_SINK 0x32
/* More of SWOOP_DIVE: it finds the ground from DIVE_PROBE_RAISE up within
 * DIVE_PROBE_DEPTH, snapping to its target height within CLIMB_SNAP. It sways
 * up to SWAY_MAX either side by SWAY_STEP a frame (SWAY_STEP_MID within
 * SWAY_MID_RANGE, SWAY_STEP_NEAR within SWAY_NEAR_RANGE). It gives up once
 * past Spyro (within PASS_RANGE, PASS_ARC off its heading), once unseen
 * with him facing PASS_LOST_ARC away, on hitting a wall (radius
 * MOVE_RADIUS), or once SIGHT_GRACE ticks in he is out of sight within
 * SIGHT_RANGE. It pitches with the ground between probes PITCH_PROBE_BACK
 * behind and 1/8 unit ahead (within PITCH_PROBE_DEPTH; slopes under
 * PITCH_MAX_STEP over PITCH_BASE, eased PITCH_EASE toward level). */
#define SWOOP_DIVE_PROBE_RAISE 0x5DC
#define SWOOP_DIVE_PROBE_DEPTH 0x1000
#define SWOOP_CLIMB_SNAP 0x47
#define SWOOP_SWAY_MAX 0x23
#define SWOOP_SWAY_STEP 0xC
#define SWOOP_SWAY_STEP_MID 0x18
#define SWOOP_SWAY_STEP_NEAR 0x20
#define SWOOP_SWAY_MID_RANGE 0x1770
#define SWOOP_SWAY_NEAR_RANGE 0xFA0
#define SWOOP_PASS_RANGE 0x640
#define SWOOP_PASS_ARC 0x3D
#define SWOOP_PASS_LOST_ARC 0x51
#define SWOOP_MOVE_RADIUS 0x15E
#define SWOOP_SIGHT_GRACE 0x3D
#define SWOOP_SIGHT_RANGE 0x2710
#define SWOOP_PITCH_PROBE_BACK 175
#define SWOOP_PITCH_PROBE_DEPTH 0x800
#define SWOOP_PITCH_MAX_STEP 0x200
#define SWOOP_PITCH_BASE 0x578
#define SWOOP_PITCH_EASE 0x20
/* SWOOP_RETURN aims RETURN_RAISE above home (beyond RETURN_FAR from home,
 * RETURN_FAR_RAISE above Spyro), holding within RETURN_SNAP; within
 * RETURN_SLOW_RANGE it slows by 5 a frame down to RETURN_MIN_SPEED. A
 * knocked swooper sinks while over a floor found KNOCK_PROBE_RAISE up, and
 * bursts with BURST_SPARK_LIFE sparks. */
#define SWOOP_RETURN_RAISE 0x200
#define SWOOP_RETURN_FAR 0x1800
#define SWOOP_RETURN_FAR_RAISE 0x800
#define SWOOP_RETURN_SNAP 0x3D
#define SWOOP_RETURN_SLOW_RANGE 0xBB8
#define SWOOP_RETURN_MIN_SPEED 0x33
#define SWOOP_KNOCK_PROBE_RAISE 0x3E8
#define SWOOP_KNOCK_PROBE_DEPTH 0x800
#define SWOOP_BURST_SPARK_LIFE 0x18

/* Per-type mesh voice indices: PlayActorMeshVoice's voiceOffset and the
 * matching m_Sounds[] slot of the type's mesh record. */
enum SparxVoice {
  SPARX_VOICE_BUZZ = 0, /* his only voice: on reappearing and on setting off for a pickup */
};
enum StatueVoice {
  STATUE_VOICE_RATTLE = 0, /* STATUE_IDLE's periodic shake */
};
enum ChestVoice {
  WOODCHEST_VOICE_BREAK = 0,   /* ACTOR_TYPE_WOOD_CHEST struck */
  WOBBLECHEST_VOICE_BREAK = 0, /* ACTOR_TYPE_WOBBLE_CHEST charged */
  LOCKEDCHEST_VOICE_OPEN = 0,  /* ACTOR_TYPE_LOCKED_CHEST opened by its key */
  SPINCHEST_VOICE_LAUNCH = 0,  /* ACTOR_TYPE_SPIN_CHEST spun fast enough: the lid flies */
  SPINLID_VOICE_BURST = 0,     /* ACTOR_TYPE_SPIN_CHEST_LID bursting */
};
/* A broken wood / wobble chest throws SHARD_PAIRS shard pairs and CHIPS
 * chips (sparkle shards for the wobble chest). */
#define WOODCHEST_SHARD_PAIRS 2
#define WOODCHEST_CHIPS 6
#define WOBBLECHEST_SHARD_PAIRS 2
#define WOBBLECHEST_CHIPS 6
enum BreakableVoice {
  BROCK_VOICE_BREAK = 0,     /* ACTOR_TYPE_BREAKABLE_ROCK supercharged */
  CBOULDER_VOICE_CRACK = 0,  /* ACTOR_TYPE_CRACKING_BOULDER: sparks while it cracks */
  CBOULDER_VOICE_BURST = 1,  /* its timer out: it bursts into falling debris */
};
enum CannonVoice {
  CANNON_VOICE_FUSE = 0, /* CANNON_FUSE, while it smokes */
  CANNON_VOICE_FIRE = 1, /* the shot, entering CANNON_FLIGHT */
};
enum ShempGuardVoice {
  SGUARD_VOICE_FALL = 0,  /* ran off a ledge: entering SGUARD_FALL */
  SGUARD_VOICE_SHOUT = 1, /* while it chases or rushes (and on the never-met ShempGuardState.watch cue) */
};
enum SpinChestVoice {
  SPINCHEST_VOICE_SPIN = 1, /* played on each spin-up hit, pitched up by its speed */
};
enum PressSwitchVoice {
  PSWITCH_VOICE_PRESS = 0, /* pressed in sequence, pitched by its place in it */
};
enum FlightTargetVoice {
  /* The flight-level targets', chests' and rings' single voice. */
  FTARGET_VOICE_BURST = 0, /* flamed or charged (the bobber also loops it while FTARGET_ACTIVE) */
  FCHEST_VOICE_BURST = 0,  /* ACTOR_TYPE_FLIGHT_CHEST struck: frees its reward */
  FCHEST_VOICE_HUM = 1,    /* its ambient hum while intact */
  FRING_VOICE_SPIN = 0,    /* ACTOR_TYPE_FLIGHT_RING, once per turn of its spin */
};
enum KnockedVoice {
  /* Voice 0 of an enemy knocked out by a charge or flame (with its death drop). */
  WARDRUM_VOICE_KNOCKED = 0,   /* ACTOR_TYPE_WAR_DRUM */
  WARDRUM_VOICE_BEAT = 1,      /* its WARDRUM_BEAT strike, frame 6 (resumes its chunk anim) */
  BGUARD_VOICE_KNOCKED = 0,    /* ACTOR_TYPE_BOLT_GUARD */
  FGUARD_VOICE_KNOCKED = 0,    /* ACTOR_TYPE_FENCE_GUARD */
  SHEPHERD_VOICE_KNOCKED = 0,  /* ACTOR_TYPE_SHEPHERD */
  KEEPER_VOICE_KNOCKED = 0,    /* ACTOR_TYPE_TETHER_KEEPER */
  LTHIEF_VOICE_KNOCKED = 0,    /* ACTOR_TYPE_LOOT_THIEF */
  SINKBLK_VOICE_FREED = 0,     /* the sinking block's partner, freed by it */
  BTHROW_VOICE_LAND = 0,       /* ACTOR_TYPE_BOULDER_THROWER(_B): knocked, comes to rest */
  TPRUNNER_VOICE_LAND = 0,     /* ACTOR_TYPE_TWO_PATH_RUNNER: knocked, first landing */
  GUNNER_VOICE_LAND = 0,       /* ACTOR_TYPE_GUNNER: knocked, first landing */
};
enum ProjectileVoice {
  /* Voice 0 of a shot, rock or bomb as it hits. */
  SHOT_VOICE_BURST = 0,      /* ACTOR_TYPE_FIREBALL hits the world or an actor */
  LGLOB_VOICE_BURST = 0,     /* ACTOR_TYPE_LOB_GUNNER_LOB bursts */
  TROCK_VOICE_BREAK = 0,     /* ACTOR_TYPE_THROWN_ROCK hits Spyro or the world, or times out */
  SNOWBALL_VOICE_HIT = 0,    /* ACTOR_TYPE_THROWN_SNOWBALL hits Spyro */
  TRAILBOMB_VOICE_HIT = 0,   /* ACTOR_TYPE_TRAIL_BOMB lands or times out */
  FCHUNK_VOICE_BURST = 0,    /* ACTOR_TYPE_FALLING_CHUNK bursts into rubble */
};
enum BreakWallVoice {
  BWALL_VOICE_BREAK = 0,         /* ACTOR_TYPE_BREAKABLE_WALL */
  BWALL_VOICE_BREAK_ARMORED = 1, /* the same, for BWALL_ARMORED */
};
enum FloatBarrelVoice {
  /* ACTOR_TYPE_FLOATING_BARREL / FLOATING_MINE. */
  FLOATBARREL_VOICE_BURST = 0,
  FLOATBARREL_VOICE_RUN = 1,     /* the mine set running by a charge */
  FLOATBARREL_VOICE_TUMBLE = 2,  /* looped while subMode is set: it tumbles (rotX) and sinks */
};
enum StandVoice {
  /* Random idle voices (one in four) on a stand or taunt anim's frames,
   * as THIEF_VOICE_TAUNT_EARLY/LATE. */
  RUNNER_VOICE_STAND_EARLY = 6,   /* ACTOR_TYPE_PATH_RUNNER + 1: RUNNER_ANIM_STAND frame 1 */
  RUNNER_VOICE_STAND_LATE = 5,    /* ... frame 0xA */
  LANEPAT_VOICE_STAND_EARLY = 6,  /* ACTOR_TYPE_LANE_PATROLLER: LANEPAT_WATCH pose, frame 1 */
  LANEPAT_VOICE_STAND_LATE = 5,   /* ... frame 0xA */
  LCRITTER_VOICE_TAUNT_EARLY = 6, /* ACTOR_TYPE_LOOT_CRITTER: LCRITTER_WATCH pose, frame 2 */
  LCRITTER_VOICE_TAUNT_LATE = 5,  /* ... frame 0xC */
  GRAZER_VOICE_CALL = 3,          /* ACTOR_TYPE_GRAZING_CRITTER, every 0x78..0xB7 ticks of GRAZER_WANDER */
};
enum SingerVoice {
  SINGER_VOICE_SING = 0,     /* struck: starts singing (SINGER_SING) */
  SINGER_VOICE_SING_END = 1, /* the song's timer out: SINGER_SING_END */
};
enum LungerVoice {
  LUNGER_VOICE_LAND = 0, /* first touch of the floor */
  LUNGER_VOICE_LEAP = 1, /* turned to its target: LUNGER_AIM -> LUNGER_LEAP */
};
enum GrabberVoice {
  GRABBER_VOICE_HIT = 1,    /* struck: GRABBER_KNOCKED */
  GRABBER_VOICE_EMERGE = 2, /* while GRABBER_ANIM_EMERGE plays */
};
enum RimRiderVoice {
  RIMRIDER_VOICE_DROPPED = 0, /* dropped by Blowhard (BLOWHARD_ANIM_DROP_RIDER) */
  RIMRIDER_VOICE_SPAWN_HELPER = 1, /* spawns its ACTOR_TYPE_RIM_RIDER_HELPER */
  RIMHELPER_VOICE_BEAT = 0,   /* the helper, once its beat timer runs out */
};
enum ShooterVoice {
  FBCANNON_VOICE_FUSE = 0,  /* ACTOR_TYPE_FIREBALL_CANNON lit: FBCANNON_FUSE */
  CCANNON_VOICE_FIRE = 1,   /* ACTOR_TYPE_COMET_CANNON fires a comet */
  CATAPULT_VOICE_SWING = 0, /* ACTOR_TYPE_CATAPULT, each tick its arm is raised */
  GNASTY_VOICE_THROW = 0,   /* Gnasty Gnorc throws */
  LGUARD_VOICE_GROWL = 0,   /* ACTOR_TYPE_BIG_LUNGING_GUARD: lunging, on frame 6 */
};
enum MiscVoice {
  FCSWITCH_VOICE_LIT = 0,     /* ACTOR_TYPE_FLAME_CHUNK_SWITCH flamed */
  SFGIVER_VOICE_CAST_END = 0, /* superflame giver: the cast anim done, SFGIVER_SUB_LINGER */
  PPOD_VOICE_BURST = 0,       /* ACTOR_TYPE_PLATFORM_POD popped */
  PPOD_VOICE_RETRACT = 1,     /* its timer out: the platforms despawn */
};

/* A level 7/8 bell's animations (ACTOR_TYPE_BELL_A/B/C). A flame rings it
 * once per cooldown (subMode 0x50); any other hit only wobbles it. */
enum BellAnim {
  BELL_ANIM_REST = 0,
  BELL_ANIM_WOBBLE = 1, /* non-flame hit while at rest, then REST */
  BELL_ANIM_SWING = 2,  /* flamed: spawns a ACTOR_TYPE_BELL_RIPPLE when done, then RING */
  BELL_ANIM_RING = 3,   /* rings on until the cooldown reaches 2 */
  BELL_ANIM_SETTLE = 4, /* cooldown at 2: settles, then REST */
};

/* ->mode of a villager (ACTOR_TYPE_VILLAGER, VillagerState; levels 12/13).
 * VILLAGER_IDLE runs VillagerState.mode's role (talker, lookout, paired,
 * follower of a leader); Spyro too close sends it running. */
enum VillagerMode {
  VILLAGER_IDLE = 0,
  VILLAGER_KNOCKED = 3, /* flamed or charged: knocked away; bursts once its chunk anim is done */
  VILLAGER_PANIC = 90,  /* cowers, then RUN */
  VILLAGER_RUN = 91,    /* runs between random path points; far enough: IDLE */
  VILLAGER_STOP = 92,   /* catches its breath facing Spyro; he comes closer: PANIC */
};

/* A villager's animations besides the poses equal to its mode (IDLE 0,
 * KNOCKED 3). */
enum VillagerAnim {
  VILLAGER_ANIM_CHATTER = 4, /* talking (chunk anim past step 1) and VILLAGER_STOP; voices 4/5 */
  VILLAGER_ANIM_COWER = 7,   /* VILLAGER_PANIC, then RUN */
  VILLAGER_ANIM_RUN = 8,
  VILLAGER_ANIM_REACT = 9,   /* VROLE_LEADER: cued at the leader's animation 4, frame 8; back to IDLE at frame 0x14 */
};

/* A villager Spyro faces (within CAM_FACING_CONE) inside its camDist pulls
 * the camera to CAM_DISTANCE behind Spyro. A talker's range grows by
 * TALK_BOOST unless Spyro is charging at a noBoost one, and it stops once he
 * is TALK_SLACK past it. A pair partner wakes within PAIR_WAKE_RANGE and
 * flees once its partner is down and Spyro is within flee range +
 * PAIR_FLEE_SLACK. */
#define VILLAGER_CAM_FACING_CONE 0x20
#define VILLAGER_CAM_DISTANCE 0xC00
#define VILLAGER_TALK_BOOST 0xC00
#define VILLAGER_TALK_SLACK 0x800
#define VILLAGER_PAIR_WAKE_RANGE 0x3400
#define VILLAGER_PAIR_FLEE_SLACK 0x7D0
/* VILLAGER_PANIC sends it running at PANIC_SPEED for RUN_MIN..MAX_FRAMES;
 * VILLAGER_RUN picks a new path point within NODE_ARRIVE at
 * RUN_MIN..MAX_SPEED and calms down past flee range + CALM_SLACK. Stopped, it
 * panics again once Spyro is STOP_SLACK closer than when it stopped.
 * Knocked, it slides from KNOCK_SPEED, losing KNOCK_DRAG a frame. */
#define VILLAGER_PANIC_SPEED 0x5F
#define VILLAGER_RUN_MIN_FRAMES 0xF0
#define VILLAGER_RUN_MAX_FRAMES 0x168
#define VILLAGER_NODE_ARRIVE 0x96
#define VILLAGER_RUN_MIN_SPEED 0x3C
#define VILLAGER_RUN_MAX_SPEED 0x6E
#define VILLAGER_CALM_SLACK 0x1388
#define VILLAGER_STOP_SLACK 0x80
#define VILLAGER_KNOCK_SPEED 0xFA
#define VILLAGER_KNOCK_DRAG 0x14
#define VILLAGER_BURST_SPARK_LIFE 0x18 /* BurstActorWithSound spark life */

/* VillagerState.mode: the villager's role. */
enum VillagerRole {
  VROLE_CHAT = 0,   /* faces Spyro; talks (its chunk anim) when he is in talk range */
  VROLE_FACE = 1,   /* faces its friend; Spyro too close: PANIC */
  VROLE_PAIR = 2,   /* waits on its partner (link), cueing it via subMode */
  VROLE_LEADER = 3, /* held by its link, a caster sorcerer: at the sorcerer's path node 7
                     * sends it to SORCERER_STEP_REST */
};

/* ->mode of a post guard (ACTOR_TYPE_POST_GUARD, PostState; level 2).
 * POSTGUARD_IDLE runs PostState.mode's role (waits between its two posts,
 * patrols, lookout, or ambushes with its partner). */
enum PostGuardMode {
  POSTGUARD_IDLE = 0,
  POSTGUARD_HOP = 1,      /* faces Spyro, hopping at him when he is close and level */
  POSTGUARD_KNOCKED = 2,  /* flamed or charged: spins away, then bursts */
  POSTGUARD_TO_POST = 3,  /* steps out to the post nearer Spyro, then HOP */
  POSTGUARD_ALERTED = 20, /* set group-wide by a patroller: turns to Spyro */
  POSTGUARD_SHOUT = 21,   /* shouts, then HOP */
};

/* PostState.sub during POSTGUARD_HOP. */
enum PostGuardHop {
  POSTGUARD_HOP_REST = 0,  /* animation 4 with PostState.flag set, until its timer and the
                            * animation run out */
  POSTGUARD_HOP_READY = 1, /* animation 5; lunges (animation 6) when its timer is out and
                            * Spyro is close and level, back to REST when 5 ends */
};

/* PostState.mode: what a post guard does while POSTGUARD_IDLE (from its
 * spawn record). */
enum PostGuardPost {
  POSTGUARD_POST_PICK = 0,   /* Spyro near one of its two posts: TO_POST that one */
  POSTGUARD_POST_PATROL = 1, /* walks its path; Spyro near: alerts the group (ALERTED) */
  POSTGUARD_POST_WATCH = 2,  /* faces Spyro; close: HOP */
  POSTGUARD_POST_AMBUSH = 3, /* PostState.phase: waits for Spyro, then runs its path at him */
};

/* PostState.phase on POSTGUARD_POST_AMBUSH. */
enum PostGuardAmbush {
  POSTGUARD_AMBUSH_WAIT = 0,  /* Spyro near, grounded and above it: cues itself and its partner */
  POSTGUARD_AMBUSH_DELAY = 1, /* random stagger (the partner's), then ROUSE */
  POSTGUARD_AMBUSH_ROUSE = 2, /* rouse animation */
  POSTGUARD_AMBUSH_RUN = 3,   /* runs its path to the end */
  POSTGUARD_AMBUSH_FACE = 4,  /* turns to Spyro, then HOP */
};

/* A post guard's animations, from the modes that play them. */
enum PostGuardAnim {
  POSTGUARD_ANIM_STAND = 0,
  POSTGUARD_ANIM_RUN = 1,
  POSTGUARD_ANIM_KNOCKED = 2,     /* the spin; KNOCKED_END follows it */
  POSTGUARD_ANIM_KNOCKED_END = 3,
  POSTGUARD_ANIM_REST = 4,        /* POSTGUARD_HOP_REST */
  POSTGUARD_ANIM_READY = 5,       /* POSTGUARD_HOP_READY */
  POSTGUARD_ANIM_LUNGE = 6,
  POSTGUARD_ANIM_SHOUT = 7,       /* the ambush rouse and POSTGUARD_SHOUT */
};

/* Post guard tuning. Hit it spins off at KNOCK_SPEED / KNOCK_VZ for
 * KNOCK_FRAMES (a charge: CHARGE_KNOCK_*). A picker steps to the post Spyro
 * comes within POST_RANGE of (unless he is in line with it, within
 * POST_CONE, and nearer than it), running TO_POST_SPEED until within
 * POST_ARRIVE (or Spyro within POST_STOP_RANGE). A patroller walks
 * PATROL_SPEED and alerts its group within PATROL_ALERT_RANGE; a watcher
 * hops within WATCH_RANGE; an ambusher wakes within AMBUSH_RANGE and runs at
 * AMBUSH_SPEED. Hopping, it rests REST_MIN..MAX_FRAMES and lunges within
 * LUNGE_RANGE / LUNGE_DZ (then waits LUNGE_REST), giving up past
 * HOP_LEAVE_RANGE. */
#define POSTGUARD_KNOCK_SPEED 0x118
#define POSTGUARD_KNOCK_VZ 0x8C
#define POSTGUARD_KNOCK_FRAMES 0x28
#define POSTGUARD_CHARGE_KNOCK_SPEED 0x15E
#define POSTGUARD_CHARGE_KNOCK_VZ 0xAA
#define POSTGUARD_CHARGE_KNOCK_FRAMES 0x3C
#define POSTGUARD_POST_RANGE 0x1C00
#define POSTGUARD_POST_CONE 0x15
#define POSTGUARD_TO_POST_SPEED 0x8C
#define POSTGUARD_POST_ARRIVE 0x100
#define POSTGUARD_POST_STOP_RANGE 0xC00
#define POSTGUARD_PATROL_SPEED 0x5A
#define POSTGUARD_PATROL_ALERT_RANGE 0x1400
#define POSTGUARD_WATCH_RANGE 0x1800
#define POSTGUARD_AMBUSH_RANGE 0x2000
#define POSTGUARD_AMBUSH_SPEED 0x96
#define POSTGUARD_REST_MIN_FRAMES 0x8C
#define POSTGUARD_REST_MAX_FRAMES 0xC8
#define POSTGUARD_LUNGE_RANGE 0x578
#define POSTGUARD_LUNGE_DZ 0x3E8
#define POSTGUARD_LUNGE_REST 0xB4
#define POSTGUARD_HOP_LEAVE_RANGE 0x2000
#define POSTGUARD_BURST_SPARK_LIFE 0x20 /* BurstActorWithSound spark life */
#define POSTGUARD_KNOCK_DRAG 0xC /* DriveActorTrackedMove friction */
#define POSTGUARD_KNOCK_GRAVITY 0xC /* DriveActorTrackedMove gravity */

/* ->mode of a patrol ward (ACTOR_TYPE_PATROL_WARD, WardState; level 3).
 * WARD_IDLE runs WardState.mode's role (sentry, node patrol, partner ring);
 * its shove (animation 8) knocks Spyro back. */
enum WardMode {
  WARD_IDLE = 0,
  WARD_ADVANCE = 1,    /* walks at Spyro, shoving when close (or rings its partner's node) */
  WARD_TURN = 2,       /* at a node: turns about, picks the next node */
  WARD_KNOCKED = 4,    /* charged: knocked spinning, then DAZED */
  WARD_DAZED = 5,      /* lies dazed; a flame burns it, Spyro leaving bursts it */
  WARD_BURN = 9,       /* flamed: bursts */
  WARD_TO_NODE = 98,   /* walks to its next path node, then TURN */
  WARD_GO_HOME = 100,  /* walks back home, then IDLE */
};

/* WardState.mode: a patrol ward's role (from its spawn record). */
enum WardRole {
  WARD_ROLE_SENTRY = 0, /* faces Spyro; close and level: ADVANCE at him until he turns away or it strays */
  WARD_ROLE_PATROL = 1, /* walks its path nodes (WARD_TO_NODE) */
  WARD_ROLE_RING = 2,   /* circles path node 0 while its link (WardState.link) is live and it stays
                         * near home; then becomes ROLE_PATROL */
};

/* A ward's animations besides the WARD_KNOCKED / WARD_DAZED poses (4, 5),
 * which play the animation equal to the mode. */
enum WardAnim {
  WARD_ANIM_DAZED_STIR = 6, /* between dazed loops; back to WARD_DAZED from frame 0x2C */
  WARD_ANIM_SHOVE = 8,      /* Spyro close and facing it */
};

/* Patrol ward tuning. Its shove lands on Spyro within SHOVE_HIT_RANGE and
 * SHOVE_HIT_CONE (knocked back at 5/256 of the unit vector, SHOVE_VZ up); it
 * starts one within SHOVE_RANGE and SHOVE_CONE, then waits SHOVE_REST
 * (cleared once Spyro is past SHOVE_RESET_RANGE). A sentry advances within
 * ADVANCE_RANGE / ADVANCE_DZ at WALK_SPEED (re-reading Spyro's heading every
 * RETARGET_FRAMES), going home after he looks away (GIVE_UP_FACING) for
 * GIVE_UP_FRAMES or past LEASH; a ring ward leaves its ring past RING_LEASH.
 * Nodes are reached within NODE_ARRIVE; home walks HOME_SPEED. Charged it
 * slides from KNOCK_SPEED (losing KNOCK_DRAG a frame), then lies dazed
 * DAZE_MIN.. +0x3F frames, bursting if Spyro is past DAZE_LEAVE_RANGE. */
#define WARD_SHOVE_HIT_RANGE 0x600
#define WARD_SHOVE_HIT_CONE 0x20
#define WARD_SHOVE_VZ 0x78
#define WARD_SHOVE_RANGE 0x800
#define WARD_SHOVE_CONE 0x40
#define WARD_SHOVE_REST 0x78
#define WARD_SHOVE_RESET_RANGE 0xC00
#define WARD_ADVANCE_RANGE 0x2000
#define WARD_ADVANCE_DZ 0x320
#define WARD_WALK_SPEED 0x64
#define WARD_RETARGET_FRAMES 0x1E
#define WARD_GIVE_UP_FACING 0x31
#define WARD_GIVE_UP_FRAMES 0x28
#define WARD_LEASH 0x2800
#define WARD_RING_LEASH 0x400
#define WARD_NODE_ARRIVE 0x80
#define WARD_HOME_SPEED 0x5A
#define WARD_KNOCK_SPEED 0xFA
#define WARD_KNOCK_DRAG 0xF
#define WARD_DAZE_MIN 0x40
#define WARD_DAZE_LEAVE_RANGE 0x2800
#define WARD_BURST_SPARK_LIFE 0x20 /* BurstActorWithSound spark life */

/* ->mode of a shooter sentry (ACTOR_TYPE_SHOOTER, ShooterState; level 7).
 * Flames bounce off while it hides behind its shield (SHOOTER_SHIELD,
 * frames 4..30). */
enum ShooterMode {
  SHOOTER_IDLE = 0,     /* blinks; Spyro in range: WAKE */
  SHOOTER_FIRE = 1,     /* fires an ACTOR_TYPE_SHOOTER_SHOT at Spyro, then SHIELD */
  SHOOTER_LOWER = 2,    /* drops its shield, then AIM */
  SHOOTER_KNOCKED = 3,  /* knocked away, then bursts */
  SHOOTER_GO_HOME = 4,  /* walks back home, then IDLE */
  SHOOTER_AIM = 6,      /* tracks Spyro; in sight: FIRE, else back to SHIELD */
  SHOOTER_SHIELD = 7,   /* behind its shield until its timer runs out */
  SHOOTER_RISE = 9,     /* plays out, then WAKE */
  SHOOTER_WAKE = 11,    /* wakes and turns to Spyro; ADVANCE if it has a range */
  SHOOTER_ADVANCE = 20, /* walks its path toward Spyro, then SHIELD */
};

/* A shooter's animations. A dozer idles on DOZE for 2..6 loops, then plays
 * BLINK_1, _2, _3 and dozes again. */
enum ShooterAnim {
  SHOOTER_ANIM_IDLE = 0,
  SHOOTER_ANIM_LOWER = 2,   /* drops the shield */
  SHOOTER_ANIM_WALK = 4,    /* SHOOTER_ADVANCE */
  SHOOTER_ANIM_AIM = 6,
  SHOOTER_ANIM_SHIELD = 7,
  SHOOTER_ANIM_DOZE = 8,
  SHOOTER_ANIM_BLINK_1 = 9,
  SHOOTER_ANIM_BLINK_2 = 10,
  SHOOTER_ANIM_BLINK_3 = 12,
};

/* ShooterState.mode: the sentry's role. */
enum ShooterRole {
  SHOOTER_ROLE_DOZER = 0, /* blinks in SHOOTER_IDLE until Spyro comes near; fires inside
                           * SHOOTER_DOZER_FIRE_RANGE */
  SHOOTER_ROLE_GUARD = 1, /* starts behind its shield; ducks while Spyro is
                           * SHOOTER_GUARD_FIRE_RANGE..+SHOOTER_GUARD_DUCK_SPAN away, fires
                           * once he is inside SHOOTER_GUARD_FIRE_RANGE */
};

/* Shooter tuning. Hit it is knocked off at KNOCK_SPEED (CHARGE_KNOCK_SPEED
 * for a charge; KNOCK_VZ up), except a non-charge blow on shield frames
 * 4..0x1E. A dozer wakes within WAKE_RANGE / WAKE_DZ (or its level-data
 * range). Shots leave at SHOT_SPEED aimed SHOT_AIM_DROP under Spyro and live
 * SHOT_LIFE; it then rests FIRE_REST. It walks home at HOME_SPEED (home
 * within HOME_ARRIVE) and advances at ADVANCE_SPEED until Spyro is within
 * ADVANCE_STOP_RANGE. */
#define SHOOTER_KNOCK_SPEED 0x118
#define SHOOTER_CHARGE_KNOCK_SPEED 0x15E
#define SHOOTER_KNOCK_VZ 0x1E
#define SHOOTER_WAKE_RANGE 0x2800
#define SHOOTER_WAKE_DZ 0x3E8
#define SHOOTER_DOZER_FIRE_RANGE 0x2800
#define SHOOTER_GUARD_FIRE_RANGE 0x2000
#define SHOOTER_GUARD_DUCK_SPAN 0x1C00
#define SHOOTER_SHOT_SPEED 0x96
#define SHOOTER_SHOT_AIM_DROP 0x12C
#define SHOOTER_SHOT_LIFE 0x88
#define SHOOTER_FIRE_REST 0x64
#define SHOOTER_HOME_SPEED 0x3C
#define SHOOTER_HOME_ARRIVE 0x100
#define SHOOTER_ADVANCE_SPEED 0x5A
#define SHOOTER_ADVANCE_STOP_RANGE 0x800
#define SHOOTER_BURST_SPARK_LIFE 0x18 /* BurstActorWithSound spark life */
#define SHOOTER_KNOCK_DRAG 0xC /* DriveActorTrackedMove friction */
#define SHOOTER_KNOCK_GRAVITY 0xA /* DriveActorTrackedMove gravity */

/* ->mode of Metalhead, the level 22 boss (ACTOR_TYPE_METALHEAD,
 * MetalheadState). Clearing the current phase's pickup group starts
 * METALHEAD_PHASE_TURN; the run steps through the phase modes with
 * mode++. */
enum MetalheadMode {
  METALHEAD_IDLE = 0,         /* picks FLING / SPRAY / LOB by Spyro's range */
  METALHEAD_FLING = 1,       /* catches the minion that jumped in (subMode MHEAD_CATCH_GO) and
                               * flings it at Spyro; the minion lets go on frame 0xE */
  METALHEAD_SPRAY = 2,        /* sprays ACTOR_TYPE_POD_SHOTs ahead of him */
  METALHEAD_LOB = 3,          /* lobs an ACTOR_TYPE_LOBBED_BOLT at Spyro's spot */
  METALHEAD_PHASE_TURN = 10,  /* turns onto his path (in the last phase: DYING) */
  METALHEAD_PHASE_ROAR = 11,
  METALHEAD_PHASE_STAGGER = 12, /* staggers along the path a while */
  METALHEAD_PHASE_RECOVER = 13,
  METALHEAD_PHASE_RUN = 14,   /* runs to the next phase's path node */
  METALHEAD_PHASE_ARRIVE = 15, /* turns to Spyro, wakes the next group, IDLE */
  METALHEAD_DYING = 20,       /* bursts; sets g_bLevelBossDefeated */
};

/* Metalhead's animations, from the modes that play them. */
enum MetalheadAnim {
  METALHEAD_ANIM_TURN = 4,     /* PHASE_TURN */
  METALHEAD_ANIM_RUN = 5,      /* queued after RECOVER, plays through PHASE_RUN */
  METALHEAD_ANIM_ROAR = 8,
  METALHEAD_ANIM_STAGGER = 9,  /* queued after the roar, plays through PHASE_STAGGER */
  METALHEAD_ANIM_RECOVER = 0xA,
  METALHEAD_ANIM_DYING = 0xB,
  METALHEAD_ANIM_SPRAY = 0xC,
  METALHEAD_ANIM_LOB = 0xD,
};

/* MetalheadState.choice: the attack Metalhead picks while METALHEAD_IDLE
 * (random: 60% fling, 25% spray, 15% lob; spray or lob only when a fling
 * is not possible). Back to NONE when its pose ends or the pick's range
 * test fails. */
enum MetalheadAttack {
  METALHEAD_ATTACK_NONE = 0,
  METALHEAD_ATTACK_FLING = 1, /* needs a live minion in the phase's group and Spyro at least
                               * METALHEAD_FLING_MIN away and level: raises MHEAD_CATCH_READY until a
                               * minion jumps in, then METALHEAD_FLING */
  METALHEAD_ATTACK_SPRAY = 2, /* METALHEAD_SPRAY */
  METALHEAD_ATTACK_LOB = 3,   /* METALHEAD_LOB */
};

/* Metalhead's ->subMode: the catch handshake with the boulder throwers and
 * gatekeepers of his phase group (their *_RIDE_FACE modes). */
enum MetalheadCatch {
  MHEAD_CATCH_NONE = 0,
  MHEAD_CATCH_READY = 1, /* Spyro is in fling range: a facing minion may jump */
  MHEAD_CATCH_GO = 2,    /* set by the minion as it jumps: enter METALHEAD_FLING */
};

/* Metalhead's tuning. While attacking, the follow camera holds on him with
 * Spyro within CAM_DZ and CAM_RANGE (CAM_RANGE_LATE after phase 0), off for
 * CAM_HOLD_FRAMES after a charge; within SEEN_RANGE the arena chunk starts.
 * Attacks: FLING (60 in 100, only with a live minion, Spyro over FLING_MIN
 * away and within FLING_PICK_DZ; flung within the phase range and FLING_DZ),
 * SPRAY (25 in 100, SPRAY_MIN..ATTACK_MAX away) and LOB (the rest,
 * LOB_MIN..ATTACK_MAX), both within ATTACK_DZ; each ends with ATTACK_REST
 * frames idle. */
#define METALHEAD_CAM_DZ 0x578
#define METALHEAD_CAM_RANGE 0x6400
#define METALHEAD_CAM_RANGE_LATE 0x6000
#define METALHEAD_CAM_HOLD_FRAMES 0x3C
#define METALHEAD_SEEN_RANGE 0x6800
#define METALHEAD_FLING_MIN 0x2000
#define METALHEAD_FLING_PICK_DZ 0x385
#define METALHEAD_FLING_DZ 0x320
#define METALHEAD_FLING_ODDS 0x3C
#define METALHEAD_SPRAY_ODDS_END 0x55
#define METALHEAD_SPRAY_MIN 0x800
#define METALHEAD_LOB_MIN 0x2800
#define METALHEAD_ATTACK_MAX 0x7000
#define METALHEAD_ATTACK_DZ 0x3E8
#define METALHEAD_ATTACK_REST 0x50
/* SPRAY fires a pod shot each frame of 5..0x14, aimed SPRAY_REACH (+150 a
 * shot, ± SPRAY_SCATTER) ahead; shots live SHOT_LIFE. LOB throws on frames
 * 4..8 every 7 frames: bolts live BOLT_LIFE at BOLT_SPEED + distance / 256. */
#define METALHEAD_SPRAY_REACH 3000
#define METALHEAD_SPRAY_SCATTER 0x320
#define METALHEAD_SHOT_LIFE 0xB4
#define METALHEAD_BOLT_LIFE 0xF0
#define METALHEAD_BOLT_SPEED 0x190
/* Between phases he staggers for STAGGER_FRAMES at STAGGER_SPEED, then runs
 * at RUN_SPEED to the next phase's node. */
#define METALHEAD_STAGGER_FRAMES 0x46
#define METALHEAD_STAGGER_SPEED 0x78
#define METALHEAD_RUN_SPEED 0x118

/* ->mode of a lunger (ACTOR_TYPE_LUNGER, LungeState; level 4). It takes
 * LungeState.hits hits (each one switches its animation set, anim += 7)
 * before a flame knocks it away; hits are ignored while it recoils. */
enum LungerMode {
  LUNGER_IDLE = 0,       /* waits; Spyro close: AIM at his spot */
  LUNGER_AIM = 1,        /* turns to the target point, then LEAP */
  LUNGER_LEAP = 2,       /* leaps at the point, pinning Spyro under it; then GO_HOME */
  LUNGER_KNOCKED = 4,    /* last hit: knocked away, then bursts */
  LUNGER_BURST = 10,     /* bursts and drops its loot (unreachable: no writer in the C sources, and level data starts every one in mode 0) */
  LUNGER_RECOIL = 13,    /* flinches from a hit, re-aims, then AIM */
  LUNGER_RECOIL_B = 14,  /* recoil variants (unreachable: no writer in the C sources, and level data starts every one in mode 0) */
  LUNGER_RECOIL_C = 15,
  LUNGER_ALERTED = 20,   /* marks LungeState.alerted, then IDLE */
  LUNGER_GO_HOME = 100,  /* walks back home and faces its heading, then IDLE */
};

/* Lunger tuning. On its last hit it is knocked off at KNOCK_SPEED / KNOCK_VZ.
 * Waiting, it aims at Spyro's spot once he is within AIM_RANGE. The leap
 * launches at LEAP_VZ plus 35/1024 of the distance, falls LEAP_GRAVITY a
 * frame down to -LEAP_MAX_FALL and flies LEAP_SPEED until within
 * LEAP_ARRIVE of the spot; high enough (LEAP_HOMING_HEIGHT) and with Spyro
 * LEAP_HOMING_MIN..+0x2CF away it homes on him at LEAP_HOMING_SPEED, and
 * within PIN_RANGE it holds him PIN_DEPTH under it. Landed, a pinned Spyro
 * burns for BURN_FRAMES; it walks home at HOME_SPEED (within HOME_ARRIVE)
 * and waits WAIT_FRAMES. Close to home it nudges Spyro off (NUDGE_RANGE)
 * every NUDGE_FRAMES. */
#define LUNGER_KNOCK_SPEED 0xE6
#define LUNGER_KNOCK_VZ 0x14
#define LUNGER_AIM_RANGE 0x898
#define LUNGER_LEAP_VZ 0x12C
#define LUNGER_LEAP_GRAVITY 0x32
#define LUNGER_LEAP_MAX_FALL 0x258
#define LUNGER_LEAP_SPEED 0xE6
#define LUNGER_LEAP_ARRIVE 0x97
#define LUNGER_LEAP_HOMING_HEIGHT 0x1F5
#define LUNGER_LEAP_HOMING_MIN 0x51
#define LUNGER_LEAP_HOMING_SPEED 0x50
#define LUNGER_PIN_RANGE 0x2BC
#define LUNGER_PIN_DEPTH 0xC8
#define LUNGER_BURN_FRAMES 0x1E
#define LUNGER_HOME_SPEED 0x82
#define LUNGER_HOME_ARRIVE 0xB4
#define LUNGER_WAIT_FRAMES 0x78
#define LUNGER_NUDGE_RANGE 0x3E8
#define LUNGER_NUDGE_FRAMES 0x10
#define LUNGER_BURST_SPARK_LIFE 0x18 /* BurstActorWithSound spark life */
#define LUNGER_KNOCK_DRAG 0xC /* DriveActorTrackedMove friction */
#define LUNGER_KNOCK_GRAVITY 0xA /* DriveActorTrackedMove gravity */

/* ->mode of an alarm post guard (ACTOR_TYPE_ALARM_POST_GUARD, GuardState;
 * level 4). It falls back post by post (GuardState.stage 0..2); the
 * stage-1 hit hands it off to its partner. Hits are ignored in 10..19 and
 * APGUARD_HANDED_OFF. */
enum AlarmGuardMode {
  APGUARD_HOLD = 0,        /* keeps between its post and Spyro; group cleared: WATCH */
  APGUARD_HIT = 10,        /* reacts per stage (stage 1 wakes the partner at APGUARD_HIT_PLAYOUT) */
  APGUARD_HIT_PLAYOUT = 11, /* plays out the hit; stage 2 bursts */
  APGUARD_RECOVER = 12,    /* rearms its alarm timer */
  APGUARD_TO_POST = 13,    /* walks to the next stage's post, then HOLD */
  APGUARD_WATCH = 20,      /* faces Spyro; close when its timer runs out: SHOVE */
  APGUARD_SHOVE = 21,      /* shoves Spyro back, then WATCH */
  APGUARD_HANDED_OFF = 98, /* hidden after the hand-off; no collision */
};

/* An alarm post guard's animations. GuardState.anim is the base of its set:
 * 0 for the first guard, APGUARD_ANIM_PARTNER_SET for the partner it hands
 * off to, with STAND..HIT_END at the same offsets in both. */
enum AlarmGuardAnim {
  APGUARD_ANIM_STAND = 0,       /* base + 0: facing Spyro */
  APGUARD_ANIM_STEP = 1,        /* base + 1: sidestepping round its post, walking to the next */
  APGUARD_ANIM_HIT = 2,         /* base + 2 */
  APGUARD_ANIM_SHOVE = 3,       /* base + 3: APGUARD_SHOVE */
  APGUARD_ANIM_SET_LEN = 4,     /* below base + 4 a hit or shove blends in, else restarts */
  APGUARD_ANIM_FIDGET = 4,      /* APGUARD_WATCH, a random 2..4 loops apart */
  APGUARD_ANIM_HAND_OFF = 5,    /* the stage-1 hit, waking the partner */
  APGUARD_ANIM_HIDDEN = 6,      /* APGUARD_HANDED_OFF */
  APGUARD_ANIM_PARTNER_SET = 7, /* the partner's base */
};

/* ->mode of a path patroller (ACTOR_TYPE_PATH_PATROLLER, PathState; levels
 * 0/1): a critter that scatters along its path nodes away from Spyro. */
enum PatrollerMode {
  PATROLLER_WAIT = 0,      /* Spyro near and level: PICK (and its group with it) */
  PATROLLER_PICK = 1,      /* picks the neighbour node away from Spyro */
  PATROLLER_FLEE = 2,      /* runs for it, then WAIT */
  PATROLLER_KNOCKED = 3,   /* knocked away, then despawns */
};

/* A path patroller's animations. */
enum PatrollerAnim {
  PATROLLER_ANIM_IDLE = 0,
  PATROLLER_ANIM_RUN = 1,
  PATROLLER_ANIM_KNOCKED = 3,
};

/* A waiting patroller (and its group on the same node) scatters once Spyro
 * is within NOTICE_RANGE (2D) and NOTICE_DZ, after a START_MIN..MAX_DELAY;
 * it runs for a neighbour node at up to MIN..MAX_SPEED (gaining ACCEL a
 * frame), its flee heading 108/128 of the way from Spyro's bearing to the
 * node's (± AIM_JITTER), for up to FLEE_MAX_FRAMES before curving onto the
 * node. It waits again within ARRIVE_RANGE of the node, or within
 * NOTICE_RANGE after 4 blocked moves (else it veers BLOCKED_VEER_MIN..MAX).
 * Knocked, it slides from KNOCK_SPEED, losing KNOCK_DRAG a frame. */
#define PATROLLER_NOTICE_RANGE 0x1400
#define PATROLLER_NOTICE_DZ 0x400
#define PATROLLER_START_MIN_DELAY 6
#define PATROLLER_START_MAX_DELAY 0x28
#define PATROLLER_AIM_JITTER 0x20
#define PATROLLER_MIN_SPEED 0x6E
#define PATROLLER_MAX_SPEED 0xA0
#define PATROLLER_ACCEL 0xA
#define PATROLLER_FLEE_MAX_FRAMES 0x5A
#define PATROLLER_ARRIVE_RANGE 0xC00
#define PATROLLER_BLOCKED_VEER_MIN 0x3C
#define PATROLLER_BLOCKED_VEER_MAX 0x64
#define PATROLLER_KNOCK_SPEED 0x190
#define PATROLLER_KNOCK_DRAG 0xF

/* ->mode of a lunging guard (ACTOR_TYPE_LUNGING_GUARD and
 * ACTOR_TYPE_BIG_LUNGING_GUARD; level 24). A blast toggles its size kind
 * through SHRINK / GROW. */
enum LungingGuardMode {
  LGUARD_IDLE = 0,     /* faces Spyro; close and level: LUNGE */
  LGUARD_PACE = 1,     /* paces its path once, then IDLE */
  LGUARD_LUNGE = 2,    /* lunges (the small kind bumps Spyro back) */
  LGUARD_KNOCKED = 3,  /* spins away, then bursts */
  LGUARD_SHRINK = 10,  /* scales down to the other kind, then IDLE */
  LGUARD_GROW = 20,    /* scales up and bursts into the other kind, then IDLE */
  LGUARD_ESCORT = 30,  /* keeps its partner between itself and Spyro */
};

/* LungingGuardState.kind: the guard's current size (a big guard toggles it
 * as it shrinks and grows back). */
enum LungingGuardKind {
  LGKIND_SMALL = 0, /* lunges from close; a blast shrinks it away */
  LGKIND_BIG = 1,   /* lunges from far; shrugs off a plain charge; a blast grows it */
};

/* Lunging guard tuning. Hit it spins off at KNOCK_SPEED (CHARGE_KNOCK_SPEED
 * for a charge). It lunges with Spyro within LUNGE_DZ of its height and
 * SMALL_LUNGE_RANGE (small) / BIG_LUNGE_RANGE (big). A small lunge bumps
 * Spyro back at SMALL_BUMP_SPEED (SMALL_BUMP_VZ up), resting
 * SMALL_LUNGE_REST; a big one gives up past BIG_LUNGE_LEAVE (rest
 * BIG_LUNGE_REST) and skips to its strike within BIG_STRIKE_RANGE. A pacer
 * walks PACE_SPEED until Spyro is within PACE_STOP_RANGE. A blasted big one
 * grows to GROW_SCALE and bursts into the small kind; an escort orbits its
 * cannon at ESCORT_RADIUS, ESCORT_SPEED. */
#define LGUARD_KNOCK_SPEED 0xC8
#define LGUARD_CHARGE_KNOCK_SPEED 0x15E
#define LGUARD_LUNGE_DZ 0x190
#define LGUARD_SMALL_LUNGE_RANGE 0x5DC
#define LGUARD_BIG_LUNGE_RANGE 0x1F40
#define LGUARD_SMALL_BUMP_SPEED 0x23
#define LGUARD_SMALL_BUMP_VZ 0x46
#define LGUARD_SMALL_LUNGE_REST 0x78
#define LGUARD_BIG_LUNGE_LEAVE 0x2328
#define LGUARD_BIG_LUNGE_REST 0x96
#define LGUARD_BIG_STRIKE_RANGE 0x9C4
#define LGUARD_PACE_SPEED 0x8C
#define LGUARD_PACE_STOP_RANGE 0xC00
#define LGUARD_GROW_SCALE 0x38
#define LGUARD_ESCORT_RADIUS 0x5DC
#define LGUARD_ESCORT_SPEED 0x32
/* The big-lunging-guard variant (ACTOR_TYPE_BIG_LUNGING_GUARD) shares these
 * but is knocked off at BLG_KNOCK_SPEED / BLG_KNOCK_VZ (BLG_CHARGE_KNOCK_SPEED
 * for a charge), lunges small within BLG_SMALL_LUNGE_RANGE, gives up a big
 * lunge past BLG_LUNGE_LEAVE and strikes within BLG_STRIKE_RANGE; its
 * followers reload FOLLOW1_RELOAD / FOLLOW2_RELOAD after it fires. */
#define LGUARD_BLG_KNOCK_SPEED 0x118
#define LGUARD_BLG_KNOCK_VZ 0x96
#define LGUARD_BLG_CHARGE_KNOCK_SPEED 0x17C
#define LGUARD_BLG_SMALL_LUNGE_RANGE 0x4B0
#define LGUARD_BLG_LUNGE_LEAVE 0x2710
#define LGUARD_BLG_STRIKE_RANGE 0x6A4
#define LGUARD_FOLLOW1_RELOAD 0x28
#define LGUARD_FOLLOW2_RELOAD 0x50

/* ->mode of a grabber (ACTOR_TYPE_GRABBER, GrabberState; level 20).
 * GrabberState.held is shared: one grabber goes for Spyro at a time. */
enum GrabberMode {
  GRABBER_LURK = 0,        /* faces Spyro; rolls its grab chance while unseen-ish */
  GRABBER_RUSH = 1,        /* rushes Spyro (shoving gunners in its way); close: SEIZE */
  GRABBER_RETREAT = 2,     /* back home, then LURK */
  GRABBER_KNOCKED = 4,     /* flamed: drops Spyro, tumbles away and bursts */
  GRABBER_RECOVER = 6,     /* plays out, then RUSH (unreachable: no writer in the C sources, and level data starts every one in mode 0) */
  GRABBER_SEIZE = 10,      /* closes on Spyro and takes hold (hides him) */
  GRABBER_CARRY = 11,
  GRABBER_THROW = 12,      /* throws Spyro clear, then RETREAT */
  GRABBER_DEVOUR = 13,     /* Sparx is gone: the hold kills Spyro */
  GRABBER_THROW_SHORT = 14, /* the shorter throw used after the first one */
  GRABBER_HUSK = 99,       /* the copy left behind by a knock: plays out, despawns */
};

/* A grabber's animations, from the modes that play them. */
enum GrabberAnim {
  GRABBER_ANIM_LURK = 0,
  GRABBER_ANIM_EMERGE = 1,  /* the lurk's grab roll came up; voice 2 plays over it */
  GRABBER_ANIM_REACH = 2,   /* rushing, Spyro inside GRABBER_REACH_RANGE */
  GRABBER_ANIM_MOVE = 3,    /* rushing from further out, and retreating */
  GRABBER_ANIM_CARRY = 4,
  GRABBER_ANIM_THROW = 5,
  GRABBER_ANIM_THROW_SHORT = 7,
  GRABBER_ANIM_DEVOUR = 8,
  GRABBER_ANIM_HIT = 9,     /* flamed: entering GRABBER_KNOCKED */
  GRABBER_ANIM_HUSK = 0xA,  /* GRABBER_HUSK, the copy a flame knock leaves */
};

/* Grabber tuning. Flamed it is knocked off at KNOCK_SPEED / KNOCK_VZ. Lurking,
 * it resets its grab chance past CALM_RANGE and rolls one
 * (CHANCE_MIN..+0x3F) within NOTICE_RANGE; each tick it grabs if a byte roll
 * is under chance - distance / 64, it is inside the camera's CAM_CONE and
 * Spyro is within GRAB_DZ (no more than GRAB_BELOW below it), then waits
 * GRAB_COOLDOWN. It rushes at RUSH_SPEED (reaching within REACH_RANGE,
 * super-hitting gunners in its path) and seizes within SEIZE_RANGE, closing
 * at SEIZE_SLOW_SPEED (SEIZE_FAST_SPEED from SEIZE_FAST_RANGE out); the
 * throw sets Spyro down THROW_OFFSET ahead, flung at THROW_SPEED / THROW_VZ.
 * It walks home at RETREAT_SPEED, home within HOME_ARRIVE, resting
 * HOME_REST. */
#define GRABBER_KNOCK_SPEED 0x15E
#define GRABBER_KNOCK_VZ 0xF0
#define GRABBER_CALM_RANGE 0x2C00
#define GRABBER_NOTICE_RANGE 0x2400
#define GRABBER_CHANCE_MIN 0x50
#define GRABBER_CAM_CONE 0x28
#define GRABBER_GRAB_BELOW 0x1F3
#define GRABBER_GRAB_DZ 0x578
#define GRABBER_GRAB_COOLDOWN 0x3C
#define GRABBER_REACH_RANGE 0xFA0
#define GRABBER_RUSH_SPEED 0x15E
#define GRABBER_SEIZE_RANGE 0x44C
#define GRABBER_SEIZE_SLOW_SPEED 0x50
#define GRABBER_SEIZE_FAST_SPEED 0xFA
#define GRABBER_SEIZE_FAST_RANGE 0x12D
#define GRABBER_THROW_OFFSET 0x4B
#define GRABBER_THROW_SPEED 0x64
#define GRABBER_THROW_VZ 0x37
#define GRABBER_RETREAT_SPEED 0xC8
#define GRABBER_HOME_ARRIVE 0x100
#define GRABBER_HOME_REST 0x78
#define GRABBER_BURST_SPARK_LIFE 0x30 /* BurstActorWithSound spark life */
#define GRABBER_KNOCK_DRAG 0xC /* DriveActorTrackedMove friction */
#define GRABBER_KNOCK_GRAVITY 0xE /* DriveActorTrackedMove gravity */

/* ->mode of a sorcerer (ACTOR_TYPE_SORCERER, SorcererState; level 13).
 * Modes below 0x60 double as the pose's animation index; the cue modes
 * hand over to the queued animation (mode = nextAnim). */
enum SorcererMode {
  SORCERER_IDLE = 0,         /* sentry (watches its path start) or a wisp-caster on a path */
  SORCERER_SETTLE = 1,       /* plays out, then IDLE */
  SORCERER_TAUNT = 2,        /* taunts, turns angry, then IDLE */
  SORCERER_CONJURE = 3,      /* raises its hands, then CAST */
  SORCERER_CAST = 4,         /* casts ACTOR_TYPE_HOMING_WISPs at Spyro */
  SORCERER_KNOCKED = 6,      /* flamed: bursts */
  SORCERER_BREAK_LINK = 10,  /* the path caster was flamed: calms its target, then SETTLE */
  SORCERER_WAIT_TAUNT = 96,  /* waits, then TAUNT */
  SORCERER_WAIT_CONJURE = 97, /* waits, then CONJURE */
  SORCERER_CUE = 98,         /* takes the queued pose (unreachable: no writer in the C sources, and level data starts every one in mode 0) */
  SORCERER_ALERTED = 99,     /* set group-wide: turns angry, takes the queued pose */
};

/* A sorcerer's walk and rest animations (KNOCKED plays its own index, 6). */
enum SorcererAnim {
  SORCERER_ANIM_WALK = 1,
  SORCERER_ANIM_REST = 7,  /* SORCERER_STEP_REST; a caster resting ignores Spyro */
};
/* Sorcerer tuning. It calms down beyond CALM_RANGE. A sentry conjures on
 * its timer with Spyro within `range` of its post, or within NOTICE_RANGE;
 * casting stops once he is past `range` + LOSE_MARGIN of the post and
 * LOSE_RANGE of it (angry: reconjures after RECONJURE_TICKS). A caster
 * walks its path at WALK_SPEED (nodes within PIVOT_THRESH), casting at its
 * target from node CAST_NODE, and breaks off with Spyro on the ground
 * within BREAK_RANGE (resting BREAK_REST ticks; the freed villager's marker
 * rises MARKER_RAISE). A cast throws WISPS homing wisps WISP_GAP ticks apart
 * from frame CAST_FRAME: at its target (WISP_TARGET_RAISE up, living
 * WISP_LIFE) or at Spyro (spawned on him within SNAP_RANGE and SNAP_DZ).
 * Flamed, it bursts with BURST_SPARK_LIFE sparks. */
#define SORCERER_CALM_RANGE 0x2000
#define SORCERER_NOTICE_RANGE 0x1000
#define SORCERER_LOSE_MARGIN 0x400
#define SORCERER_LOSE_RANGE 0x1800
#define SORCERER_RECONJURE_TICKS 0x5A
#define SORCERER_WALK_SPEED 0x6E
#define SORCERER_PIVOT_THRESH 0x200
#define SORCERER_CAST_NODE 8
#define SORCERER_BREAK_RANGE 0x2800
#define SORCERER_BREAK_REST 0x3C
#define SORCERER_MARKER_RAISE 0x200
#define SORCERER_WISPS 3
#define SORCERER_WISP_GAP 4
#define SORCERER_CAST_FRAME 6
#define SORCERER_WISP_LIFE 0xF
#define SORCERER_SNAP_RANGE 0xA00
#define SORCERER_SNAP_DZ 0x400
#define SORCERER_BURST_SPARK_LIFE 0x30

/* SorcererState.mode: the sorcerer's role, read in SORCERER_IDLE. Flaming a
 * CASTER does not kill it: SORCERER_BREAK_LINK demotes it (and its target)
 * to SENTRY. */
enum SorcererRole {
  SORCERER_ROLE_SENTRY = 0, /* faces Spyro; casts when he nears its path start or comes close */
  SORCERER_ROLE_CASTER = 1, /* walks its path, casting wisps at SorcererState.target (->sub) */
};

/* SorcererState.sub: a SORCERER_ROLE_CASTER's step. */
enum SorcererCasterStep {
  SORCERER_STEP_WALK = 0, /* pivots along its path; the path's cast cue (0x108): CAST */
  SORCERER_STEP_CAST = 1, /* turns to its target, three wisps, then WALK */
  SORCERER_STEP_REST = 2, /* waits out its timer, plays pose 7, then WALK; set by its
                           * VROLE_LEADER villager */
};

/* ->mode of a hatch (ACTOR_TYPE_HATCH, HatchState; level 31); the mode is
 * also the animation index. The idle pair loops on a subMode cue. */
enum HatchMode {
  HATCH_IDLE = 0,        /* plays its idle, then WAIT (the sealed kind: SEALED_IDLE) */
  HATCH_WAIT = 1,        /* until cued (subMode 1), then IDLE */
  HATCH_OPEN = 2,        /* struck: released its barrel or mine; recloses once Spyro is away */
  HATCH_SEALED_IDLE = 3, /* the sealed kind's idle (only a charge opens it) */
  HATCH_SEALED_WAIT = 4,
};

/* HatchState.kind (spawn record). */
enum HatchKind {
  HATCH_KIND_BARREL = 0, /* any hit opens it; holds an ACTOR_TYPE_FLOATING_BARREL */
  HATCH_KIND_SEALED = 1, /* only a charge or supercharge opens it; holds an ACTOR_TYPE_FLOATING_MINE */
};

/* ->mode of a superflame giver (ACTOR_TYPE_SUPERFLAME_GIVER, level 14;
 * _ALT / _B, level 27; SuperflameGiverState). */
enum SuperflameGiverMode {
  SFGIVER_IDLE = 0,   /* faces Spyro; he walks up facing it (guard gone): GRANT */
  SFGIVER_GRANT = 1,  /* freezes Spyro and grants the superflame (SuperflameGiverSub) */
  SFGIVER_RETURN = 2, /* floats back home, then IDLE */
  SFGIVER_WALK = 3,   /* cued: walks its waypoints to a new home */
  SFGIVER_CIRCLE = 4, /* alerted: circles path node 0 until its guard is gone */
};

/* ->subMode while SFGIVER_GRANT. */
enum SuperflameGiverSub {
  SFGIVER_SUB_APPROACH = 0, /* floats to Spyro's front */
  SFGIVER_SUB_CAST = 1,     /* casts: motes, then sets g_nSpyroSuperflameTimer */
  SFGIVER_SUB_LINGER = 2,   /* short hold, then SFGIVER_RETURN */
};

/* A superflame giver's animations. */
enum SuperflameGiverAnim {
  SFGIVER_ANIM_IDLE = 0,
  SFGIVER_ANIM_CAST = 1,   /* SFGIVER_SUB_CAST: motes from frame 6 */
  SFGIVER_ANIM_CIRCLE = 2, /* SFGIVER_CIRCLE */
};

/* ->mode of Jacques (ACTOR_TYPE_JACQUES, JacquesState; level 28). */
enum JacquesMode {
  JACQUES_WATCH = 0,     /* faces Spyro; seen from afar: THROW; Spyro close: HOP */
  JACQUES_THROW = 2,     /* throws an ACTOR_TYPE_THROWN_ROCK, then WATCH */
  JACQUES_DEFEATED = 3,  /* struck at his last stop: bursts */
  JACQUES_HOP = 10,      /* hops along his ring to the next stop, then WATCH */
};
#define JACQUES_BURST_SPARK_LIFE 0x18 /* BurstActorWithSound spark life */

/* ->mode of a bolt sentry (ACTOR_TYPE_BOLT_SENTRY, BoltSentryState;
 * level 19); the mode is also the animation index. Its guards are three
 * ACTOR_TYPE_ATTACH_BEADs, summoned once Spyro comes in range. */
enum BoltSentryMode {
  BSENTRY_IDLE = 0,    /* faces Spyro; close and level: ROUSE */
  BSENTRY_PATROL = 1,  /* walks its path; Spyro ahead: THROW */
  BSENTRY_ROUSE = 2,   /* then WATCH */
  BSENTRY_WATCH = 3,   /* tracks Spyro; close: THROW, far: LOWER */
  BSENTRY_THROW = 4,   /* throws an ACTOR_TYPE_AIMED_BOLT; a hit on Spyro: RECOVER */
  BSENTRY_LOWER = 5,   /* then IDLE */
  BSENTRY_KNOCKED = 6, /* flamed up close: knocked away, then bursts */
  BSENTRY_RECOVER = 7, /* gloats a while, then IDLE */
  BSENTRY_CUE = 8,     /* waits on BoltSentryState.mode's cue: LEAP or PATROL */
  BSENTRY_LEAP = 9,    /* leaps down to its post */
  BSENTRY_SUMMON = 10, /* summons its guards, then IDLE */
};

/* BoltSentryState.mode: the cue a bolt sentry waits on (spawn record);
 * cleared once it fires. */
enum BoltSentryCue {
  BSENTRY_CUE_NONE = 0,
  BSENTRY_CUE_LEAP = 1,   /* Spyro in range and level: BSENTRY_LEAP down its path */
  BSENTRY_CUE_PATROL = 2, /* Spyro ahead within PATROL_CUE_RANGE: BSENTRY_PATROL */
};
/* Bolt-sentry tuning (`range` is in 0x400 units). Only flamed within
 * HIT_RANGE: it slides off along Spyro's heading from KNOCK_SPEED, losing
 * KNOCK_DRAG a frame (contact KNOCK_CONTACT, radius MOVE_RADIUS), and bursts
 * with BURST_SPARK_LIFE sparks. IDLE rouses with Spyro within ROUSE_RANGE
 * and ROUSE_DZ; PATROL walks its path at PATROL_SPEED (nodes within
 * PIVOT_THRESH) and throws at Spyro within THROW_RANGE and THROW_FACING;
 * WATCH throws (THROW_GAP ticks apart) within WATCH_THROW_RANGE and ROUSE_DZ
 * and lowers beyond LOWER_RANGE. A throw holds a glow over frames FX_FRAME
 * .. +FX_FRAMES, spawns the bolt in hand on frames BOLT_FRAME .. +BOLT_FRAMES
 * (Spyro within THROW_RANGE) and lets go on RELEASE_FRAME; hitting Spyro
 * gloats RECOVER_TICKS. CUE_LEAP leaps with Spyro within `range` and
 * LEAP_DZ, covering the distance in LEAP_STEPS moves over frames LEAP_FRAME
 * .. +LEAP_FRAMES; CUE_PATROL fires within PATROL_CUE_RANGE and
 * THROW_FACING. SUMMON raises its guards from frame SUMMON_FRAME. */
#define BSENTRY_HIT_RANGE 0xE10
#define BSENTRY_KNOCK_SPEED 0xF0
#define BSENTRY_KNOCK_DRAG 0x10
#define BSENTRY_KNOCK_CONTACT 0x1F4
#define BSENTRY_MOVE_RADIUS 0x2BC
#define BSENTRY_BURST_SPARK_LIFE 0x34
#define BSENTRY_ROUSE_RANGE 0x1C00
#define BSENTRY_ROUSE_DZ 0x44C
#define BSENTRY_PATROL_SPEED 0x6E
#define BSENTRY_PIVOT_THRESH 0x100
#define BSENTRY_THROW_RANGE 0x1194
#define BSENTRY_THROW_FACING 0x40
#define BSENTRY_WATCH_THROW_RANGE 0xFA0
#define BSENTRY_LOWER_RANGE 0x2000
#define BSENTRY_THROW_GAP 0x3C
#define BSENTRY_FX_FRAME 2
#define BSENTRY_FX_FRAMES 0x1A
#define BSENTRY_BOLT_FRAME 0xF
#define BSENTRY_BOLT_FRAMES 3
#define BSENTRY_RELEASE_FRAME 0x1C
#define BSENTRY_RECOVER_TICKS 3
#define BSENTRY_LEAP_DZ 0x3E8
#define BSENTRY_LEAP_STEPS 12
#define BSENTRY_LEAP_FRAME 8
#define BSENTRY_LEAP_FRAMES 7
#define BSENTRY_PATROL_CUE_RANGE 0x3000
#define BSENTRY_SUMMON_FRAME 4

/* ->mode of a paired guard (ACTOR_TYPE_GUARD_PAIR, GuardPairState; level 2);
 * the mode is also the animation index. */
enum GuardPairMode {
  GPAIR_IDLE = 0,     /* faces Spyro; near and level: ROUSE; facing its partner: CUE */
  GPAIR_ROUSE = 2,    /* then WATCH */
  GPAIR_WATCH = 3,    /* close and level: SWING; far: LOWER */
  GPAIR_SWING = 4,    /* a swing that pins Spyro sends it to GLOAT */
  GPAIR_LOWER = 5,    /* then IDLE */
  GPAIR_KNOCKED = 6,  /* flamed: knocked away, then bursts */
  GPAIR_GLOAT = 7,    /* then IDLE */
  GPAIR_CUE = 10,     /* signals its partner (subMode steps the gesture), then IDLE */
};

/* ->subMode while GPAIR_CUE: the signal gesture. */
enum GuardPairCueStep {
  GPCUE_RAISE = 0,  /* plays the ROUSE animation */
  GPCUE_SIGNAL = 1, /* GPAIR_ANIM_SIGNAL; on its frame 8 the partner enters mode 0xD */
  GPCUE_LOWER = 2,  /* plays the LOWER animation, then IDLE */
};

/* A paired guard's animations besides the poses equal to its mode. */
enum GuardPairAnim {
  GPAIR_ANIM_SIGNAL = 8, /* GPCUE_SIGNAL */
};
/* Guard-pair tuning. IDLE rouses with Spyro within ROUSE_RANGE and LEVEL_DZ
 * and cues its partner every CUE_GAP ticks (the signal lands on frame
 * SIGNAL_FRAME); WATCH swings within SWING_RANGE and LEVEL_DZ and lowers
 * beyond LOWER_RANGE. Flamed, it slides off along Spyro's heading from
 * KNOCK_SPEED losing KNOCK_DRAG a frame (contact KNOCK_CONTACT, radius
 * MOVE_RADIUS), and bursts with BURST_SPARK_LIFE sparks. */
#define GPAIR_ROUSE_RANGE 0x2000
#define GPAIR_LEVEL_DZ 0x5DC
#define GPAIR_SWING_RANGE 0xBB8
#define GPAIR_LOWER_RANGE 0x2400
#define GPAIR_CUE_GAP 0x258
#define GPAIR_SIGNAL_FRAME 8
#define GPAIR_KNOCK_SPEED 0xF0
#define GPAIR_KNOCK_DRAG 0x10
#define GPAIR_KNOCK_CONTACT 0x1F4
#define GPAIR_MOVE_RADIUS 0x2BC
#define GPAIR_BURST_SPARK_LIFE 0x48

/* ->mode of a sweep gunner (ACTOR_TYPE_SWEEP_GUNNER, SweepGunnerState;
 * level 32). */
enum SweepGunnerMode {
  SWEEPGUN_IDLE = 0,     /* faces Spyro; in range and level: SWEEP */
  SWEEPGUN_ADVANCE = 2,  /* Spyro entered its trigger box: walks its path, then IDLE */
  SWEEPGUN_SWEEP = 3,    /* sprays shots, sweeping between its yaw limits (SweepGunnerSweepStep) */
  SWEEPGUN_KNOCKED = 4,  /* flamed: knocked away; bursts, or sinks on water */
  SWEEPGUN_TURN = 10,    /* turns to Spyro, then IDLE */
  SWEEPGUN_SINK = 20,    /* knocked into the water: despawns after a moment */
};

/* ->subMode while SWEEPGUN_SWEEP. */
enum SweepGunnerSweepStep {
  SWEEPGUN_STEP_FIRE = 0,    /* sweeps toward Spyro's bearing, a shot every other frame */
  SWEEPGUN_STEP_REVERSE = 1, /* overshot him: plays its turn, reverses sweepRate, FIRE */
};

/* A sweep gunner's animations besides the poses equal to its mode. */
enum SweepGunnerAnim {
  SWEEPGUN_ANIM_TURN = 5, /* SWEEPGUN_STEP_REVERSE */
};

/* Sweep gunner tuning. Flamed it is knocked off at KNOCK_SPEED / KNOCK_VZ,
 * sinking SINK_STEP a frame when it lands in water within SPLASH_DZ. It
 * sweeps once Spyro is within SWEEP_RANGE / SWEEP_DZ and stops past
 * LEAVE_RANGE / LEAVE_DZ; it reverses once its aim overshoots him by
 * OVERSHOOT (then holds REVERSE_HOLD frames). Shots leave every other frame
 * from MUZZLE_FWD ahead and MUZZLE_UP up at SHOT_SPEED, dipping SHOT_DROP a
 * frame. It walks its path at ADVANCE_SPEED. */
#define SWEEPGUN_KNOCK_SPEED 0xC8
#define SWEEPGUN_KNOCK_VZ 0x104
#define SWEEPGUN_SPLASH_DZ 0x64
#define SWEEPGUN_SINK_STEP 0x64
#define SWEEPGUN_SWEEP_RANGE 0x3400
#define SWEEPGUN_SWEEP_DZ 0x5DC
#define SWEEPGUN_LEAVE_RANGE 0x4000
#define SWEEPGUN_LEAVE_DZ 0x898
#define SWEEPGUN_OVERSHOOT 0x1A
#define SWEEPGUN_REVERSE_HOLD 0x1E
#define SWEEPGUN_MUZZLE_FWD 0x177
#define SWEEPGUN_MUZZLE_UP 0x258
#define SWEEPGUN_SHOT_SPEED 0x190
#define SWEEPGUN_SHOT_DROP 0x29
#define SWEEPGUN_ADVANCE_SPEED 0xA0
/* Knocked out of the water it bursts with BURST_SPARK_LIFE sparks; it
 * sinks for SINK_TICKS. */
#define SWEEPGUN_BURST_SPARK_LIFE 0x30
#define SWEEPGUN_SINK_TICKS 0xA
#define SWEEPGUN_KNOCK_DRAG 0xC /* DriveActorTrackedMove friction */
#define SWEEPGUN_KNOCK_GRAVITY 0x16 /* DriveActorTrackedMove gravity */

/* ->mode of a lurker (ACTOR_TYPE_LURKER, LurkerState; level 20). */
enum LurkerMode {
  LURKER_WATCH = 0,    /* Spyro close in front: LASH; else hops between path nodes */
  LURKER_KNOCKED = 2,  /* struck: drops its tether, knocked away; bursts or sinks */
  LURKER_SPIN = 4,     /* spins a while, then WATCH */
  LURKER_AIM = 10,     /* turns to its next path node, then HOP */
  LURKER_HOP = 11,     /* hops to it, then WATCH */
  LURKER_FACE = 15,    /* turns to Spyro, then WATCH */
  LURKER_LASH = 20,    /* lashes its ACTOR_TYPE_LURKER_TETHER at Spyro and drags him */
  LURKER_SINK = 30,    /* knocked into the water: despawns after a moment */
};

/* A lurker's animations besides the poses equal to its mode. */
enum LurkerAnim {
  LURKER_ANIM_TURN = 1, /* LURKER_AIM and LURKER_FACE */
  LURKER_ANIM_LASH = 3,
};

/* Lurker tuning. Struck, it is knocked off at KNOCK_SPEED (FLAME_KNOCK_SPEED
 * for a flame; KNOCK_VZ up) and bursts after STUN_FRAMES, or sinks
 * (SINK_STEP a frame) when it lands in water within SPLASH_DZ. It lashes once
 * Spyro is within LASH_RANGE / LASH_DZ (and NOTICE_RANGE), inside the
 * camera's CAM_CONE and its own LASH_CONE, then rests LASH_REST frames. The
 * tether's scale is TETHER_SCALE_NUM / length (5..0x7F), the length held to
 * TETHER_MIN..TETHER_MAX. A roamer hops to a random node after
 * ROAM_MIN..MAX_FRAMES with Spyro past NOTICE_RANGE, or toward him once he is
 * past CHASE_MIN_RANGE and a node is CHASE_GAIN closer; it turns to face him
 * past FACE_CONE. */
#define LURKER_KNOCK_SPEED 0x140
#define LURKER_FLAME_KNOCK_SPEED 0xF0
#define LURKER_KNOCK_VZ 0x64
#define LURKER_STUN_FRAMES 0x50
#define LURKER_SPLASH_DZ 0x64
#define LURKER_SINK_STEP 0x64
#define LURKER_NOTICE_RANGE 0x2000
#define LURKER_LASH_RANGE 0xC80
#define LURKER_LASH_DZ 0x5DC
#define LURKER_CAM_CONE 0x28
#define LURKER_LASH_CONE 0x14
#define LURKER_LASH_REST 0xB4
#define LURKER_TETHER_SCALE_NUM 0x1C000
#define LURKER_TETHER_MIN 0x578
#define LURKER_TETHER_MAX 0xC80
#define LURKER_ROAM_MIN_FRAMES 0x28
#define LURKER_ROAM_MAX_FRAMES 0x5A
#define LURKER_CHASE_MIN_RANGE 0xFA1
#define LURKER_CHASE_GAIN 0x5DD
#define LURKER_FACE_CONE 0x13
#define LURKER_BURST_SPARK_LIFE 0x18 /* BurstActorWithSound spark life */
#define LURKER_KNOCK_DRAG 0xC /* DriveActorTrackedMove friction */

/* ->mode of a gunner (ACTOR_TYPE_GUNNER, GunnerState; level 20).
 * GUNNER_IDLE runs GunnerState.mode's role (guard, patrol, lookout). */
enum GunnerMode {
  GUNNER_IDLE = 0,
  GUNNER_WAKE = 1,     /* nudges its partner awake, then TURN */
  GUNNER_TURN = 2,     /* turns to the partner, then CHAT */
  GUNNER_CHAT = 3,     /* faces the partner until its timer or Spyro: GUARD */
  GUNNER_GUARD = 4,    /* tracks Spyro; in range: ADVANCE; close: FIRE */
  GUNNER_KNOCKED = 5,  /* charged: knocked away; bursts, or sinks on water */
  GUNNER_ADVANCE = 7,  /* walks at Spyro, then FIRE */
  GUNNER_FIRE = 8,     /* fires, then GUARD */
  GUNNER_STARTLE = 9,  /* the lookout jumps at Spyro, then GUARD */
  GUNNER_SINK = 10,    /* knocked into the water: sinks and despawns */
};

/* GunnerState.mode: a gunner's role while GUNNER_IDLE (spawn record). */
enum GunnerRole {
  GUNNER_ROLE_GUARD = 0,   /* straight to GUNNER_GUARD */
  GUNNER_ROLE_PATROL = 1,  /* walks its path until its partner leaves idle, then GUARD */
  GUNNER_ROLE_LOOKOUT = 2, /* Spyro close (partner idle): STARTLE; cued (subMode): WAKE */
};
/* Gunner tuning (`range` under 0x20 is in 0x400 units). Charged, it
 * slides from KNOCK_SPEED and KNOCK_VZ up (supercharged: straight up at
 * SUPER_KNOCK_VZ), losing KNOCK_DRAG a frame; landing in water within
 * SPLASH_DZ it sinks SINK_STEP a frame for SINK_TICKS, else bursts with
 * BURST_SPARK_LIFE sparks. A patroller walks its path at WALK_SPEED (nodes
 * within PIVOT_THRESH); a lookout startles at Spyro within STARTLE_RANGE and
 * wakes its partner on frame WAKE_FRAME. It chats CHAT_TICKS (or until
 * Spyro is within FIRE_RANGE); GUARD advances on Spyro within `range` for
 * up to ADVANCE_TICKS at WALK_SPEED (moving with MOVE_RADIUS), firing on
 * the timer within FIRE_RANGE or once within ADVANCE_STOP_RANGE. */
#define GUNNER_KNOCK_SPEED 0x190
#define GUNNER_KNOCK_VZ 0xAA
#define GUNNER_SUPER_KNOCK_VZ 0x104
#define GUNNER_KNOCK_DRAG 0xC
#define GUNNER_SPLASH_DZ 0x64
#define GUNNER_SINK_STEP 0x64
#define GUNNER_SINK_TICKS 0xA
#define GUNNER_BURST_SPARK_LIFE 0x20
#define GUNNER_WALK_SPEED 0xA0
#define GUNNER_PIVOT_THRESH 0x100
#define GUNNER_MOVE_RADIUS 0x12C
#define GUNNER_STARTLE_RANGE 0x640
#define GUNNER_WAKE_FRAME 4
#define GUNNER_CHAT_TICKS 0x78
#define GUNNER_ADVANCE_TICKS 0x78
#define GUNNER_FIRE_RANGE 0x514
#define GUNNER_ADVANCE_STOP_RANGE 0x3E8

/* ->mode of a charging ram (ACTOR_TYPE_CHARGING_RAM / _BIG_, ChargingRamState;
 * level 28); the mode is also the animation index. A butt returns to the
 * mode saved in ChargingRamState.saved. */
enum ChargingRamMode {
  CRAM_GRAZE = 0,      /* grazes, facing Spyro; close: BUTT */
  CRAM_BUTT = 1,       /* head-butts Spyro back */
  CRAM_WATCH = 2,      /* the awake kind; Spyro looking away from afar: REAR */
  CRAM_BUTT_WATCH = 3, /* the awake kind's butt */
  CRAM_KNOCKED = 4,    /* knocked away, then bursts */
  CRAM_CHARGE = 5,     /* charges Spyro; too far or he turns away: GO_HOME */
  CRAM_REAR = 6,       /* rears up, then CHARGE */
  CRAM_GO_HOME = 100,  /* walks home, then WATCH */
};

/* Charging ram tuning. It butts Spyro within BUTT_RANGE (BIG_BUTT_RANGE for
 * the big kind), shoving him at 5/256 of the unit vector (BUTT_VZ up; the
 * big one 0x1E off its heading) and resting BUTT_REST; once Spyro is past
 * CHARGED_RESET_RANGE it may butt again. The awake kind rears and charges
 * once Spyro, within REAR_RANGE, faces it (REAR_FACING) and it is in the
 * camera's REAR_CAM_CONE. Charging at CHARGE_SPEED (re-aiming every
 * REAIM_FRAMES) it butts within BUTT_RANGE - CHARGE_BUTT_INSET inside
 * CHARGE_CONE, and goes home past LEASH or after Spyro looks away
 * (GIVE_UP_FACING) for GIVE_UP_FRAMES. Hit it is knocked off at KNOCK_SPEED
 * / KNOCK_VZ; home walks HOME_SPEED, within HOME_ARRIVE. */
#define CRAM_BUTT_RANGE 0x708
#define CRAM_BIG_BUTT_RANGE 0x5DC
#define CRAM_BUTT_VZ 0x1E
#define CRAM_BUTT_REST 0xA0
#define CRAM_CHARGED_RESET_RANGE 0xC00
#define CRAM_REAR_RANGE 0x2000
#define CRAM_REAR_FACING 0x26
#define CRAM_REAR_CAM_CONE 0x23
#define CRAM_CHARGE_SPEED 0x64
#define CRAM_REAIM_FRAMES 0x1E
#define CRAM_CHARGE_BUTT_INSET 0x258
#define CRAM_CHARGE_CONE 0x40
#define CRAM_LEASH 0x2800
#define CRAM_GIVE_UP_FACING 0x31
#define CRAM_GIVE_UP_FRAMES 0x28
#define CRAM_KNOCK_SPEED 0x190
#define CRAM_KNOCK_VZ 0xAA
#define CRAM_HOME_SPEED 0x5A
#define CRAM_HOME_ARRIVE 0x80
#define CRAM_BURST_SPARK_LIFE 0x20 /* BurstActorWithSound spark life */
#define CRAM_KNOCK_DRAG 0xC /* DriveActorTrackedMove friction */

/* ->mode of a rock thrower (ACTOR_TYPE_ROCK_THROWER, RockThrowerState;
 * level 27); the mode is also the animation index. */
enum RockThrowerMode {
  ROCKTHROW_WATCH = 0,   /* faces Spyro; in sight: THROW; triggered: WALK */
  ROCKTHROW_WALK = 1,    /* walks its path (throwing if Spyro is close), then TURN */
  ROCKTHROW_THROW = 2,   /* throws an ACTOR_TYPE_FALLING_CHUNK at Spyro, then WATCH */
  ROCKTHROW_KNOCKED = 3, /* drops its rock, knocked away, then bursts */
  ROCKTHROW_ESCORT = 4,  /* follows its partner until Spyro trips its box */
  ROCKTHROW_TURN = 10,   /* turns to Spyro, then WATCH */
};

/* A rock thrower's animations besides the poses equal to its mode. */
enum RockThrowerAnim {
  ROCKTHROW_ANIM_DOWN = 4, /* after the knock plays out, until it bursts */
};
/* Rock-thrower tuning. Hit, it slides from KNOCK_SPEED and KNOCK_VZ up
 * (losing KNOCK_DRAG and KNOCK_GRAVITY a frame) for KNOCK_TICKS, then bursts
 * with BURST_SPARK_LIFE sparks; it drops its rock DROP_SKEW off the knock
 * heading at ROCK_SPEED, DROP_VZ up, falling DROP_GRAVITY a frame. WATCH
 * advances (when allowed) with Spyro within ADVANCE_DZ and `range`, and
 * throws with him in sight, within `range2` and less than THROW_DZ above;
 * WALK walks its path at WALK_SPEED (nodes within PIVOT_THRESH), throwing
 * once Spyro is within WALK_THROW_RANGE. The rock leaves on frame
 * THROW_FRAME at ROCK_SPEED; THROW_GAP ticks pass between throws. An escort
 * walks at ESCORT_SPEED, ESCORT_SLOW_SPEED within ESCORT_NEAR of its partner,
 * ESCORT_FAST_SPEED beyond ESCORT_FAR. */
#define ROCKTHROW_KNOCK_SPEED 0x15E
#define ROCKTHROW_KNOCK_VZ 0x8C
#define ROCKTHROW_KNOCK_TICKS 0x3C
#define ROCKTHROW_KNOCK_DRAG 0xC
#define ROCKTHROW_KNOCK_GRAVITY 0x10
#define ROCKTHROW_BURST_SPARK_LIFE 0x18
#define ROCKTHROW_DROP_SKEW 10
#define ROCKTHROW_ROCK_SPEED 0x4B
#define ROCKTHROW_DROP_VZ 0xDC
#define ROCKTHROW_DROP_GRAVITY 0xC
#define ROCKTHROW_ADVANCE_DZ 0x1F4
#define ROCKTHROW_THROW_DZ 0xC8
#define ROCKTHROW_WALK_SPEED 0x8C
#define ROCKTHROW_PIVOT_THRESH 0x100
#define ROCKTHROW_WALK_THROW_RANGE 0x1400
#define ROCKTHROW_THROW_FRAME 0x1A
#define ROCKTHROW_THROW_GAP 0x28
#define ROCKTHROW_ESCORT_SPEED 0x78
#define ROCKTHROW_ESCORT_SLOW_SPEED 0x64
#define ROCKTHROW_ESCORT_FAST_SPEED 0x8C
#define ROCKTHROW_ESCORT_NEAR 0xA8C
#define ROCKTHROW_ESCORT_FAR 0xC80
/* A falling chunk (a thrower's rock) with `grav` falls to -MAX_FALL;
 * without, it rides HOVER over the ground (height steps capped at MAX_VZ,
 * ignored under VZ_DEADBAND and after RIDE_TICKS). Thrown, it bursts on
 * Spyro (within HIT_RADIUS: pushed at a quarter of its velocity, PUSH_VZ
 * up), on a wall, or after LIFE ticks, into DEBRIS_PAIRS rebound-debris
 * pairs and BURST_SPARKS sparks. */
#define FCHUNK_MAX_FALL 0xF0
#define FCHUNK_HOVER 0x78
#define FCHUNK_MAX_VZ 0x28
#define FCHUNK_VZ_DEADBAND 0x1E
#define FCHUNK_RIDE_TICKS 0x28
#define FCHUNK_HIT_RADIUS 0xDC
#define FCHUNK_LIFE 0x5B
#define FCHUNK_PUSH_VZ 0x28
#define FCHUNK_DEBRIS_PAIRS 6
#define FCHUNK_BURST_SPARKS 0x12

/* ->mode / ->subMode of a thrown boulder (ACTOR_TYPE_ROLLING_BOULDER,
 * BoulderState; levels 21/22). */
enum RollingBoulderMode {
  BOULDER_WHOLE = 0, /* held, thrown or dropped (->subMode), then bursts into SHARDs */
  BOULDER_SHARD = 1, /* tumbles and bounces until its life or view runs out */
};
enum RollingBoulderSub {
  BOULDER_HELD = 0,    /* in the thrower's hands; the thrower moves it */
  BOULDER_THROWN = 1,  /* rolls along its velocity; bursts on Spyro, a wall or its timer */
  BOULDER_DROPPED = 2, /* its thrower was knocked: bursts at once, shards along the knock */
};

/* A thrown boulder rides ROLL_HOVER over the ground (height steps capped at
 * ROLL_MAX_VZ, ignored under ROLL_VZ_DEADBAND and for its first 10 ticks)
 * and bursts on Spyro (within ROLL_HIT_RADIUS: knocked back at 3/8 of its
 * velocity, ROLL_PUSH_VZ up), on a wall, or after ROLL_LIFE ticks. Its
 * shards fall SHARD_GRAVITY faster a tick down to -SHARD_MAX_FALL and keep
 * 7/8 of their speed off each bounce. */
#define BOULDER_ROLL_HOVER 0x50
#define BOULDER_ROLL_MAX_VZ 0x50
#define BOULDER_ROLL_VZ_DEADBAND 0x32
#define BOULDER_ROLL_LIFE 0x78
#define BOULDER_ROLL_HIT_RADIUS 0xDC
#define BOULDER_ROLL_PUSH_VZ 0x1E
#define BOULDER_SHARD_GRAVITY 0x14
#define BOULDER_SHARD_MAX_FALL 0xA0

/* ->mode of a boulder thrower (ACTOR_TYPE_BOULDER_THROWER / _B,
 * BoulderThrowerState; levels 21/22). Modes 40-43 are the paired charge
 * (wait on the partner, circle it, launch at Spyro); 50-54 the ride, where a
 * partner (BoulderThrowerState.rider) catches and flings it.
 *
 * Group wake (SetActorGroupSubstate): a thrower in BTHROW_IDLE takes a
 * non-zero ->subMode as the mode to enter, so the signal value is a thrower
 * mode: ROUSE from a rousing thrower, WATCH (skip the rouse) from a
 * gatekeeper or the level 21 egg runner. Gatekeepers share these groups
 * (level data: groups 0..4 in level 21, 0..1 in level 22) and treat any
 * non-zero value as "rouse". */
enum BoulderThrowerMode {
  BTHROW_IDLE = 0,          /* holds its spawn pose until roused */
  BTHROW_ROUSE = 1,         /* rousing animation (may alert its group), then WATCH */
  BTHROW_WATCH = 2,         /* faces Spyro; in range and sight: THROW */
  BTHROW_KNOCKED = 3,       /* drops its boulder, knocked away, then bursts */
  BTHROW_THROW = 4,         /* picks up an ACTOR_TYPE_ROLLING_BOULDER and hurls it */
  BTHROW_PATROL = 5,        /* walks its path; Spyro close: THROW */
  BTHROW_PAIR_WAIT = 40,    /* waits for the partner's go (subMode) */
  BTHROW_PAIR_CIRCLE = 41,  /* circles the partner */
  BTHROW_PAIR_READY = 42,   /* faces Spyro until signalled, then HURTLE */
  BTHROW_HURTLE = 43,       /* flies at Spyro, knocking him back; bursts on a timer */
  BTHROW_RIDE_WAIT = 50,    /* waits for BoulderThrowerState.partnerSignal */
  BTHROW_RIDE_WALK = 51,    /* walks its path to the catcher */
  BTHROW_RIDE_FACE = 52,    /* faces the catcher (Metalhead in level 22) until it is ready
                             * (subMode != 0), then jumps, setting its subMode to 2 */
  BTHROW_RIDE_LEAP = 53,    /* jumps into the catcher's hands */
  BTHROW_RIDE_HELD = 54,    /* held; flung at Spyro (HURTLE) on the throw frame */
};

/* A boulder thrower's animations besides the poses equal to its mode. */
enum BoulderThrowerAnim {
  BTHROW_ANIM_LEAP = 6,   /* BTHROW_RIDE_LEAP */
  BTHROW_ANIM_HURTLE = 7, /* BTHROW_HURTLE */
};

/* Boulder thrower tuning. Hit it is knocked off at KNOCK_SPEED
 * (CHARGE_KNOCK_SPEED for a charge; KNOCK_VZ up, a blast just pops it up
 * BLAST_VZ). It throws (and an advancing one patrols) with Spyro within
 * SIGHT_DZ of its height; a patroller throws within PATROL_THROW_RANGE and
 * walks at PATROL_SPEED. A boulder spawns in its hands on frames 3..7 and
 * leaves on frame RELEASE_FRAME at ROCK_SPEED; it rests REST_FRAMES
 * (REST_FRAMES_B for the type-B thrower) between throws. */
#define BTHROW_KNOCK_SPEED 0x118
#define BTHROW_CHARGE_KNOCK_SPEED 0x15E
#define BTHROW_KNOCK_VZ 0x1E
#define BTHROW_BLAST_VZ 0x28
#define BTHROW_SIGHT_DZ 0x1F4
#define BTHROW_PATROL_THROW_RANGE 0x1400
#define BTHROW_PATROL_SPEED 0x8C
#define BTHROW_RELEASE_FRAME 0x10
#define BTHROW_ROCK_SPEED 0x4B
#define BTHROW_REST_FRAMES 0x78
#define BTHROW_REST_FRAMES_B 0x50
/* Paired throwers circle their partner at PAIR_CIRCLE_SPEED, then hurtle at
 * Spyro at HURTLE_SPEED for HURTLE_FRAMES, lofted at distance / 128 (at most
 * HURTLE_MAX_VZ; flat within HURTLE_FLAT_RANGE, or HURTLE_LOFT_RANGE with
 * him over HURTLE_LOFT_DZ off its height), bouncing at HURTLE_BOUNCE_VZ.
 * A hurtle hits Spyro within HURTLE_HIT_RANGE / HURTLE_HIT_DZ, knocking him
 * at HURTLE_KNOCK_SPEED and itself off at HURTLE_RECOIL_SPEED; it bursts past
 * HURTLE_MAX_RANGE. A flung one (RIDE_HELD) hurtles at FLUNG_SPEED. */
#define BTHROW_PAIR_CIRCLE_SPEED 0x64
#define BTHROW_HURTLE_SPEED 0x15E
#define BTHROW_HURTLE_FRAMES 0xF0
#define BTHROW_HURTLE_MAX_VZ 0x6E
#define BTHROW_HURTLE_FLAT_RANGE 0x1800
#define BTHROW_HURTLE_LOFT_RANGE 0x2800
#define BTHROW_HURTLE_LOFT_DZ 0x3E8
#define BTHROW_HURTLE_BOUNCE_VZ 0x50
#define BTHROW_HURTLE_HIT_RANGE 0x2BC
#define BTHROW_HURTLE_HIT_DZ 0x2BC
#define BTHROW_HURTLE_KNOCK_SPEED 0x6E
#define BTHROW_HURTLE_RECOIL_SPEED 0xDC
#define BTHROW_HURTLE_MAX_RANGE 0x9470
#define BTHROW_FLUNG_SPEED 0x1C2
#define BTHROW_BURST_SPARK_LIFE 0x18 /* BurstActorWithSound spark life */

/* ->mode of a gatekeeper (ACTOR_TYPE_GATEKEEPER / _B, GatekeeperState;
 * levels 21/22). Modes 40-44 hand out its run of keys (GatekeeperState.keys)
 * one at a time; 43 and 50-54 match the boulder thrower's. */
enum GatekeeperMode {
  GATEKEEPER_IDLE = 0,         /* holds its spawn pose until roused */
  GATEKEEPER_ROUSE = 1,        /* rousing animation (may alert its group), then WATCH */
  GATEKEEPER_WATCH = 2,        /* faces Spyro; close: STALK */
  GATEKEEPER_STALK = 3,        /* faces Spyro; in reach on the timer: SWIPE; far: WATCH */
  GATEKEEPER_SWIPE = 4,        /* knocks Spyro back during frames 6-9, then STALK */
  GATEKEEPER_KNOCKED = 5,      /* knocked away, then bursts */
  GATEKEEPER_KEYS_ARM = 40,    /* Spyro near: readies the next key */
  GATEKEEPER_KEYS_WAIT = 41,   /* waits for Spyro in range, facing it, in sight */
  GATEKEEPER_KEYS_RELEASE = 42, /* releases the key on frame 6, then KEYS_RECOVER */
  GATEKEEPER_HURTLE = 43,      /* flies at Spyro, knocking him back; bursts on a timer */
  GATEKEEPER_KEYS_RECOVER = 44, /* back to KEYS_WAIT while keys remain */
  GATEKEEPER_RIDE_WAIT = 50,   /* waits for GatekeeperState.partnerSignal */
  GATEKEEPER_RIDE_WALK = 51,   /* walks its path to the catcher */
  GATEKEEPER_RIDE_FACE = 52,   /* faces the catcher (Metalhead in level 22) until it is
                                * ready (subMode != 0), then jumps, setting its subMode to 2 */
  GATEKEEPER_RIDE_LEAP = 53,   /* jumps into the catcher's hands */
  GATEKEEPER_RIDE_HELD = 54,   /* held; flung at Spyro (HURTLE) on the throw frame */
};
/* GatekeeperState.mode (levels 21/22). */
enum GatekeeperPost {
  GKEEPER_POST_FIXED = 0,
  GKEEPER_POST_TWO_BOX = 2, /* jumps to path node 0 / 1 when Spyro enters box0 / box1 */
};

/* A gatekeeper's animations besides the poses equal to its mode. */
enum GatekeeperAnim {
  GATEKEEPER_ANIM_WALK = 6,   /* GATEKEEPER_RIDE_WALK */
  GATEKEEPER_ANIM_LEAP = 7,   /* GATEKEEPER_RIDE_LEAP */
  GATEKEEPER_ANIM_HURTLE = 8, /* GATEKEEPER_HURTLE */
};

/* Gatekeeper tuning. Flamed or blasted it is knocked off at KNOCK_SPEED
 * (KNOCK_VZ up; a blast just pops it up BLAST_VZ). It stalks Spyro within
 * STALK_RANGE (back to WATCH past WATCH_RANGE) and swipes within SWIPE_RANGE
 * with him 0xC9..0x577 above it (one unsigned dz test), resting SWIPE_REST
 * after. The swipe reaches (frame - 5) * SWIPE_REACH_STEP on
 * frames 6..9 (capped at SWIPE_REACH_MAX), knocking Spyro back at
 * SWIPE_KNOCK_SPEED, SWIPE_KNOCK_VZ up. */
#define GATEKEEPER_KNOCK_SPEED 0xF0
#define GATEKEEPER_KNOCK_VZ 0x1E
#define GATEKEEPER_BLAST_VZ 0x28
#define GATEKEEPER_STALK_RANGE 0x1C00
#define GATEKEEPER_WATCH_RANGE 0x2400
#define GATEKEEPER_SWIPE_RANGE 0xD48
#define GATEKEEPER_SWIPE_REST 0x78
#define GATEKEEPER_SWIPE_REACH_STEP 0x3E8
#define GATEKEEPER_SWIPE_REACH_MAX 0xDAC
#define GATEKEEPER_SWIPE_KNOCK_SPEED 0x8C
#define GATEKEEPER_SWIPE_KNOCK_VZ 0x46
/* Flung by its catcher (RIDE_HELD past the throw frame): it hurtles at
 * Spyro at HURTLE_SPEED for HURTLE_FRAMES (bouncing at HURTLE_BOUNCE_VZ),
 * hitting him within HURTLE_HIT_RANGE / HURTLE_HIT_DZ (knocked back at
 * HURTLE_KNOCK_SPEED, HURTLE_KNOCK_VZ up) and bursting platform pods it
 * passes; it bursts past HURTLE_MAX_RANGE from him. On the ride it walks at
 * RIDE_WALK_SPEED and leaps at RIDE_LEAP_SPEED / RIDE_LEAP_VZ. */
#define GATEKEEPER_HURTLE_SPEED 0x1F4
#define GATEKEEPER_HURTLE_FRAMES 0xB4
#define GATEKEEPER_HURTLE_BOUNCE_VZ 0x5A
#define GATEKEEPER_HURTLE_HIT_RANGE 0x3E8
#define GATEKEEPER_HURTLE_HIT_DZ 0x320
#define GATEKEEPER_HURTLE_KNOCK_SPEED 0x6E
#define GATEKEEPER_HURTLE_KNOCK_VZ 0x28
#define GATEKEEPER_HURTLE_MAX_RANGE 0x9470
#define GATEKEEPER_RIDE_WALK_SPEED 0x78
#define GATEKEEPER_RIDE_LEAP_SPEED 0x5A
#define GATEKEEPER_RIDE_LEAP_VZ 0x78
#define GATEKEEPER_BURST_SPARK_LIFE 0x34 /* BurstActorWithSound spark life */

/* ->mode of a tethered guard (ACTOR_TYPE_TETHERED_GUARD, TetherState; level
 * 15); the mode is also the animation index. TetherState.mode is the TetherCommand
 * a tether keeper or sentinel hands it (1 bound to its owner, 4-6 keeper
 * cues). */
enum TetheredGuardMode {
  TGUARD_IDLE = 0,        /* faces Spyro (or turns along its leader's path); near: RISE */
  TGUARD_RISE = 1,        /* rising animation, then GUARD */
  TGUARD_GUARD = 2,       /* faces Spyro; close: STRIKE; far: LOWER; on the timer: LOOK */
  TGUARD_STRIKE = 3,      /* strikes; Spyro pinned: STRIKE_HOLD */
  TGUARD_LOWER = 4,       /* lowers, then IDLE */
  TGUARD_KNOCKED = 5,     /* flamed: knocked away, then bursts */
  TGUARD_STRIKE_HOLD = 6, /* holds after pinning Spyro, then IDLE */
  TGUARD_BOUND = 7,       /* frozen until its owner goes dormant */
  TGUARD_LOOK = 8,        /* looks about (or toward its keeper), then RISE */
  TGUARD_SIGNAL = 9,      /* cues its keeper (who watches for this mode) */
};

/* A tethered guard rises for Spyro within RISE_RANGE (2D) and RANGE_DZ of its
 * height, strikes within STRIKE_RANGE and lowers past LOWER_RANGE. Risen, it
 * looks about after GUARD_FRAMES, and after a look waits LOOK_REST_FRAMES.
 * Flamed, it slides from KNOCK_SPEED, losing KNOCK_DRAG a frame. */
#define TGUARD_RISE_RANGE 0x1770
#define TGUARD_RANGE_DZ 0x5DC
#define TGUARD_STRIKE_RANGE 0xFA0
#define TGUARD_LOWER_RANGE 0x1B58
#define TGUARD_GUARD_FRAMES 0x40
#define TGUARD_LOOK_REST_FRAMES 0x5A
#define TGUARD_KNOCK_SPEED 0xC8
#define TGUARD_KNOCK_DRAG 0x10
#define TGUARD_BURST_SPARK_LIFE 0x32 /* BurstActorWithSound spark life */

/* TetherState.mode / KeeperState.mode: the command a tether keeper hands its
 * tethered guard once the guard reports READY. */
enum TetherCommand {
  TCMD_NONE = 0,
  TCMD_BOUND = 1, /* frozen (TGUARD_BOUND) until its owner goes dormant */
  TCMD_FREED = 2, /* owner gone: plays out the bound pose, then READY */
  TCMD_READY = 3, /* waiting for its keeper's command */
  TCMD_SWAT = 4,  /* signals once; the swing knocks its keeper away (KEEPER_STRUCK) */
  TCMD_WATCH = 5, /* guards facing its keeper, signalling it between looks */
  TCMD_RISE = 6,  /* rises at once; its signal sends the keeper out (KEEPER_STEP_OUT) */
};

/* ->mode of a tether keeper (ACTOR_TYPE_TETHER_KEEPER, KeeperState; level
 * 15); the mode is also the animation index. */
enum TetherKeeperMode {
  KEEPER_IDLE = 0,       /* faces Spyro when near; may start WATCH_GUARD; close: REACT */
  KEEPER_REACT = 1,      /* reacts to Spyro, then IDLE after a pause */
  KEEPER_COMMAND = 2,    /* turns to its guard after handing it KeeperState.mode */
  KEEPER_WATCH_GUARD = 3, /* faces its guard until Spyro comes near */
  KEEPER_STEP_OUT = 4,   /* steps out from its post and back (guard cue 6) */
  KEEPER_KNOCKED = 5,    /* charged: knocked away, then bursts */
  KEEPER_KNOCKED_ALT = 6, /* the same, with the other animation */
  KEEPER_STRUCK = 7,     /* struck by its guard's SIGNAL (cue 4), then bursts */
};

/* A tether keeper faces Spyro within FACE_RANGE and reacts to him within
 * REACT_RANGE and under REACT_ABOVE above him, resting REACT_REST_FRAMES
 * after. Knocked or struck, it slides from KNOCK_SPEED. KEEPER_STEP_OUT
 * slides it up to STEP_OUT_DIST along its facing from its post (animation
 * position (nextFrame << 7) + (frameProgress << 1)), back from
 * STEP_BACK_AT. */
#define KEEPER_FACE_RANGE 0x1000
#define KEEPER_REACT_RANGE 0x898
#define KEEPER_REACT_ABOVE 0x190
#define KEEPER_REACT_REST_FRAMES 0x78
#define KEEPER_KNOCK_SPEED 0x15E
#define KEEPER_STEP_OUT_DIST 0x180
#define KEEPER_STEP_BACK_AT 0x780
#define KEEPER_BURST_SPARK_LIFE 0x18 /* BurstActorWithSound spark life */
#define KEEPER_KNOCK_DRAG 0xC /* DriveActorTrackedMove friction */

/* ->mode of a sentinel (ACTOR_TYPE_SENTINEL, SentinelState; level 15); the
 * mode is also the animation index. A sentinel walks its waypoints past a
 * chain of tethered guards (SentinelState.link), powering each with homing
 * orbs. The 0x60 modes are the group-alert relay (SetActorGroupState). */
enum SentinelMode {
  SENTINEL_IDLE = 0,       /* waits (or aims along its heading) for Spyro in range */
  SENTINEL_WALK = 1,       /* walks its waypoints; at a guard's stop: POWER_GUARD */
  SENTINEL_ALERT = 2,      /* alerted animation, then WALK or IDLE */
  SENTINEL_TRACK = 3,      /* faces Spyro, then CAST */
  SENTINEL_CAST = 4,       /* fires up to 3 ACTOR_TYPE_HOMING_WISPs */
  SENTINEL_POWER_GUARD = 5, /* faces its guard, sending homing orbs at it */
  SENTINEL_KNOCKED = 6,    /* flamed: knocked away, then bursts */
  SENTINEL_RELAY_ALERT_WAIT = 0x60, /* random delay, then ALERT */
  SENTINEL_RELAY_TRACK_WAIT = 0x61, /* random delay, then TRACK */
  SENTINEL_RELAY_ALERT = 0x62,      /* group cue: ALERT once back at animation 0 */
  SENTINEL_RELAY_TRACK = 0x63,      /* group cue: TRACK once back at animation 0 */
};

/* Sentinel tuning. Struck it is knocked off at SUPER_KNOCK_SPEED plus Spyro's
 * speed / 64 (SUPER_KNOCK_VZ up), or FLAME_KNOCK_SPEED / FLAME_KNOCK_VZ for a
 * flame, its chain of guards powered down with homing orbs. In its range it
 * engages Spyro within ENGAGE_DZ of its height (an aiming one also within
 * AIM_CONE of its facing) and only while he is above it, by under
 * ABOVE_LIMIT. CAST fires up to 3 homing wisps, 4 frames apart from frame 7;
 * one lands on Spyro within WISP_DROP_RANGE / WISP_DROP_DZ. POWER_GUARD sends
 * orbs from POWER_ORB_FWD ahead, POWER_ORB_SIDE aside and POWER_ORB_UP up.
 * It walks its waypoints at WALK_SPEED. */
#define SENTINEL_SUPER_KNOCK_SPEED 0x1C2
#define SENTINEL_SUPER_KNOCK_VZ 0x50
#define SENTINEL_FLAME_KNOCK_SPEED 0xAA
#define SENTINEL_FLAME_KNOCK_VZ 0x5A
#define SENTINEL_ENGAGE_DZ 0x200
#define SENTINEL_AIM_CONE 0x30
#define SENTINEL_ABOVE_LIMIT 0x1000
#define SENTINEL_WISP_DROP_RANGE 0xA00
#define SENTINEL_WISP_DROP_DZ 0x400
#define SENTINEL_POWER_ORB_FWD 0x800
#define SENTINEL_POWER_ORB_SIDE (-0x100)
#define SENTINEL_POWER_ORB_UP 0x400
#define SENTINEL_WALK_SPEED 0x80
#define SENTINEL_BURST_SPARK_LIFE 0x30 /* BurstActorWithSound spark life */
#define SENTINEL_KNOCK_DRAG 0xC /* DriveActorTrackedMove friction */
#define SENTINEL_KNOCK_GRAVITY 0xC /* DriveActorTrackedMove gravity */

/* ->mode of a mount (ACTOR_TYPE_MOUNT, MountState; level 31); the mode is
 * also the animation index. Its rider is a floating barrel or mine
 * (FloatingBarrelMode), dropped when the mount is struck. */
enum MountMode {
  MOUNT_IDLE = 0,    /* faces Spyro; close: REACT */
  MOUNT_REACT = 1,   /* reacts to Spyro, then IDLE */
  MOUNT_KNOCKED = 2, /* struck, or its rider burst or ran: knocked away, then bursts */
};
#define MOUNT_BURST_SPARK_LIFE 0x18 /* BurstActorWithSound spark life */
#define MOUNT_KNOCK_DRAG 0xC /* DriveActorTrackedMove friction */

/* ->mode of a hatch carrier (ACTOR_TYPE_HATCH_CARRIER, HatchCarrierState;
 * level 31); the mode is also the animation index. */
enum HatchCarrierMode {
  HCARRIER_WAIT = 0,    /* on the timer, if its hatch waits: LIFT */
  HCARRIER_LIFT = 1,    /* lifts the hatch's barrel or mine out on frame 4 */
  HCARRIER_THROW = 2,   /* throws it on frame 6, then WAIT */
  HCARRIER_KNOCKED = 3, /* flamed: knocked away, then bursts */
};
#define HCARRIER_BURST_SPARK_LIFE 0x30 /* BurstActorWithSound spark life */
#define HCARRIER_KNOCK_DRAG 0xC /* DriveActorTrackedMove friction */

/* ->mode of a carried-prop thrower (ACTOR_TYPE_CARRIED_THROWER,
 * CarriedThrowerState; level 31); the mode is also the animation index.
 * Modes 0-5 hold a barrel or mine in both hands (the swing hits Spyro with
 * it); 6-10 are the empty-handed set, where flame can knock it away. */
enum CarriedThrowerMode {
  CTHROW_HELD_WAIT = 0,  /* Spyro near: RISE after a delay */
  CTHROW_HELD_RISE = 1,  /* rising animation, then READY */
  CTHROW_HELD_READY = 2, /* close: SWING; far: LOWER */
  CTHROW_HELD_SWING = 3, /* swings the held prop at Spyro; scored: GLOAT */
  CTHROW_HELD_LOWER = 4, /* lowers, then WAIT */
  CTHROW_HELD_GLOAT = 5, /* the swing hit Spyro: gloats, then WAIT */
  CTHROW_BARE_WAIT = 6,  /* the empty-handed set of the above */
  CTHROW_BARE_RISE = 7,
  CTHROW_BARE_READY = 8,
  CTHROW_BARE_SWING = 9,
  CTHROW_BARE_GLOAT = 10,
  CTHROW_DROP = 11,      /* its prop burst or ran: drop animation, then BARE_WAIT */
  CTHROW_KNOCKED = 12,   /* flamed while bare: knocked away, then bursts */
};
/* Carried-thrower tuning. WAIT rises with Spyro within NOTICE_RANGE and
 * NOTICE_DZ (holding: after RISE_DELAY ticks); READY swings within
 * HELD_SWING_RANGE (bare: BARE_SWING_RANGE, after SWING_DELAY ticks) and
 * SWING_DZ, and gives up beyond HELD_LOSE_RANGE (BARE_LOSE_RANGE) or
 * LOSE_DZ. A held swing hits Spyro within HIT_RADIUS of the prop and of the
 * point between it and the thrower. Flamed bare, it slides from KNOCK_SPEED
 * and KNOCK_VZ up, losing KNOCK_DRAG a frame, and bursts with
 * BURST_SPARK_LIFE sparks. */
#define CTHROW_NOTICE_RANGE 0x1400
#define CTHROW_NOTICE_DZ 0xC00
#define CTHROW_RISE_DELAY 0x2D
#define CTHROW_HELD_SWING_RANGE 0xE00
#define CTHROW_BARE_SWING_RANGE 0x1000
#define CTHROW_SWING_DELAY 0x2D
#define CTHROW_SWING_DZ 0x800
#define CTHROW_HELD_LOSE_RANGE 0x1600
#define CTHROW_BARE_LOSE_RANGE 0x1800
#define CTHROW_LOSE_DZ 0x1000
#define CTHROW_HIT_RADIUS 0x200
#define CTHROW_KNOCK_SPEED 0x78
#define CTHROW_KNOCK_VZ 0x3C
#define CTHROW_KNOCK_DRAG 0xC
#define CTHROW_BURST_SPARK_LIFE 0x30

/* ->mode of a floating barrel or mine (ACTOR_TYPE_FLOATING_BARREL /
 * ACTOR_TYPE_FLOATING_MINE, FloatingBarrelState; level 31). Mounts, hatch
 * carriers and carried-prop throwers hold them (FloatingBarrelState.link). */
enum FloatingBarrelMode {
  FLOATBARREL_FLOAT = 0, /* bobs; its holder gone dormant: despawn */
  FLOATBARREL_FUSE = 1,  /* the barrel was flamed: bursts after a delay */
  FLOATBARREL_BURST = 2, /* bursts into staves and splinters, then BLAST */
  FLOATBARREL_BLAST = 3, /* the push-back blast grows for 8 frames, then despawn */
  FLOATBARREL_RUN = 4,   /* the mine was charged: runs at the nearest target, then BURST */
};

/* Barrel / mine tuning. A charged mine hops RUN_HOP up and runs at RUN_SPEED
 * along the knock heading, re-aimed at the best target within SEEK_CONE of it
 * (under SEEK_QUICK Manhattan and SEEK_RANGE exact, scored by distance outside
 * SEEK_NEAR..SEEK_FAR plus 256 per unit of angle); its contact arms after
 * RUN_ARM_FRAMES. Airborne, sink speed starts at RUN_SINK and loses
 * SINK_GRAVITY a frame down to -RUN_SINK. A burst drops it BURST_DROP; the
 * blast grows 0x90 a tick for BLAST_TICKS, pushing Spyro at
 * (PUSH_RANGE - distance) / 64 with PUSH_VZ up. A tumbling one floats
 * TUMBLE_HEIGHT over the ground, rolling at TUMBLE_SPEED. */
#define FLOATBARREL_RUN_HOP 0xC8
#define FLOATBARREL_RUN_SPEED 0xF0
#define FLOATBARREL_RUN_ARM_FRAMES 0x10
#define FLOATBARREL_RUN_SINK 0x80
#define FLOATBARREL_SINK_GRAVITY 0xA
#define FLOATBARREL_SEEK_QUICK 0x6000
#define FLOATBARREL_SEEK_CONE 0xC
#define FLOATBARREL_SEEK_RANGE 0x4000
#define FLOATBARREL_SEEK_NEAR 0x1800
#define FLOATBARREL_SEEK_FAR 0x2800
#define FLOATBARREL_BURST_DROP 0x1CD
#define FLOATBARREL_BLAST_TICKS 8
#define FLOATBARREL_PUSH_RANGE 0x1000
#define FLOATBARREL_PUSH_VZ 0x46
#define FLOATBARREL_TUMBLE_HEIGHT 0x180
#define FLOATBARREL_TUMBLE_SPEED 0x80

/* ->mode of a level 0 path enemy (ACTOR_TYPE_PATH_ENEMY, WalkerState); the
 * mode is also the animation index. It flees from Spyro along its path. */
enum PathEnemyMode {
  PENEMY_IDLE = 0,         /* Spyro near: SPOT; on the timer: FIDGET or FACE_PARTNER */
  PENEMY_FIDGET = 1,       /* idle variant, then IDLE */
  PENEMY_FACE_PARTNER = 2, /* turns to its partner (WalkerState.tgtIdx), then IDLE */
  PENEMY_RETURN = 3,       /* walks its path back to node 0, then IDLE */
  PENEMY_SPOT = 4,         /* faces Spyro, then FLEE (or COWER while he charges) */
  PENEMY_FLEE = 5,         /* runs its path; at the end: HIDE */
  PENEMY_HIDE = 6,         /* waits out of Spyro's reach, then RETURN */
  PENEMY_FLAMED = 7,       /* knocked spinning into the air, then FALL */
  PENEMY_CHARGED = 8,      /* charged: knocked away, then bursts */
  PENEMY_FALL = 9,         /* lands, then bursts */
  PENEMY_COWER = 0x14,     /* while Spyro charges, then FLEE */
};

/* Path enemy tuning. It spots Spyro once he passes a QUICK_RANGE Manhattan
 * pre-test and is within SPOT_RANGE (2D) / SPOT_DZ; its partner within
 * PARTNER_RANGE (Manhattan PARTNER_QUICK) gets a face-to-face instead of a
 * fidget. It walks a path node within NODE_ARRIVE, moving only once within
 * WALK_CONE of its bearing and turning at most TURN_STEP a frame (RETURN at
 * RETURN_SPEED, FLEE at FLEE_SPEED); hidden at the end it waits HIDE_FRAMES
 * out of Spyro's sight. Flamed it is thrown at FLAME_SPEED / FLAME_VZ,
 * charged at CHARGE_SPEED / CHARGE_VZ. */
#define PENEMY_QUICK_RANGE 0x4000
#define PENEMY_SPOT_RANGE 0x2800
#define PENEMY_SPOT_DZ 0x800
#define PENEMY_PARTNER_QUICK 0x1800
#define PENEMY_PARTNER_RANGE 0x1000
#define PENEMY_NODE_ARRIVE 0x200
#define PENEMY_WALK_CONE 0x20
#define PENEMY_TURN_STEP 8
#define PENEMY_RETURN_SPEED 0x40
#define PENEMY_FLEE_SPEED 0x82
#define PENEMY_HIDE_FRAMES 0x78
#define PENEMY_FLAME_SPEED 0x118
#define PENEMY_FLAME_VZ 0xB4
#define PENEMY_CHARGE_SPEED 0x190
#define PENEMY_CHARGE_VZ 0x46

/* ->mode of a flock leader (ACTOR_TYPE_FLOCK_LEADER, FlockState; level 0);
 * the mode is also the animation index. Each hit costs it one follower
 * (FlockState.chainA) until none are left. */
enum FlockLeaderMode {
  FLOCK_IDLE = 0,     /* faces Spyro; close: ROUSE */
  FLOCK_ROUSE = 1,    /* rousing animation, then RUN */
  FLOCK_RUN = 2,      /* runs its looped path away from Spyro; far: IDLE */
  FLOCK_RUN_ALT = 3,  /* RUN with the other animation (the two alternate) */
  FLOCK_BOLT = 4,     /* lost a follower: runs fast */
  FLOCK_KNOCKED = 5,  /* the last hit: drops its flock's prizes, then bursts */
};
/* Flock-leader tuning. The last hit drops each prize a third of a turn
 * (PRIZE_ANGLE_STEP, 12-bit) apart round the leader, then it slides from
 * KNOCK_SPEED losing KNOCK_DRAG a frame and bursts with BURST_SPARK_LIFE
 * sparks. IDLE rouses with Spyro within ROUSE_RANGE and ROUSE_DZ (and faces
 * him within WATCH_RANGE); RUN picks the ring node away from him on reaching
 * one (within NODE_RANGE), dropping back to IDLE once he is beyond
 * ROUSE_RANGE or LOSE_DZ. It turns at most TURN_STEP a frame, moves only
 * when lined up within MOVE_FACING, at RUN_SPEED (BOLT_SPEED bolting) with
 * MOVE_RADIUS. */
#define FLOCK_PRIZE_ANGLE_STEP 0x555
#define FLOCK_KNOCK_SPEED 0x118
#define FLOCK_KNOCK_DRAG 0xC
#define FLOCK_BURST_SPARK_LIFE 0x20
#define FLOCK_ROUSE_RANGE 0x1400
#define FLOCK_ROUSE_DZ 0x400
#define FLOCK_WATCH_RANGE 0x2800
#define FLOCK_NODE_RANGE 0x400
#define FLOCK_LOSE_DZ 0x800
#define FLOCK_TURN_STEP 8
#define FLOCK_MOVE_FACING 0x20
#define FLOCK_RUN_SPEED 0x60
#define FLOCK_BOLT_SPEED 0xD2
#define FLOCK_MOVE_RADIUS 0x200

/* ->mode of a sequenced prop (ACTOR_TYPE_SEQUENCED_PROP, PropAnimState;
 * level 0): a world-chunk animation started by a subMode cue. */
enum SequencedPropMode {
  SEQPROP_WAIT = 0,      /* until cued (subMode 0x1F) */
  SEQPROP_START = 5,     /* resumes the chunk animation and its sound */
  SEQPROP_RUN = 0xA,     /* the two RUN modes alternate every 30 frames */
  SEQPROP_RUN_ALT = 0xB,
  SEQPROP_DONE = 0x63,   /* the animation halted (or was already done) */
};

/* ->mode of a charging yak (ACTOR_TYPE_CHARGING_YAK, ChargingYakState; level
 * 9); the mode is also the animation index. */
enum ChargingYakMode {
  YAK_GRAZE = 0,     /* grazes; Spyro near: ROUSE */
  YAK_ROUSE = 2,     /* rousing animation, then WATCH */
  YAK_WATCH = 3,     /* faces Spyro; close: BUTT; far: SETTLE */
  YAK_BUTT = 4,      /* butts Spyro; pinned him: BUTT_HOLD */
  YAK_SETTLE = 5,    /* settles back, then GRAZE */
  YAK_KNOCKED = 6,   /* flamed: knocked away, then bursts */
  YAK_BUTT_HOLD = 7, /* holds after pinning Spyro, then GRAZE */
};
#define YAK_BURST_SPARK_LIFE 0x18 /* BurstActorWithSound spark life */

/* The grazing pair a charging yak alternates in YAK_GRAZE (8 a random 2..4
 * times, then 9); its other animations are its mode indices. */
enum ChargingYakAnim {
  YAK_ANIM_GRAZE = 8,
  YAK_ANIM_GRAZE_ALT = 9,
};

/* ->mode of a tough ram (ACTOR_TYPE_TOUGH_RAM, ToughRamState; level 9); the
 * mode is also the animation index. The yak's machine, but four hits. */
enum ToughRamMode {
  TRAM_GRAZE = 0,     /* grazes; Spyro near: ROUSE */
  TRAM_ROUSE = 1,     /* rousing animation, then WATCH */
  TRAM_WATCH = 2,     /* faces Spyro; close: BUTT; far: GRAZE */
  TRAM_BUTT = 3,      /* butts Spyro; pinned him: BUTT_HOLD */
  TRAM_SETTLE = 4,    /* back to GRAZE (unreachable: no writer in the C sources, and level data starts every one in mode 0) */
  TRAM_STAGGER = 5,   /* hit: thrown up and back, then GRAZE */
  TRAM_KNOCKED = 6,   /* the fourth hit (or thrown off a ledge): falls, then bursts */
  TRAM_BUTT_HOLD = 7, /* holds after pinning Spyro, then GRAZE */
};

/* The grazing pair a tough ram alternates in TRAM_GRAZE (8 a random 2..4
 * times, then 9); its other animations are its mode indices. */
enum ToughRamAnim {
  TRAM_ANIM_GRAZE = 8,
  TRAM_ANIM_GRAZE_ALT = 9,
};
/* Tough-ram tuning. It takes HITS charges; each staggers it along Spyro's
 * heading from STAGGER_SPEED (losing STAGGER_DRAG a frame from frame
 * STAGGER_DRAG_FRAME), riding FLOOR_LIFT over the floor found PROBE_RAISE
 * up and PROBE_DEPTH down; staggered over a drop deeper than FALL_DEPTH it
 * is knocked off. Knocked, it slides at KNOCK_SPEED and falls GRAVITY
 * faster a frame up to MAX_FALL, bursting with BURST_SPARK_LIFE sparks.
 * GRAZE rouses with Spyro within ROUSE_RANGE and LEVEL_DZ; WATCH butts
 * within BUTT_RANGE and LEVEL_DZ and calms beyond CALM_RANGE; a butt that
 * pins Spyro holds PIN_HOLD_TICKS. */
#define TRAM_HITS 4
#define TRAM_STAGGER_SPEED 0x80
#define TRAM_STAGGER_DRAG 8
#define TRAM_STAGGER_DRAG_FRAME 4
#define TRAM_FLOOR_LIFT 0x10
#define TRAM_PROBE_RAISE 0x200
#define TRAM_PROBE_DEPTH 0x1000
#define TRAM_FALL_DEPTH 0x800
#define TRAM_KNOCK_SPEED 0x80
#define TRAM_GRAVITY 0xC
#define TRAM_MAX_FALL 0xF0
#define TRAM_BURST_SPARK_LIFE 0x18
#define TRAM_ROUSE_RANGE 0x1C00
#define TRAM_LEVEL_DZ 0x5DC
#define TRAM_BUTT_RANGE 0xFA0
#define TRAM_CALM_RANGE 0x2000
#define TRAM_PIN_HOLD_TICKS 3

/* ->mode of a path snowball thrower (ACTOR_TYPE_PATH_SNOWBALL_THROWER / +1,
 * SnowballThrowerState; level 9). */
enum SnowballThrowerMode {
  SNOWTHROW_WAIT = 0,      /* Spyro near: wakes its group, then ADVANCE */
  SNOWTHROW_ADVANCE = 1,   /* circles its post toward Spyro; in sight: THROW */
  SNOWTHROW_THROW = 2,     /* throws an ACTOR_TYPE_THROWN_SNOWBALL, then RETREAT */
  SNOWTHROW_KNOCKED = 3,   /* knocked away, then bursts */
  SNOWTHROW_RETREAT = 4,   /* circles away from Spyro, then AIM or TURN_AWAY */
  SNOWTHROW_AIM = 5,       /* faces Spyro; in sight on the timer: THROW */
  SNOWTHROW_TURN_AWAY = 6, /* faces away from Spyro, then WAIT */
};

/* Snowball thrower tuning. Hit it slides from KNOCK_SPEED (CHARGE_KNOCK_SPEED
 * for a charge), losing KNOCK_DRAG a frame. It wakes its group within
 * WAKE_RANGE (or its level-data range) and throws once a clear line from its
 * hand (EYE_FWD ahead, EYE_UP up) to Spyro has held SIGHT_FRAMES; the ball
 * leaves on THROW_FRAME (THROW_FRAME_B for the second type) at BALL_SPEED
 * and lives BALL_LIFE, the recoil losing RECOIL_DRAG a frame. Back on its
 * ring it rests TURN_AWAY_FRAMES facing away, or aims AIM_FRAMES. */
#define SNOWTHROW_KNOCK_SPEED 0xB4
#define SNOWTHROW_CHARGE_KNOCK_SPEED 0x12C
#define SNOWTHROW_KNOCK_DRAG 0xC
#define SNOWTHROW_WAKE_RANGE 0x3800
#define SNOWTHROW_EYE_FWD 175
#define SNOWTHROW_EYE_UP 0x2BC
#define SNOWTHROW_SIGHT_FRAMES 0x1E
#define SNOWTHROW_THROW_FRAME 0x17
#define SNOWTHROW_THROW_FRAME_B 0x13
#define SNOWTHROW_BALL_SPEED 0xC8
#define SNOWTHROW_BALL_LIFE 0x8C
#define SNOWTHROW_RECOIL_DRAG 0x14
#define SNOWTHROW_TURN_AWAY_FRAMES 0x5A
#define SNOWTHROW_AIM_FRAMES 0x78
#define SNOWTHROW_BURST_SPARK_LIFE 0x18 /* BurstActorWithSound spark life */

/* ->mode of a circling bird (ACTOR_TYPE_CIRCLING_BIRD, CirclingBirdState;
 * level 9); the mode is also the animation index. */
enum CirclingBirdMode {
  CBIRD_PERCH = 0,    /* Spyro near: TAKE_OFF */
  CBIRD_CIRCLE = 1,   /* circles over Spyro, then flies home and LANDs */
  CBIRD_TAKE_OFF = 2, /* take-off animation, then CIRCLE */
  CBIRD_LAND = 3,     /* landing animation, then PERCH */
  CBIRD_KNOCKED = 4,  /* hit: falls out of the air and despawns */
};

/* ->mode of a lob gunner (ACTOR_TYPE_LOB_GUNNER, LobGunnerState; level 32);
 * the mode is also the animation index. */
enum LobGunnerMode {
  LOBGUN_WATCH = 0,   /* faces Spyro; close: SWIPE; far, on the timer: LOB */
  LOBGUN_WALK = 2,    /* Spyro tripped its box: walks its path, then TURN */
  LOBGUN_SWIPE = 3,   /* close swipe, then WATCH */
  LOBGUN_KNOCKED = 4, /* drops its lob, knocked away (into water: SINK), then bursts */
  LOBGUN_LOB = 5,     /* lobs an ACTOR_TYPE_LOB_GUNNER_LOB along its target path */
  LOBGUN_TURN = 10,   /* turns to Spyro, then WATCH */
  LOBGUN_SINK = 20,   /* splashed down: despawns after a delay */
};

/* A lob gunner's animations besides the poses equal to its mode. */
enum LobGunnerAnim {
  LOBGUN_ANIM_FIDGET = 1, /* LOBGUN_WATCH: half the time once an animation ends */
};
/* Lob-gunner tuning. Flamed, it slides from KNOCK_SPEED and KNOCK_VZ up,
 * losing KNOCK_DRAG a frame; landing in water within SPLASH_DZ it sinks
 * SINK_STEP a frame for SINK_TICKS, else bursts with BURST_SPARK_LIFE
 * sparks. WATCH swipes (SWIPE_GAP ticks apart) at Spyro within SWIPE_RANGE
 * and SWIPE_DZ, and lobs beyond SWIPE_RANGE every THROW_GAP ticks; WALK
 * walks its path at WALK_SPEED (nodes within PIVOT_THRESH). The lob spawns
 * in hand on frame SPAWN_FRAME and leaves on THROW_FRAME at LOB_SPEED
 * (LOB_GRAVITY a frame, living LOB_LIFE), at its target path's next point
 * when Spyro is past `aimRange`, else AIM_DROP below Spyro. */
#define LOBGUN_KNOCK_SPEED 0x12C
#define LOBGUN_KNOCK_VZ 0x104
#define LOBGUN_KNOCK_DRAG 0xC
#define LOBGUN_SPLASH_DZ 0x64
#define LOBGUN_SINK_STEP 0x64
#define LOBGUN_SINK_TICKS 0xA
#define LOBGUN_BURST_SPARK_LIFE 0x30
#define LOBGUN_SWIPE_RANGE 0x9C4
#define LOBGUN_SWIPE_DZ 0x258
#define LOBGUN_SWIPE_GAP 0x64
#define LOBGUN_THROW_GAP 0x3C
#define LOBGUN_WALK_SPEED 0xAA
#define LOBGUN_PIVOT_THRESH 0x100
#define LOBGUN_SPAWN_FRAME 5
#define LOBGUN_THROW_FRAME 0xA
#define LOBGUN_LOB_SPEED 0x104
#define LOBGUN_LOB_GRAVITY 6
#define LOBGUN_LOB_LIFE 0x8C
#define LOBGUN_AIM_DROP 0x17C

/* ->mode of a burst gunner (ACTOR_TYPE_BURST_GUNNER, BurstGunnerState; level
 * 32); the mode is also the animation index. */
enum BurstGunnerMode {
  BURSTGUN_WATCH = 0,   /* faces Spyro; in range: FIRE */
  BURSTGUN_WALK = 2,    /* walks its path, then TURN */
  BURSTGUN_FIRE = 3,    /* fires ACTOR_TYPE_BURST_GUNNER_SHOTs, then WATCH */
  BURSTGUN_KNOCKED = 4, /* knocked away (into water: SINK), then bursts */
  BURSTGUN_TURN = 10,   /* turns to Spyro, then WATCH */
  BURSTGUN_SINK = 20,   /* splashed down: despawns after a delay */
};
/* Burst-gunner tuning. Hit, it slides from KNOCK_SPEED (CHARGE_KNOCK_SPEED
 * charged) and KNOCK_VZ up, losing KNOCK_DRAG a frame; landing in water
 * within SPLASH_DZ it sinks SINK_STEP a frame for SINK_TICKS, else bursts
 * with BURST_SPARK_LIFE sparks. WATCH fires with Spyro within FIRE_RANGE,
 * itself within FIRE_FACING of the camera's view and its feet at least
 * FIRE_RISE below Spyro; WALK walks its path at WALK_SPEED (nodes within
 * PIVOT_THRESH) until he is within WATCH_RANGE. A burst throws a shot every
 * SHOT_GAP ticks on frames FIRE_FRAME .. FIRE_FRAME + FIRE_FRAMES - 1, at
 * SHOT_SPEED toward Spyro (jittered up to SHOT_SPREAD), living SHOT_LIFE. */
#define BURSTGUN_KNOCK_SPEED 0xDC
#define BURSTGUN_CHARGE_KNOCK_SPEED 0x154
#define BURSTGUN_KNOCK_VZ 0x82
#define BURSTGUN_KNOCK_DRAG 0xC
#define BURSTGUN_SPLASH_DZ 0x64
#define BURSTGUN_SINK_STEP 0x64
#define BURSTGUN_SINK_TICKS 0xA
#define BURSTGUN_BURST_SPARK_LIFE 0x18
#define BURSTGUN_FIRE_RANGE 0x1388
#define BURSTGUN_FIRE_FACING 0x28
#define BURSTGUN_FIRE_RISE 0xC8
#define BURSTGUN_WALK_SPEED 0x8C
#define BURSTGUN_PIVOT_THRESH 0x100
#define BURSTGUN_WATCH_RANGE 0x9C4
#define BURSTGUN_SHOT_GAP 4
#define BURSTGUN_FIRE_FRAME 0x10
#define BURSTGUN_FIRE_FRAMES 0xA
#define BURSTGUN_SHOT_SPEED 0x12C
#define BURSTGUN_SHOT_SPREAD 0x18
#define BURSTGUN_SHOT_LIFE 0x26
/* A gunner's shot (ACTOR_TYPE_BURST_GUNNER_SHOT) hits Spyro within
 * SHOT_REACH and SHOT_HIGH (while longReachTimer runs: LONG_REACH,
 * BEHIND_REACH when he is over SHOT_BEHIND off its heading, and LONG_HIGH),
 * knocking him back at a quarter of its velocity and SHOT_PUSH_VZ up. It
 * despawns outside the level box (0..SHOT_MAX_X, 0..SHOT_MAX_Y, Z under
 * SHOT_MAX_Z). */
#define BGSHOT_REACH 0x12C
#define BGSHOT_HIGH 0x384
#define BGSHOT_LONG_REACH 0x1F4
#define BGSHOT_LONG_HIGH 0x640
#define BGSHOT_BEHIND 0x64
#define BGSHOT_BEHIND_REACH 0x708
#define BGSHOT_PUSH_VZ 0x28
#define BGSHOT_MAX_X 0x20800
#define BGSHOT_MAX_Y 0x1F000
#define BGSHOT_MAX_Z 0x4800

/* ->mode of a bolt guard (ACTOR_TYPE_BOLT_GUARD, BoltGuardState; level 19).
 * BoltGuardState.mode picks its trigger: 1 runs its path when Spyro trips its
 * box (cueing the group), 2 fires at the linked actor (BoltGuardState.link). */
enum BoltGuardMode {
  BGUARD_WATCH = 0,        /* faces Spyro; close on the timer: WIND_UP */
  BGUARD_ROUSE = 1,        /* rousing animation, then WIND_UP */
  BGUARD_FIRE = 2,         /* fires ACTOR_TYPE_AIMED_BOLT pairs, then WATCH */
  BGUARD_KNOCKED = 3,      /* charged: shatters, tumbles away, then bursts */
  BGUARD_WIND_UP = 5,      /* wind-up animation, then FIRE */
  BGUARD_RUN = 10,         /* runs its path; done or Spyro near: WATCH */
  BGUARD_FACE_LINK = 20,   /* faces its link, then FIRE_AT_LINK */
  BGUARD_FIRE_AT_LINK = 21, /* fires bolt pairs at its link, then the next link */
};

/* A bolt guard's animations besides the poses equal to its mode. */
enum BoltGuardAnim {
  BGUARD_ANIM_RUN = 4,     /* BGUARD_RUN */
  BGUARD_ANIM_KNOCKED = 6, /* charged: the shatter */
};

/* BoltGuardState.mode: the one-shot cue a bolt guard waits on in
 * BGUARD_WATCH (spawn record); cleared once played out. */
enum BoltGuardCue {
  BGUARD_CUE_NONE = 0,
  BGUARD_CUE_RUN = 1,  /* Spyro in its box or range: BGUARD_RUN (the whole group) */
  BGUARD_CUE_LINK = 2, /* Spyro in range: fires along its link chain (BGUARD_FACE_LINK) */
};
/* Bolt-guard tuning (`range` is in 0x400 units). Charged, it shatters
 * into SHARDS ground-bounce shards and tumbles from KNOCK_SPEED and KNOCK_VZ
 * up (losing KNOCK_DRAG and KNOCK_GRAVITY a frame), bursting with
 * BURST_SPARK_LIFE sparks on landing. Cues fire with Spyro within CUE_DZ;
 * WATCH winds up with him within FIRE_RANGE and FIRE_DZ. A volley fires a
 * pair of BOLT_SCALE bolts every VOLLEY_GAP ticks for FIRE_TICKS
 * (LINK_FIRE_TICKS at a link), reloading RELOAD_TICKS; bolts at Spyro live
 * BOLT_LIFE, at a link LINK_BOLT_LIFE, aimed LINK_AIM_RAISE above it.
 * FACE_LINK turns for FACE_LINK_TICKS and gives up once Spyro is within
 * LINK_ABORT_RANGE. RUN runs its path at RUN_SPEED (nodes within
 * PIVOT_THRESH), stopping within RUN_STOP_RANGE of Spyro. */
#define BGUARD_SHARDS 0xA
#define BGUARD_KNOCK_SPEED 0x12C
#define BGUARD_KNOCK_VZ 0xE6
#define BGUARD_KNOCK_DRAG 0xC
#define BGUARD_KNOCK_GRAVITY 0x10
#define BGUARD_BURST_SPARK_LIFE 0x18
#define BGUARD_CUE_DZ 0xBB8
#define BGUARD_FIRE_RANGE 0xFA0
#define BGUARD_FIRE_DZ 0x320
#define BGUARD_BOLT_SCALE 0x48
#define BGUARD_VOLLEY_GAP 8
#define BGUARD_FIRE_TICKS 0x28
#define BGUARD_LINK_FIRE_TICKS 0x19
#define BGUARD_RELOAD_TICKS 0xB4
#define BGUARD_BOLT_LIFE 0x1E
#define BGUARD_LINK_BOLT_LIFE 0x2D
#define BGUARD_LINK_AIM_RAISE 0xFA
#define BGUARD_FACE_LINK_TICKS 0x3C
#define BGUARD_LINK_ABORT_RANGE 0x1400
#define BGUARD_RUN_SPEED 0x5A
#define BGUARD_PIVOT_THRESH 0x100
#define BGUARD_RUN_STOP_RANGE 0xC00

/* ->mode of a bolt caster (ACTOR_TYPE_BOLT_CASTER, BoltCasterState; level
 * 14); the mode is also the animation index. With no Spyro about it practises
 * on its target actor (BoltCasterState.target). */
enum BoltCasterMode {
  BCASTER_IDLE = 0,    /* Spyro near: NOTICE (then AIM); on the timer: AIM at its target */
  BCASTER_NOTICE = 1,  /* notices Spyro, then IDLE */
  BCASTER_AIM = 2,     /* faces Spyro or its target, then CAST */
  BCASTER_CAST = 3,    /* casts up to 3 ACTOR_TYPE_HOMING_WISPs */
  BCASTER_KNOCKED = 4, /* flamed: bursts */
  BCASTER_FLINCH = 5,  /* flinch animation (flag bit 0), then IDLE */
};

/* ->mode of a rearing critter (ACTOR_TYPE_REARING_CRITTER, RearState; level
 * 1) and the level 4 spinner (ACTOR_TYPE_SPINNER, SpinnerState), which runs
 * the same machine; the mode is also the animation index. */
enum RearingCritterMode {
  REAR_IDLE = 0,    /* faces Spyro; near: NOTICE (or straight to REAR) */
  REAR_NOTICE = 2,  /* notices Spyro, then REAR */
  REAR_REAR = 3,    /* rears up, then REARED */
  REAR_REARED = 4,  /* stands reared; close: SHOVE; far or out of reach: DROP */
  REAR_SHOVE = 5,   /* shoves Spyro back; knocked him: GLOAT */
  REAR_DROP = 6,    /* drops back down, then IDLE */
  REAR_KNOCKED = 7, /* spins away, then bursts */
  REAR_GLOAT = 8,   /* after knocking Spyro back, then REARED */
};

/* A rearing critter's idle fidgets: one every 3..6 loops of the pose. */
enum RearAnim {
  REAR_ANIM_FIDGET = 1,         /* REAR_IDLE */
  REAR_ANIM_REARED_FIDGET = 10, /* REAR_REARED, Spyro beyond REAR_FIDGET_RANGE */
};
/* Rearing-critter tuning. IDLE rears with Spyro within ROUSE_RANGE and
 * ROUSE_DZ (off cooldown); REARED fidgets beyond FIDGET_RANGE, shoves within
 * SHOVE_RANGE and SHOVE_DZ, and drops back beyond LOSE_RANGE or LOSE_DZ. A
 * shove that knocks Spyro back gloats GLOAT_TICKS (looping GLOAT_LOOP_END
 * back to GLOAT_LOOP_START). Hit, it spins for SPIN_FRAMES and slides from
 * KNOCK_SPEED (CHARGE_KNOCK_SPEED charged) losing KNOCK_DRAG a frame (radius
 * MOVE_RADIUS), then bursts with BURST_SPARK_LIFE sparks. */
#define REAR_ROUSE_RANGE 0x2000
#define REAR_ROUSE_DZ 0x5DC
#define REAR_FIDGET_RANGE 0xC00
#define REAR_SHOVE_RANGE 0xBB8
#define REAR_SHOVE_DZ 0x4B0
#define REAR_LOSE_RANGE 0x2400
#define REAR_LOSE_DZ 0x6A4
#define REAR_GLOAT_TICKS 0x8C
#define REAR_GLOAT_LOOP_END 0x1C
#define REAR_GLOAT_LOOP_START 0xF
#define REAR_SPIN_FRAMES 0xA
#define REAR_KNOCK_SPEED 0x190
#define REAR_CHARGE_KNOCK_SPEED 0x1F4
#define REAR_KNOCK_DRAG 0x1E
#define REAR_MOVE_RADIUS 0x12C
#define REAR_BURST_SPARK_LIFE 0x20

/* ->mode of a sentry (ACTOR_TYPE_SENTRY, SentryState; level 1). Only one
 * sentry engages at a time (*SentryState.alert); it pulls the camera toward
 * itself while it does. */
enum SentryMode {
  SENTRY_WATCH = 0,      /* faces Spyro; near and none engaged: CIRCLE */
  SENTRY_CIRCLE = 1,     /* circles in on Spyro's flank; lined up: LUNGE; lost: GO_HOME */
  SENTRY_KNOCKED = 2,    /* spins away, then bursts */
  SENTRY_LUNGE = 3,      /* lunges at Spyro, then RECOVER */
  SENTRY_RECOVER = 4,    /* slides to a stop, then CIRCLE */
  SENTRY_GO_HOME = 0x64, /* walks home, then WATCH */
};

/* Sentry tuning (the level 1 cousin of the charging guard). Hit it slides
 * from KNOCK_SPEED (FLAME_KNOCK_SPEED for a flame), losing KNOCK_DRAG a frame.
 * It engages Spyro within ENGAGE_RANGE / ENGAGE_DZ, circles at CIRCLE_SPEED
 * re-reading his heading every RETARGET_FRAMES, and gives up when he turns
 * away (GIVE_UP_FACING) or it strays past LEASH from home. Within LUNGE_RANGE
 * and lined up (LUNGE_CONE) it lunges at LUNGE_SPEED, slowing 5 a frame to
 * LUNGE_MIN_SPEED; within HIT_RANGE / HIT_DZ Spyro is knocked back (camera
 * held CAM_HOLD_FRAMES from CAM_HIT_PULLBACK behind; while camHold runs the
 * camera frames it from CAM_PULLBACK past it). GO_HOME walks HOME_SPEED, home
 * within HOME_ARRIVE. */
#define SENTRY_KNOCK_SPEED 0x1C2
#define SENTRY_FLAME_KNOCK_SPEED 0xFA
#define SENTRY_KNOCK_DRAG 0xF
#define SENTRY_ENGAGE_RANGE 0x2800
#define SENTRY_ENGAGE_DZ 0x5DC
#define SENTRY_CIRCLE_SPEED 0x64
#define SENTRY_RETARGET_FRAMES 0x1E
#define SENTRY_GIVE_UP_FACING 0x31
#define SENTRY_LEASH 0x2800
#define SENTRY_LUNGE_RANGE 0xED8
#define SENTRY_LUNGE_CONE 0x14
#define SENTRY_LUNGE_SPEED 0x64
#define SENTRY_LUNGE_MIN_SPEED 0x50
#define SENTRY_HIT_RANGE 0x5DC
#define SENTRY_HIT_DZ 0x384
#define SENTRY_CAM_HOLD_FRAMES 0x32
#define SENTRY_CAM_PULLBACK 0x1000
#define SENTRY_CAM_HIT_PULLBACK 0xC00
#define SENTRY_HOME_SPEED 0x5A
#define SENTRY_HOME_ARRIVE 0x80
#define SENTRY_BURST_SPARK_LIFE 0x18 /* BurstActorWithSound spark life */

/* ->mode of a post grabber (ACTOR_TYPE_POST_GRABBER, PostGrabberState; level
 * 25); the mode is also the animation index. A grabber and its lookout are
 * one actor that swaps ->type: a lookout whose partner is roused becomes a
 * grabber in PGRAB_EMERGE, and a grabber standing down becomes a lookout in
 * LOOKOUT_CHASE. PostGrabberState.slot holds the one actor allowed to engage. */
enum PostGrabberMode {
  PGRAB_WAIT = 0,       /* faces Spyro; close in front and the slot free: CHASE */
  PGRAB_GRAB = 1,       /* seizes Spyro, holds him, then throws him back out */
  PGRAB_CHASE = 2,      /* closes on Spyro (close: GRAB); subMode 1: walks back to its post */
  PGRAB_STAND_DOWN = 3, /* partner signalled (subMode 3): becomes a lookout */
  PGRAB_EMERGE = 4,     /* just turned grabber: sweeps a growing contact sphere */
  PGRAB_KNOCKED = 5,    /* flamed: bursts */
};

/* ->mode of a grabber lookout (ACTOR_TYPE_GRABBER_LOOKOUT, PostGrabberState;
 * level 25); the mode is also the animation index. See PostGrabberMode. */
enum GrabberLookoutMode {
  LOOKOUT_WAIT = 0,    /* faces Spyro; close in front and the slot free: CHASE */
  LOOKOUT_CHASE = 1,   /* closes on Spyro (close: SNAP); subMode 1: walks back to its post */
  LOOKOUT_SNAP = 2,    /* snaps at Spyro, then CHASE back home */
  LOOKOUT_KNOCKED = 3, /* knocked away, then bursts */
};
/* Grabber / lookout tuning, shared by both forms. Either engages Spyro in
 * front of it within FACING, chasing for ENGAGE_TICKS (the grabber holding
 * the slot HOLD_TICKS), then walks back to its post for up to RETURN_TICKS
 * at up to HOME_SPEED, arriving within HOME_RANGE. The grabber notices
 * Spyro (on the ground, not charging) within NOTICE_RANGE and GRAB_DZ,
 * chases at CHASE_SPEED and grabs within GRAB_RANGE and GRAB_DZ, moving with
 * MOVE_RADIUS. Its grab pulls Spyro to GRIP_FWD / GRIP_UP in front of it
 * over PULL_FRAMES from frame PULL_FRAME and throws him out on
 * RELEASE_FRAME. EMERGE pushes Spyro out of a sphere growing from
 * EMERGE_REACH to MOVE_RADIUS. It bursts with BURST_SPARK_LIFE sparks. */
#define PGRAB_FACING 0x20
#define PGRAB_ENGAGE_TICKS 0x3C
#define PGRAB_HOLD_TICKS 0x3C
#define PGRAB_RETURN_TICKS 0xF0
#define PGRAB_HOME_SPEED 0x80
#define PGRAB_HOME_RANGE 0x20
#define PGRAB_NOTICE_RANGE 0x1800
#define PGRAB_GRAB_DZ 0x200
#define PGRAB_GRAB_RANGE 0x5DC
#define PGRAB_CHASE_SPEED 0x200
#define PGRAB_MOVE_RADIUS 0x500
#define PGRAB_GRIP_FWD 0x25F
#define PGRAB_GRIP_UP 0x367
#define PGRAB_PULL_FRAME 7
#define PGRAB_PULL_FRAMES 25
#define PGRAB_RELEASE_FRAME 0x20
#define PGRAB_EMERGE_REACH 0x200
#define PGRAB_BURST_SPARK_LIFE 0x34
/* The lookout notices Spyro within NOTICE_RANGE and NOTICE_DZ, chases at
 * CHASE_SPEED (moving with MOVE_RADIUS) and snaps within SNAP_RANGE and
 * SNAP_DZ. Struck, it slides from KNOCK_SPEED (FLAME_KNOCK_SPEED when
 * flamed) losing KNOCK_DRAG and KNOCK_GRAVITY a frame, then bursts with
 * BURST_SPARK_LIFE sparks. */
#define LOOKOUT_NOTICE_RANGE 0xC00
#define LOOKOUT_NOTICE_DZ 0x400
#define LOOKOUT_CHASE_SPEED 0xF0
#define LOOKOUT_MOVE_RADIUS 0x200
#define LOOKOUT_SNAP_RANGE 0x300
#define LOOKOUT_SNAP_DZ 0x320
#define LOOKOUT_KNOCK_SPEED 0x190
#define LOOKOUT_FLAME_KNOCK_SPEED 0xC8
#define LOOKOUT_KNOCK_DRAG 0xC
#define LOOKOUT_KNOCK_GRAVITY 0x10
#define LOOKOUT_BURST_SPARK_LIFE 0x18

/* ->mode of a bomb lobber (ACTOR_TYPE_BOMB_LOBBER, BombLobberState; levels
 * 25/26); the mode is also the animation index. */
enum BombLobberMode {
  BLOBBER_WATCH = 0,   /* faces Spyro; in range on the timer: LOB */
  BLOBBER_LOB = 1,     /* lobs an ACTOR_TYPE_LOBBED_BOMB on frame 6, then WATCH */
  BLOBBER_KNOCKED = 2, /* flamed: knocked away, then bursts */
};
/* Bomb-lobber tuning. WATCH lobs (drawn, off its `reload` timer) with Spyro
 * within LOB_RANGE and LOB_DZ; the bomb leaves on frame LOB_FRAME. Flamed,
 * it slides off along Spyro's heading from KNOCK_SPEED losing KNOCK_DRAG a
 * frame (contact KNOCK_CONTACT, radius MOVE_RADIUS), and bursts with
 * BURST_SPARK_LIFE sparks. */
#define BLOBBER_LOB_RANGE 0x2800
#define BLOBBER_LOB_DZ 0xA00
#define BLOBBER_LOB_FRAME 6
#define BLOBBER_KNOCK_SPEED 0xF0
#define BLOBBER_KNOCK_DRAG 0x10
#define BLOBBER_KNOCK_CONTACT 0x1F4
#define BLOBBER_MOVE_RADIUS 0x2BC
#define BLOBBER_BURST_SPARK_LIFE 0x28

/* ->mode of an alarm sentry (ACTOR_TYPE_ALARM_SENTRY, AlarmSentryState; level
 * 25); the mode is also the animation index. Like the grabber pair, an alarm
 * sentry and a roused sentry are one actor that swaps ->type. */
enum AlarmSentryMode {
  ASENTRY_WATCH = 0,  /* faces Spyro; in front: FIRE after a delay */
  ASENTRY_FIRE = 1,   /* fires an ACTOR_TYPE_FLYING_EMBER, then WATCH */
  ASENTRY_ROUSE = 2,  /* partner signalled (subMode 3): becomes a roused sentry */
  ASENTRY_SETTLE = 3, /* just turned back from roused, then WATCH */
};

/* ->mode of a roused sentry (ACTOR_TYPE_ROUSED_SENTRY, AlarmSentryState;
 * level 25); the mode is also the animation index. */
enum RousedSentryMode {
  RSENTRY_WATCH = 0,   /* as ASENTRY_WATCH */
  RSENTRY_FIRE = 1,    /* as ASENTRY_FIRE */
  RSENTRY_KNOCKED = 2, /* charged: knocked spinning into the air, then FALL */
  RSENTRY_FALL = 3,    /* falls; landed: BURST */
  RSENTRY_BURST = 4,   /* bursts */
};
#define RSENTRY_BURST_SPARK_LIFE 0x18 /* BurstActorWithSound spark life */

/* ->mode of a loot critter (ACTOR_TYPE_LOOT_CRITTER, CritterState; level
 * 34); the mode is also the animation index. */
enum LootCritterMode {
  LCRITTER_WATCH = 0,   /* faces Spyro; near: FLEE */
  LCRITTER_FLEE = 1,    /* runs from Spyro around its home or along its path */
  LCRITTER_KNOCKED = 2, /* knocked away (dropping its loot once), then bursts */
};
/* A homed loot critter's ->subMode while LCRITTER_FLEE. */
enum LootCritterHomeStep {
  LCRITTER_HOME_ORBIT = 0,    /* steers round its home point away from Spyro; stuck there with
                               * Spyro off the home side: BREAKOUT */
  LCRITTER_HOME_BREAKOUT = 1, /* runs on along Spyro-to-home; turned or ORBIT_RADIUS out: ORBIT */
};

/* CritterState.mode: how a homeless loot critter (no CritterState.hasHome)
 * runs its path during LCRITTER_FLEE. Spyro within NOTICE_RANGE always restarts at
 * PICK; reaching a path end drops it to LCRITTER_WATCH. */
enum LootCritterRun {
  LCRITTER_RUN_PICK = 0,     /* sets the path direction away from Spyro, then DASH */
  LCRITTER_RUN_OPPOSITE = 1, /* heads for the path point opposite Spyro; far off: WANDER */
  LCRITTER_RUN_WANDER = 2,   /* walks the path */
  LCRITTER_RUN_DASH = 3,     /* runs fast; once clear of Spyro: OPPOSITE */
};
/* Loot-critter tuning. Hit, it slides from KNOCK_SPEED (CHARGE_KNOCK_SPEED
 * charged) and KNOCK_VZ up, losing KNOCK_DRAG and KNOCK_GRAVITY a frame, and
 * bursts with BURST_SPARK_LIFE sparks. LCRITTER_WATCH flees once Spyro is
 * within NOTICE_RANGE. A homed critter flees at FLEE_BASE_SPEED less 1/32 of
 * Spyro's distance (FLEE_MIN_SPEED..FLEE_MAX_SPEED): within HOME_NEAR of
 * home it just runs from him; else it orbits home at ORBIT_RADIUS while
 * Spyro is within ORBIT_RANGE and off its home bearing by ORBIT_FACING
 * (ORBIT_FACING_WIDE on animation 4), settling back to WATCH within
 * ORBIT_SETTLED. Turning harder than STUCK_TURN for STUCK_FRAMES with Spyro
 * off the home side by BREAKOUT_FACING breaks out, until turned BREAKOUT_TURN
 * off or ORBIT_RADIUS from home. A homeless critter runs its path (reaching
 * nodes within PIVOT_THRESH) at WANDER_SPEED, DASH_SPEED when dashing; DASH
 * ends beyond CLEAR_RANGE, OPPOSITE beyond WANDER_RANGE. */
#define LCRITTER_KNOCK_SPEED 0x96
#define LCRITTER_CHARGE_KNOCK_SPEED 0xE1
#define LCRITTER_KNOCK_VZ 0x80
#define LCRITTER_KNOCK_DRAG 0xC
#define LCRITTER_KNOCK_GRAVITY 0x10
#define LCRITTER_BURST_SPARK_LIFE 0x18
#define LCRITTER_NOTICE_RANGE 0x1800
#define LCRITTER_FLEE_BASE_SPEED 0x154
#define LCRITTER_FLEE_MIN_SPEED 0x8C
#define LCRITTER_FLEE_MAX_SPEED 0xF0
#define LCRITTER_HOME_NEAR 0x1000
#define LCRITTER_ORBIT_RADIUS 0x1800
#define LCRITTER_ORBIT_RANGE 0x3000
#define LCRITTER_ORBIT_FACING 10
#define LCRITTER_ORBIT_FACING_WIDE 0x28
#define LCRITTER_ORBIT_SETTLED 0x1E
#define LCRITTER_STUCK_TURN 0x5A
#define LCRITTER_STUCK_FRAMES 0x1E
#define LCRITTER_BREAKOUT_FACING 0x20
#define LCRITTER_BREAKOUT_TURN 0x50
#define LCRITTER_PIVOT_THRESH 0x200
#define LCRITTER_WANDER_SPEED 0xC8
#define LCRITTER_DASH_SPEED 0xF0
#define LCRITTER_CLEAR_RANGE 0x2800
#define LCRITTER_WANDER_RANGE 0x3000

/* ->mode of a loot thief (ACTOR_TYPE_LOOT_THIEF, LootThiefState; level 34);
 * the mode is also the animation index. LootThiefState.leg steps its run. */
enum LootThiefMode {
  LTHIEF_RUN = 0,     /* runs its ring and spur paths away from Spyro */
  LTHIEF_KNOCKED = 1, /* flamed: shatters into chest shards */
};

/* LootThiefState.leg for a ring thief (onRing). */
enum LootThiefLeg {
  LTHIEF_LEG_PARKED = 0,   /* hovers at its spur end; Spyro within NOTICE_RANGE: SPUR_IN on
                            * the spur that leads round the ring away from him */
  LTHIEF_LEG_SPUR_IN = 1,  /* flies the spur onto the ring, then RING */
  LTHIEF_LEG_RING = 2,     /* flies the ring to its end, then CROSS (a lap);
                            * Spyro past LOSE_RANGE: RING_OUT */
  LTHIEF_LEG_CROSS = 3,    /* flies the cross path (LootThiefState.path), then RING */
  LTHIEF_LEG_RING_OUT = 4, /* rounds the ring away from Spyro to a spur (a lap), then
                            * SPUR_OUT; Spyro back within NOTICE_RANGE: RING */
  LTHIEF_LEG_SPUR_OUT = 5, /* flies a spur back out, then PARKED */
};

/* A ring thief flies RING_SPEED less LAP_SLOWDOWN a lap (laps cap at
 * MAX_LAPS - 1), easing 1/8 of the way to each node's height a frame (at
 * most CLIMB_STEP on the spurs, RING_CLIMB_STEP on the ring). Flamed it is
 * knocked off at KNOCK_SPEED. A first-run thief starts its intro tween at
 * INTRO_SPEED, slowing INTRO_DRAG a leg to INTRO_MIN_SPEED, and while facing
 * Spyro climbs toward DODGE_Z. */
#define LTHIEF_NOTICE_RANGE 0x2000
#define LTHIEF_LOSE_RANGE 0x3800
#define LTHIEF_RING_SPEED 0x190
#define LTHIEF_LAP_SLOWDOWN 10
#define LTHIEF_MAX_LAPS 6
#define LTHIEF_CLIMB_STEP 0x100
#define LTHIEF_RING_CLIMB_STEP 0x80
#define LTHIEF_KNOCK_SPEED 0x15E
#define LTHIEF_INTRO_SPEED 0x18
#define LTHIEF_INTRO_DRAG 8
#define LTHIEF_INTRO_MIN_SPEED 2
#define LTHIEF_DODGE_Z 0x61A8
#define LTHIEF_BURST_SPARK_LIFE 0x18 /* BurstActorWithSound spark life */
#define LTHIEF_KNOCK_DRAG 0xC /* DriveActorTrackedMove friction */
#define LTHIEF_KNOCK_GRAVITY 0xC /* DriveActorTrackedMove gravity */

/* ->mode of a two-path runner (ACTOR_TYPE_TWO_PATH_RUNNER, RunnerState; level
 * 8); the mode is also the animation index. With RunnerState.mode TPARM_RUN it runs
 * whichever of its two paths ends nearer Spyro (RunnerState.which). */
enum TwoPathRunnerMode {
  TPRUNNER_IDLE = 0,       /* until RunnerState.mode arms it: GUARD */
  TPRUNNER_GUARD = 4,      /* faces Spyro; close on the timer: SWIPE; armed: RUN */
  TPRUNNER_KNOCKED = 5,    /* charged: knocked away, then bursts */
  TPRUNNER_REACT = 6,      /* faces Spyro, then GUARD (unreachable: no writer in the C sources, and level data starts every one in mode 0) */
  TPRUNNER_SWIPE = 8,      /* swipes, then GUARD */
  TPRUNNER_WALK_BACK = 10, /* walks to its path node, then GUARD (a level 8 lookout, GLOOK_ROUSE, sets its linked runner to 10) */
  TPRUNNER_RUN = 0x14,     /* runs its chosen path to RunnerState.node, then GUARD */
};

/* A two-path runner's animations besides the poses equal to its mode. */
enum TwoPathRunnerAnim {
  TPRUNNER_ANIM_RUN = 7, /* TPRUNNER_RUN; a lookout's GLOOK_ROUSE also starts it on its runner */
};

/* RunnerState.mode: how far a two-path runner is armed. */
enum TwoPathRunnerArm {
  TPARM_NONE = 0,  /* stays IDLE */
  TPARM_GUARD = 1, /* GUARD only (set again once a run ends) */
  TPARM_RUN = 2,   /* GUARD, and RUN when Spyro nears a path's node */
};
/* Two-path-runner tuning (`range` is in 0x400 units). GUARD swipes
 * (SWIPE_GAP ticks apart) at Spyro within SWIPE_RANGE and SWIPE_DZ. A run
 * takes path A at RUN_SPEED (OFFSCREEN_SPEED while not drawn) or path B at
 * B_SPEED (nodes within PIVOT_THRESH), ending at its node or once Spyro is
 * within CATCH_RANGE. WALK_BACK walks at WALK_SPEED, arriving within
 * PIVOT_THRESH. Charged, it slides from KNOCK_SPEED and KNOCK_VZ up (losing
 * KNOCK_DRAG and KNOCK_GRAVITY a frame; blocked, it drops at WALL_DROP),
 * probing PROBE_DIST, then bursts with BURST_SPARK_LIFE sparks. */
#define TPRUNNER_SWIPE_RANGE 0x44C
#define TPRUNNER_SWIPE_DZ 0x2BC
#define TPRUNNER_SWIPE_GAP 0x78
#define TPRUNNER_RUN_SPEED 0x64
#define TPRUNNER_OFFSCREEN_SPEED 0x8C
#define TPRUNNER_B_SPEED 0x50
#define TPRUNNER_PIVOT_THRESH 0x80
#define TPRUNNER_CATCH_RANGE 0x5DC
#define TPRUNNER_WALK_SPEED 0x6E
#define TPRUNNER_KNOCK_SPEED 0x190
#define TPRUNNER_KNOCK_VZ 0xAA
#define TPRUNNER_KNOCK_DRAG 0xC
#define TPRUNNER_KNOCK_GRAVITY 0x10
#define TPRUNNER_WALL_DROP 0x64
#define TPRUNNER_PROBE_DIST 0x64
#define TPRUNNER_BURST_SPARK_LIFE 0x10

/* ->mode of a rim rider (ACTOR_TYPE_RIM_RIDER, RimState; level 16); the mode
 * is also the animation index. RimState.mode picks how it drifts. */
enum RimRiderMode {
  RIMRIDER_EMERGE = 0,  /* rises on its parent's rim, facing Spyro, then DRIFT */
  RIMRIDER_DRIFT = 1,   /* drifts out toward Spyro's last position; Spyro close: POP */
  RIMRIDER_SINK = 2,    /* sinks back and despawns, freeing its parent */
  RIMRIDER_STOP = 3,    /* reached its range, then POP */
  RIMRIDER_SPAWN = 4,   /* spawns its ACTOR_TYPE_RIM_RIDER_HELPER, then POP */
  RIMRIDER_POP = 5,     /* pop animation, then SINK */
};

/* RimState.mode: how RIMRIDER_DRIFT moves, picked at spawn from the
 * parent's mode (0x48 / 0x52 / 0x5C). */
enum RimRiderDrift {
  RIMDRIFT_TETHERED = 1, /* slides out along the parent's heading, reach..range from it */
  RIMDRIFT_CHASE = 2,    /* runs along its own heading at Spyro's last-seen spot */
  RIMDRIFT_CHASE_HELPER = 3, /* CHASE, first spawning its ACTOR_TYPE_RIM_RIDER_HELPER */
};

/* ->mode of a brawler (ACTOR_TYPE_BRAWLER, BrawlerState; level 7); the mode
 * is also the animation index. It carries a swooper, which flies off
 * knocked (SWOOP_KNOCKED) when the brawler goes down. */
enum BrawlerMode {
  BRAWLER_IDLE = 0,    /* faces Spyro; near: RAISE */
  BRAWLER_SEND = 1,    /* sends an ACTOR_TYPE_BRAWLER_SPAWN out on frame 0x11 (unreachable: no writer in the C sources, and level data starts every one in mode 0) */
  BRAWLER_RAISE = 2,   /* raises its fists, then GUARD */
  BRAWLER_GUARD = 3,   /* faces Spyro; close on the timer: PUNCH; far: LOWER */
  BRAWLER_PUNCH = 4,   /* punches, then RAISE */
  BRAWLER_LOWER = 5,   /* lowers its fists, then IDLE */
  BRAWLER_KNOCKED = 6, /* flamed: its swooper flies off, then it bursts */
};
/* Brawler tuning. IDLE raises its fists with Spyro within ROUSE_RANGE and
 * ROUSE_DZ; GUARD punches (PUNCH_GAP ticks apart) within PUNCH_RANGE and
 * PUNCH_DZ, and lowers them beyond LOWER_RANGE. Flamed, it slides from
 * KNOCK_SPEED losing KNOCK_DRAG a frame (moving with MOVE_RADIUS) and
 * bursts with BURST_SPARK_LIFE sparks; its swooper flies off SWOOPER_RAISE
 * up at SWOOPER_SPEED with DEBRIS_PAIRS bounce-debris pairs. */
#define BRAWLER_ROUSE_RANGE 0x2000
#define BRAWLER_ROUSE_DZ 0x3E8
#define BRAWLER_PUNCH_RANGE 0x9C4
#define BRAWLER_PUNCH_DZ 0x320
#define BRAWLER_LOWER_RANGE 0x2400
#define BRAWLER_PUNCH_GAP 0x8C
#define BRAWLER_KNOCK_SPEED 0xFA
#define BRAWLER_KNOCK_DRAG 0x14
#define BRAWLER_MOVE_RADIUS 0x2BC
#define BRAWLER_BURST_SPARK_LIFE 0x30
#define BRAWLER_SWOOPER_RAISE 0x5DC
#define BRAWLER_SWOOPER_SPEED 0x12C
#define BRAWLER_DEBRIS_PAIRS 7

/* ->mode of a chasing path guard (ACTOR_TYPE_CHASING_PATH_GUARD,
 * ChasingPathGuardState; level 26); the mode is also the animation index. */
enum ChasingPathGuardMode {
  CPGUARD_PATROL = 0,  /* walks its waypoints (or faces Spyro at home); sees him: CHASE */
  CPGUARD_RETURN = 1,  /* back to its path or home, then PATROL */
  CPGUARD_CHASE = 2,   /* runs at Spyro; facing him: ATTACK; lost him: RETURN */
  CPGUARD_ATTACK = 3,  /* lunges, then RETURN */
  CPGUARD_KNOCKED = 4, /* knocked away, then bursts */
};

/* Chasing path guard tuning. Knocked off at KNOCK_SPEED / KNOCK_VZ
 * (CHARGE_KNOCK_*, SUPER_KNOCK_* plus Spyro's speed / 64 for a superflame).
 * A patroller walks PATROL_SPEED and chases Spyro once he is drawn, in sight,
 * within SIGHT_RANGE / SIGHT_DZ and its SIGHT_CONE, for CHASE_FRAMES; it
 * chases at CHASE_SPEED and attacks (lunging at ATTACK_SPEED) once lined up
 * within ATTACK_CONE. Returning it steers at up to RETURN_SPEED (1/16 of the
 * distance) once within RETURN_CONE, home within RETURN_ARRIVE; RETURN_FRAMES
 * of blocked steering give up. */
#define CPGUARD_KNOCK_SPEED 0x12C
#define CPGUARD_KNOCK_VZ 0x10
#define CPGUARD_CHARGE_KNOCK_SPEED 0x1F4
#define CPGUARD_CHARGE_KNOCK_VZ 0x20
#define CPGUARD_SUPER_KNOCK_SPEED 0x1C2
#define CPGUARD_SUPER_KNOCK_VZ 0x50
#define CPGUARD_PATROL_SPEED 0x5C
#define CPGUARD_SIGHT_RANGE 0x800
#define CPGUARD_SIGHT_DZ 0x800
#define CPGUARD_SIGHT_CONE 0x20
#define CPGUARD_CHASE_FRAMES 0x78
#define CPGUARD_CHASE_SPEED 0x80
#define CPGUARD_ATTACK_CONE 0x18
#define CPGUARD_ATTACK_SPEED 0x40
#define CPGUARD_RETURN_CONE 0x18
#define CPGUARD_RETURN_SPEED 0x5C
#define CPGUARD_RETURN_ARRIVE 0x200
#define CPGUARD_RETURN_FRAMES 0x3C
#define CPGUARD_BURST_SPARK_LIFE 0x18 /* BurstActorWithSound spark life */
#define CPGUARD_KNOCK_DRAG 0xC /* DriveActorTrackedMove friction */
#define CPGUARD_KNOCK_GRAVITY 0x10 /* DriveActorTrackedMove gravity */

/* ->mode of a formation escort (ACTOR_TYPE_FORMATION_ESCORT,
 * FormationEscortState; level 26). */
enum FormationEscortMode {
  ESCORT_WAIT = 0,     /* until released (dropKind set): JOIN at its leader */
  ESCORT_TWEEN_IN = 1, /* tweens in along its path, then JOIN */
  ESCORT_JOIN = 2,     /* steers to its slot around the leader, then HOLD */
  ESCORT_HOLD = 3,     /* holds formation; the formation breaks: BREAK_AWAY */
  ESCORT_BREAK_AWAY = 4, /* spirals up and away, then bursts into streaks */
};
/* Formation-escort tuning. An escort tweens in at TWEEN_SPEED, then joins
 * its slot (SLOT_ANGLE apart) on a RING_RADIUS ring round the leader at
 * JOIN_SPEED, holding once within JOIN_SETTLED; it keeps HOVER_DROP below
 * the leader, climbing at most CLIMB a frame. The lead escort breaks away
 * with Spyro within BREAK_RANGE of the leader (its partners follow): it
 * spirals out BREAK_SPIRAL ahead of the bearing from the leader, speeding up
 * from BREAK_SPEED to MAX_SPEED and rising with it, then bursts after
 * BREAK_TICKS. */
#define ESCORT_TWEEN_SPEED 0x20
#define ESCORT_SLOT_ANGLE 0x55
#define ESCORT_RING_RADIUS 0x600
#define ESCORT_JOIN_SPEED 0x80
#define ESCORT_JOIN_SETTLED 0x10
#define ESCORT_HOVER_DROP 0x200
#define ESCORT_CLIMB 0x40
#define ESCORT_BREAK_RANGE 0x1800
#define ESCORT_BREAK_SPIRAL 0x78
#define ESCORT_BREAK_SPEED 0x20
#define ESCORT_MAX_SPEED 0x100
#define ESCORT_BREAK_TICKS 0x78

/* ->mode of a partner bouncer (ACTOR_TYPE_PARTNER_BOUNCER,
 * PartnerBouncerState; level 26); the mode is also the animation index. */
enum PartnerBouncerMode {
  PBOUNCER_RIDE = 0,   /* rides its partner's attach point; cued and Spyro near: BOUNCE */
  PBOUNCER_BOUNCE = 1, /* bounce animation, then RIDE */
  PBOUNCER_THROWN = 2, /* superflamed, or its partner went: thrown off, then FALL */
  PBOUNCER_FALL = 3,   /* falls until it lands (burst) or drops out of the level */
};
#define PBOUNCER_BURST_SPARK_LIFE 0x30 /* BurstActorWithSound spark life */
#define PBOUNCER_KNOCK_DRAG 0xC /* DriveActorTrackedMove friction */
#define PBOUNCER_KNOCK_GRAVITY 0x10 /* DriveActorTrackedMove gravity */

/* ->mode of a fence captain (ACTOR_TYPE_FENCE_CAPTAIN, FenceCaptainState;
 * level 27); the mode is also the animation index. */
enum FenceCaptainMode {
  FCAPTAIN_WATCH = 0,    /* faces Spyro; next guard's box tripped: SUMMON; close: VOLLEY */
  FCAPTAIN_SUMMON = 1,   /* sends path wisps to wake its next fence guard */
  FCAPTAIN_VOLLEY = 3,   /* fires three ACTOR_TYPE_SPLIT_SHOTs, then RELOCATE */
  FCAPTAIN_KNOCKED = 4,  /* knocked away, then bursts */
  FCAPTAIN_RELOCATE = 5, /* walks to a random node of its path, then WATCH */
};

/* FenceCaptainState.shots: one FCAPTAIN_VOLLEY fires three split shots, on
 * entry and at animation frames 2 and 6. */
#define FCAPTAIN_SHOTS_NONE 0
#define FCAPTAIN_SHOTS_1 1
#define FCAPTAIN_SHOTS_2 2
#define FCAPTAIN_SHOTS_3 0x3C
#define FCAPTAIN_BURST_SPARK_LIFE 0x18 /* BurstActorWithSound spark life */
#define FCAPTAIN_KNOCK_DRAG 0xA /* DriveActorTrackedMove friction */
#define FCAPTAIN_KNOCK_GRAVITY 0x10 /* DriveActorTrackedMove gravity */

/* ->mode of a fence guard (ACTOR_TYPE_FENCE_GUARD, FenceGuardState; level
 * 27); the mode is also the animation index. subMode 1 wakes it, 2 (the
 * captain's reset) puts it back to sleep. */
enum FenceGuardMode {
  FGUARD_ASLEEP = 0, /* until woken */
  FGUARD_WAKE = 1,   /* waking animation, then WATCH */
  FGUARD_WATCH = 2,  /* faces Spyro along its fence; close and in sight: ATTACK */
  FGUARD_ATTACK = 3, /* swings, pinning Spyro on frames HIT_FRAME .. HIT_FRAME_END - 1 */
};
/* Fence-guard tuning. Supercharged, it shatters into PIECES rubble shards
 * fanned PIECE_FAN / PIECE_FAN_WIDE either side of Spyro's heading (lifting
 * ASLEEP_LIFT less when asleep). On its fence it slides at most SLIDE_STEP a
 * frame toward Spyro's nearest fence point. WATCH attacks once its timer is
 * under ATTACK_TIMER with Spyro within NOTICE_RANGE and in sight. ATTACK
 * reaches STRIKE_RANGE (SIDE_STRIKE_RANGE when Spyro faces GUARDED_ARC away
 * from it) and STRIKE_DZ, gives up beyond LOSE_RANGE (RELOAD_TICKS once a
 * swing ends), jumps from the wind-up to SWING_FRAME while Spyro is in
 * reach before WINDUP_END, loops LOOP_END back to LOOP_START, and pins Spyro
 * on frames HIT_FRAME .. HIT_FRAME_END - 1. */
#define FGUARD_PIECES 6
#define FGUARD_PIECE_FAN 0x14
#define FGUARD_PIECE_FAN_WIDE 0x19
#define FGUARD_ASLEEP_LIFT 0x1E
#define FGUARD_SLIDE_STEP 0xC8
#define FGUARD_ATTACK_TIMER 0x3C
#define FGUARD_NOTICE_RANGE 0x1194
#define FGUARD_STRIKE_RANGE 0xAF0
#define FGUARD_SIDE_STRIKE_RANGE 0x7D0
#define FGUARD_GUARDED_ARC 0x47
#define FGUARD_STRIKE_DZ 0x320
#define FGUARD_LOSE_RANGE 0x1770
#define FGUARD_RELOAD_TICKS 0xB4
#define FGUARD_WINDUP_END 0x1C
#define FGUARD_SWING_FRAME 0x1F
#define FGUARD_LOOP_END 0x1E
#define FGUARD_LOOP_START 0xA
#define FGUARD_HIT_FRAME 0x2D
#define FGUARD_HIT_FRAME_END 0x33

/* ->mode of a path charger (ACTOR_TYPE_PATH_CHARGER, PatrolState; level 2);
 * the mode is also the animation index. Only a flame from behind or the side
 * knocks it away. */
enum PathChargerMode {
  PCHARGER_PATROL = 0,  /* walks its path back and forth; Spyro ahead: CHARGE */
  PCHARGER_KNOCKED = 1, /* flamed: bursts */
  PCHARGER_CHARGE = 2,  /* charges down the path; reaching Spyro, butts him off it */
  PCHARGER_TAUNT = 6,   /* at a turn with Spyro ahead: faces him, then PATROL */
  PCHARGER_TAUNT_ALT = 7, /* the same (unreachable: no writer in the C sources, and level data starts every one in mode 0) */
};

/* PatrolState.mode: what a path charger did at its last path turn. */
enum PathChargerTurn {
  PCTURN_FIRST = 0,   /* no turn yet: the first one always taunts a Spyro ahead */
  PCTURN_PLAIN = 1,   /* later turns taunt one time in three */
  PCTURN_TAUNTED = 2, /* taunted: the next leg charges from further (PatrolState.near) */
};

/* A path charger's animations besides the poses equal to its mode. */
enum PathChargerAnim {
  PCHARGER_ANIM_BUTT = 4, /* reaching Spyro: butts him off the path on frame 4 */
};
/* Path-charger tuning. Only flamed from behind or the side (Spyro at
 * least GUARDED_ARC off its heading); it bursts with BURST_SPARK_LIFE sparks.
 * PATROL walks its path at WALK_SPEED when lined up within WALK_FACING,
 * reaching nodes within NODE_RANGE; at a path end it taunts Spyro within
 * TAUNT_RANGE and TAUNT_FACING. Off cooldown, with its feet no more than
 * CHARGE_DROP below Spyro, it charges a Spyro within CHARGE_LEAD of the
 * node distance (and CHARGE_RANGE; after a taunt NEAR_CHARGE_LEAD /
 * NEAR_CHARGE_RANGE) whose bearing is within CHARGE_FACING of the node's.
 * A charge runs at CHARGE_SPEED (radius MOVE_RADIUS), butting within
 * BUTT_RANGE and giving up (COOLDOWN_TICKS) when blocked or past
 * BUTT_RANGE off its path with path and Spyro behind lined up. The butt on
 * frame BUTT_FRAME throws Spyro along the path at BUTT_PUSH, BUTT_PUSH_VZ
 * up. */
#define PCHARGER_GUARDED_ARC 0x47
#define PCHARGER_BURST_SPARK_LIFE 0x48
#define PCHARGER_WALK_SPEED 0x50
#define PCHARGER_WALK_FACING 0x10
#define PCHARGER_NODE_RANGE 0x80
#define PCHARGER_TAUNT_RANGE 0x2000
#define PCHARGER_TAUNT_FACING 0x40
#define PCHARGER_CHARGE_DROP 0x1F4
#define PCHARGER_CHARGE_LEAD 0x1400
#define PCHARGER_CHARGE_RANGE 0x1800
#define PCHARGER_NEAR_CHARGE_LEAD 0x2000
#define PCHARGER_NEAR_CHARGE_RANGE 0x2400
#define PCHARGER_CHARGE_FACING 0x20
#define PCHARGER_CHARGE_SPEED 0x78
#define PCHARGER_MOVE_RADIUS 0x1F4
#define PCHARGER_BUTT_RANGE 0x5DC
#define PCHARGER_COOLDOWN_TICKS 0x78
#define PCHARGER_BUTT_FRAME 4
#define PCHARGER_BUTT_PUSH 0x96
#define PCHARGER_BUTT_PUSH_VZ 0x78

/* ->mode of a linked follower (ACTOR_TYPE_LINKED_FOLLOWER, Follower; level
 * 3); the mode is also the animation index. It trails its leader
 * (Follower.link) while the leader is active. */
enum LinkedFollowerMode {
  LFOLLOW_IDLE = 0,    /* faces Spyro; leader near: SET_OFF; Spyro close: TAUNT */
  LFOLLOW_FOLLOW = 1,  /* runs its path after the leader, then IDLE */
  LFOLLOW_KNOCKED = 3, /* knocked spinning into the air, then bursts */
  LFOLLOW_SET_OFF = 5, /* turns to the leader's heading, then FOLLOW */
  LFOLLOW_TAUNT = 8,   /* faces Spyro, then IDLE */
};
#define LFOLLOW_BURST_SPARK_LIFE 0x20 /* BurstActorWithSound spark life */

/* A linked follower's animations besides the poses equal to its mode. */
enum LinkedFollowerAnim {
  LFOLLOW_ANIM_TUMBLE = 4, /* LFOLLOW_KNOCKED, after the knock (3) */
};

/* ->mode of a balloon (ACTOR_TYPE_BALLOON). */
enum BalloonMode {
  BALLOON_INIT = 0, /* records its rest height */
  BALLOON_BOB = 1,  /* bobs about it (subMode is the phase) */
};

/* ->mode of a war drum (ACTOR_TYPE_WAR_DRUM, WarDrumState; levels 18/19); the
 * mode is also the animation index. */
enum WarDrumMode {
  WARDRUM_WAIT = 0,    /* until WarDrumState.wait runs out: BEAT */
  WARDRUM_BEAT = 2,    /* beats on frame 6 (resuming its chunk animation), then WAIT */
  WARDRUM_KNOCKED = 3, /* knocked spinning away, then bursts: GONE */
  WARDRUM_GONE = 4,    /* burst; despawns once its chunk animation halts */
};
/* War-drum tuning. A beat on frame BEAT_FRAME runs its chunk animation for
 * BEAT_TICKS, then branches it back over FADE_TICKS. Hit, it spins off from
 * KNOCK_SPEED (FLAME_KNOCK_SPEED flamed) with KNOCK_VZ (FLAME_KNOCK_VZ) up,
 * losing KNOCK_DRAG and KNOCK_GRAVITY a frame, and bursts with
 * BURST_SPARK_LIFE sparks on landing or after KNOCK_LIFE ticks. */
#define WARDRUM_BEAT_FRAME 6
#define WARDRUM_BEAT_TICKS 0x64
#define WARDRUM_FADE_TICKS 0x14
#define WARDRUM_KNOCK_SPEED 0x12C
#define WARDRUM_KNOCK_VZ 0x82
#define WARDRUM_FLAME_KNOCK_SPEED 0x96
#define WARDRUM_FLAME_KNOCK_VZ 0x46
#define WARDRUM_KNOCK_DRAG 0xC
#define WARDRUM_KNOCK_GRAVITY 0xC
#define WARDRUM_KNOCK_LIFE 0x32
#define WARDRUM_BURST_SPARK_LIFE 0x18

/* ->mode of a lantern (ACTOR_TYPE_LANTERN, LanternState; level 25); the mode
 * is also the animation index. ->subMode holds the LANTERN_F_* bits, which
 * the grabbers and alarm sentries that link a lantern (level data) watch. */
#define LANTERN_F_LIT 0x1     /* lit (its glow node grows) */
#define LANTERN_F_SETTLED 0x2 /* not mid-toggle; a hit sets it with LIT */
#define LANTERN_F_HELD 0x4    /* a grabber holds it as is: IDLE stops toggling */
enum LanternMode {
  LANTERN_IDLE = 0,     /* on the timer, toggles: lit ones SNUFF, dark ones LIGHT */
  LANTERN_SNUFF = 1,    /* goes out, then IDLE */
  LANTERN_LIGHT = 2,    /* lights up, then IDLE */
  LANTERN_STRUCK = 3,   /* Spyro hit it: lit for good, with an ACTOR_TYPE_LANTERN_FLAME */
  LANTERN_HOLD_LIT = 4, /* stays lit, then spawns the closing flame: DONE */
  LANTERN_DONE = 5,
};

/* ->mode of a lantern flame (ACTOR_TYPE_LANTERN_FLAME); the mode is also
 * the animation index. Both despawn when their animation ends. */
enum LanternFlameMode {
  LFLAME_STRIKE = 0, /* LANTERN_STRUCK's burst */
  LFLAME_CLOSE = 1,  /* LANTERN_HOLD_LIT's last flame: puts the lantern back to LANTERN_IDLE */
};

/* CarriedRockState.mode while the rock is TROCK_HELD. */
enum CarriedRockHold {
  CROCK_RELEASED = 0, /* let go on the thrower's frame 4: aims and flies */
  CROCK_IN_HANDS = 1, /* set by the thrower at spawn: rides between its hands */
  CROCK_PARKED = 0x63, /* no contact test and no motion; nothing in level 28 sets it */
};

/* ->mode of Jacques' thrown rock (ACTOR_TYPE_THROWN_ROCK, CarriedRockState;
 * level 28). */
enum ThrownRockMode {
  TROCK_HELD = 0,    /* carried; released on the throw frame: aims at Spyro, FLY */
  TROCK_BREAK = 1,   /* touched Spyro (or dropped): break animation, then despawn */
  TROCK_SMASH = 2,   /* hit the ground or timed out: smash animation, then despawn */
  TROCK_FLY = 9,     /* flies its arc toward Spyro's feet */
};

/* ->mode of a comet cannon (ACTOR_TYPE_COMET_CANNON, CometCannonState; level
 * 24). */
enum CometCannonMode {
  CCANNON_WATCH = 0,  /* lit by Spyro's flame from behind it: ARM; cued (subMode): LOB */
  CCANNON_ARM = 1,    /* sets the fuse, then FIRE */
  CCANNON_FUSE = 2,   /* smokes until the fuse runs out, then FIRE (unreachable: no writer in the C sources, and level data starts every one in mode 0) */
  CCANNON_FIRE = 3,   /* fires an ACTOR_TYPE_COMET at the best target ahead, then WATCH */
  CCANNON_LOB = 10,   /* turns to the cued target and lobs a comet at it, then WATCH */
};

/* Comet cannon tuning. With Spyro beside it (within LOCK_RANGE, LOCK_RANGE_HELD
 * once locked) the camera locks on it. Flamed from behind with its fuse
 * clear it fires: the comet leaves MUZZLE_FWD ahead and MUZZLE_UP above at
 * COMET_SPEED (COMET_VZ up, live COMET_LIFE), re-aimed at the best target
 * within SEEK_CONE ahead (under SEEK_QUICK Manhattan, exact under SEEK_EXACT,
 * at most SEEK_RANGE; scored by distance outside SEEK_NEAR..SEEK_FAR plus
 * 4096 per unit of angle) and lobbed at AIM_SPEED (a lunging guard aimed
 * GUARD_AIM_LIFT up). A cued LOB fires LOB_SPEED from LOB_UP up. */
#define CCANNON_LOCK_RANGE 0x4E2
#define CCANNON_LOCK_RANGE_HELD 0x578
#define CCANNON_FUSE_FRAMES 0x4B
#define CCANNON_MUZZLE_FWD 0x258
#define CCANNON_MUZZLE_UP 0x352
#define CCANNON_COMET_SPEED 0x320
#define CCANNON_COMET_VZ 0x96
#define CCANNON_COMET_LIFE 0x8C
#define CCANNON_SEEK_QUICK 0x10FFF
#define CCANNON_SEEK_CONE 7
#define CCANNON_SEEK_EXACT 0xBFFF
#define CCANNON_SEEK_RANGE 0xAFFF
#define CCANNON_SEEK_NEAR 0x4000
#define CCANNON_SEEK_FAR 0x5800
#define CCANNON_AIM_SPEED 0x258
#define CCANNON_GUARD_AIM_LIFT 0x320
#define CCANNON_LOB_SPEED 0x320
#define CCANNON_LOB_UP 0x578
#define CCANNON_LOB_LIFE 0x78

/* ->mode of a comet (ACTOR_TYPE_COMET, CometState; level 24). */
enum CometMode {
  COMET_FLY = 0,  /* flies its arc, dropping sparks and laying its trail */
  COMET_HIT = 1,  /* hit something or timed out: hides, then FADE */
  COMET_FADE = 2, /* the trail fades out, then despawn */
};

/* ->mode of a trail bomb (ACTOR_TYPE_TRAIL_BOMB, TrailBombState; level 33,
 * thrown by Gnasty); the comet's machine. */
enum TrailBombMode {
  TRAILBOMB_FLY = 0,  /* flies, laying its trail; landing spawns an ACTOR_TYPE_SHOCKWAVE */
  TRAILBOMB_HIT = 1,  /* landed or timed out: hides, then FADE */
  TRAILBOMB_FADE = 2, /* the trail fades out, then despawn */
};

/* A shockwave's animations (ACTOR_TYPE_SHOCKWAVE): it spreads while its
 * timer runs, hitting Spyro inside its radius, then fades and despawns. */
enum ShockwaveAnim {
  SHOCKWAVE_ANIM_SPREAD = 0,
  SHOCKWAVE_ANIM_FADE = 1,
};

/* ->mode of a lane patroller (ACTOR_TYPE_LANE_PATROLLER, LanePatrollerState;
 * level 33); the mode is also the animation index. */
enum LanePatrollerMode {
  LANEPAT_WATCH = 0,   /* faces Spyro; near: RUN its lane */
  LANEPAT_RUN = 1,     /* runs its lane path, turning back at Spyro, to the far end */
  LANEPAT_KNOCKED = 2, /* knocked away, then bursts */
};

/* Lane patroller tuning. Hit it is knocked off at KNOCK_SPEED
 * (CHARGE_KNOCK_SPEED for a charge; KNOCK_VZ up). It runs its lane once
 * Spyro is within NOTICE_RANGE / NOTICE_DZ, from RUN_SPEED, slowing
 * SLOW_STEP a frame to MIN_SPEED after SLOW_FRAMES (LANE1_SLOW_FRAMES on
 * lane 1); each finished run (up to MAX_RUNS - 1, g_anLevelEscapeRunCount)
 * takes 6 off both. It falls GRAVITY a frame down to -MAX_FALL (floor
 * probed FLOOR_PROBE_RAISE up), jumps at JUMP_VZ on PATH_JUMP_LOW nodes, is
 * put back on its node below FELL_BELOW_Z, and within TURN_RANGE turns back
 * at Spyro just ahead on its lane. */
#define LANEPAT_KNOCK_SPEED 0xFA
#define LANEPAT_CHARGE_KNOCK_SPEED 0x145
#define LANEPAT_KNOCK_VZ 0x28
#define LANEPAT_NOTICE_RANGE 0x1964
#define LANEPAT_NOTICE_DZ 0x578
#define LANEPAT_RUN_SPEED 0x12C
#define LANEPAT_SLOW_FRAMES 0x6E
#define LANEPAT_LANE1_SLOW_FRAMES 0x96
#define LANEPAT_SLOW_STEP 0xA
#define LANEPAT_MIN_SPEED 0xDC
#define LANEPAT_MAX_RUNS 6
#define LANEPAT_GRAVITY 0x1E
#define LANEPAT_MAX_FALL 0x190
#define LANEPAT_FLOOR_PROBE_RAISE 0xBB8
#define LANEPAT_JUMP_VZ 0x12C
#define LANEPAT_FELL_BELOW_Z 0x800
#define LANEPAT_TURN_RANGE 0x3000
#define LANEPAT_BURST_SPARK_LIFE 0x20 /* BurstActorWithSound spark life */
#define LANEPAT_KNOCK_DRAG 0xC /* DriveActorTrackedMove friction */

/* ->mode of a runner spawner (ACTOR_TYPE_RUNNER_SPAWNER, RunnerSpawnerState;
 * level 14); the mode is also the animation index. */
enum RunnerSpawnerMode {
  RSPAWNER_IDLE = 0,    /* on the timer, with no runner out: RELEASE; else FIDGET */
  RSPAWNER_FIDGET = 1,  /* idle variant, then IDLE */
  RSPAWNER_FLINCH = 2,  /* flinch animation (flag bit 0), then IDLE */
  RSPAWNER_RELEASE = 3, /* sends out an ACTOR_TYPE_WAYPOINT_RUNNER on frame 5 */
  RSPAWNER_KNOCKED = 4, /* bursts */
};
#define RSPAWNER_BURST_SPARK_LIFE 0x18 /* BurstActorWithSound spark life */

/* ->mode of a waypoint runner (ACTOR_TYPE_WAYPOINT_RUNNER,
 * WaypointRunnerState; level 14). On reaching its target it makes the
 * target flinch (flag bit 0). */
enum WaypointRunnerMode {
  WPRUNNER_EMERGE = 0, /* emerge animation (gone if its spawner is), then RUN */
  WPRUNNER_RUN = 1,    /* runs its waypoints; touching Spyro or its target: POP */
  WPRUNNER_POP = 2,    /* pops and despawns, freeing its spawner */
};

/* SoldierState.kind of a level 6 soldier (ACTOR_TYPE_SOLDIER / _B): which
 * behaviour its SOLDIER_IDLE and SOLDIER_CHASE run. SoldierState.link is the
 * soldier's cannon for the cannon kinds. */
enum SoldierKind {
  SOLDIERKIND_PATROL = 0,        /* walks its path; Spyro near: ALERT, then chases him */
  SOLDIERKIND_SENTRY = 1,        /* stands; Spyro near: alerts its group, then chases */
  SOLDIERKIND_CANNON_RUNNER = 2, /* alerts its group, runs to its cannon, then GUNNER */
  SOLDIERKIND_DUELIST = 3,       /* fires its cannon at a rival (SOLDIER_DUEL) */
  SOLDIERKIND_BOUNDED_PATROL = 4, /* patrols; chases until it enters SoldierState.vol */
  SOLDIERKIND_WANDER_TO_POST = 5, /* wanders; Spyro near: its group heads TO_POST */
  SOLDIERKIND_WANDER_TO_CANNON = 6, /* the same; with its post gone becomes CANNON_RUNNER */
  SOLDIERKIND_GUNNER = 8,        /* mans its cannon from the start (SOLDIER_GUNNER) */
  SOLDIERKIND_SHORT_LEASH = 9,   /* faces Spyro; chases him only near home */
  SOLDIERKIND_BOUNDED_SENTRY = 10, /* stands; chases until it enters SoldierState.vol */
  SOLDIERKIND_SHOVER = 11,       /* faces Spyro; shoves him when close */
};

/* ->mode of the simple flight-level targets: ACTOR_TYPE_RIDING_FLIGHT_TARGET,
 * ACTOR_TYPE_BLASTABLE_PATH_SMOKER / _PROP (levels 5/29),
 * ACTOR_TYPE_CYCLING_FLIGHT_TARGET (23) and ACTOR_TYPE_BOBBING_FLIGHT_TARGET
 * (29). */
enum FlightTargetMode {
  FTARGET_ACTIVE = 0, /* moves or idles until struck */
  FTARGET_STRUCK = 1, /* struck: pays out (or bursts and despawns) */
  FTARGET_SPENT = 2,  /* the cycling and bobbing kinds: faces Spyro afterwards */
};

/* ->mode of the level 2 hopping critter (ACTOR_TYPE_HOPPING_CRITTER) and the
 * level 3 grazing critter (ACTOR_TYPE_GRAZING_CRITTER, GrazerState); the mode
 * is also the animation index. */
enum GrazerMode {
  GRAZER_WANDER = 0,  /* UpdateActorWander; on its timer: PAUSE */
  GRAZER_PAUSE = 1,   /* idle animation, then WANDER */
  GRAZER_KNOCKED = 2, /* hit: bursts */
};
#define GRAZER_BURST_SPARK_LIFE 0x10 /* BurstActorWithSound spark life */
#define GRAZER_KNOCK_SPEED 0xFA /* knock-back speed */
#define GRAZER_KNOCK_DRAG 0xC /* DriveActorTrackedMove friction */

/* ->mode of a lobbed bomb (ACTOR_TYPE_LOBBED_BOMB; levels 25/26). */
enum LobbedBombMode {
  LOBBOMB_FLY = 0,   /* flies; landing, the timer, a wall or Spyro: BURST */
  LOBBOMB_BURST = 1, /* sparks and despawns */
};

/* ->mode of the level 14 fleeing walker (ACTOR_TYPE_FLEEING_WALKER,
 * FleeingWalkerState); the mode is also the animation index. */
enum FleeingWalkerMode {
  FLEEWALK_WALK = 0,    /* walks its path (also held while Spyro is hurt) */
  FLEEWALK_STAND = 1,   /* at a path node / end: stands */
  FLEEWALK_KNOCKED = 2, /* supercharged: slides off along Spyro's heading, then bursts */
  FLEEWALK_FLEE = 3,    /* Spyro close: runs from him, steering off walls */
};

/* ->mode of the level 14 chunk operator (ACTOR_TYPE_CHUNK_OPERATOR,
 * ChunkOperatorState); the mode is also the animation index. */
enum ChunkOperatorMode {
  CHUNKOP_WAIT = 0,    /* waits for its chunk to halt at step 0 (Spyro near) or 0xE (far): START */
  CHUNKOP_RUN = 1,     /* keeps the chunk running (riding operators, after a FLINCH) */
  CHUNKOP_START = 2,   /* restarted the chunk; when the animation ends: WAIT */
  CHUNKOP_KNOCKED = 3, /* hit: finishes the chunk's run and bursts */
  CHUNKOP_FLINCH = 4,  /* flamed: flinches, then RUN */
};
#define CHUNKOP_BURST_SPARK_LIFE 0x18 /* BurstActorWithSound spark life */

/* ->mode of the overhead thrower (ACTOR_TYPE_OVERHEAD_THROWER; levels 9/10);
 * the mode is also the animation index. */
enum OverheadThrowerMode {
  OTHROW_HOLD = 0, /* spins its gem (state word 0) overhead, COLLECT_INERT */
  OTHROW_HIT = 1,  /* charged or supercharged: flings the gem at Spyro (COLLECT_BOUNCE), then HOLD */
};

/* ->mode of the level 8 roaming critter (ACTOR_TYPE_ROAMING_CRITTER,
 * RoamingCritterState). A hit always lands in DYING; KNOCKED's slide only
 * runs when entered directly. */
enum RoamingCritterMode {
  ROAMCRIT_WANDER = 0,  /* UpdateActorWander */
  ROAMCRIT_KNOCKED = 5, /* slides along the knock heading, bursts; falls into DYING */
  ROAMCRIT_DYING = 6,   /* death animation 6 (wandering its first frames), then ROAMCRIT_ANIM_DEAD; despawns */
};
#define ROAMCRIT_BURST_SPARK_LIFE 0x10 /* BurstActorWithSound spark life */

/* A roaming critter's animations besides the poses equal to its mode. */
enum RoamingCritterAnim {
  ROAMCRIT_ANIM_DEAD = 7,
};

/* ->mode of a checkpoint (ACTOR_TYPE_CHECKPOINT .. + 2; every ground level). */
enum CheckpointMode {
  CHECKPT_REARM = 0,     /* waits for Spyro to walk away (> CHECKPT_REARM_RANGE), then ARMED */
  CHECKPT_ARMED = 1,     /* Spyro touches it: sets the respawn anchor, FLASH */
  CHECKPT_FLASH = 2,     /* subMode counts frames to CHECKPT_FLASH_FRAMES, then REARM */
  CHECKPT_HELD = 3,      /* its dragon statue holds it until the rescue (no arm runs) */
};

/* Checkpoint ranges to Spyro (2D ApproxDist2D / height difference) and the
 * flash length. The save host re-arms on the same distance from its anchor
 * checkpoint (SAVEHOST_REARM_RANGE). */
#define CHECKPT_REARM_RANGE 0xA00
#define CHECKPT_TOUCH_RANGE 0x380
#define CHECKPT_TOUCH_DZ 0x200
#define CHECKPT_FLASH_FRAMES 0x30

/* A checkpoint's animations: the flash plays while CHECKPT_FLASH runs. */
enum CheckpointAnim {
  CHECKPT_ANIM_REST = 0,
  CHECKPT_ANIM_FLASH = 1,
};

/* Animated scenery (ACTOR_TYPE_ANIMATED_SCENERY / _B): its idle loop, the
 * hit it plays when flamed, and the loop it settles into after the hit. */
enum SceneryAnim {
  SCENERY_ANIM_IDLE = 0,
  SCENERY_ANIM_HIT = 1,
  SCENERY_ANIM_AFTER_HIT = 2,
};

/* A checkpoint that is not waiting for Spyro's touch: REARM, or FLASH at
 * least 16 frames in. A save host (SaveHostState.anchor is always a
 * checkpoint in the level data) only opens the save menu at a settled one. */
#define CHECKPT_SETTLED(a) \
  ((a)->mode == CHECKPT_REARM || ((a)->mode == CHECKPT_FLASH && (a)->subMode >= 0x10))

/* ->mode of a lookout (LookoutState): ACTOR_TYPE_GUARD_LOOKOUT (level 8) and
 * ACTOR_TYPE_SHEMP_LOOKOUT (level 10); the update switches on a local copy. */
enum GuardLookoutMode {
  GLOOK_WATCH = 0,   /* idles facing home / Spyro; rouses its idle guard when Spyro is in range */
  GLOOK_SWIPE = 6,   /* Spyro close below it: swipes him away, then WATCH */
  GLOOK_KNOCKED = 7, /* flamed or supercharged: knocked away, then bursts */
  GLOOK_ROUSE = 10,  /* turns to its guard and wakes it on frame 5, then WATCH */
};

/* A lookout's animations (GLOOK_WATCH's own index, 0, is its stand facing
 * Spyro). Far from Spyro it loops IDLE (frames 5..0xF, a random 3..5 times),
 * then picks FIDGET (one in five) or STAND; near him it alternates the watch
 * stand with WATCH_ALT. */
enum GuardLookoutAnim {
  GLOOK_ANIM_FIDGET = 1,
  GLOOK_ANIM_IDLE = 2,
  GLOOK_ANIM_STAND = 4,     /* also before and after GLOOK_ROUSE's call */
  GLOOK_ANIM_WATCH_ALT = 5,
  GLOOK_ANIM_CALL = 8,      /* GLOOK_ROUSE: the guard wakes on frame 5 */
};
/* Lookout tuning (levels 8 and 10; `range` in 0x400 units). WATCH rouses its
 * guard with Spyro above it within `range` and ROUSE_DZ, idles with him
 * beyond IDLE_RANGE (its idle loops from IDLE_LOOP_END back to
 * IDLE_LOOP_START), and swipes (SWIPE_GAP ticks apart) at Spyro above it
 * within SWIPE_RANGE and SWIPE_DZ. The swipe on frame SWIPE_FRAME throws
 * Spyro within SWIPE_HIT_RANGE back at SWIPE_PUSH, SWIPE_PUSH_VZ up, and
 * holds the camera on the lookout for CAM_TICKS at least CAM_DIST (or
 * CAM_BACK past Spyro) away. ROUSE calls on frame CALL_FRAME, cueing the
 * guard CUE_TICKS and waiting CHASE_WAIT (or the guard's chaseDelay) before
 * the next. Flamed, it slides off along Spyro's heading from KNOCK_SPEED,
 * losing KNOCK_DRAG a frame (contact KNOCK_CONTACT, radius MOVE_RADIUS),
 * and bursts with BURST_SPARK_LIFE sparks. */
#define GLOOK_ROUSE_DZ 0x5DC
#define GLOOK_IDLE_RANGE 0x2000
#define GLOOK_IDLE_LOOP_END 0xF
#define GLOOK_IDLE_LOOP_START 5
#define GLOOK_SWIPE_GAP 0x8C
#define GLOOK_SWIPE_RANGE 0x9C4
#define GLOOK_SWIPE_DZ 0x3E8
#define GLOOK_SWIPE_FRAME 3
#define GLOOK_SWIPE_HIT_RANGE 0xA8C
#define GLOOK_SWIPE_PUSH 0x82
#define GLOOK_SWIPE_PUSH_VZ 0x32
#define GLOOK_CAM_TICKS 0x28
#define GLOOK_CAM_DIST 0xFA0
#define GLOOK_CAM_BACK 0xC00
#define GLOOK_CALL_FRAME 5
#define GLOOK_CUE_TICKS 0x1A
#define GLOOK_CHASE_WAIT 0x14
#define GLOOK_KNOCK_SPEED 0xF0
#define GLOOK_KNOCK_DRAG 0x10
#define GLOOK_KNOCK_CONTACT 0x1F4
#define GLOOK_MOVE_RADIUS 0x2BC
#define GLOOK_BURST_SPARK_LIFE 0x30
/* Level 8's guard lookout shares the tuning above, but swipes (no
 * cooldown) within GUARD_SWIPE_RANGE, throwing Spyro back at
 * GUARD_SWIPE_PUSH, GUARD_SWIPE_PUSH_VZ up. */
#define GLOOK_GUARD_SWIPE_RANGE 0xA8C
#define GLOOK_GUARD_SWIPE_PUSH 0x78
#define GLOOK_GUARD_SWIPE_PUSH_VZ 0x28

/* ->mode of the level 16 humming bell (ACTOR_TYPE_HUMMING_BELL,
 * HummingBellState). */
enum HummingBellMode {
  HBELL_INIT = 0,   /* first frame: HUM */
  HBELL_STRUCK = 1, /* flamed or supercharged: bursts, runs its chunk to step 0, despawns */
  HBELL_HUM = 2,    /* sparks at its target and restarts its chunk every period */
};
#define HBELL_BURST_SPARK_LIFE 0x20 /* BurstActorWithSound spark life */

/* ->mode of an attach bead (ACTOR_TYPE_ATTACH_BEAD, AttachBeadState; level
 * 19), one of the three kids a bead sentry keeps streaming between its two
 * attach points; ->subMode is its index. */
enum AttachBeadMode {
  BEAD_INIT = 0,    /* staggers its start by its index, then DELAY */
  BEAD_DELAY = 1,   /* counts up to 0, then STREAM */
  BEAD_STREAM = 2,  /* slides attach point 1 -> 0, scaling up */
  BEAD_DESPAWN = 3, /* set by the sentry when it is knocked */
};

/* ->mode of the lob gunner's lob (ACTOR_TYPE_LOB_GUNNER_LOB, LobState;
 * level 32). It waits in the gunner's hand until ->subMode is set. */
enum LobGunnerLobMode {
  LGLOB_FLY = 0,     /* flies its arc; hitting something or its timer: BURST */
  LGLOB_BURST = 1,   /* knocks Spyro back when near, sparks, despawns */
  LGLOB_DESPAWN = 2, /* set by the gunner: knocked holding it, or no arc to the target */
};

/* ->mode of the level 22 platform pod (ACTOR_TYPE_PLATFORM_POD,
 * PlatformPodState); the mode is also the animation index. */
enum PlatformPodMode {
  PPOD_WAIT = 0, /* closed; on its timer: OPEN */
  PPOD_OPEN = 1, /* opens; when done, raises its ACTOR_TYPE_BOBBING_PLATFORMs: UP */
  PPOD_UP = 2,   /* lobs ACTOR_TYPE_POD_SHOTs at its partner; on its timer drops them, WAIT */
};

/* ->mode of the level 6 breakable prop (ACTOR_TYPE_BREAKABLE_PROP); the mode
 * is also the animation index. */
enum BreakablePropMode {
  BPROP_IDLE = 0,
  BPROP_WOBBLE = 1, /* plays out, then IDLE; set by a soldier ducking into it (SOLDIER_ENTER_POST) */
  BPROP_BREAK = 2,  /* flamed, or supercharged twice in half a second: breaks, despawns */
};

/* ->mode of the level 7 shooter's shot (ACTOR_TYPE_SHOOTER_SHOT,
 * ShooterShotState). */
enum ShooterShotMode {
  SSHOT_FLY = 0,   /* flies; hitting a wall, Spyro (knocked back) or its timer: BURST */
  SSHOT_BURST = 1, /* debris and sparks, despawns */
};

/* ->mode of the level 0 pressure switch (ACTOR_TYPE_PRESSURE_SWITCH,
 * SwitchState); the mode is also the animation index, and ->subMode tracks
 * Spyro standing on it. */
enum PressureSwitchMode {
  PSWITCH_UP = 0,
  PSWITCH_DOWN = 1, /* its bit in the linked actor's subMode is set */
};

/* ->subMode of the pressure switch: Spyro stepping on it. */
enum PressureSwitchStep {
  PSWITCH_CLEAR = 0,    /* Spyro steps on: toggle its bit in the prop's subMode */
  PSWITCH_STOOD_ON = 1, /* until Spyro steps off */
};

/* ->mode of the simple two-mode projectiles: ACTOR_TYPE_FLYING_EMBER and
 * ACTOR_TYPE_FALLING_ROCK (level 25), ACTOR_TYPE_BURST_GUNNER_SHOT (level 32),
 * ACTOR_TYPE_LOBBED_BOLT (level 22) and ACTOR_TYPE_FIREBALL (level 6). */
enum ShotMode {
  SHOT_FLY = 0,   /* moves; landing, a wall, Spyro or its timer: BURST */
  SHOT_BURST = 1, /* bursts (or plays its shatter), then despawns */
};

/* ->mode of the level 26 partner platform (ACTOR_TYPE_PARTNER_PLATFORM,
 * PartnerPlatformState); the mode is also the animation index. */
enum PartnerPlatformMode {
  PPLAT_BOB = 0,   /* bobs on its phase, flagging its partner's subMode near the top */
  PPLAT_BREAK = 1, /* flamed or supercharged: breaks, then despawns */
};

/* ->mode of the level 26 sinking block (ACTOR_TYPE_SINKING_BLOCK,
 * SinkingBlockState); the mode is also the animation index. */
enum SinkingBlockMode {
  SINKBLK_IDLE = 0,
  SINKBLK_SINK = 1, /* struck: sets its partner's mode to 1 and sinks into the floor */
};

/* ->mode of a homeworld talker (ACTOR_TYPE_HOME_TALKER .. + 4; levels
 * 0/6/12/18/24). Level 30's boss-door trigger (+ 5) runs the same two. */
enum HomeTalkerMode {
  TALKER_REARM = 0, /* waits for Spyro to walk away (TALKER_REARM_RANGE), then READY */
  TALKER_READY = 1, /* Spyro walks up facing it: starts its actor cutscene, REARM */
};
/* A READY talker starts talking when Spyro, on foot or charging, is within
 * TALK_RANGE (2D), heading within SPYRO_FACING_ANGLE (8-bit) of it, and it
 * faces him within FACING_ANGLE; it re-arms once he is REARM_RANGE away. */
#define TALKER_REARM_RANGE 0x1000
#define TALKER_TALK_RANGE 0x780
#define TALKER_SPYRO_FACING_ANGLE 0x20
#define TALKER_FACING_ANGLE 0x28

/* ->mode of the level 32 flame chunk switch (ACTOR_TYPE_FLAME_CHUNK_SWITCH,
 * FlameChunkSwitchState); the mode is also the animation index. */
enum FlameChunkSwitchMode {
  FCSWITCH_IDLE = 0,
  FCSWITCH_TRIPPED = 1, /* flamed: resumes chunk 0's animation; re-arms after 0x50 frames */
};

/* BreakableWallState.kind (ACTOR_TYPE_BREAKABLE_WALL; level 27). */
enum BreakableWallKind {
  BWALL_PLAIN = 0,   /* any hit on a part breaks it */
  BWALL_ARMORED = 1, /* only a supercharge or superflame (ACTOR_HIT_SUPER) breaks it */
};

/* A breakable wall's animations: the armored kind wears ARMORED, and so
 * does its rubble. */
enum BreakableWallAnim {
  BWALL_ANIM_PLAIN = 0,
  BWALL_ANIM_ARMORED = 1,
};

/* SuperflameGiverState.kind: how long the granted superflame lasts. */
enum SuperflameGiverKind {
  SFKIND_TIMED = 0,       /* SFGIVER_TIMED_FRAMES */
  SFKIND_ENDLESS = 1,     /* SFGIVER_ENDLESS_FRAMES: effectively for good */
  SFKIND_SHORT = 2,       /* SFGIVER_SHORT_FRAMES */
  SFKIND_ENDLESS_FAR = 3, /* ENDLESS, granted from SFGIVER_GRANT_RANGE_FAR */
};
/* Superflame durations (g_nSpyroSuperflameTimer frames) per kind. */
#define SFGIVER_TIMED_FRAMES 0x384
#define SFGIVER_SHORT_FRAMES 0x30C
#define SFGIVER_ENDLESS_FRAMES 0x9450C00
/* Superflame-giver tuning. SFGIVER_IDLE grants once Spyro (no superflame
 * yet) is within GRANT_RANGE (GRANT_RANGE_FAR for SFKIND_ENDLESS_FAR) and
 * GRANT_DZ, facing it within GRANT_FACING; an alert drops it CIRCLE_DROP
 * into SFGIVER_CIRCLE, which steers round its first path node at
 * CIRCLE_RADIUS and CIRCLE_SPEED. SFGIVER_GRANT flies (FLY_SPEED, snapping
 * within ARRIVE_RANGE; climbing at most CLIMB a frame, snapping within
 * SETTLE_DZ) to AHEAD in front of and AHEAD_DROP below Spyro for
 * APPROACH_TICKS, with the camera anchored CAM_AHEAD in front of and
 * CAM_RAISE above him at CAM_DISTANCE. The cast throws a spread mote
 * MOTE_REACH ahead (MOTE_REACH_ALT for the ALT giver) from frame MOTE_FRAME
 * and grants on frame GRANT_FRAME (GRANT_FRAME_ALT), lingering LINGER_TICKS.
 * SFGIVER_RETURN flies home the same way; SFGIVER_WALK walks its path at
 * WALK_SPEED. */
#define SFGIVER_GRANT_RANGE 0xA8C
#define SFGIVER_GRANT_RANGE_FAR 0xD48
#define SFGIVER_GRANT_DZ 0x7D0
#define SFGIVER_GRANT_FACING 0x1E
#define SFGIVER_CIRCLE_DROP 0x1F4
#define SFGIVER_CIRCLE_RADIUS 0xBB8
#define SFGIVER_CIRCLE_SPEED 0x78
#define SFGIVER_FLY_SPEED 0x64
#define SFGIVER_ARRIVE_RANGE 0x50
#define SFGIVER_CLIMB 0x32
#define SFGIVER_SETTLE_DZ 0x28
#define SFGIVER_AHEAD 0xE1
#define SFGIVER_AHEAD_DROP 0x17C
#define SFGIVER_APPROACH_TICKS 0x4B
#define SFGIVER_CAM_AHEAD 0x4B
#define SFGIVER_CAM_RAISE 0x64
#define SFGIVER_CAM_DISTANCE 0x400
#define SFGIVER_MOTE_REACH 0x140
#define SFGIVER_MOTE_REACH_ALT 0x190
#define SFGIVER_MOTE_FRAME 6
#define SFGIVER_GRANT_FRAME 7
#define SFGIVER_GRANT_FRAME_ALT 0xE
#define SFGIVER_LINGER_TICKS 0xA
#define SFGIVER_WALK_SPEED 0x96

/* PathRunnerState.mode: which of its two runs a path runner makes. */
enum PathRunnerRun {
  PRUN_FIRST = 0,  /* drives goal flag 0 */
  PRUN_SECOND = 1, /* drives goal flag 1 */
};
/* Path-runner tuning. Hit, it slides from KNOCK_SPEED (CHARGE_KNOCK_SPEED
 * charged) and KNOCK_VZ up, losing KNOCK_DRAG and KNOCK_GRAVITY a frame, and
 * bursts with BURST_SPARK_LIFE sparks. RUNNER_RUN: RUN_SPEED (or the
 * node's), at least CHASED_SPEED with Spyro within CHASED_RANGE; it reaches
 * a node within PIVOT_THRESH, falls RUN_GRAVITY a frame (FLEE_GRAVITY when
 * fleeing) to -MAX_FALL onto the floor found FLOOR_PROBE_RAISE above it, and
 * stops to prod its target within PROD_RANGE. A run's goal flag is raised
 * at its goal node with Spyro within GOAL_RANGE and GOAL_DZ. */
#define PRUN_KNOCK_SPEED 0x3C
#define PRUN_CHARGE_KNOCK_SPEED 0x55
#define PRUN_KNOCK_VZ 0x3C
#define PRUN_KNOCK_DRAG 0xC
#define PRUN_KNOCK_GRAVITY 0xC
#define PRUN_BURST_SPARK_LIFE 0x18
#define PRUN_RUN_SPEED 0xB4
#define PRUN_CHASED_SPEED 0xAA
#define PRUN_CHASED_RANGE 0x1400
#define PRUN_PIVOT_THRESH 0x17C
#define PRUN_RUN_GRAVITY 0x1E
#define PRUN_FLEE_GRAVITY 10
#define PRUN_MAX_FALL 0x190
#define PRUN_FLOOR_PROBE_RAISE 0xBB8
#define PRUN_PROD_RANGE 0x9C4
#define PRUN_FIRST_GOAL_NODE 0x1C
#define PRUN_FIRST_GOAL_RANGE 0x1C00
#define PRUN_FIRST_GOAL_DZ 0x898
#define PRUN_SECOND_GOAL_NODE 0xA
#define PRUN_SECOND_GOAL_RANGE 0x1D4C
#define PRUN_SECOND_GOAL_DZ 0xBB8
/* Jump launches (PATH_JUMP_*): running RUN_JUMP_LOW / MID; fleeing
 * FLEE_JUMP_LOW, FLEE_JUMP_MID (FLEE_JUMP_MID_SECOND on the second run),
 * FLEE_JUMP_HIGH. */
#define PRUN_RUN_JUMP_LOW 0xDC
#define PRUN_RUN_JUMP_MID 0x140
#define PRUN_FLEE_JUMP_LOW 0x17C
#define PRUN_FLEE_JUMP_MID 0x104
#define PRUN_FLEE_JUMP_MID_SECOND 0xD2
#define PRUN_FLEE_JUMP_HIGH 0xC8
/* RUNNER_WAIT flees once Spyro is within `range` and facing it within
 * SPOT_FACING; RUNNER_FLEE runs at FLEE_SPEED (or the node's), kicking up
 * sparks above SPARK_SPEED. RUNNER_DODGE circles its path point away from
 * Spyro at DODGE_SPEED, creeping (CREEP_SPEED) after DODGE_TICKS lined up
 * within DODGE_SETTLED; CREEP taunts once within CREEP_SETTLED and both
 * dodge again when Spyro charges or comes within CREEP_ALERT_RANGE /
 * TAUNT_ALERT_RANGE. LOOKOUT returns home (RETURN_SPEED, arriving within
 * HOME_RANGE) once Spyro is below it and within LOOKOUT_RANGE of its base. */
#define PRUN_SPOT_FACING 0x20
#define PRUN_FLEE_SPEED 0xF0
#define PRUN_SPARK_SPEED 0x118
#define PRUN_DODGE_SPEED 0x64
#define PRUN_DODGE_SETTLED 0x23
#define PRUN_DODGE_TICKS 0x78
#define PRUN_CREEP_SPEED 0xC8
#define PRUN_CREEP_SETTLED 0x46
#define PRUN_CREEP_ALERT_RANGE 0xFA0
#define PRUN_TAUNT_ALERT_RANGE 0x12C0
#define PRUN_LOOKOUT_RANGE 0x1964
#define PRUN_RETURN_SPEED 0x118
#define PRUN_HOME_RANGE 0x12C

/* LootVortexState.mode (ACTOR_TYPE_LOOT_VORTEX; levels 33/34). */
enum LootVortexStage {
  LVORTEX_LOCKED = 0, /* hidden until its key's loot thief is beaten: OPEN (key carried) */
  LVORTEX_OPEN = 1,   /* Spyro walks into its pad facing the target: ACTIVE */
  LVORTEX_ACTIVE = 2, /* freezes Spyro and plays its sequence; when he is free again: FINISH */
  LVORTEX_FINISH = 3, /* runs its chunk anim, resets the camera stick and the key state */
};
/* A loot vortex opens once its key actor is dormant or, for a lane
 * patroller key, knocked; Spyro then collects it within TOUCH_RANGE (2D and
 * height). LVORTEX_ACTIVE replays the locked chest's key sequence on the
 * same clock (LOCKCHEST_LOCK_FWD/UP, KEY_SPIN/TURN_TICK, INSERT/BURST/
 * SKIP_TICK), with its target actor as the chest. */
#define LVORTEX_TOUCH_RANGE 0x200

/* ->mode of a flight-level HUD tally icon (the per-class icon types the
 * flight HUD controller's HudState.types lists; TallyState). */
enum TallyIconMode {
  TALLY_ICON_UNLIT = 0,  /* class target not hit yet */
  TALLY_ICON_LIT = 1,    /* hit: tinted and spinning (also a filled finish slot) */
  TALLY_ICON_SPEND = 2,  /* row complete: set off toward the finish slot */
  TALLY_ICON_TO_SLOT = 3,/* swinging onto g_nFlightTallySlot's slot */
  TALLY_ICON_PARKED = 4, /* arrived (slot, or the end of the finale) */
  TALLY_ICON_FINALE = 5, /* all four slots filled: set off toward the results ring */
  TALLY_ICON_TO_RING = 6,
  TALLY_ICON_SPIRAL = 7, /* spirals into the centre, then PARKED */
};

/* AmbientSoundState.mode picks the sample-bank voice set. Level data only
 * places modes 0..7 in the Artisans levels (0..3), where the slots they play
 * are level-specific, so those values stay literal; AMBIENT_MODE_PATH also
 * carries the source along its path, one leg at a time. -1 = silent. */
enum AmbientSoundMode {
  AMBIENT_MODE_PATH = 2,
  AMBIENT_MODE_PORTAL = 8,    /* SFX_PORTAL_HUM: the source on every exit whirlwind
                               * (type 9) and home-world portal (type 1) */
  AMBIENT_MODE_WHIRLWIND = 9, /* SFX_WHIRLWIND; no level places it */
};

/* AmbientSoundState.sub while AMBIENT_MODE_PATH. */
enum AmbientSoundPathStep {
  AMBIENT_STEP_WAIT = 0, /* on its timer: plays a voice, then TRAVEL */
  AMBIENT_STEP_TRAVEL = 1, /* steps along the leg; at its end snaps to the next node, WAIT */
};

/* ->mode of the level 18 bell post (ACTOR_TYPE_BELL_POST); each mode plays
 * its own animation. A flame leaves it in the FLAMED pair for good. */
enum BellPostMode {
  BELLPOST_IDLE = 0,        /* a charge rings it, a flame scorches it */
  BELLPOST_RING = 1,        /* rings once, then IDLE (a flame still scorches it) */
  BELLPOST_SCORCH = 2,      /* flamed: plays out into FLAMED */
  BELLPOST_FLAMED = 3,      /* scorched idle: any hit rings it */
  BELLPOST_FLAMED_RING = 4, /* rings once, back to FLAMED */
};

/* ->mode of a charge-reactive prop (ACTOR_TYPE_CHARGE_REACT_PROP, levels
 * 2/25); each mode plays its own animation. */
enum ChargeReactMode {
  CREACT_IDLE = 0,  /* a charge starts REACT */
  CREACT_REACT = 1, /* plays the reaction once, back to IDLE */
};

/* ->mode of the level 2 flame trigger (ACTOR_TYPE_FLAME_CHUNK_TRIGGER). */
enum FlameTriggerMode {
  FLAMETRIG_ARMED = 0, /* a flame resumes its world chunk animation */
  FLAMETRIG_FIRED = 1, /* plays animation 1, then SPENT */
  FLAMETRIG_SPENT = 2, /* holds animation 2 */
};

/* ->mode of the level 20 one-shot prop (ACTOR_TYPE_ONE_SHOT_PROP); each mode
 * plays its own animation. A gunner partnered with it (level data) sets PLAY
 * from GUNNER_WAKE. */
enum OneShotPropMode {
  ONESHOT_IDLE = 0,
  ONESHOT_PLAY = 1,     /* to frame 0xE, then PLAYED */
  ONESHOT_PLAYED = 2,   /* holds */
  ONESHOT_HIT = 3,      /* any hit before HIT: to frame 0x13, then HIT_DONE */
  ONESHOT_HIT_DONE = 4, /* holds */
};

/* World chunk-anim handles (ModifyWorldChunkAnimFlags / GetWorldChunkAnimState /
 * SetWorldChunkAnimStep ids) that actor code names with a literal. Each is the
 * id byte of an entry in that level's chunk-anim tables, so the names hold
 * only within the level (or clone family) noted. */
enum ChunkAnimHandle {
  /* Level 30 (Gnorc Gnexus), ACTOR_TYPE_HUB_PATH_GATES: one per path. */
  PATHGATE_CHUNK_TWILIGHT_HARBOR = 0,
  PATHGATE_CHUNK_GNASTY_GNORC = 1,
  PATHGATE_CHUNK_GNASTYS_LOOT = 2,
  /* Level 6, ACTOR_TYPE_REWARD_BURST: resumed as it bursts (or at once
   * when its gems were already taken). */
  REWARD_BURST_CHUNK = 0,
  /* Level 32, ACTOR_TYPE_FLAME_CHUNK_SWITCH: resumed each time it is flamed
   * while armed. */
  FCSWITCH_CHUNK = 0,
  /* Level 33 (Gnasty Gnorc). */
  GNASTY_CHUNK_DOOR = 0,     /* GNASTY_AT_DOOR, Spyro in its box: he escapes */
  GNASTY_CHUNK_ESCAPE = 1,   /* flamed at his main path's end: GNASTY_PHASE_ESCAPE */
  LANEPAT_CHUNK_REVEAL = 4,  /* lane-1 patrollers stay hidden until it starts moving */
  /* Level 16 (Blowhard): the chunk his first hit releases, replayed when a
   * checkpoint restores him past phase 1. */
  BLOWHARD_CHUNK_PHASE1 = 0,
  /* Level 22 (Metalhead): resumed at init, on first sight and as he dies.
   * The phase chunks are METALHEAD_CHUNK_PHASE. */
  METALHEAD_CHUNK_ARENA = 3,
  /* Level 28 (Jacques). */
  JACQUES_CHUNK_START_A = 7, /* both resumed on his first tick */
  JACQUES_CHUNK_START_B = 8,
  JACQUES_CHUNK_STOP11 = 9,  /* struck at path stop 11 */
  JACQUES_CHUNK_STOP13 = 10, /* struck, or Spyro close, at path stop 13 */
  /* Levels 24/28, SINGER_ROLE_TWO_GATE: the singer opens A, or B once A
   * has moved. */
  SINGER_CHUNK_GATE_A = 1,
  SINGER_CHUNK_GATE_B = 2,
  /* Levels 12/13, VROLE_LEADER villager, as its sorcerer reaches path node 7. */
  VILLAGER_CHUNK_LEADER_CUE = 9,
};
/* Level 10 (Doctor Shemp): chunk N is resumed as phase N ends (stunned; in
 * SHEMP_PHASE_3, dying); all three when he is already beaten. */
#define SHEMP_PHASE_CHUNK(phase) (phase)
#define SHEMP_BURST_SPARK_LIFE 0x40 /* BurstActorWithSound spark life */
/* Level 22: Metalhead's phase N (0, 1) ends by resuming chunk N + 4. */
#define METALHEAD_CHUNK_PHASE(phase) ((phase) + 4)
#define METALHEAD_BURST_SPARK_LIFE 0x40 /* BurstActorWithSound spark life */
/* Level 4: an alarm post guard hit at stage N (0..2) resumes chunk N. */
#define APGUARD_STAGE_CHUNK(stage) (stage)
#define APGUARD_BURST_SPARK_LIFE 0x20 /* BurstActorWithSound spark life */

#endif
