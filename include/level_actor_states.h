/* level_actor_states.h — actor state blocks shared by the ground-level *_UpdateActors.c clones.
 *
 * Hoisted verbatim from the clones; each type is spelled identically in
 * every file that defines it. Included after level_update_actors.h. */

#ifndef OPEN_SPYRO_LEVEL_ACTOR_STATES_H
#define OPEN_SPYRO_LEVEL_ACTOR_STATES_H

/* Case 34 — a recovered dragon egg: spirals up off its spot, flies to just
 * above Spyro, then requests the HUD egg-counter roll (Cross skips). */
typedef struct EggRiseState {
  int idx;               /* 0x00 */
  short centre[3];       /* 0x04 where it rose from (spiral centre) */
  short apex[3];         /* 0x0A the point above Spyro it flies to */
  unsigned short radius; /* 0x10 */
  unsigned char phase;   /* 0x12 */
  unsigned char angle;   /* 0x13 */
} EggRiseState;

/* Cases 14/15/83..87 — collectibles: 14 extra life, 15 Sparx food, 83..87
 * gems (CollectPickupActor). They bounce out of a broken chest or fodder,
 * glitter, and home in on Spyro once he (or Sparx, fetching) gets close. The
 * same record as level_update_actors.h PickupState, in the field view these
 * clones were matched with (signed lean/roll). */
typedef struct CollectibleState {
  int vel[3];           /* 0x00 velocity, also read as a vector */
  short groundTri;      /* 0x0C collision triangle it rests on */
  unsigned char mode;   /* 0x0E resting sub-state within ->subMode */
  unsigned char timer;  /* 0x0F */
  unsigned char bounce; /* 0x10 bounces left */
  signed char lean;     /* 0x11 body pitch */
  signed char roll;     /* 0x12 */
  unsigned char spin;   /* 0x13 */
  unsigned char attach; /* 0x14 attached smoke/spark handle, 0xFF = none */
} CollectibleState;

/* Cases 255/256, 310/311, 257, 309 — debris shards thrown by a break. */
typedef struct ShardState {
  short vel[3];  /* 0x00 */
  short spin[3]; /* 0x06 pitch / yaw / roll per frame */
  int life;      /* 0x0C */
  int floor;     /* 0x10 */
} ShardState;

/* Type 227 (levels 14, 27): superflame giver. When Spyro faces it in range
 * it freezes him, flies in front of him under a scripted camera (`cam`), and
 * sets g_nSpyroSuperflameTimer by `kind`; then flies back to `home`. */
typedef struct SuperflameGiverState {
  int home[3];    /* 0x00 */
  int timer;      /* 0x0C */
  int cam[3];     /* 0x10 camera anchor */
  int pad1C;      /* 0x1C */
  int kind;       /* 0x20 */
  int count;      /* 0x24 */
  PathData *path; /* 0x28 */
  int cool;       /* 0x2C */
  int alert;      /* 0x30 */
  int link;       /* 0x34 pool index, -1 = none */
  int heading;    /* 0x38 -1 = unset */
} SuperflameGiverState;

/* Case 461 -- war drum. */
typedef struct WarDrumState {
  int chan;   /* 0x00 voice channel */
  int wait;   /* 0x04 */
  int beat;   /* 0x08 */
  int fade;   /* 0x0C */
  int knock;  /* 0x10 knock-back heading */
  int speed;  /* 0x14 */
  int lift;   /* 0x18 */
  int life;   /* 0x1C */
  int reload; /* 0x20 */
  int voice;  /* 0x24 */
} WarDrumState;

/* Case 466 -- roaming ram. */
typedef struct RoamingRamState {
  char pad00[0xC];
  PathData *path; /* 0x0C */
  int knock;      /* 0x10 knock-back heading */
  int speed;      /* 0x14 */
  int lift;       /* 0x18 */
  int mode;       /* 0x1C */
  int partner;    /* 0x20 index into g_pActorListBase */
  int target;     /* 0x24 index into g_pActorListBase */
  int chan;       /* 0x28 voice channel, -1 = none */
  int p2C;        /* 0x2C */
  char pad30[0x14];
  int call;       /* 0x44 */
} RoamingRamState;

