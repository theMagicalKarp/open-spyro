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

/* ->mode of a life chest (ACTOR_TYPE_LIFE_CHEST, LifeChestState). */
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
  RAM_RUN_PATH = 10, /* path rammer (RoamingRamState.mode 1): runs its path */
  RAM_PATH_END = 11, /* at the end: wakes its partner, then idles calling */
};

/* ->mode of a path walker (ACTOR_TYPE_PATH_WALKER, PathState). */
enum PathWalkerMode {
  PATHWALK_WAIT = 0,  /* until Spyro is near and level with it */
  PATHWALK_PACE = 1,  /* paces its path, heading for the node away from Spyro */
  PATHWALK_DYING = 2, /* hit: plays out its death animation */
};

/* ->mode of a wandering critter (ACTOR_TYPE_WANDERING_CRITTER / _B / _C,
 * CritterKnockState / WanderKnockState). */
enum WanderingCritterMode {
  CRITTER_WANDER = 0,  /* UpdateActorWander; the _C kind rests on a timer */
  CRITTER_KNOCKED = 1, /* hit: slides away and despawns */
  CRITTER_REST = 2,    /* the _C kind's pause, then WANDER */
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

/* PathWalkerState.mode: the walker's role, fixed at spawn. */
enum SingerRole {
  SINGER_ROLE_PLAIN = 0,
  SINGER_ROLE_DROPPED = 1, /* was CARRIED; struck off its rider */
  SINGER_ROLE_PAIRED = 2,  /* sings with PathWalkerState.partner */
  SINGER_ROLE_CARRIED = 3, /* rides PathWalkerState.rider */
  SINGER_ROLE_TWO_GATE = 4, /* releases chunk animation 1, else 2 */
  SINGER_ROLE_ABSENT = 6,  /* despawns at init */
};

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

/* ->mode of a path guard (ACTOR_TYPE_PATH_GUARD, PathGuardState; levels
 * 12/15/16). */
enum PathGuardMode {
  PATHGUARD_WALK = 0,    /* walks its ring with its partner; Spyro near: THROW */
  PATHGUARD_THROW = 1,   /* winds up and throws ACTOR_TYPE_SPLIT_SHOTs */
  PATHGUARD_KNOCKED = 2, /* flamed or charged: knocked away, then bursts */
};

/* ->mode of a split shot (ACTOR_TYPE_SPLIT_SHOT), set at spawn from the
 * thrower's animation frame; the mode doubles as the animation index. */
enum SplitShotMode {
  SPLITSHOT_GATHER = 0,   /* before the thrower's frame 2: glows in its hand */
  SPLITSHOT_RAISE = 1,    /* frames 2..5: held overhead */
  SPLITSHOT_FLY = 2,      /* frame 6+: launched; splits on reaching Spyro */
  SPLITSHOT_FRAGMENT = 3, /* one of the four a split leaves behind */
};

/* ->subMode of a cannon (ACTOR_TYPE_CANNON, CannonState; levels 8/19/25/34). */
enum CannonSubMode {
  CANNON_UNLIT = 0,  /* waits for a flame; despawns once CannonState.link is dormant */
  CANNON_FUSE = 1,   /* smokes, then sparks and recoils, then fires */
  CANNON_FLIGHT = 2, /* flies the shot along its path; at the end strikes the link */
};

/* ->mode of a banking flight target (ACTOR_TYPE_BANKING_FLIGHT_TARGET,
 * RollerState; flight levels 5/17/23). */
enum BankerMode {
  BANKER_FLY = 0,    /* tweens its path, banking into turns */
  BANKER_STRUCK = 1, /* flamed: pays out a time bonus, tallies, flees */
  BANKER_FLEE = 2,   /* keeps running until it splashes down */
};

/* ->mode of a flight time bonus (ACTOR_TYPE_TIME_BONUS..+2), set to
 * TIMEBONUS_COLLECT by the target that pays it out. */
enum TimeBonusMode {
  TIMEBONUS_WAIT = 0,    /* faces Spyro beside its target */
  TIMEBONUS_COLLECT = 1, /* banks the bonus and moves to screen space */
  TIMEBONUS_TO_HUD = 2,  /* flies to the HUD clock */
  TIMEBONUS_ARRIVED = 3, /* adds the seconds after a short hold */
};

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

/* ->mode of a level 6 fireball cannon (ACTOR_TYPE_FIREBALL_CANNON,
 * FireballCannonState). While its crew (FireballCannonState.link) stands, the
 * crew fires it; once the crew is down, Spyro can flame the fuse. */
enum FireballCannonMode {
  FBCANNON_IDLE = 0,
  FBCANNON_LIT = 1,  /* flamed from behind: starts the fuse */
  FBCANNON_FUSE = 2, /* smokes for 0x3C ticks */
  FBCANNON_FIRE = 3, /* lobs a fireball at the best target ahead, back to IDLE */
};

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
  CAPTAIN_POST_PATROL = 4,       /* walks between path points once its partner calls */
  CAPTAIN_POST_LEDGE_WAIT = 5,   /* faces Spyro until cued, then arms as POST_LEDGE */
};

