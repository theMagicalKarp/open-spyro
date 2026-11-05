/* actor_states.h — class-specific state blocks (Actor->state) that main and
 * the level overlays both read, or that a level's SpawnActor and UpdateActors
 * share (level_spawn_actor.h and level_update_actors.h both include this).
 * They stand alone, without the Actor type, so overlay headers that only
 * forward-declare struct Actor can include this. */

#ifndef OPEN_SPYRO_ACTOR_STATES_H
#define OPEN_SPYRO_ACTOR_STATES_H

/* A trigger box for IsPointInRotatedBox: a point, half-extents in the box's
 * own frame, and that frame's yaw. A zero half-width marks an unused box. */
typedef struct TriggerBox {
  int x;         /* 0x00 */
  int y;         /* 0x04 */
  int z;         /* 0x08 */
  int halfWidth; /* 0x0C 0 = unused */
  int halfDepth; /* 0x10 */
  int yaw;       /* 0x14 sine-LUT index */
} TriggerBox;

/* Dragon-statue burst shard (type 0xFB), ticked by TickStatueBurstShard:
 * integrates vel with gravity while life runs and posZ stays above floorZ. */
typedef struct BurstShardState {
  int vel[3];          /* 0x00 */
  int floorZ;          /* 0x0C the statue's height */
  unsigned char spinX; /* 0x10 per-tick ->rotX/rotY/rotZ deltas */
  unsigned char spinY;
  unsigned char spinZ;
  unsigned char life;  /* 0x13 ticks left */
} BurstShardState;

/* Sparx (type 0x78, case 120 in the UpdateActors clones; g_pSparxActor):
 * hovers at an offset beside Spyro and chases one target at a time, a
 * butterfly to eat (g_nSparxHealth + 1) or a pickup to fetch. SpawnActor
 * zeroes it all. */
typedef struct SparxState {
  int timer;             /* 0x00 hover re-aim / chase countdown */
  short drift[3];        /* 0x04 hover offset from Spyro, x/y/z */
  char pad0A[0x2];
  struct FxNode *glow;   /* 0x0C sparkle node, shown at full health */
  struct Actor *target;  /* 0x10 butterfly or pickup being chased */
} SparxState;

/* Crystal dragon statue (ACTOR_TYPE_DRAGON_STATUE). The UpdateActors clones
 * shake it and, once Spyro is close, count the rescue or hand it to
 * BeginDragonRescue; DragonRescue_Update stages the cutscene from the camera,
 * orbit and speech fields (BeginDragonRescue rewrites them for the Gnorc
 * Gnexus statue once level 0x21 is exited). */
typedef struct DragonStatueState {
  int camPos[3];    /* 0x00 camera position the rescue sweep ends at */
  int camEuler[3];  /* 0x0C camera roll / pitch / yaw (12-bit) it ends at */
  int cutsceneId;   /* 0x18 rescue cutscene id; == reward (-1) when the
                     * statue has none and counts the rescue directly */
  int facing;       /* 0x1C respawn heading when there is no partner */
  int partner;      /* 0x20 actor index of its checkpoint (level data), -1 =
                     * none: CHECKPT_HELD while the statue stands, anim 1
                     * during the rescue, CHECKPT_ARMED after; its yaw is the
                     * respawn heading */
  int reward;       /* 0x24 */
  int orbitYaw;     /* 0x28 Spyro's orbit target yaw around the statue */
  int orbitRadius;  /* 0x2C and radius */
  int dragonYaw;    /* 0x30 12-bit yaw the freed dragon turns to (>> 4) */
  int noSkip;       /* 0x34 nonzero: X does not cut the speech short */
  int dragonName;   /* 0x38 g_apDragonNames index (DrawDragonRescuedBanner) */
  int speechRate;   /* 0x3C speech sample rate in Hz (pitch = rate * 1024 / 11025) */
  int speechFrame;  /* 0x40 rescue frame the speech starts on */
  int phase;        /* 0x44 shake phase */
  int rotX;         /* 0x48 rest pose */
  int rotY;         /* 0x4C */
  int restZ;        /* 0x50 */
} DragonStatueState;

/* Type 16 (ACTOR_TYPE_BUTTERFLY): flutters about its spawn point, turning
 * and climbing on random timers and staying 0..0x7D0 out from home and
 * 300..1024 above the ground. SpawnActor seeds offscreenTimer with 0x708:
 * until it runs out the butterfly flutters even when it was not drawn last
 * frame, after that only while drawn. Sparx eating it (BUTTERFLY_CIRCLE)
 * makes it circle Spyro on the side circleDir picks. */