/* Case 137 -- guard. */
typedef struct ChargingGuardState {
  int mode;          /* 0x00 0 idle, 1 patrol, 2 post, 3 follow */
  int vec[3];        /* 0x04 home point */
  int timer;         /* 0x10 */
  int range;         /* 0x14 sight range >> 10 */
  PathData *path;    /* 0x18 */
  int pad1C;         /* 0x1C */
  int knock;         /* 0x20 knock-back heading */
  int speed;         /* 0x24 */
  int turn;          /* 0x28 */
  int timer2;        /* 0x2C */
  int cam;           /* 0x30 camera phase */
  int camDist;       /* 0x34 */
  int pad38;         /* 0x38 */
  int timer3;        /* 0x3C */
  int *flag;         /* 0x40 shared alert flag */
  int dist2;         /* 0x44 */
  int target;        /* 0x48 index into g_pActorListBase */
  int filter;        /* 0x4C type it drives before it */
  int charging;      /* 0x50 */
  int init;          /* 0x54 */
  int lurk;          /* 0x58 */
  int speedOverride; /* 0x5C */
} ChargingGuardState;

/* Type 28 state (level 20): the gunner, paired with a `partner`; fires
 * type 400 shots in mode 5. */
typedef struct GunnerState {
  void *path;  /* 0x00 */
  int partner; /* 0x04 index into g_pActorListBase */
  int knockSpeed;     /* 0x08 knock-back speed */
  int knockZSpeed;     /* 0x0C */
  int knockHeading;     /* 0x10 knock-back heading */
  int timer;   /* 0x14 */
  int mode;    /* 0x18 0 guard, 1 patrol, 2 lookout */
  int hit;     /* 0x1C */
  int range;   /* 0x20 sight range (< 0x20: in units of 0x400) */
} GunnerState;

/* Type 403 state (level 20): the lurker with its `tether`; can hold Spyro. */
typedef struct LurkerState {
  int mode;      /* 0x00 0 roams its path */
  FlyPath *path; /* 0x04 */
  int knockHeading;       /* 0x08 knock-back heading */
  int knockSpeed;       /* 0x0C knock-back speed */
  int knockZSpeed;       /* 0x10 */
  int u14;       /* 0x14 */
  int roamTimer;       /* 0x18 roam timer */
  int hopSpeed;       /* 0x1C hop speed */
  Actor *tether; /* 0x20 */
  int coolDown;       /* 0x24 cool-down */
  int holdsSpyro;       /* 0x28 holds Spyro */
  int u2C[3];    /* 0x2C */
  int stunTimer;       /* 0x38 stun timer */
} LurkerState;

/* Type 476 state (level 20): the grabber that carries Spyro off to `home`. */
typedef struct GrabberState {
  int u00[2];   /* 0x00 */
  int home[3];  /* 0x08 drop-off point */
  int grabChance;      /* 0x14 grab chance */
  int timer;      /* 0x18 timer */
  int u1C;      /* 0x1C */
  int knockHeading;      /* 0x20 knock-back heading */
  int knockSpeed;      /* 0x24 knock-back speed */
  int knockZSpeed;      /* 0x28 */
  int *held;    /* 0x2C shared "Spyro is held" latch */
  int hasSpyro;      /* 0x30 has Spyro */
  int grabCoolDown;      /* 0x34 grab cool-down */
  int voice;      /* 0x38 */
} GrabberState;

/* Type 57 state (boulder and its shards). */
typedef struct BoulderState {
  short vx;                /* 0x00 */
  short vy;                /* 0x02 */
  short vz;                /* 0x04 */
  short u06;               /* 0x06 */
  short u08;               /* 0x08 */
  short life;              /* 0x0A */
  unsigned char spin[3];   /* 0x0C */
  unsigned char heading;   /* 0x0F */
} BoulderState;

