/* flight_actor_states.h — actor state blocks shared by the five flight-level *_UpdateActors.c clones.
 *
 * Hoisted verbatim from the clones; each type is spelled identically in
 * every file that defines it. Included after level_update_actors.h. */

#ifndef OPEN_SPYRO_FLIGHT_ACTOR_STATES_H
#define OPEN_SPYRO_FLIGHT_ACTOR_STATES_H

/* Case 34 — the vortex/whirlwind pad that lifts Spyro out of the level. */
typedef struct VortexState {
  int idx;               /* 0x00 */
  int centre[3];         /* 0x04 pad centre */
  int apex[3];           /* 0x0A (short-aligned) rise target */
  unsigned short radius; /* 0x10 */
  unsigned char phase;   /* 0x12 */
  unsigned char angle;   /* 0x13 */
} VortexState;

/* Cases 14/15/83..87 — the generic enemy: a walker with a chase/charge/knock
 * -back state machine driven by ->subMode. */
typedef struct EnemyState {
  int vel[3];           /* 0x00 velocity, also read as a vector */
  short node;           /* 0x0C current path node */
  unsigned char mode;   /* 0x0E sub-mode within ->subMode */
  unsigned char timer;  /* 0x0F */
  signed char bounce;   /* 0x10 remaining knock-back bounces */
  signed char lean;     /* 0x11 body pitch */
  signed char roll;     /* 0x12 */
  unsigned char spin;   /* 0x13 */
  unsigned char attach; /* 0x14 attached smoke/spark handle, 0xFF = none */
} EnemyState;

/* Cases 255/256, 310/311, 257, 309 — debris shards thrown by a break. */
typedef struct ShardState {
  short vel[3];        /* 0x00 */
  signed char spin[3]; /* 0x06 pitch / yaw / roll per frame */
  char pad09[0x3];
  int life;  /* 0x0C */
  int floor; /* 0x10 */
} ShardState;

/* Case 398 — the level-exit vortex: drives the scripted camera and hands the
 * game over to the world map. */
typedef struct ExitState {
  PortalDesc *portal; /* 0x00 */
  int centre[3];      /* 0x04 */
  int dest;           /* 0x14 */
  int preset;         /* 0x18 */
} ExitState;

/* Flight targets (0x154 / 0x161): a helper actor spawned alongside, and the
 * sound to play when Spyro flies through. */
typedef struct TargetState {
  int pad00;
  int pad04;
  int pad08;
  int pad0C;
  Actor *helper;  /* 0x10 */
  int helperType; /* 0x14, spawned as helperType + 0x158 */
  int sound;      /* 0x18 */
} TargetState;

/* Particle burst shards spawned by the 0x161 target. */
typedef struct BurstState {
  int posX, posY, posZ; /* 0x00 velocity */
  signed char spinX;    /* 0x0C */
  signed char spinY;
  signed char spinZ;
  char pad0F;
  int life;             /* 0x10 */
} BurstState;

/* HUD tally icon flight (0x183..0x185 / 0x1EB): pivot, radius, angle and the
 * swept phase. */
typedef struct TallyState {
  int x;        /* 0x00 pivot */
  int y;        /* 0x04 */
  int z;        /* 0x08 */
  short radius; /* 0x0C */
  short angle;  /* 0x0E */
  short phase;  /* 0x10 */
} TallyState;

/* Flight HUD controller (0x166): clock digits, the per-class tally icons,
 * icons being spent into a slot, and the four finish slots. */
typedef struct HudState {
  int time;         /* 0x00 starting clock, in ticks */
  Actor *digit[8];  /* 0x04 clock digits; 4..7 only in time attack */
  Actor *icon[8];   /* 0x24 tally row */
  Actor *spent[8];  /* 0x44 a completed row flying to its slot */
  Actor *slot[4];   /* 0x64 finish slots */
  int types[4];     /* 0x74 icon actor type per class */
  int pad84[4];     /* 0x84 */
  int timer;        /* 0x94 */
  int count;        /* 0x98 icons still to spend */
  int hurry;        /* 0x9C under a minute left */
} HudState;

/* Type 0x11: a drifting shard. Moves by ->vel until its timer runs out or
 * it hits something. */
typedef struct DriftState {
  int vel[3]; /* 0x00 */
  int timer;  /* 0x0C */
} DriftState;

/* Types 0x1E1..0x1E3: the walker's embers. */
typedef struct EmberState {
  short vel[3];  /* 0x00 */
  short spin[3]; /* 0x06 */
  int life;      /* 0x0C */
} EmberState;

/* Type 0x4E: a rider that sits on its host actor; when hit it pays out,
 * bursts and retires. */
typedef struct Rot3 {
  unsigned char r[3];
} Rot3;

typedef struct RiderState {
  int reward; /* 0x00 reward actor type - 0x158 */
  int tally;  /* 0x04 */
  int host;   /* 0x08 index into the actor array */
  int timer;  /* 0x0C */
} RiderState;

/* Type 0x134: a path walker that rolls with its turns and, when flamed,
 * spawns its reward and three embers and runs off. */
typedef struct RollerState {
  unsigned char *path; /* 0x00 [0]=node count, [1]=nearest node */
  int reward;          /* 0x04 reward actor type - 0x158 */
  int tally;           /* 0x08 */
  int roll;            /* 0x0C */
  int rollVel;         /* 0x10 */
  int zVel;           /* 0x14 passed by address to DriveActorTrackedMove */
  int heading;         /* 0x18 */
  int moveSpeed;           /* 0x1C passed by address to DriveActorTrackedMove */
  int legVec[3];        /* 0x20 */
  int legStep;           /* 0x2C */
  int ready;           /* 0x30 nearest node found */
} RollerState;

/* Types 0x197/0x198: a smoking walker. */
typedef struct SmokerState {
  unsigned char *path; /* 0x00 */
  int legStep;           /* 0x04 */
  int legVec[3];        /* 0x08 */
  int timer;           /* 0x14 */
} SmokerState;

#endif /* OPEN_SPYRO_FLIGHT_ACTOR_STATES_H */