typedef struct ButterflyState {
  int circleDir;              /* 0x00 +1 / -1, from the side of Spyro it is on */
  int home[3];                /* 0x04 spawn point it flutters near */
  unsigned char vz;           /* 0x10 climb step, a signed byte */
  unsigned char heading;      /* 0x11 */
  unsigned char headingTimer; /* 0x12 frames to the next heading change */
  unsigned char climbTimer;   /* 0x13 frames to the next climb change */
  short offscreenTimer;       /* 0x14 frames it still flutters undrawn */
} ButterflyState;

/* Type 110 (ACTOR_TYPE_SAVE_HOST): the save-prompt host. Waits until Spyro
 * is away from its anchor, then until its arming actor goes dormant; pops in
 * beside Spyro (particles, spin, shrink from scale 0x60), then hovers on a
 * cosine bob, wandering around the anchor actor. When Spyro stands idle at
 * the anchor it calls BeginSaveMenuFromWorld (the save menu pans this actor
 * around Spyro); it leaves when Spyro moves away. */
typedef struct SaveHostState {
  int armer;       /* 0x00 actor index of the dragon whose rescue (dormant, mode >= 0x80)
                    arms the host; save-menu option 1 replays that rescue */
  int anchor;      /* 0x04 actor index of the checkpoint (ACTOR_TYPE_CHECKPOINT..+2,
                    level data) the host hovers around and Spyro stands at; the
                    host reappears after a prompt once Spyro re-arms it */
  int timer;       /* 0x08 pop-in delay, then the bob phase */
  int baseZ;       /* 0x0C bob centre height */
  int wanderTo[3]; /* 0x10 next hover point near the anchor */
  int faceYaw;     /* 0x1C bearing from the hover point to Spyro */
  int wanderTimer; /* 0x20 frames until the next wander */
  int prompted;    /* 0x24 the save menu was opened from this visit */
} SaveHostState;

/* Type 300 (whirlwind) state block. While Spyro is inside the cylinder it
 * posts SPYRO_EV_LIFT with a request to rise to actor->posZ + spanZ (the LIFT
 * state carries him up, then he glides) and plays SFX_WHIRLWIND. An
 * owner-gated one stays off until its linked actor (key picked up, dragon
 * freed, gem taken) says so. */
typedef struct WhirlwindState {
  int spanZ; /* 0x00 height; Spyro is lifted to posZ + spanZ */
  int radius; /* 0x04 horizontal radius */
  int flags; /* 0x08 WHIRL_F_* */
  int timer; /* 0x0C wake / idle countdown */
  int idx;   /* 0x10 owner: index into the actor pool at g_pActorListBase */
} WhirlwindState;

/* WhirlwindState.flags. */
#define WHIRL_F_TARGET_CAM 0x01 /* lift with the target camera (else the front spring camera) */
#define WHIRL_F_IDLE 0x02       /* idle: wakes 0xB4 frames after Spyro comes near */
#define WHIRL_F_WOKEN 0x04      /* woken: idles again 0x78 frames after a lift */
#define WHIRL_F_GATED 0x08      /* waits on its owner (`idx`: a key's subMode, a statue or gem gone) */
#define WHIRL_F_MARK 0x10       /* also sets ->subMode to 1 while active */

/* Whirlwind ranges to Spyro (2D / height): an idle one wakes within WAKE /
 * WAKE_DZ; one gated on a key that was taken stays shut until Spyro is
 * within KEY_RANGE. */
#define WHIRL_WAKE_RANGE 0x1000
#define WHIRL_WAKE_DZ 0x400
#define WHIRL_KEY_RANGE 0x1C00

/* Level 14 type 303 (ACTOR_TYPE_RUNNER_SPAWNER): every so often (`timer`,
 * 0xB4) rewinds `path` and spawns one waypoint runner (304), handing it the
 * path and `value`; killable. */
typedef struct RunnerSpawnerState {
  struct PathData *path; /* 0x00 the runners' waypoint path; cur reset to 0 per spawn */
  int flags;             /* 0x04 bit 0: a runner is out */
  int wait;              /* 0x08 */
  int timer;             /* 0x0C */
  int value;             /* 0x10 copied to the spawn's WaypointRunnerState.target */
} RunnerSpawnerState;

/* Level 14 type 304 (ACTOR_TYPE_WAYPOINT_RUNNER): walks its spawner's `path`
 * until it reaches Spyro, the path end, or its `target` actor (which it
 * flags), then clears the spawner's flags bit 0 and leaves. */
typedef struct WaypointRunnerState {
  struct PathData *path;  /* 0x00 inherited from the spawner */
  struct Actor *spawner;  /* 0x04 the RunnerSpawnerState owner */
  int smoothVel[3];       /* 0x08 WalkActorAlongWaypoints' smoothed velocity */
  short target;           /* 0x14 pool index, < 0 = none */
} WaypointRunnerState;