/* Type 100 state (boulder thrower). */
typedef struct BoulderThrowerState {
  int partner;     /* 0x00 index into g_pActorListBase, -1 none */
  int rider;       /* 0x04 index into g_pActorListBase */
  int groupAlert;         /* 0x08 level data: on waking, alert the group and its chunk anim */
  int sound;         /* 0x0C sound handle */
  int idlePose2;         /* 0x10 level data: idle in pose 2 */
  int partnerSignal; /* 0x14 set to 1 by the partner (its state word 5) */
  int u18[3];      /* 0x18 */
  FlyPath *path;   /* 0x24 */
  int flags;         /* 0x28 flags */
  int timer;         /* 0x2C timer */
  int heading;         /* 0x30 heading */
  int speed;         /* 0x34 speed */
  int u38;         /* 0x38 */
  int zSpeed;         /* 0x3C */
  Actor *rock;     /* 0x40 boulder being carried */
  int sightRange;         /* 0x44 sight range */
  int throwRange;         /* 0x48 throw range */
  int box0[3];      /* 0x4C trigger box */
  int box0HalfWidth;         /* 0x58 */
  int u5C[2];      /* 0x5C */
  int box1[6];      /* 0x64 trigger box */
  int command;         /* 0x7C posted: 1 = mode 0x28, else reverse along the path (mode 0x32); cleared when taken */
  int orbitRadius;         /* 0x80 */
  int jitterA;         /* 0x84 random 8..15 on entering mode 0x2B; not read here */
  int jitterB;         /* 0x88 likewise */
  int *releaseFlag;        /* 0x8C cleared at the throw's release frame (0xE) */
  int u90;         /* 0x90 */
  int landSoundPlayed;         /* 0x94 */
} BoulderThrowerState;

/* Type 101 state (gatekeeper). */
typedef struct GatekeeperState {
  int partner;     /* 0x00 index into g_pActorListBase, -1 none */
  int rider;       /* 0x04 index into g_pActorListBase */
  int groupAlert;         /* 0x08 level data: on waking, alert the group and its chunk anim */
  int sound;         /* 0x0C sound handle */
  int idlePose2;         /* 0x10 level data: idle in pose 2 */
  int partnerSignal; /* 0x14 set to 1 by the partner (its state word 5) */
  int mode;        /* 0x18 */
  int timer;         /* 0x1C timer */
  int u20[3];      /* 0x20 */
  int heading;         /* 0x2C heading */
  int speed;         /* 0x30 speed */
  int zSpeed;         /* 0x34 */
  FlyPath *path;   /* 0x38 */
  int box0[6];      /* 0x3C trigger box */
  int box1[6];      /* 0x54 trigger box */
  int command;         /* 0x6C posted: 1 = mode 0x28, else reverse along the path (mode 0x32); cleared when taken */
  int idx;         /* 0x70 next key */
  int range[2];    /* 0x74 release range per key (units of 0x400) */
  int keys[2];     /* 0x7C key actor indices, -1 released */
  int count;       /* 0x84 */
  int *releaseFlag;        /* 0x88 cleared at the throw's release frame (0xE) */
  int u8C;         /* 0x8C */
  int testFacing;         /* 0x90 facing to test, -1 any */
} GatekeeperState;

/* Type 293/294 state (path runner): runs its path ahead of Spyro, leads
 * him to a goal, and jumps or scatters when struck. */
typedef struct PathRunnerState {
  int speed;           /* 0x00 flee speed */
  unsigned char *path; /* 0x04 current path: [0] count, [1] cur, nodes at +8 */
  unsigned char *home; /* 0x08 home path */
  int knock;           /* 0x0C knock-back speed */
  int knockAng;        /* 0x10 knock-back heading */
  int lift;            /* 0x14 vertical speed */
  int target;          /* 0x18 index into g_pActorListBase, -1 none */
  int pathInit;        /* 0x1C */
  int camTimer;        /* 0x20 */
  unsigned int mode;   /* 0x24 0/1 = which goal flag it drives */
  int range;           /* 0x28 */
  int timer;           /* 0x2C */
  int init;            /* 0x30 */
  int volA[6];         /* 0x34 trigger box */
  int volB[6];         /* 0x4C trigger box */
  int camDone;         /* 0x64 */
} PathRunnerState;

/* Type 82 state: a thrown shot. */
typedef struct PodShotState {
  int vel[3];             /* 0x00 */
  short timer;            /* 0x0C */
  unsigned char heading;  /* 0x0E */
  unsigned char mode;     /* 0x0F 1 = spent, just times out */
} PodShotState;

/* Type 210 state: a bobbing platform. */
typedef struct BobbingPlatformState {
  int z;      /* 0x00 phase */
  int spawned;    /* 0x04 set to 1 by the pod that spawns it; not read here */
  int period; /* 0x08 */
  int baseZ;  /* 0x0C */
} BobbingPlatformState;

