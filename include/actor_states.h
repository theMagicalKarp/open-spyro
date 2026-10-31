/* actor_states.h — class-specific state blocks (Actor->state) that main and
 * the level overlays both read. They stand alone, without the Actor type, so
 * overlay headers that only forward-declare struct Actor can include this. */

#ifndef OPEN_SPYRO_ACTOR_STATES_H
#define OPEN_SPYRO_ACTOR_STATES_H

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
  int partner;      /* 0x20 actor index, -1 = none: held at mode 3 while the
                     * statue stands, anim 1 during the rescue, back to mode 1
                     * after; its yaw is the respawn heading */
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

#endif /* OPEN_SPYRO_ACTOR_STATES_H */