/* CaptainState.step during CAPTAIN_FIGHT: the sword-fight sequence. */
enum CaptainFightStep {
  CAPTAIN_STEP_GUARD = 0,    /* waits for Spyro within 0x1800 */
  CAPTAIN_STEP_DRAW = 1,     /* draw animation */
  CAPTAIN_STEP_SPAR = 2,     /* close: STRIKE; far: SHEATHE_RESET; timer2 out: SHEATHE */
  CAPTAIN_STEP_STRIKE = 3,   /* the swing; hits on frames 0x25..0x28 */
  CAPTAIN_STEP_SHEATHE_RESET = 4, /* puts the sword away, back to GUARD */
  CAPTAIN_STEP_SHEATHE = 5,  /* puts the sword away, then REST */
  CAPTAIN_STEP_REST = 6,     /* close: STRIKE; rested: DRAW again */
};
/* CaptainState.step on a ledge post instead: nonzero once armed. */
#define CAPTAIN_LEDGE_ARMED 1

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

/* ->mode of a swooping guard (ACTOR_TYPE_SWOOPER, SwoopState; levels 7/8).
 * SwoopState.alert is shared: one swoops at a time. */
enum SwoopMode {
  SWOOP_PERCH = 0,   /* hovers at home; Spyro in range, in sight, facing it: SWOOP */
  SWOOP_DIVE = 1,    /* weaves in at Spyro; a hit, a miss or losing him: RETURN */
  SWOOP_RETURN = 2,  /* flies back home, then PERCH */
  SWOOP_KNOCKED = 4, /* flamed or charged: drifts down and bursts */
};

/* SwoopState.kind: how a dive is flown. 0 and 1 are fixed weave sides and
 * are copied straight into SwoopState.side (0 sways the heading negative,
 * 1 positive), so they stay literal. */