/* Type 70 state: a lobbed bolt. */
typedef struct LobbedBoltState {
  int speed;     /* 0x00 */
  int vz;        /* 0x04 */
  int dvz;       /* 0x08 gravity */
  int flightTime;       /* 0x0C frames to the target (ComputeLobVelocityZ out) */
  short timer;   /* 0x10 */
  short heading; /* 0x12 */
} LobbedBoltState;

/* Type 65 state: a pod that raises a ring of bobbing platforms (210) and
 * lobs shots (82) at its partner's target. */
typedef struct PlatformPodState {
  int timer;      /* 0x00 */
  int period;     /* 0x04 */
  int u08;        /* 0x08 */
  int u0C;        /* 0x0C */
  int *counter;   /* 0x10 shared live-pod count */
  int initCount;  /* 0x14 */
  int init;       /* 0x18 */
  int partner;    /* 0x1C index into g_pActorListBase */
  int shotTimer;  /* 0x20 */
  Actor *kids[4]; /* 0x24 */
} PlatformPodState;

/* Type 48 state: Metalhead. Three phases, each ending in a run along the
 * path to the next arena spot. */
typedef struct MetalheadState {
  int phase;            /* 0x00 */
  unsigned char *path;  /* 0x04 */
  int timer;            /* 0x08 attack cooldown */
  int timer2;           /* 0x0C */
  int pathEnd[2];       /* 0x10 per-phase last path node */
  int range[2];         /* 0x18 per-phase stomp range (units of 0x400) */
  int choice;           /* 0x20 0 none, 1 stomp, 2 shots, 3 bolt */
  int shots;            /* 0x24 */
  int count;            /* 0x28 */
  int aim[3];           /* 0x2C */
  int hand[3];          /* 0x38 */
  int seen;             /* 0x44 */
  int init;             /* 0x48 */
  int camTimer;         /* 0x4C */
  int lodHandle;              /* 0x50 */
} MetalheadState;

/* Type 47 state: a thrown rock, carried by its partner until released. */
typedef struct CarriedRockState {
  unsigned char timer;  /* 0x00 */
  unsigned char spinX;  /* 0x01 */
  unsigned char spinY;  /* 0x02 */
  unsigned char mode;   /* 0x03 1 = held, 0x63 = idle */
  short speed;          /* 0x04 */
  short u06;            /* 0x06 */
  short vz;             /* 0x08 */
  short timer2;         /* 0x0A */
  Actor *partner;       /* 0x0C the thrower */
} CarriedRockState;

/* Type 150 state: Jacques. Hops between the stops of his path ring and
 * hands rocks (47) to throw. */
typedef struct JacquesState {
  PathData *path;          /* 0x00 */
  int u04;                 /* 0x04 */
  int u08;                 /* 0x08 */
  unsigned char stops[7];  /* 0x0C path nodes he rests at */
  unsigned char pad13;
  int idx;                 /* 0x14 current stop */
  int init;                /* 0x18 */
  int prevLeg[3];              /* 0x1C */
  int legStep[3];              /* 0x28 */
  int timer;               /* 0x34 */
  int u38;                 /* 0x38 */
  Actor *stopActor;              /* 0x3C level actor tied to the stop just reached (38 or 41) */
  int atStop;                 /* 0x40 set on reaching each stop */
  Actor *child;            /* 0x44 rock in hand */
  int u48;                 /* 0x48 */
  int freezesSpyro;                 /* 0x4C level data: hops block gameplay and freeze Spyro's input */
  int standGround;                 /* 0x50 1 at stops 0/7/11: does not hop away from a close Spyro */
  int u54;                 /* 0x54 */
} JacquesState;

/* Types 45/46 state: a charging ram (45 is the bigger one). */
typedef struct ChargingRamState {
  int init;     /* 0x00 */
  int mode;     /* 0x04 nonzero = starts awake */
  int knock;    /* 0x08 knock-back speed */
  int lift;     /* 0x0C */
  int u10;      /* 0x10 */
  int heading;  /* 0x14 */
  int u18;      /* 0x18 */
  int u1C;      /* 0x1C */
  int u20;      /* 0x20 */
  int home[3];  /* 0x24 */
  int timer3;   /* 0x30 */
  int timer2;   /* 0x34 */
  int charged;  /* 0x38 */
  int u3C;      /* 0x3C */
  int saved;    /* 0x40 state to return to */
  int heavy;    /* 0x44 */
  int timer;    /* 0x48 cooldown */
} ChargingRamState;

