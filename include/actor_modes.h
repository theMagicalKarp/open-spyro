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

#endif