enum SwoopKind {
  SWOOP_KIND_RANDOM = -1, /* each dive picks side 0 or 1 at random */
  SWOOP_KIND_STRAIGHT = 2, /* no weave; dives to Spyro's height and turns back sharply */
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

/* VillagerState.mode: the villager's role. */
enum VillagerRole {
  VROLE_CHAT = 0,   /* faces Spyro; talks (its chunk anim) when he is in talk range */
  VROLE_FACE = 1,   /* faces its friend; Spyro too close: PANIC */
  VROLE_PAIR = 2,   /* waits on its partner (link), cueing it via subMode */
  VROLE_LEADER = 3, /* watches its leader (LeaderState) */
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

/* ShooterState.mode: the sentry's role. */
enum ShooterRole {
  SHOOTER_ROLE_DOZER = 0, /* blinks in SHOOTER_IDLE until Spyro comes near; fires inside 0x2800 */
  SHOOTER_ROLE_GUARD = 1, /* starts behind its shield; ducks while Spyro is 0x2000..0x3C00 away,
                           * fires once he is inside 0x2000 */
};

/* ->mode of Metalhead, the level 22 boss (ACTOR_TYPE_METALHEAD,
 * MetalheadState). Clearing the current phase's pickup group starts
 * METALHEAD_PHASE_TURN; the run steps through the phase modes with
 * mode++. */
enum MetalheadMode {
  METALHEAD_IDLE = 0,         /* picks STOMP / SPRAY / LOB by Spyro's range */
  METALHEAD_STOMP = 1,
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

/* MetalheadState.choice: the attack Metalhead picks while METALHEAD_IDLE
 * (random: 60% stomp, 25% spray, 15% lob; spray or lob only when Spyro is
 * out of stomp reach). Back to NONE when its pose ends or the pick's range
 * test fails. */
enum MetalheadAttack {
  METALHEAD_ATTACK_NONE = 0,
  METALHEAD_ATTACK_STOMP = 1, /* waits for Spyro in range, then METALHEAD_STOMP */
  METALHEAD_ATTACK_SPRAY = 2, /* METALHEAD_SPRAY */
  METALHEAD_ATTACK_LOB = 3,   /* METALHEAD_LOB */
};

/* ->mode of a lunger (ACTOR_TYPE_LUNGER, LungeState; level 4). It takes
 * LungeState.hits hits (each one switches its animation set, anim += 7)
 * before a flame knocks it away; hits are ignored while it recoils. */
enum LungerMode {
  LUNGER_IDLE = 0,       /* waits; Spyro close: AIM at his spot */
  LUNGER_AIM = 1,        /* turns to the target point, then LEAP */
  LUNGER_LEAP = 2,       /* leaps at the point, pinning Spyro under it; then GO_HOME */
  LUNGER_KNOCKED = 4,    /* last hit: knocked away, then bursts */
  LUNGER_BURST = 10,     /* bursts and drops its loot (no writer in the C sources) */
  LUNGER_RECOIL = 13,    /* flinches from a hit, re-aims, then AIM */
  LUNGER_RECOIL_B = 14,  /* recoil variants (no writer in the C sources) */
  LUNGER_RECOIL_C = 15,
  LUNGER_ALERTED = 20,   /* marks LungeState.alerted, then IDLE */
  LUNGER_GO_HOME = 100,  /* walks back home and faces its heading, then IDLE */
};

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

/* ->mode of a path patroller (ACTOR_TYPE_PATH_PATROLLER, PathState; levels
 * 0/1): a critter that scatters along its path nodes away from Spyro. */
enum PatrollerMode {
  PATROLLER_WAIT = 0,      /* Spyro near and level: PICK (and its group with it) */
  PATROLLER_PICK = 1,      /* picks the neighbour node away from Spyro */
  PATROLLER_FLEE = 2,      /* runs for it, then WAIT */
  PATROLLER_KNOCKED = 3,   /* knocked away, then despawns */
};

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

/* ->mode of a grabber (ACTOR_TYPE_GRABBER, GrabberState; level 20).
 * GrabberState.held is shared: one grabber goes for Spyro at a time. */
enum GrabberMode {
  GRABBER_LURK = 0,        /* faces Spyro; rolls its grab chance while unseen-ish */
  GRABBER_RUSH = 1,        /* rushes Spyro (shoving gunners in its way); close: SEIZE */
  GRABBER_RETREAT = 2,     /* back home, then LURK */
  GRABBER_KNOCKED = 4,     /* flamed: drops Spyro, tumbles away and bursts */
  GRABBER_RECOVER = 6,     /* plays out, then RUSH (no writer in the C sources) */
  GRABBER_SEIZE = 10,      /* closes on Spyro and takes hold (hides him) */
  GRABBER_CARRY = 11,
  GRABBER_THROW = 12,      /* throws Spyro clear, then RETREAT */
  GRABBER_DEVOUR = 13,     /* Sparx is gone: the hold kills Spyro */
  GRABBER_THROW_SHORT = 14, /* the shorter throw used after the first one */
  GRABBER_HUSK = 99,       /* the copy left behind by a knock: plays out, despawns */
};

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
  SORCERER_CUE = 98,         /* takes the queued pose (no writer in the C sources) */
  SORCERER_ALERTED = 99,     /* set group-wide: turns angry, takes the queued pose */
};

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
  SORCERER_STEP_REST = 2, /* waits out its timer, plays pose 7, then WALK (no writer) */
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

/* ->mode of Jacques (ACTOR_TYPE_JACQUES, JacquesState; level 28). */
enum JacquesMode {
  JACQUES_WATCH = 0,     /* faces Spyro; seen from afar: THROW; Spyro close: HOP */
  JACQUES_THROW = 2,     /* throws an ACTOR_TYPE_THROWN_ROCK, then WATCH */
  JACQUES_DEFEATED = 3,  /* struck at his last stop: bursts */
  JACQUES_HOP = 10,      /* hops along his ring to the next stop, then WATCH */
};

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
  BSENTRY_CUE_PATROL = 2, /* Spyro ahead within 0x3000: BSENTRY_PATROL */
};

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

/* ->mode of a sweep gunner (ACTOR_TYPE_SWEEP_GUNNER, SweepGunnerState;
 * level 32). */
enum SweepGunnerMode {
  SWEEPGUN_IDLE = 0,     /* faces Spyro; in range and level: SWEEP */
  SWEEPGUN_ADVANCE = 2,  /* Spyro entered its trigger box: walks its path, then IDLE */
  SWEEPGUN_SWEEP = 3,    /* sprays shots, sweeping between its yaw limits (subMode 1: reverses) */
  SWEEPGUN_KNOCKED = 4,  /* flamed: knocked away; bursts, or sinks on water */
  SWEEPGUN_TURN = 10,    /* turns to Spyro, then IDLE */
  SWEEPGUN_SINK = 20,    /* knocked into the water: despawns after a moment */
};

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
  BTHROW_RIDE_FACE = 52,    /* faces the catcher until it is ready, then jumps */
  BTHROW_RIDE_LEAP = 53,    /* jumps into the catcher's hands */
  BTHROW_RIDE_HELD = 54,    /* held; flung at Spyro (HURTLE) on the throw frame */
};

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
  GATEKEEPER_RIDE_FACE = 52,   /* faces the catcher until it is ready, then jumps */
  GATEKEEPER_RIDE_LEAP = 53,   /* jumps into the catcher's hands */
  GATEKEEPER_RIDE_HELD = 54,   /* held; flung at Spyro (HURTLE) on the throw frame */
};

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

