/* level_update_actors.h — declarations shared by the *_UpdateActors.c clones, declared once.
 *
 * Each item is spelled identically in every file that includes this header;
 * files that declare any of these names differently keep their own
 * declarations and do not include it. */

#ifndef OPEN_SPYRO_LEVEL_UPDATE_ACTORS_H
#define OPEN_SPYRO_LEVEL_UPDATE_ACTORS_H

#include "sparx.h"

#include "actor_states.h"
#include "cutscene_path.h"
#include "portal.h"
#include "model.h"

/* The update list, and the per-actor values the loop publishes before it
 * dispatches. */
extern int g_nActorFrameDelta;      /* frame delta, as seen by the callees */

/* Ground-trigger region type under a probe point. Unprototyped on purpose:
 * some clones pass the point, others call it bare with $a0 already holding it. */
extern int GetGroundTriggerRegionType();
extern int g_nSparxHealth; /* scalar form; the array users go through g_anSparxHealthBlock */

/* Spyro's persistent euler Z byte, re-read on every access. */
extern volatile unsigned char g_bSpyroPersistentEulerZ;

/* Model table entry, indexed by actor->type (model.h). */
extern Model *g_apActorMeshTable[]; /* per-type model table, indexed by ->type */

/* ==== Per-class state blocks ====
 *
 * Every actor class keeps its private state behind actor->state. The blocks
 * below sit next to the globals and callees their case needs.
 */

/* PathData (waypoint paths) lives in actor_type.h. */
/* Type 16: butterfly. Flutters around an anchor until Sparx fetches it. */
typedef struct ChaseState {
  int dir;               /* 0x00 -1 or 1, picked from which side Spyro is on */
  int tgtX;              /* 0x04 */
  int tgtY;              /* 0x08 */
  int pad0C;             /* 0x0C */
  unsigned char vz;      /* 0x10 vertical step, sign-magnitude in a byte */
  unsigned char heading; /* 0x11 */
  unsigned char headingTimer;     /* 0x12 heading-change timer */
  unsigned char climbTimer;     /* 0x13 vertical-change timer */
  unsigned char idleTimer;     /* 0x14 */
} ChaseState;

/* Sparx's state block, as seen by the actors that send him fetching. */
typedef struct ChaserState {
  int timer; /* 0x00 */
  int pad04;
  int pad08;
  int pad0C;
  Actor *owner; /* 0x10 actor being fetched, or null */
} ChaserState;

/* Engine callees shared by most behaviours: update list, collision and
 * movement helpers, random numbers and angle arithmetic. */
extern void BuildActiveActorList(void);

extern int WrapAngleDelta8(int a, int b); /* signed angle delta */

extern struct {
  short v;
} g_nSfxOverridePitch;

/* Egg sequence flags, then the vector library: copy, add, subtract, scale,
 * shift, sine and cosine. */
extern struct {
  int v;
} g_nHudWorldEggRollRequest;

extern void AimCameraStickTowardPoint(int *pos);

extern void SVectorToVectorShl2(int *dst, int *s);

extern void CommitActorKillGems(int value, void *actor);

/* Types 260..269: a floating gem-value digit (0x104 + digit), spawned by
 * SpawnFloatingDigitActors when a kill pays out gems. */
typedef struct GemDigitState {
  int life;   /* 0x00 */
  int pad04;  /* 0x04 */
  int vel[3]; /* 0x08 */
} GemDigitState;

/* Surface normal produced by the collision probes. */
extern int g_anCollisionProbeVec;

/* Single-field views for reading a scalar symbol through a struct type. */
typedef struct {
  int v;
} I32;

extern int CollideSphereWithWorldAndActors(int *pos, int r1, int r2); /* ground/wall probe */

/* g_aRotBillboards records (SpawnActorRotBillboard) used as sparkles around items. */
typedef struct RotBillboard {
  int pos[3];         /* 0x00 */
  unsigned char life; /* 0x0C frames left; 0 = slot free */
  unsigned char pad[11];
} RotBillboard;

extern RotBillboard g_aRotBillboards[];

/* Type 187: talking dragon. Dragon count, dialogue selection, and the latch
 * in the save-options block. */
extern void ResetSpyroLinearMotion(void);

/* Type 194: wooden chest. Breaks into fragments when damaged. */
typedef struct BreakState {
  int pad00; /* 0x00 */
  int snapToGround; /* 0x04 nonzero: re-snap to the floor every frame */
} BreakState;

/* Shake table: x/y rotation offsets, one row per two ticks. */
typedef struct AngPair {
  unsigned char a; /* 0x00 */
  unsigned char b; /* 0x01 */
} AngPair;

extern AngPair g_aShakeRotOffsets[];

/* Type 421: life chest. Shakes and pops its lid on a timer, spawning a
 * child actor; breaks into fragments when damaged. */
typedef struct LiftState {
  Actor *passenger; /* 0x00 the spawned passenger, valid once ->mode is 1 */
  int phase;    /* 0x04 phase */
  int savedRotX;    /* 0x08 saved actor->rotX */
  int savedRotY;    /* 0x0C saved actor->rotY */
  int savedPosZ;    /* 0x10 saved actor->posZ */
  int rideMode;    /* 0x14 mode; 1 selects the extra tick and the rearm */
} LiftState;

/* 0x18-byte spherical camera coordinates; g_aCameraXformPresets holds the presets. */
typedef struct Xform {
  int v[6];
} Xform;

/* ==== Shared objects ====
 *
 * The camera as one object at 0x80076DD0, with the fields this function
 * touches. */
typedef struct {
  char pad000[0x28];
  int posX;      /* 0x28 m_Position */
  int posY;      /* 0x2C */
  int posZ;      /* 0x30 */
  char pad034[0x18];
  short rotX;    /* 0x4C m_Rotation */
  short rotY;    /* 0x4E */
  short rotZ;    /* 0x50 */
  short pad52;   /* 0x52 */
  int occlusion; /* 0x54 */
  unsigned int state; /* 0x58 m_State */
  int pad5C;     /* 0x5C */
  Xform lastSim; /* 0x60 */
  Xform sphere;  /* 0x78 m_Sphere */
  Xform sim;     /* 0x90 m_Simulation */
  Xform springDelta; /* 0xA8 g_anCameraSpringDeltaState */
  int nextMode;  /* 0xC0 g_nCameraNextMode */
  char padC4[0x14];
  Xform *preset; /* 0xD8 m_SphericalPreset */
} CamObj;

extern CamObj g_anWorldToCameraRotMtx;

/* PortalCamPath (a hub portal's camera path) lives in portal.h. */

extern void StubRewardHook(int script, int a);

/* Callees used by the collectible and fairy behaviours. */
extern void UnpackWorldCollisionTri(int chunk, int *out);

extern int CheckLineOfSightSegment(int *from, int *to);

extern int ReflectVectorOffNormal(int *v, int *n, int *out);

/* Glow node owned by Sparx; follows its anchor position. */
typedef struct FxNode {
  int kind;            /* 0x00 */
  void *desc;          /* 0x04 */
  int *anchor;         /* 0x08 */
  unsigned char r;     /* 0x0C */
  unsigned char g;     /* 0x0D */
  unsigned char b;     /* 0x0E */
  unsigned char pad0F; /* 0x0F */
  int size;            /* 0x10 grows 0x20/frame, clamped at 0x400 */
  int offX;           /* 0x14 draw offset from *anchor (DrawWorldSparkleParticles) */
  int offY;           /* 0x18 */
  int offZ;           /* 0x1C */
  int sizeBias;        /* 0x20 added to the projected size (DrawWorldSparkleParticles) */
} FxNode;

extern FxNode *AllocWorldSparkle(void);

extern void ClearWord(FxNode *node);

extern int StepAngle8Toward(int to, int from, int maxStep);
extern int ApproachAngle8(int target, int cur, int step, int lim);

/* Types 257/425: falling chest fragments. Integrate until life, visibility
 * or the floor runs out. */
typedef struct TumbleState {
  short vel[3];  /* 0x00 */
  short spin[3]; /* 0x06 per-frame delta for ->rotX/rotY/rotZ */
  int life;      /* 0x0C ticks remaining */
  int floorZ;    /* 0x10 retire once the actor sinks to this height */
} TumbleState;