/* Type 126 state: a path walker that can carry a rider and sings while
 * it rests. */
typedef struct PathWalkerState {
  int init;            /* 0x00 */
  int mode;            /* 0x04 0..6; 3 = carried, 6 = absent */
  PathData *path;      /* 0x08 */
  int next;            /* 0x0C state to enter after the turn, 0x63 = done */
  int prevLeg[3];          /* 0x10 */
  int legStep[5];          /* 0x1C */
  int sound;           /* 0x30 */
  int period;          /* 0x34 */
  int timer;           /* 0x38 */
  int u3C;             /* 0x3C */
  int startCur;        /* 0x40 */
  int partner;         /* 0x44 index into g_pActorListBase */
  int flag;            /* 0x48 */
  int u4C;             /* 0x4C */
  int rideMode;        /* 0x50 1 = held, 2 = thrown */
  int rider;           /* 0x54 index into g_pActorListBase */
  int speed;           /* 0x58 */
  int vz;              /* 0x5C */
  int dvz;             /* 0x60 */
  int call;            /* 0x64 */
} PathWalkerState;

/* A trail point (as level 33's type 169). */
typedef struct CometTrailPoint {
  int pos[3]; /* 0x00 */
  int u0C[3]; /* 0x0C */
  int age;    /* 0x18 */
} CometTrailPoint;   /* 0x1C */

/* Type 104 state: a comet that leaves a fading trail and drops sparks. */
typedef struct CometState {
  short life;           /* 0x00 */
  short lifeOn;         /* 0x02 */
  CometTrailPoint *trail;      /* 0x04 */
  short speed;          /* 0x08 */
  short vz;             /* 0x0A */
  short spark;          /* 0x0C spark timer */
  short dvz;            /* 0x0E */
  short timer;          /* 0x10 */
  unsigned char steps;  /* 0x12 extra steps this frame */
  unsigned char count;  /* 0x13 trail points */
} CometState;

/* Type 91 state: a cannon. Fires comets (104) at Spyro's targets or at the
 * actor its trigger names; each comet keeps its trail in one of three
 * buffers here. */
typedef struct CometCannonState {
  int steps;      /* 0x00 comet sub-steps */
  int link;       /* 0x04 index into g_pActorListBase (its trigger) */
  int target;     /* 0x08 index into g_pActorListBase */
  int u0C;        /* 0x0C */
  int timer;      /* 0x10 */
  int heading;    /* 0x14 */
  int camFlag;    /* 0x18 */
  int u1C;        /* 0x1C */
  int idx;        /* 0x20 trail buffer in use */
  int init;       /* 0x24 */
  char trails[1]; /* 0x28 */
} CometCannonState;

/* Types 92/130 state: a guard whose animations start at `base`. */
typedef struct LungingGuardState {
  int knock;           /* 0x00 knock-back speed */
  int heading;         /* 0x04 knock-back heading */
  int lift;            /* 0x08 */
  int base;            /* 0x0C first animation */
  int kind;            /* 0x10 */
  int timer;           /* 0x14 */
  int vol[6];          /* 0x18 trigger box */
  unsigned char *path; /* 0x30 */
  int flags;           /* 0x34 */
  int init;            /* 0x38 */
  int range;           /* 0x3C <0 = waiting */
  int partner;         /* 0x40 index into g_pActorListBase */
  int msg;             /* 0x44 written into the partner's state */
  int range2;          /* 0x48 */
  int timer2;          /* 0x4C */
  int reload;          /* 0x50 */
  int spin;            /* 0x54 */
} LungingGuardState;