/* ->mode of a mount (ACTOR_TYPE_MOUNT, MountState; level 31); the mode is
 * also the animation index. Its rider is a floating barrel or mine
 * (FloatingBarrelMode), dropped when the mount is struck. */
enum MountMode {
  MOUNT_IDLE = 0,    /* faces Spyro; close: REACT */
  MOUNT_REACT = 1,   /* reacts to Spyro, then IDLE */
  MOUNT_KNOCKED = 2, /* struck, or its rider burst or ran: knocked away, then bursts */
};

/* ->mode of a hatch carrier (ACTOR_TYPE_HATCH_CARRIER, HatchCarrierState;
 * level 31); the mode is also the animation index. */
enum HatchCarrierMode {
  HCARRIER_WAIT = 0,    /* on the timer, if its hatch waits: LIFT */
  HCARRIER_LIFT = 1,    /* lifts the hatch's barrel or mine out on frame 4 */
  HCARRIER_THROW = 2,   /* throws it on frame 6, then WAIT */
  HCARRIER_KNOCKED = 3, /* flamed: knocked away, then bursts */
};

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

/* ->mode of a tough ram (ACTOR_TYPE_TOUGH_RAM, ToughRamState; level 9); the
 * mode is also the animation index. The yak's machine, but four hits. */