/* Spyro as one object at 0x80078A58, with the fields this function touches. */
typedef struct {
  int posX;                       /* 0x000 */
  int posY;                       /* 0x004 */
  int posZ;                       /* 0x008 */
  char pad00C[0x2];
  unsigned char bodyRotZ;         /* 0x00E */
  char pad00F[0x18];
  unsigned char otBinBias;        /* 0x027 g_bSpyroOtBinBias */
  char pad028[0x4];
  int triggerEvents;                /* 0x02C */
  char pad030[0x4];
  int bodyMtx;                    /* 0x034 g_anSpyroBodyMtx[0] */
  char pad038[0x40];
  int state;                      /* 0x078 */
  char pad07C[0x4];
  int stateTimer;                 /* 0x080 g_nSpyroStateTimer */
  char pad084[0x18];
  int airborneFrames;             /* 0x09C g_nSpyroAirborneFrames */
  char pad0A0[0x6C];
  int motionX;                    /* 0x10C g_anSpyroMotionVec[0] */
  int motionY;                    /* 0x110 g_nSpyroMotionY */
  int motionZ;                    /* 0x114 g_nSpyroMotionZ */
  int motionSpeed;                /* 0x118 g_nSpyroMotionSpeed */
  int bodyYaw;                    /* 0x11C g_nSpyroBodyYaw */
  char pad120[0x8];
  int frameMotionLength;          /* 0x128 g_nSpyroFrameMotionLength */
  char pad12C[0x38];
  int sparxHealth;                  /* 0x164 g_nSparxHealth */
  char pad168[0x8];
  int triggerEventId;             /* 0x170 g_nSpyroTriggerCurrentEventId */
  char pad174[0x8];
  int firstContactPoint[3];       /* 0x17C g_anSpyroFirstContactPoint */
  char pad188[0x1C];
  int headLook;                   /* 0x1A4 */
  char pad1A8[0x4C];
  int requestMask;               /* 0x1F4 g_dwSpyroRequestMask (SPYRO_RQ_*) */
  char pad1F8[0x8];
  int requestTargetZ;             /* 0x200 g_nSpyroRequestTargetZ */
  char pad204[0x2];
  unsigned char requestYaw;       /* 0x206 g_bSpyroRequestYaw */
  char pad207[0x1];
  int requestVelX;                /* 0x208 g_anSpyroRequestVelocity[0] */
  int requestVelY;                /* 0x20C g_nSpyroRequestVelocityY */
  int requestVelZ;                /* 0x210 g_nSpyroRequestVelocityZ */
  char pad214[0x4];
  int requestState;               /* 0x218 g_nSpyroRequestState */
  int *pCamAnchor;                /* 0x21C */
  void *pCamTarget;               /* 0x220 */
  Actor *pScriptedActor;          /* 0x224 */
  char pad228[0x18];
  PortalCamPath *pPortalCamPath;  /* 0x240 g_pPortalCamPathData */
  int camPathMaxStep;             /* 0x244 g_nPortalCamPathMaxStep */
  int camPathReverse;             /* 0x248 g_nPortalCamPathReverseFlag */
} SpyroObj;

extern SpyroObj g_anSpyroWorldPosBlock;

/* A SpyroObj field addressed off the UpdateActors clones' held base
 * (`base = (char *)&g_nSpyroWorldPosZ`, i.e. SpyroObj + 8). The single folded
 * constant keeps the original `base + N` address; `spy->field` instead makes
 * gcc materialize the SpyroObj pointer and reallocates the whole loop. */
#define SPY_HELD_FIELD(base, type, field)                                      \
  ((type *)((char *)(base) + ((int)&((SpyroObj *)0)->field - 8)))

/* Case 13 — tethered follower: rides a parent actor (by table index) and,
 * once released, re-points every other follower at the same parent. */
typedef struct FollowState {
  int parent;   /* 0x00 parent actor index into g_pActorListBase */
  int pos[3];   /* 0x04 carried world position */
  int detached; /* 0x10 set once the parent slot has gone dormant */
  int active;   /* 0x14 */
  int trackPos; /* 0x18 copy the parent's position each frame */
  int release;  /* 0x1C release countdown, 0 while held */
  int count;    /* 0x20 number of followers sharing this parent */
  int dropToSpyro;    /* 0x24 nonzero: the death drop flies straight to Spyro (kind 4) */
} FollowState;

/* Case 173 — the level key (ACTOR_TYPE_KEY): rocks in place and sparkles;
 * touching it sets g_nLevelKeyState = 1, and the locked chest (174) then
 * flies it into its lock. */
typedef struct KeyState {
  short timer;          /* 0x00 */
  unsigned char angle;  /* 0x02 */
  unsigned char attach; /* 0x03 attached spark billboard, 0xFF = none */
} KeyState;

/* Case 195 — the same chest with a wobble pose played before it breaks. */
typedef struct WobbleState {
  int phase;  /* 0x00 wobble phase, 0 when at rest */
  int rotX;   /* 0x04 rest pose */
  int rotY;   /* 0x08 */
  int restZ;  /* 0x0C */
  int snapToGround;  /* 0x10 nonzero: re-snap to the floor every frame */
  int handle; /* 0x14 */
} WobbleState;

/* Cases 76 and 426..451 — a letter of a floating name (SpawnNameLetterActors:
 * A..Z are 426..451, anything else 76): rides its sign at a fixed radius, or
 * falls back to a plain spin when the sign is not a spinner. */
typedef struct NameLetterState {
  Actor *parent; /* 0x00 */
  short radius;  /* 0x04 */
  short phase;   /* 0x06 */
} NameLetterState;

/* Case 286 — ambient sound source: plays random ambient sounds at its
 * sound range; mode 2 also steps it along a waypoint path. */
typedef struct AmbientSoundState {
  int mode;       /* 0x00 which ambient pattern (0..9) */
  int soundRange;     /* 0x04 republished into actor->soundRange */
  PathData *path; /* 0x08 */
  int timer;       /* 0x0C */
  int sub;        /* 0x10 sub-state inside mode 2 */
  int speed;      /* 0x14 */
  int step[3];    /* 0x18 per-frame step along the leg */
  int left;       /* 0x24 frames left on this leg */
} AmbientSoundState;

/* DragonStatueState (case 250) lives in actor_states.h: main reads it too. */

extern struct {
  int x;
  int y;
} g_anSpyroRequestVelocity; /* scripted camera look-at */

/* ==== Globals shared with level 4 ==== */
extern void RefreshHudLevelGemCounter(void);

extern void ResetSpyroEntity(int full);

/* Case 9 -- level-exit whirlwind: shows its name letters near the camera,
 * sparkles, lifts Spyro (SPYRO_EV_LIFT toward its z + 0x4000) and, once he
 * is above `depth` with the camera tilted up, starts the hub return. */
typedef struct ExitWhirlState {
  int pad00;
  int tick; /* 0x04 sparkle counter */
  int pad08;
  int depth; /* 0x0C lift height that triggers the exit, 0 = 0x2000 */
} ExitWhirlState;

extern int PivotActorAlongPath(Actor *actor, PathData *path, int thresh, int speed, int group, int step, int lim, int kind, int flags);

extern int RaiseSpyroTriggerInRadius(int *pos, int radius, int flags);

/* Cases 152/153 -- sparking debris. */
typedef struct DebrisFallState {
  int vel[3];             /* 0x00 */
  int floor;              /* 0x0C */
  unsigned char spin[3];  /* 0x10 */
} DebrisFallState;

/* Case 312 -- cracking boulder. */
typedef struct CrackingBoulderState {
  int timer;  /* 0x00 shake time, then burst ring */
  int rotX;   /* 0x04 rest pose */
  int rotY;   /* 0x08 */
  int restZ;  /* 0x0C */
  int pushed; /* 0x10 camera knocked back */
} CrackingBoulderState;

typedef struct SmokeJet {
  short pos[3]; /* 0x00 */
  short vel[3]; /* 0x06 */
} SmokeJet;

extern SmokeJet g_aSmokeJetEmitters[];

/* Case 329 -- seed pod. */
typedef struct SeedPodState {
  Actor *prize; /* 0x00 */
  int timer;    /* 0x04 */
  int rotX;     /* 0x08 rest pose */
  int rotY;     /* 0x0C */
  int restZ;    /* 0x10 */
} SeedPodState;

extern int CheckSpyroLineOfSightTo(int *pos);

extern int IsPointInRotatedBox(int *pos, int *rec);