/* Type 130 state: the bigger guard (as type 92's, with two followers). */
typedef struct BigLungingGuardState {
  int knock;           /* 0x00 */
  int heading;         /* 0x04 */
  int lift;            /* 0x08 */
  int base;            /* 0x0C */
  int kind;            /* 0x10 */
  int timer;           /* 0x14 */
  int vol[6];          /* 0x18 */
  unsigned char *path; /* 0x30 */
  int flags;           /* 0x34 */
  int init;            /* 0x38 */
  int range;           /* 0x3C */
  int partner;         /* 0x40 */
  int msg;             /* 0x44 */
  int range2;          /* 0x48 */
  int timer2;          /* 0x4C */
  int reload;          /* 0x50 */
  int follow1;         /* 0x54 index into g_pActorListBase, -1 none */
  int follow2;         /* 0x58 */
  int spin;            /* 0x5C */
  int u60;             /* 0x60 */
  int flip;            /* 0x64 */
  int sound;           /* 0x68 */
} BigLungingGuardState;

/* Type 135 state: a falling chunk. */
typedef struct FallingChunkState {
  short vx;               /* 0x00 */
  short vy;               /* 0x02 */
  short vz;               /* 0x04 */
  unsigned char b06;      /* 0x06 */
  unsigned char noFloor;  /* 0x07 */
  short u08;              /* 0x08 */
  short age;              /* 0x0A */
  short grav;             /* 0x0C */
  short u0E;              /* 0x0E */
  unsigned char spin[3];  /* 0x10 */
} FallingChunkState;

/* Type 136 state: a breakable wall made of four parts. */
typedef struct BreakableWallState {
  Actor *part[4]; /* 0x00 */
  int kind;       /* 0x10 1 = only a flame-charge breaks it */
  int init;       /* 0x14 */
  int lodHandle;        /* 0x18 */
} BreakableWallState;

/* Type 134 state: a rock thrower. */
typedef struct RockThrowerState {
  int heading;          /* 0x00 knock-back heading */
  int speed;            /* 0x04 */
  int lift;             /* 0x08 */
  int flags;            /* 0x0C bit 0 = watch, bit 1 = escort */
  int vol[6];           /* 0x10 trigger box */
  int timer;            /* 0x28 */
  int range;            /* 0x2C */
  int range2;           /* 0x30 */
  unsigned char *path;  /* 0x34 */
  int partner;          /* 0x38 index into g_pActorListBase */
  int u3C;              /* 0x3C */
  int link;             /* 0x40 index into g_pActorListBase, -1 none */
  int trig;             /* 0x44 */
} RockThrowerState;

/* Type 203 state: a guard that walks a fence segment. */
typedef struct FenceGuardState {
  int reactDelay;    /* 0x00 read by the fence's owner as its reaction timer */
  int onPath; /* 0x04 1 = keep to the segment in path */
  int wake;   /* 0x08 */
  int *path;  /* 0x0C segment ends at [2],[3] and [6],[7] */
  int u10;    /* 0x10 */
  int timer;  /* 0x14 */
  int link;   /* 0x18 index into g_pActorListBase, -1 none */
  int u1C;    /* 0x1C */
  int timer0; /* 0x20 timer reload */
} FenceGuardState;

/* Type 215 state: one of the guard's six pieces. */
typedef struct FenceGuardPieceState {
  int u00;   /* 0x00 */
  short lift; /* 0x04 */
} FenceGuardPieceState;

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

/* Type 204 state: the fence-guard captain. Wakes the guards (203) whose
 * boxes Spyro has entered and sends them in one at a time. */
typedef struct FenceCaptainState {
  unsigned char *path; /* 0x000 */
  int u04[7];          /* 0x004 */
  int link[7];         /* 0x020 the guards, index into g_pActorListBase, -1 none */
  int cur;             /* 0x03C */
  TriggerBox box[7];       /* 0x040 one per guard */
  int hit[7];          /* 0x0E8 */
  int heading;         /* 0x104 */
  int speed;           /* 0x108 */
  int zSpeed;            /* 0x10C */
  TriggerBox reset;        /* 0x110 */
  int timer;           /* 0x128 */
  int stage;           /* 0x12C */
  int puff;            /* 0x130 */
  TriggerBox all1;         /* 0x134 */
  TriggerBox all2;         /* 0x14C */
} FenceCaptainState;