enum ToughRamMode {
  TRAM_GRAZE = 0,     /* grazes; Spyro near: ROUSE */
  TRAM_ROUSE = 1,     /* rousing animation, then WATCH */
  TRAM_WATCH = 2,     /* faces Spyro; close: BUTT; far: GRAZE */
  TRAM_BUTT = 3,      /* butts Spyro; pinned him: BUTT_HOLD */
  TRAM_SETTLE = 4,    /* back to GRAZE (no writer in the C sources) */
  TRAM_STAGGER = 5,   /* hit: thrown up and back, then GRAZE */
  TRAM_KNOCKED = 6,   /* the fourth hit (or thrown off a ledge): falls, then bursts */
  TRAM_BUTT_HOLD = 7, /* holds after pinning Spyro, then GRAZE */
};

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

/* BoltGuardState.mode: the one-shot cue a bolt guard waits on in
 * BGUARD_WATCH (spawn record); cleared once played out. */
enum BoltGuardCue {
  BGUARD_CUE_NONE = 0,
  BGUARD_CUE_RUN = 1,  /* Spyro in its box or range: BGUARD_RUN (the whole group) */
  BGUARD_CUE_LINK = 2, /* Spyro in range: fires along its link chain (BGUARD_FACE_LINK) */
};

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

/* ->mode of a bomb lobber (ACTOR_TYPE_BOMB_LOBBER, BombLobberState; levels
 * 25/26); the mode is also the animation index. */
enum BombLobberMode {
  BLOBBER_WATCH = 0,   /* faces Spyro; in range on the timer: LOB */
  BLOBBER_LOB = 1,     /* lobs an ACTOR_TYPE_LOBBED_BOMB on frame 6, then WATCH */
  BLOBBER_KNOCKED = 2, /* flamed: knocked away, then bursts */
};

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

/* ->mode of a loot critter (ACTOR_TYPE_LOOT_CRITTER, CritterState; level
 * 34); the mode is also the animation index. */
enum LootCritterMode {
  LCRITTER_WATCH = 0,   /* faces Spyro; near: FLEE */
  LCRITTER_FLEE = 1,    /* runs from Spyro around its home or along its path */
  LCRITTER_KNOCKED = 2, /* knocked away (dropping its loot once), then bursts */
};

/* CritterState.mode: how a homeless loot critter (no CritterState.hasHome)
 * runs its path during LCRITTER_FLEE. Spyro within 0x1800 always restarts at
 * PICK; reaching a path end drops it to LCRITTER_WATCH. */
enum LootCritterRun {
  LCRITTER_RUN_PICK = 0,     /* sets the path direction away from Spyro, then DASH */
  LCRITTER_RUN_OPPOSITE = 1, /* heads for the path point opposite Spyro; far off: WANDER */
  LCRITTER_RUN_WANDER = 2,   /* walks the path */
  LCRITTER_RUN_DASH = 3,     /* runs fast; once clear of Spyro: OPPOSITE */
};

/* ->mode of a loot thief (ACTOR_TYPE_LOOT_THIEF, LootThiefState; level 34);
 * the mode is also the animation index. LootThiefState.leg steps its run. */
enum LootThiefMode {
  LTHIEF_RUN = 0,     /* runs its ring and spur paths away from Spyro */
  LTHIEF_KNOCKED = 1, /* flamed: shatters into chest shards */
};

/* ->mode of a two-path runner (ACTOR_TYPE_TWO_PATH_RUNNER, RunnerState; level
 * 8); the mode is also the animation index. With RunnerState.mode TPARM_RUN it runs
 * whichever of its two paths ends nearer Spyro (RunnerState.which). */