/* Case 174 -- the locked chest (ACTOR_TYPE_LOCKED_CHEST): with the key held,
 * Spyro facing it up close freezes, the key flies into its lock, and it
 * bursts into wood shards and drops its reward (g_pLockedChestReward). */
typedef struct LockedChestState {
  int idx;    /* 0x00 g_pActorListBase index of its key */
  int shake;  /* 0x04 shake timer, then the key-flight timer */
  int rotX;   /* 0x08 rest pose */
  int rotY;   /* 0x0C */
  int restZ;  /* 0x10 */
  int handle; /* 0x14 for SelectActorLodBand */
} LockedChestState;

extern void DivideVector3(int *dst, int *s, int k);

extern void GetActorAttachPoint(Actor *actor, int slot, int *pos);

/* Type 33: egg thief. Carries a stolen egg (a type 0x22 actor it spawns),
 * runs a looping path away from Spyro, digs between path sections, and drops
 * the egg when hit. */
typedef struct ThiefState {
  PathData *path; /* 0x00 */
  Actor *egg;     /* 0x04 the carried egg, or null once dropped */
  int mode;       /* 0x08 idle behaviour (enum ThiefKind), picked by the level data */
  int vz;         /* 0x0C vertical speed while hopping */
  int landed;     /* 0x10 */
  int speed;      /* 0x14 path speed */
  int stuck;      /* 0x18 frames spent cornered */
  int heading;    /* 0x1C knockback heading */
  int knock;      /* 0x20 knockback speed */
  int pad24;
  int node;       /* 0x28 node the path should stop at */
  int range;      /* 0x2C wake-up range, in 0x400 units */
  int turned;     /* 0x30 has reversed once on this leg */
  int needLos;    /* 0x34 */
  int dist;       /* 0x38 */
  int voice;      /* 0x3C taunt timer */
  int lastVoice;  /* 0x40 */
  int area[3];    /* 0x44 trigger volume for mode 4 */
} ThiefState;

/* Case 390 -- spinning chest. */
typedef struct SpinChestState {
  Actor *lid; /* 0x00 its ACTOR_TYPE_SPIN_CHEST_LID */
  int angle;  /* 0x04 */
  int speed;  /* 0x08 */
  int timer;  /* 0x0C */
  int handle; /* 0x10 for SelectActorLodBand */
} SpinChestState;

/* Case 392 -- the spin chest's lid, launched when the chest bursts. */
typedef struct SpinChestLidState {
  int vel[3]; /* 0x00 */
  int timer;  /* 0x0C */
} SpinChestLidState;

typedef struct BounceState {
  short vx;           /* 0x00 */
  short vy;           /* 0x02 */
  short vz;           /* 0x04 */
  short rx;           /* 0x06 */
  short ry;           /* 0x08 */
  short rz;           /* 0x0A */
  unsigned char life; /* 0x0C */
  unsigned char slow; /* 0x0D frames until the push dies off */
  short sway;         /* 0x0E */
  int mode;           /* 0x10 */
} BounceState;

typedef struct SwoopState {
  int speed;     /* 0x00 */
  int heading;   /* 0x04 */
  int home[3];   /* 0x08 */
  int range;     /* 0x14 in 1024s */
  int timer;     /* 0x18 */
  int weaving;   /* 0x1C */
  int sway;      /* 0x20 */
  int swayDir;   /* 0x24 1 out, -1 back, 0 done */
  int side;      /* 0x28 */
  int kind;      /* 0x2C 2 = dives to Spyro's height, -1 = random side */
  int *alert;    /* 0x30 shared by the guards of one group */
  int count;     /* 0x34 */
} SwoopState;

typedef struct CritterKnockState {
  char pad00[0x38];
  int knock; /* 0x38 knock-back heading */
  int speed; /* 0x3C knock-back speed */
} CritterKnockState;

/* Type 184 (level 7): a stationary brawler. Turns to face Spyro and swings
 * when he is close (mode 4, re-armed by `timer`); flame or superflame knocks
 * it back along `heading`/`speed` (mode 6) and it bursts into type 230/288/289
 * debris. */
typedef struct BrawlerState {
  int link;    /* 0x00 handed to the spawned bolt */
  int flag;    /* 0x04 */
  int u08;     /* 0x08 */
  int heading; /* 0x0C */
  int speed;   /* 0x10 */
  int timer;   /* 0x14 */
} BrawlerState;

/* Type 183, the bolt level 7's type 184 spawns at frame 0x11 of its mode 1.
 * Nothing in case 184 enters mode 1, and Level7_UpdateActors has no arm for
 * type 183, so these fields are write-only here and keep raw names. */
typedef struct BrawlerSpawnState {
  int link;    /* 0x00 */
  int u04;     /* 0x04 */
  int u08;     /* 0x08 */
  int heading; /* 0x0C */
  int u10;     /* 0x10 */
  int u14;     /* 0x14 */
} BrawlerSpawnState;

/* Type 186 (level 7): a blinking sentry that wakes inside `range` (or 0x2800)
 * and fires a type-197 shot at Spyro when it has line of sight; walks back to
 * `home` after a chase. */
typedef struct ShooterState {
  int home[3];   /* 0x00 */
  void *path;    /* 0x0C */
  int triggered; /* 0x10 */
  int mode;      /* 0x14 */
  int timer;     /* 0x18 */
  int heading;   /* 0x1C */
  int speed;     /* 0x20 */
  int blinks;    /* 0x24 */
  int pad28;       /* 0x28 */
  int turn;      /* 0x2C */
  int range;     /* 0x30 */
  int spawned;   /* 0x34 */
} ShooterState;

/* Type 197 (level 7): the shot type 186 fires. Flies on `vel` (z zeroed),
 * knocks Spyro back on contact, and bursts into debris on a wall hit, the
 * contact, or when `countdown0` runs out; once `countdown1` runs out MoveActorByDelta gets
 * 0x12C as its fifth argument. */
typedef struct ShooterShotState {
  int vel[3];         /* 0x00 */
  short countdown0;          /* 0x0C */
  short countdown1;          /* 0x0E */
} ShooterShotState;

/* Type 10: path patroller. */
typedef struct PathState {
  int pad00;
  int pad04;
  int pad08;
  PathData *path;      /* 0x0C */
  int loops;   /* 0x10 nonzero: wrap at the path ends instead of turning back */
  int pad14;
  int pad18;
  int heading; /* 0x1C */
  int speed;   /* 0x20 walk speed; ramps +0xA up to stepDivisor, knockback 0xC8/0x190 */
  int stepDivisor;   /* 0x24 divisor, random 0x6E..0xA0 */
  int travelTimer; /* 0x28 leg countdown, (dist / stepDivisor) / 2 clamped to <= 0x5A */
  int successCount;   /* 0x2C consecutive-success counter */
  int startDelay; /* 0x30 random 6..0x28 countdown before walking */
} PathState;

/* Type 286: ambient sound source (AmbientSoundState) with the path seen as
 * bytes, the view the other clones were matched with. */
typedef struct AmbientSoundBytesState {
  int mode;            /* 0x00 inner jump-table discriminant, 0..9 */
  int soundRange;           /* 0x04 republished into actor->soundRange every frame */
  unsigned char *path; /* 0x08 [0]=waypoint count, [1]=current index */
  int timer;           /* 0x0C */
  int sub;             /* 0x10 sub-state inside mode 2 */
  int speed;           /* 0x14 leg speed (ComputeStepTowardPoint speed) */
  int vel[3];          /* 0x18 current leg velocity, written by ComputeStepTowardPoint */
  int count;           /* 0x24 frames left on the current leg */
} AmbientSoundBytesState;

/* Type 34: dragon egg. Spirals up, flies to Spyro and is collected. */
typedef struct ShotState {
  void *killActor;        /* 0x00 actor handed to CommitActorKillGems on collection */
  short from[3];         /* 0x04 launch point */
  short to[3];           /* 0x0A destination */
  short speed;           /* 0x10 */
  unsigned char phase;   /* 0x12 timer */
  unsigned char heading; /* 0x13 */
} ShotState;

/* Type 18: pressure switch. Toggles one bit in a linked actor's sub-state. */
typedef struct SwitchState {
  int shift; /* 0x00 bit position within the target's 0x49 mask */
  int idx;   /* 0x04 index into the actor pool at g_pActorListBase */
} SwitchState;

