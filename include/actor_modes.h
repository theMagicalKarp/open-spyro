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

/* Sparx's ->subMode while he has a target (SparxState.target); with none he
 * hovers beside Spyro. */
enum SparxSubMode {
  SPARX_SUB_CHASE = 0,    /* closing in on a butterfly (set by the butterfly) */
  SPARX_SUB_EAT = 1,      /* caught it: +1 health, despawn the butterfly */
  SPARX_SUB_EAT_DONE = 2, /* back to the hover animation, drop the target */
  SPARX_SUB_FETCH = 4,    /* flying to a pickup, which he sends COLLECT_HOMING */
  SPARX_SUB_PARKED = 99,  /* takes no targets (no writer in the C sources) */
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

/* ->mode of a crystal dragon statue (ACTOR_TYPE_DRAGON_STATUE,
 * DragonStatueState). */
enum DragonStatueMode {
  STATUE_INIT = 0,      /* snapshot the rest pose, hold the partner (mode 3) */
  STATUE_IDLE = 1,      /* shakes now and then; Spyro touching it frees the dragon */
  STATUE_RESCUING = 2,  /* BeginDragonRescue ran (also the save menu's replay) */
};

/* ->mode of a butterfly (ACTOR_TYPE_BUTTERFLY, WanderState). */
enum ButterflyMode {
  BUTTERFLY_FLUTTER = 0, /* drifts about its spawn point */
  BUTTERFLY_CIRCLE = 1,  /* Sparx is coming to eat it: circles Spyro */
};

/* ->mode of a seed pod (ACTOR_TYPE_SEED_POD, SeedPodState). */
enum SeedPodMode {
  SEEDPOD_CLOSED = 0, /* struck: spawns its prize (COLLECT_INERT) and opens */
  SEEDPOD_OPEN = 1,   /* shakes with the prize floating above; closes after a while */
};

/* ->mode of a life chest (ACTOR_TYPE_LIFE_CHEST, LiftState). */
enum LifeChestMode {
  LIFECHEST_CLOSED = 0, /* shakes on a timer, then lifts its lid */
  LIFECHEST_OPEN = 1,   /* lid up, its rider showing; closes after 0xE6 frames */
};

/* ->mode of a level-exit whirlwind (ACTOR_TYPE_LEVEL_EXIT, ExitWhirlState). */
enum LevelExitMode {
  LEVELEXIT_FAR = 0,  /* camera out of range */
  LEVELEXIT_NEAR = 1, /* camera close: its name letters were spawned */
};

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

/* ->subMode of the locked chest (ACTOR_TYPE_LOCKED_CHEST, LockedChestState). */
enum LockedChestSubMode {
  LOCKCHEST_NO_KEY = 0,    /* waits for KEYSTATE_CARRIED */
  LOCKCHEST_KEY_READY = 1, /* waits for Spyro to face it up close */
  LOCKCHEST_OPENING = 2,   /* the key flies into the lock, then it bursts
                            * (a superflame hit skips straight to the burst) */
};

/* ->subMode of a cracking boulder (ACTOR_TYPE_CRACKING_BOULDER,
 * CrackingBoulderState). */
enum CrackingBoulderSubMode {
  BOULDER_INIT = 0,     /* take the drop-kind tint */
  BOULDER_INTACT = 1,   /* a charge bursts it at once, a flame or blast cracks it */
  BOULDER_CRACKING = 2, /* shakes and smokes for 0x78 frames */
  BOULDER_BURST = 3,    /* debris rings, the drop, and a push on Spyro */
};

/* ->subMode of a spin chest (ACTOR_TYPE_SPIN_CHEST, SpinChestState). */
enum SpinChestSubMode {
  SPINCHEST_INIT = 0,     /* spawn the lid */
  SPINCHEST_SPINNING = 1, /* flames spin it up; fast enough (or a superflame) bursts it */
};

/* ->subMode of the spin chest's lid (ACTOR_TYPE_SPIN_CHEST_LID,
 * SpinChestLidState). */
enum SpinChestLidSubMode {
  LID_ON_CHEST = 0, /* spun by its chest */
  LID_LAUNCH = 1,   /* the chest burst: aim at an enemy (or away from Spyro) */
  LID_FLYING = 2,   /* in flight; breaks on what it hits */
};

/* ->mode of a portal guide (ACTOR_TYPE_PORTAL_GUIDE, PortalState); set to
 * PORTALGUIDE_ENTER by DispatchSpyroTriggerEvent when Spyro enters a
 * portal. */
enum PortalGuideMode {
  PORTALGUIDE_IDLE = 0,  /* inactive; set back once the camera reaches the portal and the load starts */
  PORTALGUIDE_ENTER = 1, /* Spyro jumps toward the portal's midpoint */
  PORTALGUIDE_GLIDE = 2, /* Spyro glides along the portal camera path */
};

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

/* ->mode of a roaming ram (ACTOR_TYPE_ROAMING_RAM, RoamingRamState). */
enum RoamingRamMode {
  RAM_GRAZE = 0,     /* calls; Spyro near: picks the node facing away from him */
  RAM_WANDER = 1,    /* walks to that node */
  RAM_KNOCKED = 3,   /* hit: tumbles away and bursts */
  RAM_TRAPPED = 4,   /* inside its trigger box while the trigger is set */
  RAM_RUN_PATH = 10, /* path rammer (RoamingRamState.mode 1): runs its path */
  RAM_PATH_END = 11, /* at the end: wakes its partner, then idles calling */
};

/* ->mode of a path walker (ACTOR_TYPE_PATH_WALKER, PathState). */
enum PathWalkerMode {
  PATHWALK_WAIT = 0,  /* until Spyro is near and level with it */
  PATHWALK_PACE = 1,  /* paces its path, heading for the node away from Spyro */
  PATHWALK_DYING = 2, /* hit: plays out its death animation */
};

/* ->mode of a wandering critter (ACTOR_TYPE_WANDERING_CRITTER,
 * CritterKnockState). */
enum WanderingCritterMode {
  CRITTER_WANDER = 0,  /* UpdateActorWander */
  CRITTER_KNOCKED = 1, /* hit: slides away and despawns */
};

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

#endif