enum TwoPathRunnerMode {
  TPRUNNER_IDLE = 0,       /* until RunnerState.mode arms it: GUARD */
  TPRUNNER_GUARD = 4,      /* faces Spyro; close on the timer: SWIPE; armed: RUN */
  TPRUNNER_KNOCKED = 5,    /* charged: knocked away, then bursts */
  TPRUNNER_REACT = 6,      /* faces Spyro, then GUARD (no writer in the C sources) */
  TPRUNNER_SWIPE = 8,      /* swipes, then GUARD */
  TPRUNNER_WALK_BACK = 10, /* walks to its path node, then GUARD (a level 8 lookout, GLOOK_ROUSE, sets its linked runner to 10) */
  TPRUNNER_RUN = 0x14,     /* runs its chosen path to RunnerState.node, then GUARD */
};

/* RunnerState.mode: how far a two-path runner is armed. */
enum TwoPathRunnerArm {
  TPARM_NONE = 0,  /* stays IDLE */
  TPARM_GUARD = 1, /* GUARD only (set again once a run ends) */
  TPARM_RUN = 2,   /* GUARD, and RUN when Spyro nears a path's node */
};

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

/* ->mode of a brawler (ACTOR_TYPE_BRAWLER, BrawlerState; level 7); the mode
 * is also the animation index. It carries a swooper, which flies off
 * knocked (SWOOP_KNOCKED) when the brawler goes down. */
enum BrawlerMode {
  BRAWLER_IDLE = 0,    /* faces Spyro; near: RAISE */
  BRAWLER_SEND = 1,    /* sends an ACTOR_TYPE_BRAWLER_SPAWN out on frame 0x11 (no writer in the C sources) */
  BRAWLER_RAISE = 2,   /* raises its fists, then GUARD */
  BRAWLER_GUARD = 3,   /* faces Spyro; close on the timer: PUNCH; far: LOWER */
  BRAWLER_PUNCH = 4,   /* punches, then RAISE */
  BRAWLER_LOWER = 5,   /* lowers its fists, then IDLE */
  BRAWLER_KNOCKED = 6, /* flamed: its swooper flies off, then it bursts */
};

/* ->mode of a chasing path guard (ACTOR_TYPE_CHASING_PATH_GUARD,
 * ChasingPathGuardState; level 26); the mode is also the animation index. */
enum ChasingPathGuardMode {
  CPGUARD_PATROL = 0,  /* walks its waypoints (or faces Spyro at home); sees him: CHASE */
  CPGUARD_RETURN = 1,  /* back to its path or home, then PATROL */
  CPGUARD_CHASE = 2,   /* runs at Spyro; facing him: ATTACK; lost him: RETURN */
  CPGUARD_ATTACK = 3,  /* lunges, then RETURN */
  CPGUARD_KNOCKED = 4, /* knocked away, then bursts */
};

/* ->mode of a formation escort (ACTOR_TYPE_FORMATION_ESCORT,
 * FormationEscortState; level 26). */
enum FormationEscortMode {
  ESCORT_WAIT = 0,     /* until released (dropKind set): JOIN at its leader */
  ESCORT_TWEEN_IN = 1, /* tweens in along its path, then JOIN */
  ESCORT_JOIN = 2,     /* steers to its slot around the leader, then HOLD */
  ESCORT_HOLD = 3,     /* holds formation; the formation breaks: BREAK_AWAY */
  ESCORT_BREAK_AWAY = 4, /* spirals up and away, then bursts into streaks */
};

/* ->mode of a partner bouncer (ACTOR_TYPE_PARTNER_BOUNCER,
 * PartnerBouncerState; level 26); the mode is also the animation index. */
enum PartnerBouncerMode {
  PBOUNCER_RIDE = 0,   /* rides its partner's attach point; cued and Spyro near: BOUNCE */
  PBOUNCER_BOUNCE = 1, /* bounce animation, then RIDE */
  PBOUNCER_THROWN = 2, /* superflamed, or its partner went: thrown off, then FALL */
  PBOUNCER_FALL = 3,   /* falls until it lands (burst) or drops out of the level */
};

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

/* ->mode of a fence guard (ACTOR_TYPE_FENCE_GUARD, FenceGuardState; level
 * 27); the mode is also the animation index. subMode 1 wakes it, 2 (the
 * captain's reset) puts it back to sleep. */