/* Type 49: sequenced prop driven by the 0x8002Bxxx player. Starts once any
 * exit vortex flag is set and its linked actor is dormant. */
typedef struct AnimState {
  int chunkAnim;  /* 0x00 world chunk-anim handle (GetWorldChunkAnimState) */
  int started; /* 0x04 one-shot init latch */
  int idx;     /* 0x08 pool index of the actor this one watches */
} AnimState;

/* Types 255/256/423/424: chest fragments that bounce, reflecting their
 * velocity about the surface normal. */
typedef struct SparkState {
  short vel[3];        /* 0x00 */
  unsigned char spin0; /* 0x06 */
  unsigned char pad07;
  unsigned char spin1; /* 0x08 */
  unsigned char pad09;
  unsigned char spin2; /* 0x0A */
  unsigned char pad0B;
  int life; /* 0x0C */
} SparkState;

typedef struct {
  unsigned char *v;
} PtrU8;

/* Types 14/15/83..87: collectibles (life statue, life orb, gems). ->vel
 * doubles as the pickup origin once Sparx has fetched the item. */
typedef struct PickupState {
  int vel[3];          /* 0x00 */
  short groundTri;         /* 0x0C collision triangle it rests on (UnpackWorldCollisionTri) */
  unsigned char sub;   /* 0x0E resting sub-state, 0..3 */
  unsigned char timer; /* 0x0F */
  unsigned char count; /* 0x10 bounces left */
  unsigned char pitch; /* 0x11 */
  unsigned char yaw;   /* 0x12 */
  unsigned char roll;  /* 0x13 */
  unsigned char anim;  /* 0x14 0xFF == none */
} PickupState;

/* Type 250: crystal dragon. Shakes and sparkles; when Spyro walks into it,
 * saves a checkpoint and starts the rescue. */
typedef struct RideState {
  int pad00[6]; /* 0x00 */
  int cutsceneId;    /* 0x18 rescue cutscene id (see DragonStatueState) */
  int fallbackHeading;    /* 0x1C fallback heading when ->partner is unset */
  int partner;    /* 0x20 index into g_pActorListBase, -1 for none */
  int rewardScript;    /* 0x24 script id handed to StubRewardHook, -1 for none */
  int pad28[7]; /* 0x28 */
  int phase;    /* 0x44 phase counter, 0 .. 0x100 */
  int savedRotX;    /* 0x48 saved actor->rotX */
  int savedRotY;    /* 0x4C saved actor->rotY */
  int savedPosZ;    /* 0x50 saved actor->posZ */
} RideState;

/* Type 398 (ACTOR_TYPE_PORTAL_GUIDE): flies Spyro through a portal. The
 * portal record's word 6 names it; DispatchSpyroTriggerEvent wakes it when
 * Spyro enters the portal region (mode 1, subMode = which side he came from,
 * offset = half the portal normal toward that side, updateDist 0xFF). Mode 1
 * homes Spyro on the midpoint of the path's first two waypoints (plus offset)
 * along the portal camera path; mode 2 glides him along it. When the camera
 * sphere touches a TRIGGER_PORTAL region it starts the level load. */
typedef struct PortalState {
  PortalCamPath *desc; /* 0x00 the portal's camera path */
  int offset[3];    /* 0x04 nudge applied to the homing vector */
  int pad10;        /* 0x10 */
  int dest;         /* 0x14 destination yaw, published to SPY.requestYaw */
  int cameraPreset;        /* 0x18 index into g_aCameraXformPresets; negative selects none */
} PortalState;

/* Type 300 (whirlwind): the camera it requests while lifting Spyro, and the
 * template that camera is seeded from. */
typedef struct Xform6 {
  int w[6];
} Xform6;

extern Xform6 g_anCameraActiveTargetParams; /* live camera transform */

extern Xform6 g_anCameraParamMode2Front; /* template it is seeded from */

/* Type 323: respawner. It rides on Spyro and, every `period` frames, puts
 * each dormant actor of `type` farther than 0x6000 from him back at its home
 * position (the first words of its state block). */
typedef struct RespawnState {
  int type;   /* 0x00 the actor type this manager owns */
  int timer;  /* 0x04 */
  int period; /* 0x08 reload for ->timer */
} RespawnState;


/* Type 114: path-following enemy that runs from Spyro and can be knocked
 * back. */
typedef struct WalkerState {
  PathData *path;      /* 0x00 */
  int timer;           /* 0x04 */
  int tgtIdx;          /* 0x08 index into g_pActorListBase, -1 when unclaimed */
  int heading;         /* 0x0C */
  int moveSpeed;           /* 0x10 passed by address to DriveActorTrackedMove */
  int zVel;           /* 0x14 likewise */
} WalkerState;

/* Type 339: flock leader. Two linked chains of follower actors, a path, and
 * one formation slot per follower plus one for the leader. */
typedef struct FlockState {
  int knockAngle;           /* 0x00 knockback angle */
  int knockSpeed;           /* 0x04 knockback speed */
  PathData *path;      /* 0x08 */
  int pad0C;
  int chainA;       /* 0x10 */
  int chainB;       /* 0x14 */
  int slots[16][3]; /* 0x18 */
} FlockState;

/* Type 391: rides a collision triangle, holding its offset from the
 * triangle's centroid. */
typedef struct PatchRideState {
  int handle;    /* 0x00 */
  int offset[3]; /* 0x04 actor position relative to the centroid */
} PatchRideState;

/* Type 350: sequenced prop driven by the 0x8002Bxxx player, unlocked once. */
typedef struct PropAnimState {
  int chunkAnim;   /* 0x00 world chunk-anim handle (GetWorldChunkAnimState) */
  int timer;  /* 0x04 */
  int primed; /* 0x08 */
} PropAnimState;

/* Types 76/426..451: letters of a floating word, placed relative to their
 * parent actor. */
typedef struct OrbitState {
  Actor *owner; /* 0x00 */
  short ring;  /* 0x04 ring index; the arm always uses it as ring - 1 */
  short ringPhase;  /* 0x06 phase offset within the ring */
} OrbitState;

/* Case 16 — butterfly: flutters near its spawn point until Sparx eats it
 * (or it turns into Sparx when Spyro has none). */
typedef struct WanderState {
  int turn;              /* 0x00 turn direction, +1 / -1 */
  int home[3];           /* 0x04 spawn position it stays near */
  unsigned char speed;   /* 0x10 forward speed, signed when >= 0x80 */
  unsigned char heading; /* 0x11 */
  unsigned char reTurn;  /* 0x12 frames until the next heading change */
  unsigned char reStep;  /* 0x13 frames until the next speed change */
  unsigned char wanderTimer;   /* 0x14 */
} WanderState;

/* Case 194 — breakable chest: one hit spawns its gem shower. */
typedef struct ChestState {
  int pad00;
  int shake; /* 0x04 */
} ChestState;

/* Case 208 — the hub's music/voice trigger. */
typedef struct MusicState {
  int started; /* 0x00 */
} MusicState;

/* ACTOR_TYPE_WANDERING_CRITTER_C (level 30): pad00 is UpdateActorWander's
 * block; speed/heading drive the knock-away. */
typedef struct WanderKnockState {
  char pad00[0x20];
  int speed;   /* 0x20 */
  int heading; /* 0x24 */
  char pad28[0x4];
  int rest; /* 0x2C frames until the next idle pause (mode 2) */
} WanderKnockState;

/* Cases 260..269 — the floating gem-value digits again, with the z velocity
 * split out (the view two clones were matched with). */
typedef struct GemDigitStateZ {
  int life; /* 0x00 */
  char pad04[0x4];
  int vel[2]; /* 0x08 */
  int velZ;   /* 0x10 */
} GemDigitStateZ;

/* Type 416 (ACTOR_TYPE_BALLOON): bobs on a sine about its rest height. It
 * appears only in levels that have a balloonist cutscene. */
typedef struct BobState {
  int restZ; /* 0x00 */
} BobState;

/* Case 314 -- post guard. */
typedef struct GuardState {
  PathData *path;  /* 0x00 patrol path */
  PathData *posts; /* 0x04 guard posts, one per stage */
  int wp[2];       /* 0x08 path node to walk to, per stage */
  int stage;       /* 0x10 */
  int count;       /* 0x14 idle fidgets before the next look-around */
  int timer;       /* 0x18 */
  int anim;        /* 0x1C animation base */
  int partner;     /* 0x20 index into g_pActorListBase */
  int alarm;       /* 0x24 */
  int drop;        /* 0x28 index into g_pActorListBase, -1 for none */
} GuardState;

