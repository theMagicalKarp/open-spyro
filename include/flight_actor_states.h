/* flight_actor_states.h — actor state blocks shared by the five flight-level *_UpdateActors.c clones.
 *
 * Hoisted verbatim from the clones; each type is spelled identically in
 * every file that defines it. Included after level_update_actors.h. */

#ifndef OPEN_SPYRO_FLIGHT_ACTOR_STATES_H
#define OPEN_SPYRO_FLIGHT_ACTOR_STATES_H

/* Flight rings (ACTOR_TYPE_FLIGHT_RING): a helper actor spawned alongside, and the
 * sound to play when Spyro flies through. */
typedef struct TargetState {
  int pad00;
  int pad04;
  int pad08;
  int pad0C;
  Actor *helper;  /* 0x10 */
  int helperType; /* 0x14 its time bonus, spawned as TIME_BONUS_TYPE(helperType) */
  int tallyClass;      /* 0x18 flight target class 0..3 (TallyFlightTarget) */
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

/* HUD tally icon flight (ACTOR_TYPE_TALLY_ICON_*): pivot, radius, angle and the
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
  int tint[4];      /* 0x84 per-class icon tint word (0xA tag, see Actor tint0), copied to
                     * tint presets 3..6 (g_anFlightTallyTintPresets); icons use tintMode class + 3 */
  int timer;        /* 0x94 */
  int count;        /* 0x98 icons still to spend */
  int hurry;        /* 0x9C under a minute left */
} HudState;

/* ACTOR_TYPE_AIMED_SHOT: moves by ->vel until its timer runs out or
 * it hits something. */
typedef struct DriftState {
  int vel[3]; /* 0x00 */
  int timer;  /* 0x0C */
} DriftState;

/* ACTOR_TYPE_SPARK_DEBRIS (481..483). */
typedef struct EmberState {
  short vel[3];  /* 0x00 */
  short spin[3]; /* 0x06 */
  int life;      /* 0x0C */
} EmberState;

/* ACTOR_TYPE_RIDING_FLIGHT_TARGET: a rider that sits on its host actor; when hit it pays out,
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

/* ACTOR_TYPE_BANKING_FLIGHT_TARGET: a path walker that rolls with its turns and, when flamed,
 * spawns its reward and three embers and runs off. */
typedef struct RollerState {
  PathData *path; /* 0x00 */
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

/* Flight-level chest (ACTOR_TYPE_FLIGHT_CHEST): struck, it tallies and frees
 * its time bonus. */
typedef struct FlightChestState {
  int reward;     /* 0x00 its time bonus, spawned as TIME_BONUS_TYPE(reward) */
  int tallyClass; /* 0x04 flight target class 0..3 (TallyFlightTarget) */
} FlightChestState;

/* Flame flight target (ACTOR_TYPE_FLAME_FLIGHT_TARGET, levels 11/29). */
typedef struct FlameTargetState {
  int tallyClass; /* 0x00 flight target class 0..3 (TallyFlightTarget) */
  int reward;     /* 0x04 its time bonus, spawned as TIME_BONUS_TYPE(reward) */
  int chunkAnim;  /* 0x08 world chunk anim it resumes, or -1 */
} FlameTargetState;

#endif /* OPEN_SPYRO_FLIGHT_ACTOR_STATES_H */
