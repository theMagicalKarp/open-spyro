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

/* ->mode of a shepherd (ACTOR_TYPE_SHEPHERD, ShepherdState; levels 12/13).
 * SHEPHERD_IDLE runs ShepherdState.phase's waiting behaviour; shepherds
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

#endif