/* Case 335 -- lunging enemy. */
typedef struct LungeState {
  int pad00;
  int guard;     /* 0x04 */
  int home[3];   /* 0x08 */
  int target[3]; /* 0x14 */
  int pad20[3];  /* 0x20 */
  int anim;      /* 0x2C animation base */
  int hits;      /* 0x30 */
  int heading;   /* 0x34 -1 until first set */
  int alerted;   /* 0x38 */
  int pad3C;
  int *flag; /* 0x40 */
  int pad44;
  int wait;       /* 0x48 */
  int knockAngle; /* 0x4C */
  int knockSpeed; /* 0x50 */
  int velZ;       /* 0x54 */
  int look;       /* 0x58 */
  int burn;       /* 0x5C */
} LungeState;

/* Case 349 -- spinning enemy. */
typedef struct SpinnerState {
  int count; /* 0x00 */
  int timer; /* 0x04 */
  int angle; /* 0x08 knock-back heading */
  int speed; /* 0x0C */
  int spin;  /* 0x10 */
} SpinnerState;

/* Case 37 -- rim rider. */
typedef struct RimState {
  Actor *parent;  /* 0x00 */
  short reach;    /* 0x04 */
  short range;    /* 0x06 */
  short x;        /* 0x08 Spyro's position when last seen, >> 4 */
  short y;        /* 0x0A */
  Actor *child;   /* 0x0C its type 39 helper */
  unsigned char mode; /* 0x10 */
} RimState;

typedef struct RimParent {
  char pad00[0x48];
  int busy; /* 0x48 */
} RimParent;

/* Case 39 -- the rim rider's helper. */
typedef struct HelperState {
  int timer;    /* 0x00 */
  Actor *owner; /* 0x04 */
  int beat;     /* 0x08 */
} HelperState;

/* Case 283 -- path guard. */
typedef struct PathGuardState {
  PathData *path; /* 0x00 */
  int flags;      /* 0x04 bit 1: on watch */
  int dist;       /* 0x08 */
  int angle;      /* 0x0C heading offset */
  int timer;      /* 0x10 */
  int node;       /* 0x14 */
  int partner;    /* 0x18 index into g_pActorListBase */
  int heading;    /* 0x1C */
  int speed;      /* 0x20 knock-back speed */
  int knock;      /* 0x24 knock-back heading */
} PathGuardState;

/* Case 497 -- humming bell. */
typedef struct HummingBellState {
  char pad00[0x10];
  int chunkAnim;   /* 0x10 world chunk-anim handle (GetWorldChunkAnimState) */
  int timer;  /* 0x14 */
  int period; /* 0x18 */
  char pad1C[0x14];
  int target; /* 0x30 index into g_pActorListBase */
} HummingBellState;

/* Case 495 -- the level boss. */
typedef struct BlowhardState {
  char pad00[0x10];
  int chunkAnim;           /* 0x10 world chunk-anim handle (GetWorldChunkAnimState) */
  PathData *path;     /* 0x14 path in use */
  int tweenLeg[3];    /* 0x18 TweenActorAlongPath prev leg */
  int tweenStep;       /* 0x24 TweenActorAlongPath step counter */
  int p28;            /* 0x28 */
  int p2C;            /* 0x2C */
  PathData *paths[5]; /* 0x30 */
  int phase;          /* 0x44 path set in use */
  Actor *minion;      /* 0x48 the type 37 rider it drops */
  int floorZ;         /* 0x4C height to settle at, -1 = none */
  int delay;          /* 0x50 */
  char pad54[0x4];
  int started;        /* 0x58 */
  char pad5C[0x4];
  int pathDone;          /* 0x60 on the path */
  int prev;           /* 0x64 index into g_pActorListBase of the previous stage's gate */
  int wait;           /* 0x68 */
  char pad6C[0x8];
  int soundBox;          /* 0x74 */
} BlowhardState;

/* Case 412 -- wandering critter. */
typedef struct HopState {
  char pad00[0x20];
  int timer;   /* 0x20 */
  int heading; /* 0x24 knock-back heading */
  int time;    /* 0x28 knock-back time */
} HopState;

/* Case 115 -- patrolling guard, one of a pair. */
typedef struct GuardPairState {
  int heading; /* 0x00 knock-back heading */
  int speed;   /* 0x04 */
  int flag;    /* 0x08 */
  int timer;   /* 0x0C */
  int partner; /* 0x10 index into g_pActorListBase, -1 for none */
} GuardPairState;

/* Case 165 -- post guard. */
typedef struct PostPoint {
  int x, y, z, pad;
} PostPoint;

typedef struct PostPath {
  unsigned char count; /* 0x00 number of points */
  unsigned char sel;   /* 0x01 point in use */
  char pad02[4];
  short dir; /* 0x06 walking direction, +1 or -1 */
  PostPoint pts[2]; /* 0x08 */
} PostPath;

/* Index of the path point nearest the actor (written to *idx). */
extern int FindNearestPathPointToActor(Actor *actor, PostPath *path, int *idx);

/* Path tweens used by the patrol/post classes. */
extern int TweenActorAlongPath(Actor *actor, PathData *path, int *prev, int *step, int legs, int flags);

typedef struct PostState {
  PostPath *path; /* 0x00 */
  int mode;       /* 0x04 idle sub-state */
  int timer;      /* 0x08 */
  int heading;    /* 0x0C knock-back heading */
  int speed;      /* 0x10 knock-back speed */
  int time;       /* 0x14 knock-back time */
  int flag;       /* 0x18 */
  short spinX;    /* 0x1C */
  short spinY;    /* 0x1E */
  int sub;        /* 0x20 */
  int phase;      /* 0x24 */
  int partner;    /* 0x28 index into g_pActorListBase */
} PostState;

/* Case 166 -- path patroller. */
typedef struct PatrolState {
  PostPath *path; /* 0x00 */
  int mode;       /* 0x04 */
  int flag;       /* 0x08 */
  int near;       /* 0x0C */
  int timer;      /* 0x10 */
} PatrolState;

/* Cases 205/209 -- the snowball thrower and its snowball. */
typedef struct ThrownSnowballState {
  int vx;             /* 0x00 */
  int vy;             /* 0x04 */
  int vz;             /* 0x08 */
  int timer;          /* 0x0C */
  unsigned char kind; /* 0x10 */
} ThrownSnowballState;

/* Case 213 -- circling bird. */
typedef struct CirclingBirdState {
  int home[3]; /* 0x00 perch */
  int timer;   /* 0x0C */
  int radius;  /* 0x10 */
  int angle;   /* 0x14 */
  int height;  /* 0x18 */
  int call;    /* 0x1C */
} CirclingBirdState;

/* Case 198 -- charging yak. */
typedef struct ChargingYakState {
  int heading; /* 0x00 knock-back heading */
  int speed;   /* 0x04 */
  int rest;    /* 0x08 rest countdown after a charge */
  int pad0C;
  int timer;   /* 0x10 grazing timer */
} ChargingYakState;

/* Case 231 -- ram. */
typedef struct ToughRamState {
  int heading; /* 0x00 knock-back heading */
  int speed;   /* 0x04 */
  int rest;    /* 0x08 rest countdown, knock-back height step */
  int pad0C;
  int timer;   /* 0x10 grazing timer */
  int dropPos;   /* 0x14 */
  int pad18;
  int pad1C;
  int hits;    /* 0x20 */
} ToughRamState;

/* Cases 211/212 -- snowball thrower. */
typedef struct SnowballThrowerState {
  PathData *path; /* 0x00 */
  int dist;       /* 0x04 */
  int heading;    /* 0x08 */
  int timer;      /* 0x0C */
  int wake;       /* 0x10 wake-up distance >> 10, 0 = default */
  int turn1;      /* 0x14 */
  int turn4;      /* 0x18 */
  int knock;      /* 0x1C knock-back heading */
  int speed;      /* 0x20 knock-back speed */
  int blockHeadingA; /* 0x24 SteerActorAroundPoint blocked heading */
  int blockHeadingB; /* 0x28 SteerActorAroundPoint blocked heading */
  int throwT;     /* 0x2C */
} SnowballThrowerState;

typedef struct {
  int x;
  int y;
  int z;
} CamLook;