/* Level 19 type 343 (ACTOR_TYPE_ATTACH_BEAD): a bead that streams from its
 * owner's attach point 1 to point 0, starting subMode * 42 frames late and
 * growing as it goes. */
typedef struct AttachBeadState {
  struct Actor *owner; /* 0x00 */
  int t;               /* 0x04 */
  int pad08;           /* 0x08 cleared at spawn, never read */
} AttachBeadState;

/* Level 29 type 232 (ACTOR_TYPE_BOBBER_SPINOFF): spins off its bobbing
 * flight target and runs until it gets away or times out. */
typedef struct SpinRunState {
  int heading;   /* 0x00 */
  int zSpeed;      /* 0x04 */
  int moveSpeed; /* 0x08 */
  int timer;     /* 0x0C */
} SpinRunState;

/* Levels 13/14/15 type 295 (ACTOR_TYPE_HOMING_WISP): steers toward its
 * target actor (or Spyro), and bursts into three sparks when it arrives or
 * touches the ground. */
typedef struct WispState {
  struct Actor *target; /* 0x00 actor to reach, or null to chase Spyro */
  int timer;            /* 0x04 lifetime */
} WispState;

/* Falling debris (ACTOR_TYPE_FALLING_DEBRIS, levels 13/14/15). */
typedef struct FallingDebrisState {
  int vel[3]; /* 0x00 random spread; vel[2] falls by 6 a frame to -0x80 */
  int life;   /* 0x0C 0x20..0x3F frames */
} FallingDebrisState;

/* Levels 3/7/8 bouncing debris (ACTOR_TYPE_SWAYING_DEBRIS and kin): thrown
 * with a random push, sways while it falls and bounces off the ground. */
typedef struct BounceState {
  short vel[3];       /* 0x00 */
  short spin[3];      /* 0x06 per-frame rotX/rotY/rotZ deltas */
  unsigned char life; /* 0x0C */
  unsigned char slow; /* 0x0D frames until the push dies off */
  short sway;         /* 0x0E */
  int mode;           /* 0x10 enum BounceMotion */
} BounceState;

/* BounceState.mode, picked at random at spawn. */
enum BounceMotion {
  BOUNCE_SPIN = 0, /* tumbles: rotZ turns 0xC a frame */
  BOUNCE_SWAY = 1, /* falls rocking side to side by `sway` like a leaf */
};

/* Level 19 type 35 (ACTOR_TYPE_BOLT_GUARD): scatters when hit and fires type
 * 494 bolts. SpawnActor aims its ground-bounce shards along `knockHeading`. */
typedef struct BoltGuardState {
  int mode;    /* 0x00 enum BoltGuardCue */
  int timer;   /* 0x04 */
  int timer2;  /* 0x08 */
  int knockHeading; /* 0x0C knock-back heading (12-bit), set when it is hit */
  int speed;   /* 0x10 */
  int zSpeed;    /* 0x14 vertical speed (DriveActorTrackedMove) */
  int pad18;     /* 0x18 */
  struct PathData *path; /* 0x1C */
  int range;   /* 0x20 in 1024s */
  int countdown;     /* 0x24 */
  int link;    /* 0x28 actor index, -1 for none; initially (level data) ACTOR_TYPE_ROAMING_RAM */
  short spinX; /* 0x2C */
  short spinY; /* 0x2E */
  TriggerBox box; /* 0x30 */
} BoltGuardState;

/* ACTOR_TYPE_BLASTABLE_PATH_SMOKER / _PROP: path tweeners a blast
 * wrecks (flight levels 5/29). */
typedef struct SmokerState {
  struct PathData *path; /* 0x00 */
  int legStep;           /* 0x04 */
  int legVec[3];        /* 0x08 previous leg vector (TweenActorAlongPath); a wreck ember starts from it */
  int timer;           /* 0x14 */
} SmokerState;

/* Tumbling fragments: SpawnActor's chest/object shards (types 0x43..0x45,
 * 0xFF..0x101, 0x135..0x137 and per-level ranges) and UpdateActors' falling
 * debris shards thrown by a break (255/256, 310/311, 257, 309, 425, level 27's
 * rubble). Integrate until life, visibility or the floor runs out. Other updaters view some of these types as ChestShardState (byte
 * spins) or EmberState. */
typedef struct TumbleState {
  short vel[3];  /* 0x00 */
  short spin[3]; /* 0x06 per-frame delta for ->rotX/rotY/rotZ */
  int life;      /* 0x0C ticks remaining */
  int floorZ;    /* 0x10 retire once the actor sinks to this height */
} TumbleState;

#endif /* OPEN_SPYRO_ACTOR_STATES_H */