enum FenceGuardMode {
  FGUARD_ASLEEP = 0, /* until woken */
  FGUARD_WAKE = 1,   /* waking animation, then WATCH */
  FGUARD_WATCH = 2,  /* faces Spyro along its fence; close and in sight: ATTACK */
  FGUARD_ATTACK = 3, /* swings, pinning Spyro on frames 0x2D-0x32 */
};

/* ->mode of a path charger (ACTOR_TYPE_PATH_CHARGER, PatrolState; level 2);
 * the mode is also the animation index. Only a flame from behind or the side
 * knocks it away. */
enum PathChargerMode {
  PCHARGER_PATROL = 0,  /* walks its path back and forth; Spyro ahead: CHARGE */
  PCHARGER_KNOCKED = 1, /* flamed: bursts */
  PCHARGER_CHARGE = 2,  /* charges down the path; reaching Spyro, butts him off it */
  PCHARGER_TAUNT = 6,   /* at a turn with Spyro ahead: faces him, then PATROL */
  PCHARGER_TAUNT_ALT = 7, /* the same (no writer in the C sources) */
};

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

/* ->mode of a lantern (ACTOR_TYPE_LANTERN, LanternState; level 25); the mode
 * is also the animation index. subMode bit 0 is lit, bit 1 settled, bit 2
 * holds it as is. */
enum LanternMode {
  LANTERN_IDLE = 0,     /* on the timer, toggles: lit ones SNUFF, dark ones LIGHT */
  LANTERN_SNUFF = 1,    /* goes out, then IDLE */
  LANTERN_LIGHT = 2,    /* lights up, then IDLE */
  LANTERN_STRUCK = 3,   /* Spyro hit it: lit for good, with an ACTOR_TYPE_LANTERN_FLAME */
  LANTERN_HOLD_LIT = 4, /* stays lit, then spawns the closing flame: DONE */
  LANTERN_DONE = 5,
};

/* ->mode of Jacques' thrown rock (ACTOR_TYPE_THROWN_ROCK, CarriedRockState;
 * level 28). CarriedRockState.mode 1 rides between the thrower's hands. */
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
  CCANNON_FUSE = 2,   /* smokes until the fuse runs out, then FIRE (no writer in the C sources) */
  CCANNON_FIRE = 3,   /* fires an ACTOR_TYPE_COMET at the best target ahead, then WATCH */
  CCANNON_LOB = 10,   /* turns to the cued target and lobs a comet at it, then WATCH */
};

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

/* ->mode of a lane patroller (ACTOR_TYPE_LANE_PATROLLER, LanePatrollerState;
 * level 33); the mode is also the animation index. */
enum LanePatrollerMode {
  LANEPAT_WATCH = 0,   /* faces Spyro; near: RUN its lane */
  LANEPAT_RUN = 1,     /* runs its lane path, turning back at Spyro, to the far end */
  LANEPAT_KNOCKED = 2, /* knocked away, then bursts */
};

/* ->mode of a runner spawner (ACTOR_TYPE_RUNNER_SPAWNER, RunnerSpawnerState;
 * level 14); the mode is also the animation index. */
enum RunnerSpawnerMode {
  RSPAWNER_IDLE = 0,    /* on the timer, with no runner out: RELEASE; else FIDGET */
  RSPAWNER_FIDGET = 1,  /* idle variant, then IDLE */
  RSPAWNER_FLINCH = 2,  /* flinch animation (flag bit 0), then IDLE */
  RSPAWNER_RELEASE = 3, /* sends out an ACTOR_TYPE_WAYPOINT_RUNNER on frame 5 */
  RSPAWNER_KNOCKED = 4, /* bursts */
};

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

/* ->mode of the overhead thrower (ACTOR_TYPE_OVERHEAD_THROWER; levels 9/10);
 * the mode is also the animation index. */