typedef struct RearState {
  int timer;   /* 0x00 idle-hop countdown */
  int hitCooldown; /* 0x04 frames before it rears again after knocking Spyro back */
  int heading; /* 0x08 knockback heading */
  int speed;   /* 0x0C knockback speed */
  int spin;    /* 0x10 per-frame yaw while tumbling */
} RearState;

typedef struct SentryState {
  int homeX;      /* 0x00 post */
  int homeY;      /* 0x04 */
  int homeZ;      /* 0x08 */
  int timer;      /* 0x0C */
  int heading;    /* 0x10 */
  int speed;      /* 0x14 */
  int spin;       /* 0x18 */
  int retarget;   /* 0x1C */
  int camStage;   /* 0x20 */
  int camDist;    /* 0x24 */
  int pad28;      /* 0x28 */
  int camTimer;   /* 0x2C */
  int *alert;     /* 0x30 shared by the sentries of one group */
} SentryState;

typedef struct {
  void *v;
} PtrV;

typedef struct BreakableRockState {
  int shake;  /* 0x00 shake timer, 0 = still */
  int rotX;   /* 0x04 rest rotation */
  int rotY;   /* 0x08 */
  int restZ;  /* 0x0C */
  int handle; /* 0x10 for SelectActorLodBand */
} BreakableRockState;

/* Type 295: homing wisp. Steers toward its target actor (or Spyro), and
 * bursts into three sparks when it arrives or touches the ground. */
typedef struct WispState {
  Actor *target; /* 0x00 actor to reach, or null to chase Spyro */
  int timer;     /* 0x04 lifetime */
} WispState;

typedef struct TetherState {
  int timer;   /* 0x00 */
  int speed;   /* 0x04 knockback speed */
  int heading; /* 0x08 knockback heading */
  int link;    /* 0x0C walker to follow (index into g_pActorListBase), or -1;
                * a sentinel powering a chain of guards reads it as the next
                * guard (ending the chain when that isn't a tethered guard) */
  int owner;   /* 0x10 actor whose loss frees this one */
  int mode;    /* 0x14 TetherCommand */
  int stop;    /* 0x18 node of the sentinel's path where it powers this guard */
} TetherState;

typedef struct LeadState {
  PathData *path; /* 0x00 */
  int moving;     /* 0x04 */
} LeadState;

typedef struct KeeperState {
  int timer;   /* 0x00 knockback timer */
  int heading; /* 0x04 knockback heading */
  int count;   /* 0x08 idle countdown */
  int link;    /* 0x0C linked guard (index into g_pActorListBase), or -1 */
  int mode;    /* 0x10 TetherCommand passed to the guard */
  int baseX;   /* 0x14 post position */
  int baseY;   /* 0x18 */
} KeeperState;

typedef struct SentinelState {
  int timer;        /* 0x00 */
  int count;        /* 0x04 bolts fired this volley */
  int link;         /* 0x08 next guard in the chain, or -1 */
  PathData *path;   /* 0x0C */
  int flags;        /* 0x10 SENTINEL_F_* */
  int heading;      /* 0x14 */
  int range;        /* 0x18 */
  int knockTimer;   /* 0x1C */
  int knockHeading; /* 0x20 */
  int knockSpeed;   /* 0x24 */
} SentinelState;

/* SentinelState.flags. */
#define SENTINEL_F_ALERTED 1 /* has seen Spyro: TRACK rather than ALERT */
#define SENTINEL_F_PATROL 2  /* walks its waypoints instead of standing */
#define SENTINEL_F_AT_END 4  /* reached its path's end */
#define SENTINEL_F_AIM 8     /* standing: watches for Spyro inside `range` */


extern int WalkActorAlongWaypoints(Actor *actor, PathData *path, int arriveDist, int cap, int contactRadius, int maxTurn, int stallAngle, int f);

extern int SteerActorAlongWaypoints(Actor *actor, PathData *path, int radius, int speed, int contact, int clamp, int limit, int *f);

typedef struct CannonState {
  int link;            /* 0x00 index of the actor it wakes on arrival */
  int vel[3];          /* 0x04 */
  int timer;           /* 0x10 */
  unsigned char *path; /* 0x14 */
  int init;            /* 0x18 1 = aim the barrel along the path */
} CannonState;

typedef struct CatapultState {
  int angle;     /* 0x00 0..0x120 */
  int vel;       /* 0x04 */
  int link;      /* 0x08 actor to fling (index into g_pActorListBase), or -1 */
  Actor *flung;  /* 0x0C actor in flight */
  int timer;     /* 0x10 */
} CatapultState;

typedef struct RoamingCritterState {
  char pad00[0x20];
  int speed; /* 0x20 knock-back speed */
  int knock; /* 0x24 knock-back heading */
  char pad28[0x4];
  int wait;  /* 0x2C pause timer */
} RoamingCritterState;

/* Type 370: Doctor Shemp's guard. */
typedef struct {
  char pad00[0x38];
  int posX; /* 0x38 */
  int posY; /* 0x3C */
} GuardTarget;

typedef struct ShempGuardState {
  int mode;           /* 0x00 0 = patrol, 1 = hold near target */
  int timer04;        /* 0x04 */
  int lift;           /* 0x08 */
  int heading;        /* 0x0C */
  int speed;          /* 0x10 */
  int pad14;          /* 0x14 */
  int range;          /* 0x18 sight range, in 1024s */
  int accel;          /* 0x1C */
  int turn;           /* 0x20 */
  int timer24;        /* 0x24 */
  int watch;          /* 0x28 index into g_pActorListBase */
  int guard;          /* 0x2C index into g_pActorListBase */
  int done;           /* 0x30 */
  GuardTarget *target; /* 0x34 */
  int timer38;        /* 0x38 */
  unsigned char *path; /* 0x3C */
  int boxA[6];        /* 0x40 */
  int boxB[6];        /* 0x58 */
} ShempGuardState;

/* Type 372: Shemp's lookout. */
typedef struct LookoutState {
  int heading;   /* 0x00 knock-back heading */
  int speed;     /* 0x04 knock-back speed */
  int timer;     /* 0x08 swipe cooldown */
  int count;     /* 0x0C idle loops left */
  int home;      /* 0x10 rest heading */
  int init;      /* 0x14 */
  int link;      /* 0x18 guard to rouse (index into g_pActorListBase), or -1 */
  int camTimer;  /* 0x1C */
  int camTimer2; /* 0x20 */
  int camDist;   /* 0x24 */
  int range;     /* 0x28 in 1024s */
  int wait;      /* 0x2C */
} LookoutState;

typedef struct RunnerState {
  PathData *pathA; /* 0x00 */
  PathData *pathB; /* 0x04 */
  int knockTimer;  /* 0x08 */
  int knockSpeed;  /* 0x0C */
  int pad10;
  int heading;     /* 0x14 knockback heading */
  int timer;       /* 0x18 */
  int mode;        /* 0x1C */
  int pad20;
  int which;       /* 0x24 path in use, 1 or 2 */
  int range;       /* 0x28 in 1024s */
  int node;        /* 0x2C trigger node on either path */
  int cried;       /* 0x30 */
} RunnerState;

/* Type 270: villager. Chats when Spyro faces it up close (mode 0), turns to
 * face a friend (mode 1), waits on a partner (mode 2) or watches a leader
 * (mode 3); knocked back when hit, and runs off when Spyro gets close. */
typedef struct VillagerState {
  unsigned char *path; /* 0x00 wander path { count, cur }, points at +8 */
  int chunkAnim;             /* 0x04 world chunk-anim handle (GetWorldChunkAnimState) */
  int friend;          /* 0x08 actor it faces / sparkles at, -1 = none */
  int link;            /* 0x0C partner actor */
  int mode;            /* 0x10 */
  int timer;           /* 0x14 voice timer */
  int timerLong;       /* 0x18 */
  int timerShort;      /* 0x1C */
  int noBoost;         /* 0x20 */
  int pad24;
  int talkDist;        /* 0x28 in 1024ths */
  int camDist;         /* 0x2C in 1024ths */
  int camAlways;       /* 0x30 */
  int fleeDist;        /* 0x34 in 1024ths */
  int speed;           /* 0x38 */
  int heading;         /* 0x3C */
  int chunkAnimDone;         /* 0x40 chunk anim rewound to step < 2 (or none): ok to despawn */
  int stopVoice;       /* 0x44 */
  int runFrom;         /* 0x48 distance it started running at */
  int faceTimer;       /* 0x4C */
  int init;            /* 0x50 */
  int idleTimer;       /* 0x54 */
} VillagerState;