/* Type 145 state (level 32): gunner that fires bursts of type 156 shots. */
typedef struct BurstGunnerState {
  int flags;     /* 0x00 flags; bit 0 armed */
  int knockSpeed;     /* 0x04 knock-back speed */
  int knockHeading;     /* 0x08 knock-back heading */
  int knockZSpeed;     /* 0x0C */
  int shotTimer;     /* 0x10 shot timer */
  int shotGap;     /* 0x14 frames since the last shot of a burst */
  int shotReload;     /* 0x18 shot timer reload */
  void *path;   /* 0x1C path */
  int u20;     /* 0x20 */
  int countdown;     /* 0x24 */
  int box0[6];  /* 0x28 trigger box */
  int box1[6];  /* 0x40 trigger box */
  int box2[6];  /* 0x58 trigger box */
  int initialised;     /* 0x70 initialised */
  int sinkTimer;     /* 0x74 sink timer */
} BurstGunnerState;

/* Type 147 state (level 32): gunner that sweeps its aim between two yaw
 * limits. */
typedef struct SweepGunnerState {
  int flags;     /* 0x00 flags; bit 0 armed */
  void *path;   /* 0x04 path */
  int knockSpeed;     /* 0x08 knock-back speed */
  int knockHeading;     /* 0x0C knock-back heading */
  int knockZSpeed;     /* 0x10 */
  int u14;     /* 0x14 */
  int yawToSpyro;     /* 0x18 yaw toward Spyro */
  int yawLimitA;     /* 0x1C yaw limit (0xFF none) */
  int yawLimitB;     /* 0x20 yaw limit (0xFF none) */
  int u24;     /* 0x24 */
  int sweepRate;     /* 0x28 sweep rate */
  int sweepYaw;     /* 0x2C sweep yaw << 4 */
  int timer;     /* 0x30 timer */
  int box0[6];  /* 0x34 trigger box */
  int box1[6];  /* 0x4C trigger box */
  int box2[6];  /* 0x64 trigger box */
  int initialised;     /* 0x7C initialised */
} SweepGunnerState;

/* Type 148 state (level 32): gunner that throws a held type 176 lob. */
typedef struct LobGunnerState {
  int flags;              /* 0x00 flags; bit 0 armed */
  int knockSpeed;              /* 0x04 knock-back speed */
  int knockHeading;              /* 0x08 knock-back heading */
  int knockZSpeed;              /* 0x0C */
  int box0[6];           /* 0x10 trigger box */
  unsigned char *targetPath;   /* 0x28 target path: count, index, points */
  void *path;            /* 0x2C path */
  int timer;              /* 0x30 timer */
  int throwTimer;              /* 0x34 throw timer */
  Actor *heldLob;           /* 0x38 held lob (type 176), -1 once thrown */
  int u3C;              /* 0x3C */
  int aimRange;              /* 0x40 aim-at-path range */
  int box1[6];           /* 0x44 trigger box */
  int box2[6];           /* 0x5C trigger box */
  int initialised;              /* 0x74 initialised */
} LobGunnerState;

/* State of the lob type 148 throws. */
typedef struct LobState {
  int flightTime;            /* 0x00 frames to the target (ComputeLobVelocityZ out) */
  short h4;           /* 0x04 */
  short h6;           /* 0x06 */
  short h8;           /* 0x08 */
  short hA;           /* 0x0A */
  unsigned char bC;   /* 0x0C */
  unsigned char bD;   /* 0x0D */
  unsigned char bE;   /* 0x0E */
  unsigned char bF;   /* 0x0F */
} LobState;

/* Type 156 state. */
typedef struct BurstGunnerShotState {
  int vel[3]; /* 0x00 */
  short life;  /* 0x0C life timer */
  short longReachTimer;  /* 0x0E long-reach timer */
} BurstGunnerShotState;

/* Type 171 state. */
typedef struct FlameChunkSwitchState {
  int u00; /* 0x00 */
  int rearmTimer; /* 0x04 re-arm timer */
} FlameChunkSwitchState;

/* Type 236 state (level 32 wanderer; the head is UpdateActorWander's wander block). */
typedef struct WandererState {
  int u00[8]; /* 0x00 */
  int knockSpeed;    /* 0x20 knock-back speed */
  int knockHeading;    /* 0x24 knock-back heading */
  int u28;    /* 0x28 */
  int idleTimer;    /* 0x2C idle timer */
} WandererState;

#endif /* OPEN_SPYRO_LEVEL_ACTOR_STATES_H */