enum OverheadThrowerMode {
  OTHROW_HOLD = 0, /* holds its snowball (state word 0) overhead */
  OTHROW_HIT = 1,  /* charged or supercharged: lets the snowball fly at Spyro, then HOLD */
};

/* ->mode of the level 8 roaming critter (ACTOR_TYPE_ROAMING_CRITTER,
 * RoamingCritterState). A hit always lands in DYING; KNOCKED's slide only
 * runs when entered directly. */
enum RoamingCritterMode {
  ROAMCRIT_WANDER = 0,  /* UpdateActorWander */
  ROAMCRIT_KNOCKED = 5, /* slides along the knock heading, bursts; falls into DYING */
  ROAMCRIT_DYING = 6,   /* death animation 6 (wandering its first frames), then 7; despawns */
};

/* ->mode of a checkpoint (ACTOR_TYPE_CHECKPOINT .. + 2; every ground level). */
enum CheckpointMode {
  CHECKPT_REARM = 0,     /* waits for Spyro to walk away (> 2560), then ARMED */
  CHECKPT_ARMED = 1,     /* Spyro touches it: sets the respawn anchor, FLASH */
  CHECKPT_FLASH = 2,     /* subMode counts frames to 0x30, then REARM */
};

/* ->mode of a lookout (LookoutState): ACTOR_TYPE_GUARD_LOOKOUT (level 8) and
 * ACTOR_TYPE_SHEMP_LOOKOUT (level 10); the update switches on a local copy. */
enum GuardLookoutMode {
  GLOOK_WATCH = 0,   /* idles facing home / Spyro; rouses its idle guard when Spyro is in range */
  GLOOK_SWIPE = 6,   /* Spyro close below it: swipes him away, then WATCH */
  GLOOK_KNOCKED = 7, /* flamed or supercharged: knocked away, then bursts */
  GLOOK_ROUSE = 10,  /* turns to its guard and wakes it on frame 5, then WATCH */
};

/* ->mode of the level 16 humming bell (ACTOR_TYPE_HUMMING_BELL,
 * HummingBellState). */
enum HummingBellMode {
  HBELL_INIT = 0,   /* first frame: HUM */
  HBELL_STRUCK = 1, /* flamed or supercharged: bursts, runs its chunk to step 0, despawns */
  HBELL_HUM = 2,    /* sparks at its target and restarts its chunk every period */
};

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
  BPROP_WOBBLE = 1, /* plays out, then IDLE (no writer in this block) */
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
 * 0/6/12/18/24). */
enum HomeTalkerMode {
  TALKER_REARM = 0, /* waits for Spyro to walk away (>= 0x1000), then READY */
  TALKER_READY = 1, /* Spyro walks up facing it: starts its actor cutscene, REARM */
};

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

/* SuperflameGiverState.kind: how long the granted superflame lasts. */
enum SuperflameGiverKind {
  SFKIND_TIMED = 0,       /* 0x384 frames */
  SFKIND_ENDLESS = 1,     /* g_nSpyroSuperflameTimer 0x9450C00: effectively for good */
  SFKIND_SHORT = 2,       /* 0x30C frames */
  SFKIND_ENDLESS_FAR = 3, /* ENDLESS, granted from a wider range (0xD48) */
};

/* PathRunnerState.mode: which of its two runs a path runner makes. */
enum PathRunnerRun {
  PRUN_FIRST = 0,  /* drives goal flag 0 */
  PRUN_SECOND = 1, /* drives goal flag 1 */
};

/* LootVortexState.mode (ACTOR_TYPE_LOOT_VORTEX; levels 33/34). */
enum LootVortexStage {
  LVORTEX_LOCKED = 0, /* hidden until its key's loot thief is beaten: OPEN (key carried) */
  LVORTEX_OPEN = 1,   /* Spyro walks into its pad facing the target: ACTIVE */
  LVORTEX_ACTIVE = 2, /* freezes Spyro and plays its sequence; when he is free again: FINISH */
  LVORTEX_FINISH = 3, /* runs its chunk anim, resets the camera stick and the key state */
};

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

#endif