/* The leader a mode-3 villager watches. */
typedef struct LeaderState {
  int mode;               /* 0x00 */
  int pad04[2];
  unsigned char *script;  /* 0x0C */
  int pad10[4];
  int cue;                /* 0x20 */
} LeaderState;

/* Type 271: shepherd. Guards a flock with a partner shepherd: watches Spyro
 * (state 0, by phase), circles to block him (state 2, by phase), climbs and
 * drops back down (states 0x10-0x12), and walks home (0x64). */
typedef struct ShepherdState {
  unsigned char *path; /* 0x00 { count, cur }, points at +8 */
  int home[3];         /* 0x04 */
  int heading;         /* 0x10 -1 = take the actor's own */
  int phase;           /* 0x14 */
  int knockSpeed;      /* 0x18 */
  int knockHeading;    /* 0x1C */
  int knockSpin;       /* 0x20 */
  int partner;         /* 0x24 */
  int partner2;        /* 0x28 */
  int timer;           /* 0x2C */
  int vz;              /* 0x30 */
  int side;            /* 0x34 1 = left, 2 = right */
  int done;            /* 0x38 */
  int *doneFlag;       /* 0x3C */
  int sub;             /* 0x40 */
  int timer2;          /* 0x44 */
  int range;           /* 0x48 in 1024ths */
} ShepherdState;

/* What a shepherd spawns (type 0x13B). */
typedef struct CrookState {
  Actor *owner; /* 0x00 */
  int live;     /* 0x04 */
  int heading;  /* 0x08 */
  int life;     /* 0x0C */
  int speed;    /* 0x10 */
} CrookState;

/* Type 315: falling block. */
typedef struct FallState {
  int pad00;
  int live;    /* 0x04 */
  int heading; /* 0x08 */
  int timer;   /* 0x0C */
  int vz;      /* 0x10 */
} FallState;

/* Type 326: sorcerer. Wanders its path (mode 1) or waits by it (mode 0),
 * throws bolts (type 0x127) at a target actor or at Spyro, and teleports
 * a floor marker on cue (state 10). */
typedef struct SorcererState {
  int timer;           /* 0x00 */
  int count;           /* 0x04 bolts thrown this volley */
  int target;          /* 0x08 actor index */
  unsigned char *path; /* 0x0C { count, cur }, points at +8 */
  int mode;            /* 0x10 */
  int pad14;
  int range;           /* 0x18 */
  int angry;           /* 0x1C */
  int sub;             /* 0x20 */
  int pad24[3];
  int rest;            /* 0x30 */
} SorcererState;

/* A sorcerer's bolt. */
typedef struct BoltState {
  Actor *target; /* 0x00 */
  int life;      /* 0x04 */
} BoltState;

/* Type 253: guard captain. Stands watch with a partner (mode 0), lunges at
 * Spyro (state 6, by sub-step), and drags the camera onto him while it
 * winds up (state 7). */
typedef struct CaptainState {
  int home[3];         /* 0x00 */
  unsigned char *path; /* 0x0C { count, cur }, points at +8 */
  int mode;            /* 0x10 */
  int heading;         /* 0x14 -1 = take the actor's own */
  int knockSpeed;      /* 0x18 */
  int knockHeading;    /* 0x1C */
  int partner;         /* 0x20 actor index, -1 = none */
  int camTimer;        /* 0x24 */
  int step;            /* 0x28 */
  int pad2C;
  int timer;           /* 0x30 */
  int posed;           /* 0x34 */
  int range;           /* 0x38 in 1024ths */
  int timer2;          /* 0x3C */
} CaptainState;

typedef struct BoltFlight {
  int vel[3]; /* 0x00 */
  int timer;  /* 0x0C */
} BoltFlight;

typedef struct Debris {
  short vx;            /* 0x00 */
  short vy;            /* 0x02 */
  short vz;            /* 0x04 */
  unsigned char spinX; /* 0x06 */
  char pad07;
  unsigned char spinY; /* 0x08 */
  char pad09;
  unsigned char spinZ; /* 0x0A */
  char pad0B;
  int life;            /* 0x0C */
} Debris;

typedef struct FlyPath {
  unsigned char count; /* 0x00 */
  unsigned char cur;   /* 0x01 */
  char pad02[4];
  short dir;           /* 0x06 +1 forward, -1 back */
} FlyPath;

typedef struct LootThiefState {
  FlyPath *path;  /* 0x00 escape path */
  int onRing;     /* 0x04 */
  int roll;       /* 0x08 */
  int rollVel;    /* 0x0C */
  int knockSpin;  /* 0x10 */
  int knockHeading; /* 0x14 */
  int speed;      /* 0x18 */
  int look[3];    /* 0x1C */
  int laps;       /* 0x28 */
  int init;       /* 0x2C */
  int chunkAnim;        /* 0x30 world chunk-anim handle (GetWorldChunkAnimState) */
  FlyPath *cur;   /* 0x34 */
  FlyPath *ring;  /* 0x38 */
  FlyPath *pathA; /* 0x3C */
  FlyPath *pathB; /* 0x40 */
  int leg;        /* 0x44 */
} LootThiefState;

typedef struct CritterState {
  FlyPath *path;    /* 0x00 */
  int attach;       /* 0x04 actor it rides, 0 = none */
  int hasHome;      /* 0x08 */
  int mode;         /* 0x0C */
  int knockSpin;    /* 0x10 */
  int knockHeading; /* 0x14 */
  int knockSpeed;   /* 0x18 */
  int stuck;        /* 0x1C */
  int voiceTimer;   /* 0x20 */
  int lastVoice;    /* 0x24 */
} CritterState;

typedef struct LootVortexState {
  short timer;          /* 0x00 */
  unsigned char phase;  /* 0x02 */
  unsigned char spark;  /* 0x03 0xFF = none */
  int chunkAnim;              /* 0x04 world chunk-anim handle (GetWorldChunkAnimState) */
  int pad[6];           /* 0x08 trigger volume */
  int mode;             /* 0x20 */
  int key;              /* 0x24 actor index */
  int target;           /* 0x28 actor index */
  int seen;             /* 0x2C 1/2 picks g_nLevelVortexSeen1/2 (read as g_nCutsceneId[seen]), 0 = none */
} LootVortexState;

typedef struct BombLobberState {
  int timer;   /* 0x00 */
  int heading; /* 0x04 knock-back heading */
  int speed;   /* 0x08 knock-back speed */
  int reload;  /* 0x0C */
} BombLobberState;

typedef struct SinkingBlockState {
  int partner; /* 0x00 index into g_pActorListBase */
  int depth;   /* 0x04 */
} SinkingBlockState;

typedef struct PartnerPlatformState {
  int partner;        /* 0x00 index into g_pActorListBase */
  unsigned int phase; /* 0x04 */
  int pad08;
  int pad0C;
  int baseZ;          /* 0x10 */
  int big;            /* 0x14 */
} PartnerPlatformState;

typedef struct PartnerBouncerState {
  int partner; /* 0x00 index into g_pActorListBase */
  int speed;   /* 0x04 */
  int lift;    /* 0x08 */
  int heading; /* 0x0C */
} PartnerBouncerState;

typedef struct FormationEscortState {
  int leader;     /* 0x00 index into g_pActorListBase */
  int partner;    /* 0x04 index into g_pActorListBase, -1 = none */
  PathData *path; /* 0x08 */
  int tweenLeg[3]; /* 0x0C TweenActorAlongPath prev leg */
  int timer;      /* 0x18 */
  int slot;       /* 0x1C place in the ring */
  int speed;      /* 0x20 */
} FormationEscortState;

typedef struct ChasingPathGuardState {
  int home[3];    /* 0x00 */
  int timer;      /* 0x0C */
  int knock;      /* 0x10 knock-back heading */
  int speed;      /* 0x14 */
  int lift;       /* 0x18 */
  PathData *path; /* 0x1C */
  int node;       /* 0x20 */
  int onPath;     /* 0x24 */
  int lastSnd;    /* 0x28 */
  int sndTimer;   /* 0x2C */
} ChasingPathGuardState;

typedef struct L26Rot {
  unsigned char r[3];
} L26Rot;

typedef struct HatchState {
  int pad00;
  int kind;     /* 0x04 0 = opens to charge or flame, else charge only */
  Actor *child; /* 0x08 released occupant */
  int handle;   /* 0x0C for SelectActorLodBand */
} HatchState;

typedef struct MountState {
  int rider; /* 0x00 index into g_pActorListBase, -1 = none */
  int init;  /* 0x04 */
  int knock; /* 0x08 knock-back heading */
  int speed; /* 0x0C */
  int lift;  /* 0x10 */
} MountState;

typedef struct HatchCarrierState {
  int hatch;   /* 0x00 index of its hatch in g_pActorListBase */
  Actor *held; /* 0x04 carried occupant */
  int timer;   /* 0x08 */
  int knock;   /* 0x0C knock-back heading */
  int speed;   /* 0x10 */
  int lift;    /* 0x14 */
} HatchCarrierState;

typedef struct FloatingBarrelState {
  short timer;  /* 0x00 */
  short blast;  /* 0x02 blast pushed Spyro */
  Actor *rider; /* 0x04 */
  int heading;  /* 0x08 run heading */
  int sink;     /* 0x0C sink speed */
  int link;     /* 0x10 index into g_pActorListBase, -1 = none */
  short handle; /* 0x14 for SelectActorLodBand */
} FloatingBarrelState;

typedef struct L31Debris {
  int vx;    /* 0x00 */
  int vy;    /* 0x04 */
  int vz;    /* 0x08 */
  int floor; /* 0x0C */
  unsigned char spinX; /* 0x10 */
  unsigned char spinY; /* 0x11 */
  unsigned char spinZ; /* 0x12 */
} L31Debris;

typedef struct CarriedThrowerState {
  int held;  /* 0x00 index of the carried actor in g_pActorListBase, -1 = none */
  int scored; /* 0x04 the swing hit Spyro: gloat after it */
  int timer; /* 0x08 */
  int knock; /* 0x0C knock-back heading */
  int speed; /* 0x10 */
  int lift;  /* 0x14 */
} CarriedThrowerState;

typedef struct WardState {
  int homeX;      /* 0x00 post */
  int homeY;      /* 0x04 */
  int homeZ;      /* 0x08 */
  int retarget;   /* 0x0C */
  int heading;    /* 0x10 */
  int speed;      /* 0x14 */
  int spin;       /* 0x18 */
  int timer;      /* 0x1C */
  int camLock;    /* 0x20 */
  int pad24[4];
  int mode;       /* 0x34 0 = post, 1 = chase, 2 = patrol */
  PathData *path; /* 0x38 */
  int pathDist;   /* 0x3C */
  int link;       /* 0x40 actor that frees the patrol (index into g_pActorListBase) */
  int camDone;    /* 0x44 */
} WardState;

typedef struct Follower {
  PathData *path;    /* 0x00 */
  short hold;        /* 0x04 */
  short vz;          /* 0x06 */
  short pad08;       /* 0x08 */
  signed char spinX; /* 0x0A */
  signed char spinY; /* 0x0B */
  int pathDist;      /* 0x0C */
  int link;          /* 0x10 leader (index into g_pActorListBase) */
  int target[3];     /* 0x14 */
  int timer;         /* 0x20 */
} Follower;

typedef struct GrazerState {
  char pad00[0x20];
  int speed; /* 0x20 knock-back speed */
  int knock; /* 0x24 knock-back heading */
  int wait;  /* 0x28 graze timer */
  int voice; /* 0x2C bleat timer */
} GrazerState;

/* Type 371: Doctor Shemp. */
typedef struct ShempState {
  int ang;             /* 0x00 heading << 4 */
  int pad04[3];        /* 0x04 */
  int timer;           /* 0x10 */
  int pad14;           /* 0x14 */
  int turnDir;           /* 0x18 passed by address to SteerActorYawTowardSpyro */
  int phase;           /* 0x1C 0..2 */
  unsigned char *path; /* 0x20 byte 0 = node count */
  unsigned char *cam;  /* 0x24 camera anchors, one per phase */
  int node[3];         /* 0x28 path node per phase */
  int flag;            /* 0x34 */
  int count;           /* 0x38 */
  int chain;           /* 0x3C first of a linked list in g_pActorListBase */
  int init;            /* 0x40 */
  int timer2;          /* 0x44 */
} ShempState;

typedef struct PostGrabberState {
  int homeX;     /* 0x00 post it returns to */
  int homeY;     /* 0x04 */
  int pad08;     /* 0x08 */
  int timer;     /* 0x0C */
  Actor **slot;  /* 0x10 shared holder slot */
  int hold;      /* 0x14 hold timer */
  int grip[3];   /* 0x18 grip offset, then world grip point */
  int link;      /* 0x24 index of its partner in g_pActorListBase, -1 = none */
  int fleeDir;   /* 0x28 */
  int fleeSpeed; /* 0x2C */
  int fleeStep;  /* 0x30 */
} PostGrabberState;

typedef struct AlarmSentryState {
  int speed; /* 0x00 retreat speed (case 54) */
  int step;  /* 0x04 */
  int dir;   /* 0x08 retreat heading */
  int link;  /* 0x0C index of its partner in g_pActorListBase, -1 = none */
  int timer; /* 0x10 */
} AlarmSentryState;

typedef struct LanternState {
  int held;    /* 0x00 */
  int timer;   /* 0x04 */
  FxNode *fx;  /* 0x08 glow node */
  int pos[3];  /* 0x0C glow anchor */
  int chunkAnim;   /* 0x18 world chunk-anim handle (GetWorldChunkAnimState) */
} LanternState;

extern int FindFarthestPathPointFromSpyro(PathData *path);

/* Hoisted: declared identically (bar parameter names) in every includer. */
extern int ComputeLobVelocityZ(int *from, int *to, int speed, int g, int *out);

/* Declared identically by two or more *_UpdateActors.c files; hoisted here. */
extern void ChangeSpyroState(int state);
extern void ComputeCameraOrbitOffset(int *out);
extern void ProjectWorldPointGTEUnscaled(int *out, int *s);
extern void StopSoundVoiceByIndex(int ch, int mode);
extern void TallyFlightTarget(int tallyClass);
extern void UpdateCameraEulerAngles(void);
extern Xform g_aCameraXformPresets[];
extern int g_anLevelKillBitmapTable[];
extern volatile unsigned char g_bSpyroOtBinBias;
extern int g_nCameraSpringYaw;
extern int g_nCameraYaw;
extern int g_nPortalCamPathMaxStep;
extern int g_nScreenFadeLevel;
extern int g_nGnastyLootFallRefZCap;
extern int g_nHitRumbleTimer;
extern int g_nMotionTrailRibbonCount;
extern int g_nVblankTickCount;
extern Xform *g_pCameraTargetParams;
extern PortalCamPath *volatile g_pPortalCamPathData;
extern struct CutscenePath *g_pPathTableHead;

/* Declared identically (by type) in two or more including files; hoisted here. */
extern int FindNearestPathPointToSpyro(FlyPath *path, int *nearest);
extern int g_anCameraPos[];
extern int g_nCameraActiveTargetPitch;
extern int g_nCameraActiveTargetDistance;
extern int g_nCameraActiveTargetRoll;
extern int g_nCameraActiveTargetPitchBias;
extern int g_nCameraActiveTargetYawBias;

/* Main globals read by a single UpdateActors clone. */
extern unsigned short g_anSwitchSfxPitchOffsets[]; /* indexed by the switch's bit position */
extern unsigned char g_bAmbienceChannelMask; /* ambience channel enables */
extern int g_bLevelBossDefeated; /* boss defeated */
extern int g_bLevelPhasedBossKilled;
extern int g_bLevelUseShortReleaseAnim;
extern int g_dwActiveCameraOptions;
extern int g_nPortalCamPathReverseFlag;
extern int g_nSpyroMoveTargetYaw; /* camera yaw override */

/* Word at byte offset `off` of g_anFlightHudClassWords (the flight HUD's
 * per-class display words). The flight UpdateActors clones fill it with a
 * byte-offset induction variable next to the source pointer; an int index
 * gives loop.c a different giv set. */
#define FLIGHT_HUD_CLASS_WORD(off) \
  (*(int *)((char *)g_anFlightHudClassWords + (off)))

#endif /* OPEN_SPYRO_LEVEL_UPDATE_ACTORS_H */
