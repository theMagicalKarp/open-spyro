/* Level14_UpdateActors -- per-frame actor update for level 14 (High
 * Caves). 0x8007B64C, 53,744 bytes.
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class: a small
 * state machine over the actor's state byte (->mode), often with a sub-state
 * (->subMode), and usually a class-specific state block behind ->state.
 *
 * Most classes are shared with the other levels' actor updates (levels 0, 4,
 * 9, 12, 13, 15, 16, 25 and 26); the level-14 classes are 109 (a flying
 * carrier that follows Spyro and can be ridden), 227 (a charging turret with
 * a scripted camera), 301 (a bolt caster), 302 (a riding platform), 303 and
 * 304 (a spawner and its runner) and 305 (a path-walking critter that flees).
 *
 * Load-bearing source forms:
 *   - No base/spy carrier: `spy` is `&SPY`, so Spyro reads are absolute and
 *     the preheader hoists only the small constants.
 *   - The frame is laid out in first-use order with shared slots (svF for
 *     cases 14 and 109, s496 for 312/329/392, s512 for 329/76).
 *   - Struct-versus-scalar spellings decide memory dependences (cookbook
 *     A302): SPY.state, SPY.u11C/u128, SPY.bodyRotZ, g_anCollisionContactPoint[2].
 *   - Case 109's turn-and-walk idiom reuses `h` and `d`, and its node
 *     searches use `&p->nodes[i].x` (loop.c makes the 8 + 16*i giv) with
 *     `st->cur->cur = i` (one fewer st ref decides st vs which, s5/s4).
 *   - Case 109's camera hold block takes `int *w1 = &g_anCameraActiveTargetParams.w[1]` so
 *     cse derives &g_anCameraActiveTargetParams as w1 - 4; cases 227/109 read the yaw as
 *     SPY.bodyRotZ (a struct MEM, ordered against the st->cam stores).
 *
 *   - Case 227 reads the camera's second sine through a pointer local
 *     (`*(nv = &g_anSineLut[..])`); with the plain array read sched2 puts the
 *     split 0x80000200 constant's lui ahead of the g_anCosineLut reload.
 */

#include "actor.h"
#include "level_actors.h"
#include "vecmath.h"
#include "level_globals.h"

/* The update list, and the per-actor values the loop publishes before it
 * dispatches. */
extern int g_nActorFrameDelta;      /* frame delta, as seen by the callees */

/* Model table entry, indexed by actor->type: sound ids and one header per
 * animation. */
typedef struct AnimationHeader {
  short m_NumFrames;                /* 0x00 */
  unsigned short m_NumColors;       /* 0x02 */
  unsigned char m_IsSpyroAnimation; /* 0x04 */
  unsigned char m_Scale;
  unsigned char m_ShortEncodeShift;
  unsigned char m_Radius;
  unsigned char m_VertCountHigh; /* 0x08 */
  unsigned char m_VertCountLow;
  unsigned char m_Padding2;
  unsigned char m_DepthScale;
  unsigned char m_ProgressPerTick; /* 0x0C -- the byte TYPE_DESC reads */
  unsigned char m_Padding3;
  unsigned short m_Padding4;
  void *m_AnimationVertices; /* 0x10 */
  void *m_Faces;
  void *m_Colors;
  void *m_LpFaces;
  void *m_LpColors; /* 0x20 */
} AnimationHeader;

typedef struct Model {
  int m_NumAnimations;              /* 0x00 */
  unsigned char m_Sounds[16];       /* 0x04 */
  void *m_CollisionModels[8];       /* 0x14 */
  void *m_Data;                     /* 0x34 */
  AnimationHeader *m_Animations[1]; /* 0x38 */
} Model;

extern Model *g_apActorMeshTable[]; /* per-type model table, indexed by ->type */

/* ==== Per-class state blocks ====
 *
 * Every actor class keeps its private state behind actor->state. The blocks
 * below sit next to the globals and callees their case needs.
 *
 * Waypoint path used by the path-following classes (10, 114, 339): node
 * count, current node, then 0x10-byte nodes that begin with a position. */
typedef struct PathData {
  unsigned char count; /* 0x00 */
  unsigned char cur;   /* 0x01 */
  char unk02[4];
  short reversed; /* 0x06 */
  struct {
    int x;
    int y;
    int z;
    int unk0C;
  } nodes[1]; /* 0x08 */
} PathData;

/* Type 10: path patroller. */
typedef struct PathState {
  int unk00;
  int unk04;
  int unk08;
  PathData *path; /* 0x0C */
  int unk10;
  int unk14;
  int unk18;
  int heading; /* 0x1C */
  int unk20;   /* 0x20 */
  int unk24;   /* 0x24 divisor, random 0x6E..0xA0 */
  int unk28;   /* 0x28 clamped to <= 0x5A */
  int unk2C;   /* 0x2C consecutive-success counter */
  int unk30;   /* 0x30 */
} PathState;

/* Node accessors for paths held as byte pointers (type 286). */
#define PATH_X(p, i) (*(int *)((p) + ((i) << 4) + 0x8))
#define PATH_Y(p, i) (*(int *)((p) + ((i) << 4) + 0xC))
#define PATH_Z(p, i) (*(int *)((p) + ((i) << 4) + 0x10))

extern int ComputeStepTowardPoint(int *from, int *to, int *vel, int a);

/* Progress-per-tick byte of animation `anim` in the actor's model. */
#define TYPE_DESC(a, id)                                                     \
  (g_apActorMeshTable[(a)->type]->m_Animations[(id)]->m_ProgressPerTick)

/* Type 16: butterfly. Flutters around an anchor until Sparx fetches it. */
typedef struct ChaseState {
  int dir;               /* 0x00 -1 or 1, picked from which side Spyro is on */
  int tgtX;              /* 0x04 */
  int tgtY;              /* 0x08 */
  int unk0C;             /* 0x0C */
  unsigned char vz;      /* 0x10 vertical step, sign-magnitude in a byte */
  unsigned char heading; /* 0x11 */
  unsigned char t12;     /* 0x12 heading-change timer */
  unsigned char t13;     /* 0x13 vertical-change timer */
  unsigned char t14;     /* 0x14 */
} ChaseState;

/* Sparx's state block, as seen by the actors that send him fetching. */
typedef struct ChaserState {
  int timer; /* 0x00 */
  int unk04;
  int unk08;
  int unk0C;
  Actor *owner; /* 0x10 actor being fetched, or null */
} ChaserState;

/* Engine callees shared by most behaviours: update list, collision and
 * movement helpers, random numbers and angle arithmetic. */
extern void BuildActiveActorList(void);

/* Step a countdown timer; non-zero when it fires. */
extern int WrapAddAngle8(int a, int b);
extern int WrapAngleDelta8(int a, int b); /* signed angle delta */

/* Level actor pool, switch flags, and the sound tables and override
 * globals used when a behaviour plays a sound. */
extern struct {
  short v;
} g_nSfxOverridePitch;

/* Egg sequence flags, then the vector library: copy, add, subtract, scale,
 * shift, sine and cosine. */
extern struct {
  int v;
} g_nHudWorldEggRollRequest;

extern void AimCameraStickTowardPoint(int *pos);
extern void SVectorToVectorShl2(int *d, int *s);

/* Mark an item collected; the second argument is the actor or its id. */
extern void CommitActorKillGems(int a, void *b);

extern int SteerActorAroundPoint(Actor *actor, int *target, int a, int heading, int b,
                          int c, int d, int e, int f, int g, int h, int i,
                          int j);

extern void ModifyWorldChunkAnimFlags(int handle, int a, int b);
extern void SetWorldChunkAnimStep(int handle, int a, int b);
extern int GetWorldChunkAnimState(int handle); /* bits 8..15 are the anim marker */

/* Types 260..269: floating digits. Same bounce, different layout. */
typedef struct DebrisState {
  int life;   /* 0x00 */
  int unk04;  /* 0x04 */
  int vel[3]; /* 0x08 */
} DebrisState;

/* Surface normal produced by the collision probes. */
extern int g_anCollisionProbeVec;

/* Single-field views for reading a scalar symbol through a struct type. */
typedef struct {
  int v;
} I32;
#define AS_I32(sym) (((I32 *)&(sym))->v)

extern int CollideSphereWithWorldAndActors(int *pos, int a, int b); /* ground/wall probe */

/* g_aRotBillboards records (SpawnActorRotBillboard) used as sparkles around items. */
typedef struct RotBillboard {
  int pos[3];         /* 0x00 */
  unsigned char life; /* 0x0C frames left; 0 = slot free */
  unsigned char pad[11];
} RotBillboard;

extern RotBillboard g_aRotBillboards[];

/* Type 194: wooden chest. Breaks into fragments when damaged. */
typedef struct BreakState {
  int unk00; /* 0x00 */
  int unk04; /* 0x04 */
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
  Actor *unk00; /* 0x00 the spawned passenger, valid once ->mode is 1 */
  int unk04;    /* 0x04 phase */
  int unk08;    /* 0x08 saved actor->rotX */
  int unk0C;    /* 0x0C saved actor->rotY */
  int unk10;    /* 0x10 saved actor->posZ */
  int unk14;    /* 0x14 mode; 1 selects the extra tick and the rearm */
} LiftState;

#define LIFT_STEP(st) (((st)->unk04 - 0x1A0) >> 1)

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
  int posX; /* 0x28 m_Position */
  int posY; /* 0x2C */
  int posZ; /* 0x30 */
  char pad034[0x18];
  short rotX;         /* 0x4C m_Rotation */
  short rotY;         /* 0x4E */
  short rotZ;         /* 0x50 */
  short unk52;        /* 0x52 */
  int occlusion;      /* 0x54 */
  unsigned int state; /* 0x58 m_State */
  int unk5C;          /* 0x5C */
  Xform lastSim;      /* 0x60 */
  Xform sphere;       /* 0x78 m_Sphere */
  Xform sim;          /* 0x90 m_Simulation */
  Xform unkA8;        /* 0xA8 */
  int unkC0;          /* 0xC0 */
  char padC4[0x14];
  Xform *preset; /* 0xD8 m_SphericalPreset */
} CamObj;
extern CamObj g_anWorldToCameraRotMtx;
#define CAM g_anWorldToCameraRotMtx

/* Vortex path: node count and current node, then the two anchor positions
 * whose midpoint Spyro is pulled toward. */
typedef struct PortalDesc {
  unsigned char count;    /* 0x00 */
  unsigned char index;    /* 0x01 */
  unsigned char unk02[6]; /* 0x02 */
  int posA[3];            /* 0x08 */
  int unk14;              /* 0x14 */
  int posB[3];            /* 0x18 */
} PortalDesc;

/* Type 398 state block. */
typedef struct PortalState {
  PortalDesc *desc; /* 0x00 */
  int offset[3];    /* 0x04 nudge applied to the homing vector */
  int unk10;        /* 0x10 */
  int unk14;        /* 0x14 published to SPY.u206 as a byte */
  int unk18;        /* 0x18 index into g_aCameraXformPresets; negative selects none */
} PortalState;

/* Type 300: camera trigger volume. Live camera sphere and its template. */
typedef struct Xform6 {
  int w[6];
} Xform6;
extern Xform6 g_anCameraActiveTargetParams; /* live camera transform */

extern void StubRewardHook(int script, int a);

/* Callees used by the collectible and fairy behaviours. */
extern void UnpackWorldCollisionTri(int a, int *out);
extern int FindGroundHeightBelow(int *v, int k);
extern int GetGroundTriggerRegionType(void);
extern int CheckLineOfSightSegment(int *a, int *b);
extern int func_80017428(int *v, int *n, int *out);

extern int TrackEntityYawTowardSpyro(Actor *actor, int a, int b, int c);

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
  int unk14;           /* 0x14 */
  int unk18;           /* 0x18 */
  int unk1C;           /* 0x1C */
} FxNode;

extern FxNode *AllocWorldSparkle(void);
extern void ClearWord(FxNode *node);
extern int StepAngle8Toward(int a, int b, int c);

extern int CastRayWorldAndActors(int *a, int *b);

/* Shift a vector left by n. */

/* Types 257/425: falling chest fragments. Integrate until life, visibility
 * or the floor runs out. */
typedef struct TumbleState {
  short vel[3];  /* 0x00 */
  short spin[3]; /* 0x06 per-frame delta for ->rotX/rotY/rotZ */
  int life;      /* 0x0C ticks remaining */
  int floorZ;    /* 0x10 retire once the actor sinks to this height */
} TumbleState;

/* ==== Helper macros ====
 *
 * Absolute value. The two forms differ only at zero; both occur. */
#define ABS(x) ((x) >= 0 ? (x) : -(x))
#define ABS2(x) ((x) > 0 ? (x) : -(x))

/* Animation state: current/next animation (0x3C/0x3D), current/next frame
 * (0x3E/0x3F), frame progress (0x40) and progress per frame (0x41).
 *
 * ANIM_RESET restarts `anim` from frame 0 at the model's own speed. */
#define ANIM_RESET(a, id)                                                    \
  (a)->frameProgress = 0;                                                              \
  (a)->frameRate = TYPE_DESC(a, id);                                             \
  (a)->anim = (id);                                                         \
  (a)->nextAnim = (id);                                                         \
  (a)->frame = 0;                                                              \
  (a)->nextFrame = 1;

/* ANIM_ADVANCE promotes the queued animation and queues `next` after it. */
#define ANIM_ADVANCE(a, next)                                                  \
  (a)->frameProgress = sixteen;                                                        \
  (a)->frameRate = sixteen;                                                        \
  (a)->anim = (a)->nextAnim;                                                     \
  (a)->nextAnim = (next);                                                         \
  (a)->frame = (a)->nextFrame;                                                     \
  (a)->nextFrame = 0;                                                              \
  StubActorHook(a);

/* ENTER_POSE sets the state byte to `n`, advances to animation `n` unless it
 * is already queued, and moves on to the next actor. It ends in a bare
 * `continue`, so use it only inside braces. */
#define ENTER_POSE(a, n)                                                       \
  (a)->mode = (n);                                                            \
  if ((a)->nextAnim != (n)) {                                                     \
    (a)->frameProgress = sixteen;                                                      \
    (a)->frameRate = sixteen;                                                      \
    (a)->anim = (a)->nextAnim;                                                   \
    (a)->nextAnim = (n);                                                          \
    (a)->frame = (a)->nextFrame;                                                   \
    (a)->nextFrame = 0;                                                            \
    StubActorHook(a);                                                          \
  }                                                                            \
  continue

/* Spyro as one object at 0x80078A58, with the fields this function touches. */
typedef struct {
  int posX; /* 0x000 */
  int posY; /* 0x004 */
  int posZ; /* 0x008 */
  char pad00C[0x2];
  unsigned char bodyRotZ; /* 0x00E */
  char pad00F[0x18];
  unsigned char u027; /* 0x027 */
  char pad028[0x4];
  int damageFlags; /* 0x02C */
  char pad030[0x4];
  int u034; /* 0x034 */
  char pad038[0x40];
  int state; /* 0x078 */
  char pad07C[0x4];
  int u080; /* 0x080 */
  char pad084[0x18];
  int u09C; /* 0x09C */
  char pad0A0[0x6C];
  int u10C; /* 0x10C */
  int u110; /* 0x110 */
  int u114; /* 0x114 */
  int u118; /* 0x118 */
  int u11C; /* 0x11C */
  char pad120[0x8];
  int u128; /* 0x128 */
  char pad12C[0x38];
  int u164; /* 0x164 */
  char pad168[0x14];
  int contact; /* 0x17C */
  char pad180[0x24];
  int headLook; /* 0x1A4 */
  char pad1A8[0x4C];
  int controlFlags; /* 0x1F4 */
  char pad1F8[0x8];
  int u200; /* 0x200 */
  char pad204[0x2];
  unsigned char u206; /* 0x206 */
  char pad207[0x1];
  int u208; /* 0x208 */
  int u20C; /* 0x20C */
  int u210; /* 0x210 */
  char pad214[0x4];
  int u218;              /* 0x218 */
  int *pCamAnchor;       /* 0x21C */
  void *pCamTarget;      /* 0x220 */
  Actor *pScriptedActor; /* 0x224 */
  char pad228[0x18];
  PortalDesc *pPortal; /* 0x240 */
  int u244;            /* 0x244 */
  int u248;            /* 0x248 */
} SpyroObj;
extern SpyroObj g_anSpyroWorldPosBlock;
#define SPY g_anSpyroWorldPosBlock

/* ==== level_30 per-class state blocks ====
 *
 * Gnorc Gnexus is the hub for Gnasty's world: the balloonist, the vortex
 * pads, the two boss-door gnorcs and the shared fodder/debris classes. Each
 * block below is the ->state record of one actor class, named after the case
 * that uses it. */

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
  int unk24;    /* 0x24 */
} FollowState;

/* Case 34 — the vortex/whirlwind pad that lifts Spyro out of the level. */
typedef struct VortexState {
  int idx;               /* 0x00 */
  short centre[3];       /* 0x04 pad centre */
  short apex[3];         /* 0x0A rise target */
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
  unsigned char bounce; /* 0x10 remaining knock-back bounces */
  signed char lean;     /* 0x11 body pitch */
  signed char roll;     /* 0x12 */
  unsigned char spin;   /* 0x13 */
  unsigned char attach; /* 0x14 attached smoke/spark handle, 0xFF = none */
} EnemyState;

/* Case 110 — the lift plate that rises when its trigger actor is cleared. */
typedef struct PlateState {
  int trigger; /* 0x00 actor index that arms the plate */
  int rider;   /* 0x04 actor index carried on top */
  int timer;   /* 0x08 */
  int baseZ;   /* 0x0C rest height */
  int from[3]; /* 0x10 interpolation start */
  int yaw;     /* 0x1C */
  int step;    /* 0x20 */
  int locked;  /* 0x24 */
} PlateState;

/* Case 120 — the balloonist himself: owns the smoke emitter and the gnorc he
 * is waiting on. */
typedef struct BalloonState {
  int timer;      /* 0x00 puff countdown */
  short drift[3]; /* 0x04 wind drift, x/y/z */
  char pad0A[0x2];
  FxNode *smoke; /* 0x0C particle emitter */
  Actor *gnorc;  /* 0x10 */
} BalloonState;

/* Case 173 — ambient flyer: circles, and pops when Spyro reaches it. */
typedef struct FlyState {
  short timer;          /* 0x00 */
  unsigned char angle;  /* 0x02 */
  unsigned char attach; /* 0x03 attached spark handle, 0xFF = none */
} FlyState;

/* Case 195 — the same chest with a wobble pose played before it breaks. */
typedef struct WobbleState {
  int phase;  /* 0x00 wobble phase, 0 when at rest */
  int rotX;   /* 0x04 rest pose */
  int rotY;   /* 0x08 */
  int restZ;  /* 0x0C */
  int unk10;  /* 0x10 */
  int handle; /* 0x14 */
} WobbleState;

/* Cases 76 and 426..451 — the orbiting satellite: rides a parent actor at a
 * fixed radius, or falls back to a plain spin when the parent is not a
 * spinner. */
typedef struct SatState {
  Actor *parent; /* 0x00 */
  short radius;  /* 0x04 */
  short phase;   /* 0x06 */
} SatState;

/* Cases 255/256, 310/311, 257, 309 — debris shards thrown by a break. */
typedef struct ShardState {
  short vel[3];  /* 0x00 */
  short spin[3]; /* 0x06 pitch / yaw / roll per frame */
  int life;      /* 0x0C */
  int floor;     /* 0x10 */
} ShardState;

/* Case 286 — the supercharge/flight path rider, stepping a waypoint ring. */
typedef struct RideState30 {
  int pose;       /* 0x00 which voice/pose the rider is in */
  int volume;     /* 0x04 */
  PathData *path; /* 0x08 */
  int wait;       /* 0x0C */
  int leg;        /* 0x10 */
  int speed;      /* 0x14 */
  int step[3];    /* 0x18 per-frame step along the leg */
  int left;       /* 0x24 frames left on this leg */
} RideState30;

/* Case 250 — the boss-door gnorc: opens the door, then leaves the level. */
typedef struct DoorState {
  char pad00[0x18];
  int unk18;   /* 0x18 */
  int unk1C;   /* 0x1C */
  int partner; /* 0x20 actor index of the other door half, -1 = none */
  int reward;  /* 0x24 */
  char pad28[0x1C];
  int phase; /* 0x44 open animation phase */
  int rotX;  /* 0x48 rest pose */
  int rotY;  /* 0x4C */
  int restZ; /* 0x50 */
} DoorState;

extern volatile unsigned char
    g_bSpyroPersistentEulerZ;          /* SPY.bodyRotZ, absolute form (A240) */

extern struct {
  int x;
  int y;
} g_anSpyroRequestVelocity; /* scripted camera look-at */
#define g_nSpyroRequestVelocityY g_anSpyroRequestVelocity.y

extern void ModifyWorldChunkAnimFlags(int ch, int vol, int a);
extern void SetWorldChunkAnimStep(int ch, int n, int a);
extern int GetWorldChunkAnimState(int ch);
extern int ComputeStepTowardPoint(int *from, int *to, int *step, int speed);
extern int WrapAngleDelta8(int a, int b);

/* ==== Globals shared with level 4 ==== */
extern void RefreshHudLevelGemCounter(void);
extern void ResetSpyroEntity(int a0);

/* Case 9 -- exit drop: shows the exit text near the camera, sparkles, and
 * takes Spyro out of the level when he drops into it. */
typedef struct DropState {
  int unk00;
  int tick; /* 0x04 sparkle counter */
  int unk08;
  int depth; /* 0x0C drop depth that triggers the exit, 0 = 0x2000 */
} DropState;

extern int PivotActorAlongPath(Actor *actor, void *path, int a, int b, int c, int d,
                          int e, int f, int g);

/* An animation change that also clears the finished flag. */
#define ANIM_GO(a, n)                                                          \
  if ((a)->nextAnim != (n)) {                                                     \
    g_nActorAnimDone = 0;                                                            \
    ANIM_ADVANCE(a, n);                                                        \
  }
#define ANIM_SET(a, n)                                                         \
  if ((a)->anim != (n)) {                                                     \
    g_nActorAnimDone = 0;                                                            \
    ANIM_RESET(a, n);                                                          \
  }

/* ENTER_POSE with the already-there test first. */
#define ENTER_POSE4(a, n)                                                      \
  (a)->mode = (n);                                                            \
  if ((a)->nextAnim == (n)) {                                                     \
    continue;                                                                  \
  }                                                                            \
  ANIM_ADVANCE(a, n);                                                          \
  continue

extern int RaiseSpyroTriggerInRadius(int *pos, int a, int b);

/* Cases 152/153 -- sparking debris. */
typedef struct DebrisFallState {
  int vel[3];             /* 0x00 */
  int floor;              /* 0x0C */
  unsigned char spin[3];  /* 0x10 */
} DebrisFallState;

extern void StopSoundVoiceByIndex(int ch, int a);

/* Case 312 -- cracking boulder. */
typedef struct L16Boulder {
  int timer;  /* 0x00 shake time, then burst ring */
  int rotX;   /* 0x04 rest pose */
  int rotY;   /* 0x08 */
  int restZ;  /* 0x0C */
  int pushed; /* 0x10 camera knocked back */
} L16Boulder;
typedef struct SmokeJet {
  short pos[3]; /* 0x00 */
  short vel[3]; /* 0x06 */
} SmokeJet;
extern SmokeJet D_8006E614[];
extern int DispatchActorContactAtSphere(int *pos, int a, int b, int c, Actor *actor, int d);

/* Case 328 -- floor-snapped marker. */

/* Case 329 -- seed pod. */
typedef struct L16Pod {
  Actor *prize; /* 0x00 */
  int timer;    /* 0x04 */
  int rotX;     /* 0x08 rest pose */
  int rotY;     /* 0x0C */
  int restZ;    /* 0x10 */
} L16Pod;

extern int CheckSpyroLineOfSightTo(int *pos);
extern int IsPointInRotatedBox(int *a, int *b);

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
/* An animation change at half speed, keyed on the current animation. */
#define ANIM_BLEND(a, n)                                                       \
  if ((a)->anim != (n)) {                                                     \
    (a)->frameProgress = 8;                                                            \
    (a)->frameRate = 8;                                                            \
    (a)->anim = (a)->nextAnim;                                                   \
    (a)->nextAnim = (n);                                                          \
    (a)->frame = (a)->nextFrame;                                                   \
    (a)->nextFrame = 0;                                                            \
    StubActorHook(a);                                                          \
  }
/* Case 166 -- path patroller. */
typedef struct PatrolState {
  PostPath *path; /* 0x00 */
  int mode;       /* 0x04 */
  int flag;       /* 0x08 */
  int near;       /* 0x0C */
  int timer;      /* 0x10 */
} PatrolState;

extern void GetActorAttachPoint(Actor *actor, int a, int *pos);

/* ==== Camera and Spyro globals used by the camera arms ==== */
extern int WrapAddAngle8(int heading, int step);
extern int D_8007866C, D_80078670, D_80078674, D_80078678, D_8007867C;
typedef struct {
  int x;
  int y;
  int z;
} CamLook;
#define CAM_LOOK (*(CamLook *)&g_anSpyroRequestVelocity)

/* Type 33: egg thief. Carries a stolen egg (a type 0x22 actor it spawns),
 * runs a looping path away from Spyro, digs between path sections, and drops
 * the egg when hit. */
typedef struct ThiefState {
  PathData *path; /* 0x00 */
  Actor *egg;     /* 0x04 the carried egg, or null once dropped */
  int mode;       /* 0x08 idle behaviour, picked by the level data */
  int vz;         /* 0x0C vertical speed while hopping */
  int landed;     /* 0x10 */
  int speed;      /* 0x14 path speed */
  int stuck;      /* 0x18 frames spent cornered */
  int heading;    /* 0x1C knockback heading */
  int knock;      /* 0x20 knockback speed */
  int unk24;
  int node;       /* 0x28 node the path should stop at */
  int range;      /* 0x2C wake-up range, in 0x400 units */
  int turned;     /* 0x30 has reversed once on this leg */
  int needLos;    /* 0x34 */
  int dist;       /* 0x38 */
  int voice;      /* 0x3C taunt timer */
  int lastVoice;  /* 0x40 */
  int area[3];    /* 0x44 trigger volume for mode 4 */
} ThiefState;

extern int FindFarthestPathPointFromSpyro(PathData *path);

/* Case 390 -- spinning chest. */
typedef struct L26Spinner {
  Actor *lid; /* 0x00 */
  int angle;  /* 0x04 */
  int speed;  /* 0x08 */
  int timer;  /* 0x0C */
  int handle; /* 0x10 for SelectActorLodBand */
} L26Spinner;

/* Case 392 -- homing spark. */
typedef struct L26Spark {
  int vel[3]; /* 0x00 */
  int timer;  /* 0x0C */
} L26Spark;
/* Type 295: homing wisp. Steers toward its target actor (or Spyro), and
 * bursts into three sparks when it arrives or touches the ground. */
typedef struct WispState {
  Actor *target; /* 0x00 actor to reach, or null to chase Spyro */
  int timer;     /* 0x04 lifetime */
} WispState;

#define sixteen 0x10

#define PATH_PT(p, i) ((int *)((p) + (((i) << 4) + 0x8)))

extern unsigned char g_bSpyroPersistentEulerZ; /* SPY.bodyRotZ, absolute form */ /* kill bitmap, one bit per pool actor */

/* The anim bytes 0x3C..0x3F read as one word. */
#define ANIM_WORD(a) (*(int *)&(a)->anim)

/* Type 270: villager. Chats when Spyro faces it up close (mode 0), turns to
 * face a friend (mode 1), waits on a partner (mode 2) or watches a leader
 * (mode 3); knocked back when hit, and runs off when Spyro gets close. */
typedef struct VillagerState {
  unsigned char *path; /* 0x00 wander path { count, cur }, points at +8 */
  int snd;             /* 0x04 voice handle, -1 = none */
  int friend;          /* 0x08 actor it faces / sparkles at, -1 = none */
  int link;            /* 0x0C partner actor */
  int mode;            /* 0x10 */
  int timer;           /* 0x14 voice timer */
  int timerLong;       /* 0x18 */
  int timerShort;      /* 0x1C */
  int noBoost;         /* 0x20 */
  int unk24;
  int talkDist;        /* 0x28 in 1024ths */
  int camDist;         /* 0x2C in 1024ths */
  int camAlways;       /* 0x30 */
  int fleeDist;        /* 0x34 in 1024ths */
  int speed;           /* 0x38 */
  int heading;         /* 0x3C */
  int sndDone;         /* 0x40 */
  int stopVoice;       /* 0x44 */
  int runFrom;         /* 0x48 distance it started running at */
  int faceTimer;       /* 0x4C */
  int init;            /* 0x50 */
  int idleTimer;       /* 0x54 */
} VillagerState;

/* The leader a mode-3 villager watches. */
typedef struct LeaderState {
  int mode;               /* 0x00 */
  int unk04[2];
  unsigned char *script;  /* 0x0C */
  int unk10[4];
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

/* Spyro's facing delta to an actor. */
#define SPY_FACING(a)                                                          \
  AbsAngleDelta8(spy->bodyRotZ, ArcTan2((a)->posX - spy->posX,            \
                                             (a)->posY - spy->posY, 0))
/* An actor's height above Spyro. */
#define SPY_DZ(a) ((a)->posZ - (a)->floorOffset - SPY.posZ)

/* Type 315: falling block. */
typedef struct FallState {
  int unk00;
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
  int unk14;
  int range;           /* 0x18 */
  int angry;           /* 0x1C */
  int sub;             /* 0x20 */
  int unk24[3];
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
  int unk2C;
  int timer;           /* 0x30 */
  int posed;           /* 0x34 */
  int range;           /* 0x38 in 1024ths */
  int timer2;          /* 0x3C */
} CaptainState;

/* Advance to animation `n` with frame progress 8 (a slower blend). */
#define ANIM_GO8(a, n)                                                         \
  if ((a)->nextAnim != (n)) {                                                     \
    g_nActorAnimDone = 0;                                                            \
    (a)->frameProgress = 8;                                                            \
    (a)->frameRate = 8;                                                            \
    (a)->anim = (a)->nextAnim;                                                   \
    (a)->nextAnim = (n);                                                          \
    (a)->frame = (a)->nextFrame;                                                   \
    (a)->nextFrame = 0;                                                            \
    StubActorHook(a);                                                          \
  }

/* Re-time the current animation once its frames are within 3 of each
 * other. */
#define ANIM_RETIME(a)                                                         \
  if (ABS2((a)->nextFrame - (a)->frame) < 3) {                                     \
    (a)->frameRate = TYPE_DESC(a, (a)->anim);                                     \
  }

/* Jump the queued animation to frame `f` of animation `n`. */
#define ANIM_JUMP(a, n, f)                                                     \
  g_nActorAnimDone = 0;                                                              \
  (a)->frameProgress = sixteen;                                                        \
  (a)->frameRate = sixteen;                                                        \
  (a)->anim = (a)->nextAnim;                                                     \
  (a)->nextAnim = (n);                                                            \
  (a)->frame = (a)->nextFrame;                                                     \
  (a)->nextFrame = (f);                                                            \
  StubActorHook(a)

/* from level_12 */
/* from level_12 */
/* from level_12 */
/* from level_12 */
/* from level_12 */
typedef struct L16Guard {
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
} L16Guard;
/* from level_12 */
typedef struct CageState {
  int idx;    /* 0x00 index of its flyer in g_pActorListBase */
  int shake;  /* 0x04 shake timer, then ride timer */
  int rotX;   /* 0x08 rest pose */
  int rotY;   /* 0x0C */
  int restZ;  /* 0x10 */
  int handle; /* 0x14 for SelectActorLodBand */
} CageState;
/* from level_12 */
/* from level_12 */
/* from level_12 */
/* from level_12 */
/* from level_12 */
/* from level_12 */
typedef struct L31Rock {
  int shake;  /* 0x00 shake timer, 0 = still */
  int rotX;   /* 0x04 rest rotation */
  int rotY;   /* 0x08 */
  int restZ;  /* 0x0C */
  int handle; /* 0x10 for SelectActorLodBand */
} L31Rock;
/* from level_10 */
typedef struct RoamState {
  int mode;            /* 0x00 inner jump-table discriminant, 0..9 */
  int unk04;           /* 0x04 republished into actor->unk55 every frame */
  unsigned char *path; /* 0x08 [0]=waypoint count, [1]=current index */
  int timer;           /* 0x0C */
  int sub;             /* 0x10 sub-state inside mode 2 */
  int unk14;           /* 0x14 4th argument to ComputeStepTowardPoint */
  int vel[3];          /* 0x18 current leg velocity, written by ComputeStepTowardPoint */
  int count;           /* 0x24 frames left on the current leg */
} RoamState;
/* from level_10 */
typedef struct ShotState {
  void *unk00;           /* 0x00 egg index */
  short from[3];         /* 0x04 launch point */
  short to[3];           /* 0x0A destination */
  short speed;           /* 0x10 */
  unsigned char phase;   /* 0x12 timer */
  unsigned char heading; /* 0x13 */
} ShotState;
/* from level_10 */
typedef struct SwitchState {
  int shift; /* 0x00 bit position within the target's 0x49 mask */
  int idx;   /* 0x04 index into the actor pool at g_pActorListBase */
} SwitchState;
/* from level_10 */
/* from level_10 */
/* from level_10 */
typedef struct AnimState {
  int handle;  /* 0x00 */
  int started; /* 0x04 one-shot init latch */
  int idx;     /* 0x08 pool index of the actor this one watches */
} AnimState;
/* from level_10 */
/* from level_10 */
/* from level_10 */
/* from level_10 */
/* from level_10 */
/* from level_10 */
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
/* from level_10 */
typedef struct {
  unsigned char *v;
} PtrU8;
/* from level_10 */
#define AS_PTRU8(sym) (((PtrU8 *)&(sym))->v)
/* from level_10 */
typedef struct PickupState {
  int vel[3];          /* 0x00 */
  short unk0C;         /* 0x0C */
  unsigned char sub;   /* 0x0E resting sub-state, 0..3 */
  unsigned char timer; /* 0x0F */
  unsigned char count; /* 0x10 bounces left */
  unsigned char pitch; /* 0x11 */
  unsigned char yaw;   /* 0x12 */
  unsigned char roll;  /* 0x13 */
  unsigned char anim;  /* 0x14 0xFF == none */
} PickupState;
/* from level_10 */
typedef struct SpinState {
  short timer;         /* 0x00 */
  unsigned char angle; /* 0x02 */
  unsigned char anim;  /* 0x03 0xFF == none */
} SpinState;
/* from level_10 */
typedef struct RideState {
  int unk00[6]; /* 0x00 */
  int unk18;    /* 0x18 */
  int unk1C;    /* 0x1C fallback heading when ->unk20 is unset */
  int unk20;    /* 0x20 index into g_pActorListBase, -1 for none */
  int unk24;    /* 0x24 script id handed to StubRewardHook, -1 for none */
  int unk28[7]; /* 0x28 */
  int unk44;    /* 0x44 phase counter, 0 .. 0x100 */
  int unk48;    /* 0x48 saved actor->rotX */
  int unk4C;    /* 0x4C saved actor->rotY */
  int unk50;    /* 0x50 saved actor->posZ */
} RideState;
/* from level_10 */
extern Xform6 g_anCameraParamMode2Front; /* template it is seeded from */
/* from level_10 */
typedef struct TriggerState {
  int unk00; /* 0x00 vertical span; also the offset added to the target Z */
  int unk04; /* 0x04 horizontal radius */
  int flags; /* 0x08 bit0 style, bit1 armed, bit2 held, bit3 owner-gated,
              * bit4 also stamps actor->subMode */
  int timer; /* 0x0C */
  int idx;   /* 0x10 index into the actor pool at g_pActorListBase */
} TriggerState;
/* from level_10 */
typedef struct RespawnState {
  int type;   /* 0x00 the actor type this manager owns */
  int timer;  /* 0x04 */
  int period; /* 0x08 reload for ->timer */
} RespawnState;
/* from level_10 */
typedef struct PoleState {
  int idxA;   /* 0x00 the actor that arms this one */
  int idxB;   /* 0x04 the actor it reaches for */
  int timer;  /* 0x08 */
  int baseZ;  /* 0x0C rest height, restored each cycle */
  int tgt[3]; /* 0x10 */
  int angle;  /* 0x1C */
  int t20;    /* 0x20 */
  int flag24; /* 0x24 */
} PoleState;
/* from level_10 */
typedef struct WalkerState {
  PathData *path; /* 0x00 */
  int timer;      /* 0x04 */
  int tgtIdx;     /* 0x08 index into g_pActorListBase, -1 when unclaimed */
  int heading;    /* 0x0C */
  int unk10;      /* 0x10 passed by address to DriveActorTrackedMove */
  int unk14;      /* 0x14 likewise */
} WalkerState;
/* from level_10 */
typedef struct FlierState {
  int timer;    /* 0x00 idle / fetch timer */
  short vx;     /* 0x04 */
  short vy;     /* 0x06 */
  short vz;     /* 0x08 */
  FxNode *fx;   /* 0x0C */
  Actor *rider; /* 0x10 */
} FlierState;
/* from level_10 */
typedef struct FlockState {
  int unk00;      /* 0x00 knockback angle */
  int unk04;      /* 0x04 knockback speed */
  PathData *path; /* 0x08 */
  int unk0C;
  int chainA;       /* 0x10 */
  int chainB;       /* 0x14 */
  int slots[16][3]; /* 0x18 */
} FlockState;
/* from level_10 */
typedef struct PatchRideState {
  int handle;    /* 0x00 */
  int offset[3]; /* 0x04 actor position relative to the centroid */
} PatchRideState;
/* from level_10 */
typedef struct PropAnimState {
  int anim;   /* 0x00 handle for ModifyWorldChunkAnimFlags / B3F4 / B444 */
  int timer;  /* 0x04 */
  int primed; /* 0x08 */
} PropAnimState;
/* from level_10 */
/* from level_10 */
typedef struct OrbitState {
  Actor *owner; /* 0x00 */
  short unk04;  /* 0x04 ring index; the arm always uses it as unk04 - 1 */
  short unk06;  /* 0x06 phase offset within the ring */
} OrbitState;
/* from level_10 */
typedef struct WanderState {
  int turn;              /* 0x00 turn direction, +1 / -1 */
  int home[3];           /* 0x04 spawn position it stays near */
  unsigned char speed;   /* 0x10 forward speed, signed when >= 0x80 */
  unsigned char heading; /* 0x11 */
  unsigned char reTurn;  /* 0x12 frames until the next heading change */
  unsigned char reStep;  /* 0x13 frames until the next speed change */
  unsigned char unk14;   /* 0x14 */
} WanderState;
/* from level_10 */
typedef struct ChestState {
  int unk0;
  int shake; /* 0x04 */
} ChestState;
/* from level_10 */
typedef struct MusicState {
  int started; /* 0x00 */
} MusicState;
/* from level_10 */
typedef struct ChargeState {
  char pad00[0x20];
  int speed;   /* 0x20 */
  int heading; /* 0x24 */
  char pad28[0x4];
  int rest; /* 0x2C frames before the next charge */
} ChargeState;
/* from level_10 */
typedef struct ShotState30 {
  int life; /* 0x00 */
  char pad04[0x4];
  int vel[2]; /* 0x08 */
  int velZ;   /* 0x10 */
} ShotState30;
/* from level_10 */
typedef struct WardenState {
  int group;  /* 0x00 */
  int timer;  /* 0x04 */
  int period; /* 0x08 */
} WardenState;
/* from level_10 */
typedef struct ExitState {
  PortalDesc *portal; /* 0x00 */
  int centre[3];      /* 0x04 */
  int unk10;          /* 0x10 */
  int dest;           /* 0x14 */
  int preset;         /* 0x18 */
} ExitState;
/* from level_10 */
typedef struct BobState {
  int restZ; /* 0x00 */
} BobState;
/* from level_10 */
/* from level_10 */
/* from level_10 */
/* from level_10 */
/* from level_10 */
/* from level_10 */
/* from level_10 */
/* from level_10 */
/* from level_10 */
/* from level_10 */
/* from level_10 */
/* from level_10 */
/* from level_10 */
/* from level_10 */
extern signed char D_80078C5E; /* destination level */
/* from level_10 */
/* from level_10 */
/* from level_10 */
/* from level_10 */
/* from level_10 */
/* from level_10 */
/* from level_10 */
/* from level_10 */
/* from level_10 */
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
/* from level_10 */
typedef struct LungeState {
  int unk00;
  int guard;     /* 0x04 */
  int home[3];   /* 0x08 */
  int target[3]; /* 0x14 */
  int unk20[3];  /* 0x20 */
  int anim;      /* 0x2C animation base */
  int hits;      /* 0x30 */
  int heading;   /* 0x34 -1 until first set */
  int alerted;   /* 0x38 */
  int unk3C;
  int *flag; /* 0x40 */
  int unk44;
  int wait;       /* 0x48 */
  int knockAngle; /* 0x4C */
  int knockSpeed; /* 0x50 */
  int velZ;       /* 0x54 */
  int look;       /* 0x58 */
  int burn;       /* 0x5C */
} LungeState;
/* from level_10 */
typedef struct SpinnerState {
  int count; /* 0x00 */
  int timer; /* 0x04 */
  int angle; /* 0x08 knock-back heading */
  int speed; /* 0x0C */
  int spin;  /* 0x10 */
} SpinnerState;
/* from level_10 */
/* from level_10 */
typedef struct RimState {
  Actor *parent;  /* 0x00 */
  short reach;    /* 0x04 */
  short range;    /* 0x06 */
  short x;        /* 0x08 Spyro's position when last seen, >> 4 */
  short y;        /* 0x0A */
  Actor *child;   /* 0x0C its type 39 helper */
  unsigned char mode; /* 0x10 */
} RimState;
/* from level_10 */
typedef struct RimParent {
  char pad00[0x48];
  int busy; /* 0x48 */
} RimParent;
/* from level_10 */
typedef struct HelperState {
  int timer;    /* 0x00 */
  Actor *owner; /* 0x04 */
  int beat;     /* 0x08 */
} HelperState;
/* from level_10 */
typedef struct L16Bell {
  char pad00[0x10];
  int chan;   /* 0x10 voice channel */
  int timer;  /* 0x14 */
  int period; /* 0x18 */
  char pad1C[0x14];
  int target; /* 0x30 index into g_pActorListBase */
} L16Bell;
/* from level_10 */
typedef struct L16Boss {
  char pad00[0x10];
  int chan;           /* 0x10 voice channel, -1 = none */
  PathData *path;     /* 0x14 path in use */
  int p18[3];         /* 0x18 */
  int p24;            /* 0x24 */
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
  int unk60;          /* 0x60 on the path */
  int prev;           /* 0x64 index into g_pActorListBase of the previous stage's gate */
  int wait;           /* 0x68 */
  char pad6C[0x8];
  int unk74;          /* 0x74 */
} L16Boss;
/* from level_10 */
/* from level_10 */
/* from level_10 */
extern int TweenActorAlongPath(Actor *actor, PathData *path, int *a, int *b, int c,
                         int d);
/* from level_10 */
typedef struct HopState {
  char pad00[0x20];
  int timer;   /* 0x20 */
  int heading; /* 0x24 knock-back heading */
  int time;    /* 0x28 knock-back time */
} HopState;
/* from level_10 */
typedef struct GuardPairState {
  int heading; /* 0x00 knock-back heading */
  int speed;   /* 0x04 */
  int flag;    /* 0x08 */
  int timer;   /* 0x0C */
  int partner; /* 0x10 index into g_pActorListBase, -1 for none */
} GuardPairState;
/* from level_10 */
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
/* from level_10 */
/* from level_10 */
#define ENTER_POSE_B(a, n)                                                     \
  (a)->mode = (n);                                                            \
  if ((a)->nextAnim != (n)) {                                                     \
    do {                                                                       \
    } while (0);                                                               \
    (a)->frameProgress = sixteen;                                                      \
    (a)->frameRate = sixteen;                                                      \
    (a)->anim = (a)->nextAnim;                                                   \
    (a)->nextAnim = (n);                                                          \
    (a)->frame = (a)->nextFrame;                                                   \
    (a)->nextFrame = 0;                                                            \
    StubActorHook(a);                                                          \
  }                                                                            \
  continue
/* from level_10 */
typedef struct L9Snowball {
  int vx;             /* 0x00 */
  int vy;             /* 0x04 */
  int vz;             /* 0x08 */
  int timer;          /* 0x0C */
  unsigned char kind; /* 0x10 */
} L9Snowball;
/* from level_10 */
typedef struct L9Bird {
  int home[3]; /* 0x00 perch */
  int timer;   /* 0x0C */
  int radius;  /* 0x10 */
  int angle;   /* 0x14 */
  int height;  /* 0x18 */
  int call;    /* 0x1C */
} L9Bird;
/* from level_10 */
extern void SteerActorTowardPoint(Actor *actor, int *target, int a, int b, int c,
                          int d);
/* from level_10 */
typedef struct L9Yak {
  int heading; /* 0x00 knock-back heading */
  int speed;   /* 0x04 */
  int rest;    /* 0x08 rest countdown after a charge */
  int unk0C;
  int timer;   /* 0x10 grazing timer */
} L9Yak;
/* from level_10 */
typedef struct L9Ram {
  int heading; /* 0x00 knock-back heading */
  int speed;   /* 0x04 */
  int rest;    /* 0x08 rest countdown, knock-back height step */
  int unk0C;
  int timer;   /* 0x10 grazing timer */
  int unk14;   /* 0x14 */
  int unk18;
  int unk1C;
  int hits;    /* 0x20 */
} L9Ram;
/* from level_10 */
typedef struct L9Thrower {
  PathData *path; /* 0x00 */
  int dist;       /* 0x04 */
  int heading;    /* 0x08 */
  int timer;      /* 0x0C */
  int wake;       /* 0x10 wake-up distance >> 10, 0 = default */
  int turn1;      /* 0x14 */
  int turn4;      /* 0x18 */
  int knock;      /* 0x1C knock-back heading */
  int speed;      /* 0x20 knock-back speed */
  int p24;        /* 0x24 */
  int p28;        /* 0x28 */
  int throwT;     /* 0x2C */
} L9Thrower;
/* from level_10 */
typedef struct {
  char pad00[0x38];
  int posX; /* 0x38 */
  int posY; /* 0x3C */
} GuardTarget;
/* from level_10 */
typedef struct GuardState370 {
  int mode;           /* 0x00 0 = patrol, 1 = hold near target */
  int timer04;        /* 0x04 */
  int lift;           /* 0x08 */
  int heading;        /* 0x0C */
  int speed;          /* 0x10 */
  int unk14;          /* 0x14 */
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
} GuardState370;
/* from level_10 */
typedef struct ShempState {
  int ang;             /* 0x00 heading << 4 */
  int unk04[3];        /* 0x04 */
  int timer;           /* 0x10 */
  int unk14;           /* 0x14 */
  int unk18;           /* 0x18 passed by address to SteerActorYawTowardSpyro */
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
/* from level_10 */
/* from level_10 */
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
/* from level_25 */
typedef struct L26Lobber {
  int timer;   /* 0x00 */
  int heading; /* 0x04 knock-back heading */
  int speed;   /* 0x08 knock-back speed */
  int reload;  /* 0x0C */
} L26Lobber;
/* from level_25 */
typedef struct L26Sink {
  int partner; /* 0x00 index into g_pActorListBase */
  int depth;   /* 0x04 */
} L26Sink;
/* from level_25 */
typedef struct L26Critter {
  char pad00[0x38];
  int knock; /* 0x38 knock-back heading */
  int speed; /* 0x3C knock-back speed */
} L26Critter;
/* from level_25 */
typedef struct L26Bob {
  int partner;        /* 0x00 index into g_pActorListBase */
  unsigned int phase; /* 0x04 */
  int unk08;
  int unk0C;
  int baseZ;          /* 0x10 */
  int big;            /* 0x14 */
} L26Bob;
/* from level_25 */
typedef struct L26Rider {
  int partner; /* 0x00 index into g_pActorListBase */
  int speed;   /* 0x04 */
  int lift;    /* 0x08 */
  int heading; /* 0x0C */
} L26Rider;
/* from level_25 */
typedef struct L26Escort {
  int leader;     /* 0x00 index into g_pActorListBase */
  int partner;    /* 0x04 index into g_pActorListBase, -1 = none */
  PathData *path; /* 0x08 */
  int p0C[3];     /* 0x0C */
  int timer;      /* 0x18 */
  int slot;       /* 0x1C place in the ring */
  int speed;      /* 0x20 */
} L26Escort;
/* from level_25 */
typedef struct L26Guard {
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
} L26Guard;
/* from level_25 */
typedef struct L26Rot {
  unsigned char r[3];
} L26Rot;
/* from level_25 */
#define ANIM_QUEUE(a, n)                                                       \
  (a)->mode = (n);                                                            \
  if ((a)->nextAnim != (n)) {                                                     \
    (a)->frameProgress = sixteen;                                                      \
    (a)->frameRate = sixteen;                                                      \
    (a)->nextAnim = (n);                                                          \
    (a)->nextFrame = 0;                                                            \
    StubActorHook(a);                                                          \
  }                                                                            \
  continue
/* from level_25 */
/* from level_25 */
typedef struct L25Grabber {
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
} L25Grabber;
/* from level_25 */
#define L25_UNLINK(st)                                                         \
  if ((st)->link >= 0) {                                                       \
    g_pActorListBase[(st)->link].subMode &= 0xFB;                                      \
  }
/* from level_25 */
#define L25_DROP(st)                                                           \
  *(st)->slot = 0;                                                             \
  L25_UNLINK(st)
/* from level_25 */
typedef struct L25Sentry {
  int speed; /* 0x00 retreat speed (case 54) */
  int step;  /* 0x04 */
  int dir;   /* 0x08 retreat heading */
  int link;  /* 0x0C index of its partner in g_pActorListBase, -1 = none */
  int timer; /* 0x10 */
} L25Sentry;
/* from level_25 */
typedef struct L25Lantern {
  int held;    /* 0x00 */
  int timer;   /* 0x04 */
  FxNode *fx;  /* 0x08 glow node */
  int pos[3];  /* 0x0C glow anchor */
  int sound;   /* 0x18 sound handle, -1 = none */
} L25Lantern;
/* from level_25 */
typedef struct L25Shot {
  int link;            /* 0x00 index of the actor it wakes on arrival */
  int vel[3];          /* 0x04 */
  int timer;           /* 0x10 */
  unsigned char *path; /* 0x14 */
  int init;            /* 0x18 1 = aim the barrel along the path */
} L25Shot;
/* from level_25 */
/* from level_25 */
/* from level_15 */
typedef struct RearState {
  int timer;   /* 0x00 idle-hop countdown */
  int t04;     /* 0x04 */
  int heading; /* 0x08 knockback heading */
  int speed;   /* 0x0C knockback speed */
  int spin;    /* 0x10 per-frame yaw while tumbling */
} RearState;
/* from level_15 */
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
  int unk28;      /* 0x28 */
  int camTimer;   /* 0x2C */
  int *alert;     /* 0x30 shared by the sentries of one group */
} SentryState;
/* from level_15 */
typedef struct {
  void *v;
} PtrV;
/* from level_15 */
#define AS_PTRV(sym) (((PtrV *)&(sym))->v)
/* from level_15 */
typedef struct TetherState {
  int timer;   /* 0x00 */
  int speed;   /* 0x04 knockback speed */
  int heading; /* 0x08 knockback heading */
  int link;    /* 0x0C walker to follow (index into g_pActorListBase), or -1 */
  int owner;   /* 0x10 actor whose loss frees this one */
  int mode;    /* 0x14 */
} TetherState;
/* from level_15 */
typedef struct LeadState {
  PathData *path; /* 0x00 */
  int moving;     /* 0x04 */
} LeadState;
/* from level_15 */
typedef struct KeeperState {
  int timer;   /* 0x00 knockback timer */
  int heading; /* 0x04 knockback heading */
  int count;   /* 0x08 idle countdown */
  int link;    /* 0x0C linked guard (index into g_pActorListBase), or -1 */
  int mode;    /* 0x10 command passed to the guard */
  int baseX;   /* 0x14 post position */
  int baseY;   /* 0x18 */
} KeeperState;
/* from level_15 */
typedef struct SentinelState {
  int timer;        /* 0x00 */
  int count;        /* 0x04 bolts fired this volley */
  int link;         /* 0x08 next guard in the chain, or -1 */
  PathData *path;   /* 0x0C */
  int flags;        /* 0x10 bit 0 alerted, 1 on patrol, 2 at end, 3 aim */
  int heading;      /* 0x14 */
  int range;        /* 0x18 */
  int knockTimer;   /* 0x1C */
  int knockHeading; /* 0x20 */
  int knockSpeed;   /* 0x24 */
} SentinelState;
/* from level_15 */
typedef struct ChainState {
  int pad00[3];
  int link;  /* 0x0C next in chain */
  int pad10[2];
  int stop;  /* 0x18 path node where it waits */
} ChainState;
/* from level_15 */
extern int WalkActorAlongWaypoints(Actor *actor, PathData *path, int a, int b, int c,
                         int d, int e, int f);
/* from level_15 */
extern int SteerActorAlongWaypoints(Actor *actor, PathData *path, int a, int b, int c,
                         int d, int e, int *f);
/* from level_15 */
/* from level_15 */
#define FACE4(a, h, s, t)                                                      \
  ((int (*)(Actor *, int, int, int))TurnActorYawTowardAngle)(a, h, s, t)
/* from level_15 */
#define RESET_IDLE(a)                                                          \
  (a)->mode = 0;                                                              \
  ANIM_RESET(a, 0);                                                            \
  continue
/* from level_3 */
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
/* from level_3 */
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
  int unk24[4];
  int mode;       /* 0x34 0 = post, 1 = chase, 2 = patrol */
  PathData *path; /* 0x38 */
  int pathDist;   /* 0x3C */
  int link;       /* 0x40 actor that frees the patrol (index into g_pActorListBase) */
  int camDone;    /* 0x44 */
} WardState;
/* from level_3 */
typedef struct Follower {
  PathData *path;    /* 0x00 */
  short hold;        /* 0x04 */
  short vz;          /* 0x06 */
  short unk08;       /* 0x08 */
  signed char spinX; /* 0x0A */
  signed char spinY; /* 0x0B */
  int pathDist;      /* 0x0C */
  int link;          /* 0x10 leader (index into g_pActorListBase) */
  int target[3];     /* 0x14 */
  int timer;         /* 0x20 */
} Follower;
/* from level_3 */
typedef struct GrazerState {
  char pad00[0x20];
  int speed; /* 0x20 knock-back speed */
  int knock; /* 0x24 knock-back heading */
  int wait;  /* 0x28 graze timer */
  int voice; /* 0x2C bleat timer */
} GrazerState;
/* from level_31 */
typedef struct L31Critter {
  char pad00[0x20];
  int speed; /* 0x20 knock-back speed */
  int knock; /* 0x24 knock-back heading */
  char pad28[0x4];
  int wait;  /* 0x2C pause timer */
} L31Critter;
/* from level_31 */
typedef struct L31Hatch {
  int pad00;
  int kind;     /* 0x04 0 = opens to charge or flame, else charge only */
  Actor *child; /* 0x08 released occupant */
  int handle;   /* 0x0C for SelectActorLodBand */
} L31Hatch;
/* from level_31 */
typedef struct L31Mount {
  int rider; /* 0x00 index into g_pActorListBase, -1 = none */
  int init;  /* 0x04 */
  int knock; /* 0x08 knock-back heading */
  int speed; /* 0x0C */
  int lift;  /* 0x10 */
} L31Mount;
/* from level_31 */
typedef struct L31Carrier {
  int hatch;   /* 0x00 index of its hatch in g_pActorListBase */
  Actor *held; /* 0x04 carried occupant */
  int timer;   /* 0x08 */
  int knock;   /* 0x0C knock-back heading */
  int speed;   /* 0x10 */
  int lift;    /* 0x14 */
} L31Carrier;
/* from level_31 */
typedef struct L31Barrel {
  short timer;  /* 0x00 */
  short blast;  /* 0x02 blast pushed Spyro */
  Actor *rider; /* 0x04 */
  int heading;  /* 0x08 run heading */
  int sink;     /* 0x0C sink speed */
  int link;     /* 0x10 index into g_pActorListBase, -1 = none */
  short handle; /* 0x14 for SelectActorLodBand */
} L31Barrel;
/* from level_31 */
typedef struct L31Debris {
  int vx;    /* 0x00 */
  int vy;    /* 0x04 */
  int vz;    /* 0x08 */
  int floor; /* 0x0C */
  unsigned char spinX; /* 0x10 */
  unsigned char spinY; /* 0x11 */
  unsigned char spinZ; /* 0x12 */
} L31Debris;
/* from level_31 */
#define BARREL_BURST(a, st)                                                    \
  {                                                                            \
    Actor *sp_ = g_pfnLevelSpawnActor(0x190, a);                                         \
    Actor *w_;                                                                 \
                                                                               \
    StopSoundVoicesByOwner(a, 1);                                                       \
    PlaySoundEffect(g_apActorMeshTable[(a)->type]->m_Sounds[3], sp_, 8, &sp_->sfxSlot);    \
    w_ = (st)->rider;                                                          \
    if (w_ != 0 && w_->type == 0x9A && w_->mode == 2) {                       \
      w_->subMode = 1;                                                           \
    }                                                                          \
    DespawnActorRecord(a);                                                          \
    continue;                                                                  \
  }
/* from level_31 */
typedef struct L31Thrower {
  int held;  /* 0x00 index of the carried actor in g_pActorListBase, -1 = none */
  int duck;  /* 0x04 Spyro charged: duck instead of throwing */
  int timer; /* 0x08 */
  int knock; /* 0x0C knock-back heading */
  int speed; /* 0x10 */
  int lift;  /* 0x14 */
} L31Thrower;
/* from level_8 */
typedef struct CatapultState {
  int angle;     /* 0x00 0..0x120 */
  int vel;       /* 0x04 */
  int link;      /* 0x08 actor to fling (index into g_pActorListBase), or -1 */
  Actor *flung;  /* 0x0C actor in flight */
  int timer;     /* 0x10 */
} CatapultState;
/* from level_8 */
typedef struct FlightState {
  int pos[3];          /* 0x00 landing point */
  unsigned char pad0C[2];
  unsigned char unk0E; /* 0x0E */
  unsigned char unk0F; /* 0x0F */
  unsigned char mode;  /* 0x10 */
} FlightState;
/* from level_8 */
typedef struct RunnerState {
  PathData *pathA; /* 0x00 */
  PathData *pathB; /* 0x04 */
  int knockTimer;  /* 0x08 */
  int knockSpeed;  /* 0x0C */
  int unk10;
  int heading;     /* 0x14 knockback heading */
  int timer;       /* 0x18 */
  int mode;        /* 0x1C */
  int unk20;
  int which;       /* 0x24 path in use, 1 or 2 */
  int range;       /* 0x28 in 1024s */
  int node;        /* 0x2C trigger node on either path */
  int cried;       /* 0x30 */
} RunnerState;
/* from level_8 */
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
/* from level_8 */
/* from level_34 */
typedef struct BoltFlight {
  int vel[3]; /* 0x00 */
  int timer;  /* 0x0C */
} BoltFlight;
/* from level_34 */
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
/* from level_34 */
typedef struct FlyPath {
  unsigned char count; /* 0x00 */
  unsigned char cur;   /* 0x01 */
  char pad02[4];
  short dir;           /* 0x06 +1 forward, -1 back */
} FlyPath;
/* from level_34 */
#define FLY_Z(p, i) (*(int *)((char *)(p) + ((i) << 4) + 0x10))
/* from level_34 */
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
  int snd;        /* 0x30 */
  FlyPath *cur;   /* 0x34 */
  FlyPath *ring;  /* 0x38 */
  FlyPath *pathA; /* 0x3C */
  FlyPath *pathB; /* 0x40 */
  int leg;        /* 0x44 */
} LootThiefState;
/* from level_34 */
/* from level_34 */
#define FP_X(p, i) (*(int *)((char *)(p) + ((i) << 4) + 0x8))
/* from level_34 */
#define FP_Y(p, i) (*(int *)((char *)(p) + ((i) << 4) + 0xC))
/* from level_34 */
#define FP_PT(p, i) ((int *)((char *)(p) + ((i) << 4) + 0x8))
/* from level_34 */
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
/* from level_34 */
/* from level_34 */
/* from level_34 */
typedef struct LootVortexState {
  short timer;          /* 0x00 */
  unsigned char phase;  /* 0x02 */
  unsigned char spark;  /* 0x03 0xFF = none */
  int snd;              /* 0x04 */
  int pad[6];           /* 0x08 trigger volume */
  int mode;             /* 0x20 */
  int key;              /* 0x24 actor index */
  int target;           /* 0x28 actor index */
  int seen;             /* 0x2C index into g_nCurrentWorldId, 0 = none */
} LootVortexState;
/* from level_34 */
/* from level_34 */

/* ==== Level 14 classes ==== */
extern int g_anCollisionContactPoint[2]; /* wall probe hit x, y */

/* Type 303. */
typedef struct L14State303 {
  unsigned char *link; /* 0x00 byte +1 cleared when the spawn fires */
  int flags;           /* 0x04 bit 0: spawned */
  int wait;            /* 0x08 */
  int timer;           /* 0x0C */
  int value;           /* 0x10 copied to the spawn's state +0x14 */
} L14State303;

/* Type 304. */
typedef struct L14State304 {
  void *path;   /* 0x00 */
  Actor *link;  /* 0x04 its state's flags bit 0 is cleared on release */
  int hold;     /* 0x08 */
  int pad0C[2]; /* 0x0C */
  short target; /* 0x14 pool index, < 0 = none */
} L14State304;

/* Type 302. */
typedef struct L14State302 {
  int snd;       /* 0x00 sound handle */
  int riding;    /* 0x04 */
  char *anchor;  /* 0x08 +0x18: distance reference */
  int spark;     /* 0x0C pool index of the spark target */
  int node;      /* 0x10 path node, -1 = none */
  int pts[3][3]; /* 0x14 node triangle */
} L14State302;

/* Type 301. */
typedef struct L14State301 {
  int target; /* 0x00 pool index of the actor it fires at, -1 = none */
  int flags;  /* 0x04 bit 0: woken, bit 1: Spyro in range */
  int timer;  /* 0x08 */
  int count;  /* 0x0C bolts fired */
} L14State301;

/* Type 227. */
typedef struct L14State227 {
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
} L14State227;

/* Type 305. */
typedef struct L14State305 {
  PathData *cur;   /* 0x00 path being walked */
  PathData *pathA; /* 0x04 */
  PathData *pathB; /* 0x08 */
  PathData *pathC; /* 0x0C */
  int mode;        /* 0x10 bits 0-1 path, 2 fleeing, 3 Spyro close, 4 hold,
                      5 restart, 6 pause, 7 reverse */
  int vel[3];      /* 0x14 */
  int timer;       /* 0x20 */
  int heading;     /* 0x24 */
  int pad28;       /* 0x28 */
  int speed;       /* 0x2C */
  int range;       /* 0x30 */
} L14State305;

#define PATH_END(p) (&(p)->nodes[(p)->count - 1].x)
#define PATH_CUR(p) (&(p)->nodes[(p)->cur].x)
#define NODE_PT(p) ((int *)((char *)(p) + ((p)->cur << 4) + 8))

extern int g_nSpyroMoveTargetYaw; /* camera yaw override */
typedef struct Cam5 {
  int w[5];
} Cam5;

/* Type 109. */
typedef struct L14State109 {
  int mode;        /* 0x00 bits 0-1 riding (1/2 = side), 2 to dest, 3 path0,
                      4 follow Spyro, 5 return along cur, 6 back down cur,
                      7 camera hold; 0x2C = carried */
  PathData *pathA; /* 0x04 */
  PathData *pathB; /* 0x08 */
  PathData *pathC; /* 0x0C */
  PathData *path0; /* 0x10 */
  PathData *cur;   /* 0x14 */
  int dest[3];     /* 0x18 */
  int rider;       /* 0x24 pool index */
  int bob;         /* 0x28 */
  int vz;          /* 0x2C */
} L14State109;

/* Turn toward `h`; walk at `d`, slowed by the remaining turn. */
#define STEER(a, h, d, k1, k2, sh)                                             \
  if (TurnActorYawTowardAngle(a, h, k1, k2, 1) != 0) {                                   \
    MoveActorAlongHeading(a, d, 0, 0x200, 0x20);                                       \
  } else {                                                                     \
    h = ((h) - (a)->rotZ) & 0xFF;                                             \
    if (h > 0x80) {                                                            \
      h -= 0x100;                                                              \
    }                                                                          \
    d -= ABS2(h) << (sh);                                                      \
    if (d > 0) {                                                               \
      MoveActorAlongHeading(a, d, 0, 0x200, 0x20);                                     \
    }                                                                          \
  }

/* ==== The function ==== */
#define one 1
#define spy (&SPY)
#define oneq 1
void Level14_UpdateActors(void) {
  Actor **actorList;
  Actor *actor;
  int dt;

  /* Scratch vectors, in first-use order. */
  int svA[3];
  int svB[3];
  int svC[3];
  int svD[3];
  int svE[3];
  int svF[3];
  int acc[3][3];
  int svG[3];
  int svH[3];
  int svI[3];
  int svJ1[3];
  int svJ[3];
  int x272[3];
  int x288[3];
  int fx[6];
  int col[3];
  int svK[3];
  int svL[3];
  int v376[3];
  int t392[3][3];
  int svM[3];
  int svT[3];
  int ps[6];
  int s496[3];
  int mat2[3][3];
  int s512[3];
  int s528[3];
  int vecB[3];
  int vecC[3];
  int vecD[3];

  /* Build this frame's update list. A long frame (delta >= 3) gets an extra
   * pass. */
  BuildActiveActorList();
  TickGemCutsceneActorAnims(g_apSaveMenuAnimActorList, 0);

  if (g_nFrameStep >= 3) {
    TickGemCutsceneActorAnims(g_apSaveMenuAnimActorList, g_nFrameStep == 3 ? 0x80000001 : 0x80000000);
  }

  actorList = g_apSaveMenuAnimActorList;

  /* Visit every actor on the list; dormant ones (state >= 0x80) are skipped. */
  while (actor = *actorList++) {
    if (actor->mode >= 0x80) {
      continue;
    }

    g_nActorAnimDone = actor->animFlags & 2;
    g_nActorAnimNewFrame = actor->animFlags & 1;
    dt = g_nFrameStep;
    g_nActorFrameDelta = dt;

    switch (actor->type) {
    /* Exit drop (DropState). */
    case 9: {
      DropState *st = actor->state;
      int dist = ApproxDist2D(&actor->posX, &CAM.posX);
      int i;

      if (actor->mode != 0 ? dist < 0x2800 : dist < 0x2400) {
        if (actor->mode == 0) {
          actor->posZ += 0x600;
          SpawnNameLetterActors(actor, 0);
          actor->posZ -= 0x600;
        }
        actor->mode = oneq;
      } else {
        actor->mode = 0;
      }

      dist = ApproxDist2D(&actor->posX, &SPY.posX);

      for (i = 0; i < g_nFrameStep; i++) {
        int tick = ++st->tick;

        if (dist < 0x4000) {
          if (!(tick & 3)) {
            g_pfnLevelEmitParticles(1, 0x15, &actor->posX, (void *)tick);
          }
        } else if ((tick & 0xF) < 8 && !(tick & 3)) {
          g_pfnLevelEmitParticles(1, 0x4C, &actor->posX, (void *)tick);
        }
      }

      if (dist < 0x400) {
        char *base = (char *)&g_nSpyroWorldPosZ;
        int top = *(int *)base;
        int z = actor->posZ;
        int drop = top - z;

        if ((unsigned int)(drop - 0x201) < 0x3DFF) {
          int depth = 0x2000;

          if (st->depth != 0) {
            depth = st->depth;
          }

          if (g_nSpyroState == 0x11 || top + 0xC00 < z + depth) {
            if (g_abHudCounterDisplayState == 2) {
              RefreshHudLevelGemCounter();
            }
            g_dwSpyroRequestMask = 0x80008000;
            g_pScriptedCameraActor = actor;
            g_dwSpyroTriggerEventFlags |= 0x800;
            g_nSpyroRequestTargetZ = *(int *)((char *)actor + 0x14) + 0x4000;
            CopyVector((int *)(base + 0x174), &actor->posX);

            if (depth < drop) {
              int yaw = g_nCameraEulerPitch & 0xFFF;

              if (yaw > 0x800) {
                yaw -= 0x1000;
              }
              if (yaw < -0x200) {
                g_abLevelGemPercent[g_nLevelIntroIndex] = 1;
                g_nCdStreamState = 0;
                g_nPreserveMobIdx = -1;
                g_nScriptedRespawnFlag = one;
                g_nLevelTransitionTallyActive = 1;
                g_nLevelTransitionPhase = 0;
                g_nGamestate = 1;
                g_nGamePaused = 1;
                g_nScriptedMobTag = g_nActiveLevelId;
                g_nCurrentLevelId = g_nActiveLevelId / 10 * 10;
                g_nCameraNextMode = 0x80000012;
                g_dwSpyroRequestMask = 0;
                ResetSpyroEntity(0);
                g_nSpyroFallTrackingFlag = 0;
                g_nSpyroFlameBreathTimer = 0;
              }
            }
          }
        }
      }

      break;
    }

    /* Tethered follower (FollowState): rides a parent actor until the parent
     * goes dormant, then takes the parent's transform over and re-points the
     * other followers of that parent at itself. */
    case 13: {
      FollowState *st = actor->state;
      Actor *parent = &g_pActorListBase[st->parent];

      if (st->detached == 0) {
        if (st->trackPos != 0) {
          SubtractVector(st->pos, st->pos, &parent->posX);
          actor->updateDist = 0xFF;
        } else {
          actor->updateDist = 0x20;
        }

        if (st->active != 0 &&
            (parent->mode >= 0x80 || (parent->unk53 & 0x80))) {
          Actor *scan;
          int self;

          parent->unk53 = actor->unk53;
          DespawnActorRecord(actor);

          if (parent->mode < 0x80) {
            DespawnActorRecord(parent);
          }

          /* Take the parent's whole record over (gcc's block_move_loop). */
          *actor = *parent;
          InsertActorIntoSpatialGrid(actor);

          actor->drawn = 1;
          self = actor - g_pActorListBase;
          parent = g_pActorListBase;

          for (; parent < g_pActorPoolDynBase; parent++) {
            if (parent != actor && parent->mode < 0x80 && parent->type == 13) {
              FollowState *other = parent->state;

              if (other->parent == st->parent) {
                other->parent = self;
              }
            }
          }

          actor->mode = 0;
          ANIM_RESET(actor, 0);
        } else {
          st->detached = 1;
          goto follow_carry;
        }
      } else {
      follow_carry:
        if (st->trackPos != 0) {
          CopyVector(&actor->posX, &parent->posX);
        }

        if (st->release != 0) {
          /* Released: drift away and pop. */
          if (TickCountdownByFrameDelta(&st->release, 4) != 0) {
            int *vel = st->pos;

            if (st->trackPos != 0) {
              AddVector(vel, vel, &actor->posX);
            } else {
              CopyVector(&actor->posX, &parent->posX);
            }

            st->pos[2] += 0x400;

            if (st->unk24 != 0) {
              SpawnActorDeathDrop(actor, 4, 0, 0);
            } else if (FindGroundHeightBelow(st->pos, 0x1400) != 0) {
              st->pos[2] -= 0x400;
              SpawnActorDeathDrop(actor, 2, 0, st->pos);
            } else {
              SpawnActorDeathDrop(actor, 1, 0, 0);
            }
            MarkActorCommittedKill(actor);
            *(volatile int *)&st->count <<= 7;
            g_nSfxOverrideFlags = 2;
            g_nSfxOverridePitch.v =
                *(unsigned short *)(g_pLevelSampleBankEntries + g_pLevelSampleBankHeader[0x37] * 20 + 0xA) +
                st->count;
            PlaySoundEffect(g_pLevelSampleBankHeader[0x37], actor, 8, &actor->sfxSlot);
            DespawnActorRecord(actor);
            continue;
          }
        } else if (actor->subMode != 0 || parent->mode >= 0x80) {
          st->release = 9;
          st->count = 1;

          for (parent = g_pActorListBase; parent < actor; parent++) {
            if (parent->mode < 0x80 && parent->type == 13 &&
                *(int *)parent->state == st->parent) {
              st->release += 9;
              st->count += 1;
            }
          }

          if (st->count == 1) {
            g_nSfxOverrideFlags = 2;
            g_nSfxOverridePitch.v =
                *(unsigned short *)(g_pLevelSampleBankEntries + g_pLevelSampleBankHeader[0x37] * 20 + 0xA);
            PlaySoundEffect(g_pLevelSampleBankHeader[0x37], actor, 8, &actor->sfxSlot);
          }
        }
      }

      break;
    }

    /* Butterfly (ChaseState). */
    case 16: {
      ChaseState *st = (ChaseState *)actor->state;

      if (SPY.u164 < 3 && ApproxDist2D(&actor->posX, &SPY.posX) < 2400) {
        int actorZ = actor->posZ - actor->floorOffset;

        if (ABS2(actorZ - SPY.posZ) < 1800) {
          if (g_pLevelOverlayInitRecord == 0) {
            if (SPY.u164 >= 0) {
              g_pLevelOverlayInitRecord = g_pfnLevelSpawnActor(120, actor);

              PlayActorMeshVoice(g_pLevelOverlayInitRecord, 0, 0);

              SPY.u164 = 1;

              DespawnActorRecord(actor);
              break;
            }
          } else if (g_pLevelOverlayInitRecord->subMode != 99) {
            ChaserState *cs = (ChaserState *)g_pLevelOverlayInitRecord->state;

            if (cs->owner == 0) {
              if (WrapAngleDelta8(SPY.bodyRotZ,
                                ArcTan2(actor->posX - SPY.posX,
                                              actor->posY - SPY.posY, 0)) < 0) {
                st->dir = 1;
              } else {
                st->dir = -1;
              }

              g_pLevelOverlayInitRecord->subMode = 0;
              cs->owner = actor;
              cs->timer = RandomIntInRange(150, 210);

              actor->mode = 1;
            }
          }
        }
      }

      if (actor->unk50 & 0x80)
        break;

      switch (actor->mode) {
      case 0: {
        if (TickCountdownByFrameDelta(&st->t14, 2) == 0 || actor->drawn) {
          int refHeight, rotated, moved;

          if (TickCountdownByFrameDelta(&st->t12, 1) != 0) {
            int r = RandomSignedInRange(30, 90);
            if (GetRandomU32() & 1) {
              r = -r;
            }

            st->heading = WrapAddAngle8(st->heading, r);
            st->t12 = RandomIntInRange(60, 140);
          }

          if (TickCountdownByFrameDelta(&st->t13, 1) != 0) {
            st->vz = RandomSignedInRange(10, 25);
            st->t13 = RandomIntInRange(80, 140);
          }

          if (st->vz >= 128) {
            actor->posZ = actor->posZ + (st->vz - 256);
          } else {
            actor->posZ = actor->posZ + st->vz;
          }

          refHeight = actor->posZ - ProbeActorGroundHeight(actor);

          if (refHeight < 300) {
            st->vz = RandomIntInRange(10, 25);
            st->t13 = RandomIntInRange(80, 140);
          }

          if (1024 < refHeight && refHeight < 2048) {
            st->vz = -RandomIntInRange(10, 25);
            st->t13 = RandomIntInRange(80, 140);
          }

          rotated = TurnActorYawTowardAngle(actor, st->heading, 4, 0xA, 1);
          moved = MoveActorAlongHeading(actor, 30, 0, 50, 1);

          if (rotated && moved) {
            st->heading = WrapAddAngle8(st->heading, RandomIntInRange(98, 158));
            st->t12 = RandomIntInRange(30, 80);
          }

          if (ApproxDist2D(&actor->posX, &st->tgtX) > 2000) {
            int angle = ArcTan2(st->tgtX - actor->posX,
                                      st->tgtY - actor->posY, 0);

            st->heading = angle;
            st->t12 = 40;
          }
        } else {
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      case 1: {
        SteerActorAroundPoint(actor, &SPY.posX, 0x320,
                      WrapAddAngle8(ArcTan2(actor->posX - SPY.posX,
                                                  actor->posY - SPY.posY, 0),
                                    st->dir * 8),
                      8, 0x82, 0xE, 0x80, 0xFF, 0xFF, 0, 0, 0);

        if (g_pLevelOverlayInitRecord == 0 || g_pLevelOverlayInitRecord->subMode == 99) {
          actor->mode = 0;
        } else {
          int refHeight;

          if (TickCountdownByFrameDelta(&st->t13, 1)) {
            st->vz = RandomSignedInRange(5, 20);
            st->t13 = RandomIntInRange(40, 80);
          }

          if (st->vz >= 128) {
            actor->posZ = (actor->posZ) + (st->vz - 256);
          } else {
            actor->posZ = actor->posZ + st->vz;
          }

          refHeight = actor->posZ - ProbeActorGroundHeight(actor);

          if (refHeight < 200) {
            st->vz = RandomIntInRange(5, 20);
            st->t13 = RandomIntInRange(40, 80);
          }

          if (1000 < refHeight && refHeight < 2048) {
            st->vz = -RandomIntInRange(5, 20);
            st->t13 = RandomIntInRange(40, 80);
          }
        }
        break;
      }
      }

      break;
    }

    /* Egg thief (ThiefState). */
    case 33: {
      ThiefState *st;

      do {
      } while (0);
      st = actor->state;

      if ((actor->flags & 0xB0000) != 0 && actor->mode != 10) {
        st->knock = 0x96;
        if ((actor->flags & 0x20000) != 0) {
          st->knock = 0xE1;
        }
        st->heading = StepAndLerpAngle8(
            ArcTan2(actor->posX - spy->posX, actor->posY - spy->posY, 0),
            spy->bodyRotZ, 0x20, 0x40);
        if (actor->unk53 == 0xFF) {
          actor->unk53 = 0xF;
          SpawnActorDeathDrop(actor, 3, 0, 0);
          MarkActorCommittedKill(actor);
        }
        actor->mode = 10;
        continue;
      }

      if (actor->unk53 != 0xFF && st->egg == 0 && actor->mode != 10) {
        st->egg = g_pfnLevelSpawnActor(0x22, actor);
        st->egg->unk47 = 0;
        *(Actor **)st->egg->state = actor;
      }

      if (TickCountdownByFrameDelta(&st->voice, 4) != 0 &&
          (actor->mode == 1 || actor->mode == 0x28)) {
        int k;

        st->voice = (int)GetRandomU32() % 3 + 4;
        k = (int)GetRandomU32() % 5;
        while (k == st->lastVoice) {
          k = (int)GetRandomU32() % 5;
        }
        PlayActorMeshVoice(actor, k, 0);
        st->lastVoice = k;
      }

      if (actor->nextAnim == 4) {
        if (actor->nextFrame == 2 && (GetRandomU32() & 3) == 0 &&
            GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[6]) == 0) {
          PlayActorMeshVoice(actor, 6, 0);
        }
        if (actor->nextFrame == 0xC && (GetRandomU32() & 3) == 0 &&
            GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[5]) == 0) {
          PlayActorMeshVoice(actor, 5, 0);
        }
      }

      switch (actor->mode) {
      case 0:
        actor->unk47 = 2;
        if (st->mode == 0) {
          TrackEntityYawTowardSpyro(actor, 6, 0, 0);
          if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x1C00) {
            st->landed = 0;
            ANIM_GO(actor, 1);
            actor->mode = 1;
            continue;
          }
        } else if (st->mode == 1) {
          int spd = 0x154;

          spd -= ApproxDist2D(&actor->posX, &spy->posX) >> 5;
          if (spd < 0x8C) {
            spd = 0x8C;
          }
          if (spd > 0xF0) {
            spd = 0xF0;
          }
          if (actor->subMode == 0) {
            int home = ArcTan2(st->path->nodes[0].x - actor->posX,
                                     st->path->nodes[0].y - actor->posY, 0);

            if (ApproxDist2D(&actor->posX, &st->path->nodes[0].x) < 0x1000) {
              TurnActorYawTowardAngle(actor,
                            ArcTan2(actor->posX - spy->posX,
                                          actor->posY - SPY.posY, 0),
                            8, 0, 0);
              MoveActorAlongHeading(actor, spd, 0, 0, 5);
            } else {
              int lim = 10;
              int r;
              int a = ArcTan2(
                  st->path->nodes[st->path->cur].x - spy->posX,
                  st->path->nodes[st->path->cur].y - SPY.posY, 0);
              int b = ArcTan2(actor->posX - st->path->nodes[0].x,
                                    actor->posY - st->path->nodes[0].y, 0);

              if (actor->nextAnim == 4) {
                lim = 0x28;
              }
              if (AbsAngleDelta8(a, b) > lim &&
                  ApproxDist2D(&actor->posX, &spy->posX) < 0x3000) {
                r = SteerActorAroundPoint(actor, &st->path->nodes[0].x, 0x1800, a, 5,
                                  spd, 0xE, 0x14, 0xFF, 0xFF, 0, 0, 4);
              }
              if (r > 0x5A) {
                st->stuck++;
              } else {
                st->stuck = 0;
              }
              if (r < 0x1E) {
                TrackEntityYawTowardSpyro(actor, 6, 0, 0);
                ANIM_GO(actor, 4);
              } else {
                ANIM_GO(actor, 1);
              }
              if (st->stuck >= 0x1E &&
                  AbsAngleDelta8(home, ArcTan2(spy->posX - actor->posX,
                                                    spy->posY - actor->posY,
                                                    0)) > 0x20) {
                actor->subMode = 1;
              }
            }
          } else {
            int a = ArcTan2(st->path->nodes[0].x - spy->posX,
                                  st->path->nodes[0].y - SPY.posY, 0);
            int b = ArcTan2(st->path->nodes[0].x - actor->posX,
                                  st->path->nodes[0].y - actor->posY, 0);

            TurnActorYawTowardAngle(actor, a, 8, 0, 0);
            MoveActorAlongHeading(actor, spd, 0, 0, 5);
            if (AbsAngleDelta8(a, b) > 0x50) {
              actor->subMode = 0;
            }
            if (ApproxDist2D(&actor->posX, &st->path->nodes[0].x) > 0x1800) {
              actor->subMode = 0;
            }
          }
        } else if (st->mode == 2) {
          ANIM_GO(actor, 4);
          TrackEntityYawTowardSpyro(actor, 4, 0, 0);
          if (ApproxDist2D(&spy->posX, &st->path->nodes[6].x) < 0x1800) {
            actor->mode = 0x19;
            continue;
          }
          if (ApproxDist2D(&actor->posX, &spy->posX) < 0x1C00) {
            actor->mode = 0x14;
            continue;
          }
        } else if (st->mode == 3) {
          if (ApproxDist2D(&actor->posX, &spy->posX) < st->range << 10 &&
              ABS2(actor->posZ - actor->floorOffset - SPY.posZ) < 0x708 &&
              (st->needLos == 0 || CheckSpyroLineOfSightTo(&actor->posX) != 0)) {
            st->node = FindFarthestPathPointFromSpyro(st->path);
            st->speed = 0;
            if (st->node - st->path->reversed != st->path->cur) {
              st->turned = 0;
              if (ApproxDist2D(&actor->posX,
                                &st->path->nodes[st->path->cur].x) < 0x100) {
                st->path->cur = (st->path->cur + st->path->reversed +
                                 st->path->count) %
                                st->path->count;
              }
              actor->mode = 0x1E;
              continue;
            }
            ANIM_GO(actor, 4);
            TrackEntityYawTowardSpyro(actor, 6, 0, 0);
            break;
          }
          ANIM_GO(actor, 4);
          TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        } else if (st->mode == 4) {
          if (IsPointInRotatedBox(&SPY.posX, st->area) != 0) {
            actor->mode = 0x28;
            continue;
          }
          ANIM_GO(actor, 4);
          TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        } else if (st->mode == 5) {
          if (st->dist == 0) {
            st->dist = ApproxDist2D(&actor->posX,
                                     &st->path->nodes[st->path->cur].x);
          }
          if (ApproxDist2D(&actor->posX, &spy->posX) < 0x2000) {
            int d = ArcTan2(spy->posX - st->path->nodes[st->path->cur].x,
                                  SPY.posY - st->path->nodes[st->path->cur].y, 0);
            int b = ArcTan2(actor->posX - st->path->nodes[st->path->cur].x,
                                  actor->posY - st->path->nodes[st->path->cur].y,
                                  0) -
                    0x80;

            d = (d - b) & 0xFF;

            if (d > 0x80) {
              d -= 0x100;
            }
            if (ABS(d) > 0x30) {
              actor->mode = 0x32;
              continue;
            }
          }
        } else if (st->mode == 6) {
          actor->mode = 0x3C;
          continue;
        }
        break;

      case 1: {
        int spd = 0x154;

        spd -= ApproxDist2D(&actor->posX, &SPY.posX) >> 5;
        if (spd < 0x46) {
          spd = 0x46;
        }
        if (spd > 0xDC) {
          spd = 0xDC;
        }
        if (PivotActorAlongPath(actor, st->path, 0x73A, spd, 0xC8, 0xA, 0x80,
                          actor->group, 0) == 0x104) {
          st->vz = 0x12C;
          actor->mode = 2;
          continue;
        }
        break;
      }

      case 2:
        actor->unk47 = 0;
        TurnActorYawTowardAngle(actor,
                      ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                                    st->path->nodes[st->path->cur].y - actor->posY,
                                    0),
                      6, 0, 0);
        MoveActorAlongHeading(actor, 0x96, 0, 0, 1);
        if (ApproxDist2D(&actor->posX, &st->path->nodes[st->path->cur].x) <
            0x100) {
          actor->mode = 3;
          continue;
        }
        actor->posZ += st->vz;
        st->vz -= 0x14;
        break;

      case 3: {
        int g = ProbeActorGroundHeight(actor);

        ANIM_GO(actor, 4);
        if (st->landed == 0) {
          int z = actor->posZ + st->vz;

          if (g >= z) {
            actor->posZ = g;
            st->landed = 1;
            break;
          }
          actor->posZ = z;
          st->vz -= 0x14;
          if (st->vz < -0xC8) {
            st->vz = -0xC8;
          }
        } else {
          int k;
          int h;

          TrackEntityYawTowardSpyro(actor, 4, 0, 0);
          if (ApproxDist2D(&spy->posX, &st->path->nodes[4].x) <
              ApproxDist2D(&spy->posX, &st->path->nodes[5].x)) {
            k = 4;
          } else {
            k = 5;
          }
          h = ArcTan2(st->path->nodes[k].x - actor->posX,
                            st->path->nodes[k].y - actor->posY, 0);
          if (ApproxDist2D(&actor->posX, &st->path->nodes[k].x) > 0x100) {
            if (TurnActorYawTowardAngle(actor, h, 8, 0x14, 1) != 0) {
              MoveActorAlongHeading(actor, 0x96, 0, 0, 5);
            }
          } else if (ApproxDist2D(&spy->posX, &st->path->nodes[6].x) < 0x800 ||
                     ApproxDist2D(&spy->posX, &st->path->nodes[7].x) < 0x800) {
            st->vz = 0x96;
            actor->mode = 4;
            continue;
          }
        }
        break;
      }

      case 4: {
        int g;

        TurnActorYawTowardAngle(actor,
                      ArcTan2(st->path->nodes[0].x - actor->posX,
                                    st->path->nodes[0].y - actor->posY, 0),
                      6, 0, 0);
        if (ApproxDist2D(&actor->posX, &st->path->nodes[0].x) > 0x100) {
          MoveActorAlongHeading(actor, 0x82, 0, 0, 1);
        }
        actor->posZ += st->vz;
        st->vz -= 0x14;
        g = ProbeActorGroundHeight(actor);
        if (st->vz < 0) {
          int z = actor->posZ + st->vz;

          if (g >= z) {
            actor->posZ = g;
            st->path->cur = 0;
            ANIM_GO(actor, 0);
            actor->mode = 0;
            continue;
          }
          actor->posZ = z;
          st->vz -= 0x14;
          if (st->vz < -0xC8) {
            st->vz = -0xC8;
          }
        }
        break;
      }

      case 10:
        if (st->egg != 0) {
          g_dwSpyroRequestMask = 0x80002000;
          if (actor->nextAnim == 3 && actor->nextFrame >= 5) {
            st->egg->mode = 1;
            st->egg->unk47 = 4;
            st->egg = 0;
          }
        }
        if (st->knock != 0) {
          MoveActorAlongAngle(actor, st->heading, st->knock, 0, 0x12C, 5);
          st->knock -= 0x10;
          if (st->knock < 0) {
            st->knock = 0;
          }
        }
        ANIM_GO(actor, 3);
        if (g_nActorAnimDone == 0) {
          break;
        }
        RefreshActorDerivedState(actor, 4);
        BurstActorWithSound(actor, 0x18);
        DespawnActorRecord(actor);
        continue;

      case 0x14:
        ANIM_GO(actor, 1);
        if (PivotActorAlongPath(actor, st->path, 0x100, 0xF0, 0, 8, sixteen, 0xFF,
                          5) == 0x103) {
          actor->mode = 0x15;
          continue;
        }
        break;

      case 0x15:
        if (ApproxDist2D(&spy->posX, &st->path->nodes[6].x) < 0x1800) {
          actor->mode = 0x19;
          continue;
        }
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x2000) {
          actor->mode = 0x16;
          continue;
        }
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        ANIM_GO(actor, 4);
        break;

      case 0x16:
        ANIM_GO(actor, 1);
        if (PivotActorAlongPath(actor, st->path, 0x100, 0xDA, 0, 0xA, 0x30, 0xFF,
                          5) == 0x101) {
          actor->mode = 0x17;
          continue;
        }
        break;

      case 0x17: {
        int d = ApproxDist2D(&actor->posX, &spy->posX);

        ANIM_GO(actor, 4);
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (d < 0x2000 && SPY.posZ - actor->posZ >= -0xC7) {
          st->vz = 0x96;
          st->landed = 0;
          actor->mode = 0x18;
          continue;
        }
        break;
      }

      case 0x18: {
        int sp = 1;

        if (st->landed != 0) {
          sp = 5;
        }
        ANIM_GO(actor, 1);
        if (TurnActorYawTowardAngle(actor,
                          ArcTan2(st->path->nodes[2].x - actor->posX,
                                        st->path->nodes[2].y - actor->posY, 0),
                          8, 0x1E, 1) == 0) {
          break;
        }
        MoveActorAlongHeading(actor, 0xFA, 0, 0, sp);
        if (ApproxDist2D(&actor->posX, &st->path->nodes[2].x) < 0x100) {
          st->path->cur = 3;
          actor->mode = 0x15;
          continue;
        }
        if (st->landed == 0) {
          int g = ProbeActorGroundHeightRaised(actor, 0x1800);

          actor->posZ += st->vz;
          st->vz -= 0x14;
          if (st->vz < 0 && actor->posZ - g < ABS(st->vz)) {
            st->landed = 1;
          }
        }
        break;
      }

      case 0x19:
        CopyVector(&actor->posX, &st->path->nodes[0].x);
        st->path->cur = 1;
        SnapActorToGround(actor);
        actor->mode = 0x17;
        continue;

      case 0x1E: {
        int top = 0xCD;
        int next;
        int dCur;
        int dNext;
        int r;

        ANIM_GO(actor, 1);
        if (st->path->nodes[st->path->cur].unk0C > 0) {
          top = st->path->nodes[st->path->cur].unk0C;
        }
        if (st->speed < top) {
          st->speed += 0x1E;
          if (top < st->speed) {
            st->speed = top;
          }
        } else if (top < st->speed) {
          st->speed -= 0x1E;
          if (st->speed < top) {
            st->speed = top;
          }
        }
        next = (st->path->cur + st->path->reversed + st->path->count) %
               st->path->count;
        dCur = ApproxDist2D(&actor->posX, &st->path->nodes[st->path->cur].x);
        dNext = ApproxDist2D(&actor->posX, &st->path->nodes[next].x);
        if (dCur > 0x190 && dNext > 0x190) {
          int aCur = ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                                   st->path->nodes[st->path->cur].y - actor->posY,
                                   0);
          int aNext = ArcTan2(st->path->nodes[next].x - actor->posX,
                                    st->path->nodes[next].y - actor->posY, 0);
          int aSpy =
              ArcTan2(spy->posX - actor->posX, spy->posY - actor->posY, 0);
          int dA = AbsAngleDelta8(aCur, aSpy);
          int dB = AbsAngleDelta8(aNext, aSpy);

          if (st->turned == 0 && ABS2(dA - dB) < 0x28) {
            st->turned = 1;
            st->path->reversed = -st->path->reversed;
          } else if (dA < dB && ABS2(dA - dB) >= 0x1D && dB > 0x3C &&
                     AbsAngleDelta8(actor->rotZ,
                                   ArcTan2(spy->posX - actor->posX,
                                                 spy->posY - actor->posY,
                                                 0)) < 0x3C) {
            st->path->reversed = -st->path->reversed;
            st->path->cur = next;
          }
        }
        r = PivotActorAlongPath(actor, st->path, 0x100, st->speed, 0, 8, 0x1E, 0xFF, 5);
        if (r & 0x100) {
          if ((r & ~0x100) ==
              (st->node - st->path->reversed) % st->path->count) {
            actor->mode = 0;
            continue;
          }
          st->node = FindFarthestPathPointFromSpyro(st->path);
        }
        break;
      }

      case 0x28:
        ANIM_GO(actor, 1);
        if (actor->posZ < 0x800) {
          CopyVector(&actor->posX, &st->path->nodes[st->path->cur].x);
          SnapActorToGround(actor);
        }
        st->speed = 0xB4;
        if (st->path->nodes[st->path->cur].unk0C > 0) {
          st->speed = st->path->nodes[st->path->cur].unk0C;
        }
        if (PivotActorAlongPath(actor, st->path, 0x100, st->speed, 0, 8, 0x20, 0xFF,
                          5) == 0x10B) {
          st->vz = 0x96;
          actor->mode = 0x29;
          continue;
        }
        break;

      case 0x29: {
        int g = ProbeActorGroundHeight(actor);
        int d = ApproxDist2D(&actor->posX, &st->path->nodes[st->path->cur].x);

        if (actor->posZ < 0x800) {
          CopyVector(&actor->posX, &st->path->nodes[st->path->cur].x);
          SnapActorToGround(actor);
        }
        TurnActorYawTowardAngle(actor,
                      ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                                    st->path->nodes[st->path->cur].y - actor->posY,
                                    0),
                      6, 0, 0);
        if (d > 0x100) {
          MoveActorAlongHeading(actor, 0x96, 0, 0, 1);
        }
        if (d < 0x100 && actor->posZ == g) {
          if (++st->path->cur == 0xD) {
            actor->mode = 0x2A;
            continue;
          }
          st->vz = 0x96;
          break;
        }
        actor->posZ += st->vz;
        st->vz -= 0x14;
        if (st->vz < -0xC8) {
          st->vz = -0xC8;
        }
        if (g >= actor->posZ + st->vz) {
          actor->posZ = g;
        }
        break;
      }

      case 0x2A:
        ANIM_GO(actor, 4);
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        if (actor->posZ < 0x800) {
          CopyVector(&actor->posX, &st->path->nodes[st->path->cur].x);
          SnapActorToGround(actor);
        }
        {
          char *base = (char *)&g_nSpyroWorldPosZ;

          if (*(int *)base - st->path->nodes[10].z < -0x5DC &&
              ApproxDist2D(&actor->posX, (int *)(base - 8)) < 0x1800) {
            st->vz = 0x1A4;
            st->path->cur = 9;
            actor->mode = 0x2B;
            continue;
          }
        }
        break;

      case 0x2B: {
        int g = ProbeActorGroundHeight(actor);
        int d = ApproxDist2D(&actor->posX, &st->path->nodes[st->path->cur].x);

        ANIM_GO(actor, 1);
        TurnActorYawTowardAngle(actor,
                      ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                                    st->path->nodes[st->path->cur].y - actor->posY,
                                    0),
                      6, 0, 0);
        if (d > 0x100) {
          MoveActorAlongHeading(actor, 0x96, 0, 0, 1);
        }
        if (d < 0x100 && actor->posZ == g) {
          st->path->reversed = -st->path->reversed;
          st->path->cur = 8;
          actor->mode = 0x2C;
          continue;
        }
        actor->posZ += st->vz;
        st->vz -= 0x14;
        if (st->vz < -0xC8) {
          st->vz = -0xC8;
        }
        if (g >= actor->posZ + st->vz) {
          actor->posZ = g;
        }
        break;
      }

      case 0x2C:
        ANIM_GO(actor, 1);
        st->speed = 0xC8;
        if (actor->posZ < 0x800) {
          CopyVector(&actor->posX, &st->path->nodes[st->path->cur].x);
          SnapActorToGround(actor);
        }
        if (PivotActorAlongPath(actor, st->path, 0x100, st->speed, 0, 8, 0x20, 0xFF,
                          5) == st->path->count + 0xFF) {
          st->path->reversed = -st->path->reversed;
          st->path->cur = 1;
          actor->mode = 0;
          continue;
        }
        break;

      case 0x32:
        ANIM_GO(actor, 1);
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x2000) {
          int a = (ArcTan2(spy->posX - st->path->nodes[st->path->cur].x,
                                 SPY.posY - st->path->nodes[st->path->cur].y,
                                 0) +
                   0x80) &
                  0xFF;

          SteerActorAroundPoint(actor, &st->path->nodes[0].x, st->dist, a, 8,
                        st->speed, 0xF, 0x40, 0xFF, 0xFF, 0, 0, 0);
          a = (a - ArcTan2(actor->posX - st->path->nodes[st->path->cur].x,
                                 actor->posY - st->path->nodes[st->path->cur].y,
                                 0)) &
              0xFF;
          if (a > 0x80) {
            a -= 0x100;
          }
          if (ABS(a) < 0x20) {
            ENTER_POSE(actor, 0);
          }
          break;
        }
        ENTER_POSE(actor, 0);

      case 0x3C:
        ANIM_GO(actor, 4);
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        break;
      }

      if (st->egg != 0) {
        RefreshActorDerivedState(actor, 4);
        GetActorAttachPoint(actor, 0, &st->egg->posX);
      }
      RefreshActorDerivedState(actor, 1);
      break;
    }

    /* Vortex pad (VortexState): spins Spyro up and out of the level. */
    case 34: {
      VortexState *st = actor->state;
      unsigned int phase = actor->mode;

      if (phase != 0 && phase < 5) {
        g_dwSpyroRequestMask = 0x80002000;
        if (g_nSpyroState != 0 && g_nSpyroState != 0xC) {
          actor->mode = 5;
        }
      }

      if ((unsigned int)(actor->mode - 2) < 3) {
        AimCameraStickTowardPoint(&actor->posX);
        g_pfnLevelEmitParticles(1, 0xC, actor, (int *)0x602080);
      }

      switch (actor->mode) {
      case 1:
        g_nGameplayBlocked = 1;
        VectorToSVectorShr2(st->centre, &actor->posX);
        st->angle =
            ArcTan2_8bit(actor->posX - SPY.posX, actor->posY - SPY.posY) +
            0x40;
        st->radius = 0;
        st->phase = 0;
        g_nHudWorldEggRollRequest.v = 1;
        actor->rotX = 0;
        actor->rotY = 8;
        actor->mode += 1;
        break;

      case 2: {
        st->phase += g_nFrameStep;

        if (st->phase < 0x80) {
          int step = g_nFrameStep * 4;

          st->radius += step;
          st->angle += step;
          SVectorToVectorShl2(&actor->posX, st->centre);
          actor->posX +=
              (LookupCosine(st->angle * 0x10) * (short)st->radius) >> 12;
          actor->posY +=
              (LookupSine(st->angle * 0x10) * (short)st->radius) >> 12;
          actor->posZ -= st->phase * 8;
          actor->rotZ += g_nFrameStep * 4;
        } else {
          int cb;
          int cs;
          int x;
          int y;

          CopyVector(svA, &SPY.posX);
          cb = LookupCosine(g_bSpyroPersistentEulerZ * 0x10);
          cs = LookupSine(g_bSpyroPersistentEulerZ * 0x10);
          x = cb * 0xB5;
          y = cs * 0xB5;
          svA[0] += (y + x) * 4 >> 12;
          svA[1] += (int)((LookupSine(g_bSpyroPersistentEulerZ * 0x10) * 0xB5) -
                          (LookupCosine(g_bSpyroPersistentEulerZ * 0x10) * 0xB5U)) *
                        4 >>
                    12;
          svA[2] += 0x300;
          VectorToSVectorShr2(st->apex, svA);
          VectorToSVectorShr2(st->centre, &actor->posX);
          st->phase = 0;
          actor->mode += 1;
        }
        if (g_dwPadPressed & 0x40) {
          actor->mode = 5;
        }
        break;
      }

      case 3: {
        st->phase += g_nFrameStep;

        if (st->phase < 0x40) {
          SVectorToVectorShl2(svB, st->centre);
          SVectorToVectorShl2(svC, st->apex);
          SubtractVector(svC, svC, svB);
          ScaleVector3Sat(svC, svC, st->phase);
          RShiftVector3(svC, 6);
          AddVector(&actor->posX, svB, svC);
          actor->rotZ += g_nFrameStep * 4;
        } else {
          CopyVector(svD, &SPY.posX);
          svD[0] += LookupCosine(g_bSpyroPersistentEulerZ * 0x10) >> 3;
          svD[1] += LookupSine(g_bSpyroPersistentEulerZ * 0x10) >> 3;
          svD[2] += 0x300;
          VectorToSVectorShr2(st->centre, svD);
          st->radius = 0x200;
          st->angle = g_bSpyroPersistentEulerZ - 0x40;
          st->phase = 0;
          actor->mode += 1;
        }
        if (g_dwPadPressed & 0x40) {
          actor->mode = 5;
        }
        break;
      }

      case 4: {
        st->phase += g_nFrameStep;

        if (st->phase < 0x80) {
          int step = g_nFrameStep * 4;

          st->radius -= step;
          st->angle -= step;
          SVectorToVectorShl2(&actor->posX, st->centre);
          actor->posX +=
              (LookupCosine(st->angle * 0x10) * (short)st->radius) >> 12;
          actor->posY +=
              (LookupSine(st->angle * 0x10) * (short)st->radius) >> 12;
          actor->posZ -= st->phase * 4;
          actor->rotZ += g_nFrameStep * 4;
        } else {
          actor->mode += 1;
        }
        if (g_dwPadPressed & 0x40) {
          actor->mode = 5;
        }
        break;
      }

      case 5: {
        SubtractVector(svE, &actor->posX, &CAM.posX);
        WorldToCameraRotate(svE, svE);
        g_pfnLevelEmitParticles(0x10, 0x4D, svE, 0);
        g_nGameplayBlocked = 0;
        ZeroVector(&g_nSpyroSwingEulerPitchTarget);
        CommitActorKillGems(0, st->idx);
        g_nWorldEggsRecovered += 1;
        g_anLevelEggsRecovered[g_nLevelIntroIndex] += 1;
        g_nHudWorldEggRollRequest.v = 0;
        DespawnActorRecord(actor);
        continue;
      }
      }

      break;
    }

    /* Path walker (PathState): waits for Spyro, then paces its path, turning
     * back at either end and choosing the node that leads away from him. */
    case 36: {
      PathState *st = actor->state;

      if ((actor->flags & 0xB0000) && actor->mode != 2) {
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        actor->mode = 2;
        ANIM_RESET(actor, 2);
        continue;
      }

      switch (actor->mode) {
      case 0:
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x1800) {
          int height = actor->posZ - actor->floorOffset;
          int drop = height - SPY.posZ;

          if (drop > 0 ? drop < 0x800 : (SPY.posZ - height) < 0x800) {
            ENTER_POSE4(actor, 1);
          }
        }
        break;

      case 1:
        if (ApproxDist2D(&actor->posX,
                          &st->path->nodes[st->path->cur].x) < 0x400) {
          if (ApproxDist2D(&actor->posX, &spy->posX) < 0x1801) {
            int height = actor->posZ - actor->floorOffset;
            int drop = height - SPY.posZ;

            if (drop > 0 ? drop >= 0x801 : (SPY.posZ - height) >= 0x801) {
              goto pose0;
            }
            goto walk;
          }
        pose0:
          ENTER_POSE4(actor, 0);
        walk:
          if (st->path->cur == 0 && st->unk10 == 0) {
            st->path->cur = 1;
          } else if (st->path->cur == st->path->count - 1 && st->unk10 == 0) {
            st->path->cur--;
          } else {
            int spyroAngle;
            int nextNode;
            int previousNode;
            int nextAngle;
            int previousAngle;

            spyroAngle = ArcTan2(spy->posX - actor->posX,
                                       spy->posY - actor->posY, 0);
            nextNode = (st->path->cur + 1) % st->path->count;
            previousNode =
                (st->path->cur - 1 + st->path->count) % st->path->count;
            nextAngle =
                ArcTan2(st->path->nodes[nextNode].x - actor->posX,
                              st->path->nodes[nextNode].y - actor->posY, 0);
            previousAngle =
                ArcTan2(st->path->nodes[previousNode].x - actor->posX,
                              st->path->nodes[previousNode].y - actor->posY, 0);
            if (AbsAngleDelta8(spyroAngle, nextAngle) >
                AbsAngleDelta8(spyroAngle, previousAngle)) {
              st->path->cur = nextNode;
            } else {
              st->path->cur = previousNode;
            }
          }
        }
        if (TurnActorYawTowardAngle(actor,
                          ArcTan2(st->path->nodes[st->path->cur].x -
                                            actor->posX,
                                        st->path->nodes[st->path->cur].y -
                                            actor->posY,
                                        0),
                          8, 0x20, 1) != 0) {
          MoveActorAlongHeading(actor, 0x60, 0x200, 0x200, 0x27);
        }
        break;

      case 2:
        if (g_nActorAnimDone != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }

      RefreshActorDerivedState(actor, 1);
      break;
    }

    /* Explosion shard with a ground bounce (ShardState). */
    case 67:
    case 68:
    case 255:
    case 256: {
      ShardState *st = actor->state;

      if (st->life > 0 && actor->drawn != 0) {

      if (CollideSphereWithWorldAndActors(&actor->posX, 0x100, 0x100) != 0) {
        int *nrm = &g_anCollisionProbeVec;
        int along;
        register int push asm("$7");

        SetVector3Magnitude(nrm, 0x1000);
        along = (st->vel[0] * nrm[0]) + (st->vel[1] * g_nCollisionProbeVecY) +
                (st->vel[2] * g_nCollisionProbeVecZ);
        push = along >> 11;

        if (push < 0) {
          ScaleVector3ByRatio(nrm, 0x1000, (along >> 13) - push);
          st->vel[0] += nrm[0];
          st->vel[1] += AS_I32(g_nCollisionProbeVecY);
          st->vel[2] += AS_I32(g_nCollisionProbeVecZ);
        }
      }

      actor->posX += st->vel[0];
      actor->posY += st->vel[1];
      st->vel[2] -= 6;
      if (st->vel[2] < -0x80) {
        st->vel[2] = -0x80;
      }
      actor->posZ += st->vel[2];
      actor->rotX += st->spin[0];
      actor->rotY += st->spin[1];
      actor->rotZ += st->spin[2];

      if (!(st->life & 3)) {
        svD[0] = GetRandomU32() & 3;
        svD[1] = GetRandomU32() & 3;
        svD[2] = 0x14;
        g_pfnLevelEmitParticles(1, 1, &actor->posX, svD);
      }

      st->life -= 1;
      break;
      }
      g_pfnLevelEmitParticles(8, 0x46, &actor->posX, (int *)0x10);
      DespawnActorRecord(actor);
      continue;
    }

    /* Falling chest fragments (TumbleState). */
    case 69:
    case 151:
    case 257: {
      TumbleState *st = actor->state;

      /* Retire when out of life, no longer drawn, or sunk to the floor. */
      if (st->life == 0) {
        DespawnActorRecord(actor);
        continue;
      }
      if (actor->drawn == 0) {
        DespawnActorRecord(actor);
        continue;
      }
      if (actor->posZ <= st->floorZ) {
        DespawnActorRecord(actor);
        continue;
      }

      actor->posX += st->vel[0];
      actor->posY += st->vel[1];

      st->vel[2] -= 6;
      if (st->vel[2] < -0x80) {
        st->vel[2] = -0x80;
      }
      actor->posZ += st->vel[2];

      actor->rotX += st->spin[0];
      actor->rotY += st->spin[1];
      actor->rotZ += st->spin[2];

      st->life -= 1;
      break;
    }

    /* Collectibles: life statue, life orb, gems (PickupState). */
    case 14:
    case 15:
    case 83:
    case 84:
    case 85:
    case 86:
    case 87: {
      PickupState *st = (PickupState *)actor->state;

      int dist = ApproxDist2D(&actor->posX, &SPY.posX);

      if (st->timer < 0xFA) {
        st->timer += g_nFrameStep;
      }

      if (actor->type != 15) {

        if (st->anim != 255) {
          RefreshActorDerivedState(actor, 4);

          if (actor->type == 14) {
            RotateVectorByMatrix(actor->mtx, &g_anRotBillboardOffset5, g_aRotBillboards[st->anim].pos);
          } else {
            RotateVectorByMatrix(actor->mtx, &g_anRotBillboardOffset3, g_aRotBillboards[st->anim].pos);
          }

          AddVector(g_aRotBillboards[st->anim].pos, g_aRotBillboards[st->anim].pos,
                        &actor->posX);

          if (g_aRotBillboards[st->anim].life < 5) {
            st->anim = 255;
          }
        } else {
          if (st->timer > 0xf7) {
            int handle = SpawnActorRotBillboard(actor, &g_anRotBillboardOffset3);
            if (handle >= 0 && dist < 0x4000) {
              st->anim = handle;
            }

            st->timer = (GetRandomU32() & 0x1F) + 33;
          }
        }
      }

      if (!actor->drawn && dist > 0x2000 && actor->subMode == 1 && st->sub == 0) {
        break;
      }

      switch (actor->subMode) {
      case 0: {
        int *nrm = &g_anCollisionProbeVec;

        CopyVector(svA, &actor->posX);
        svA[2] += 0x400;
        FindGroundHeightBelow(svA, 0x10000);
        st->pitch = -ArcTan2_8bit(
            IntegerSqrt((nrm[0] * nrm[0]) + (g_nCollisionProbeVecZ * g_nCollisionProbeVecZ)),
            g_nCollisionProbeVecY);
        st->yaw = -ArcTan2_8bit(g_nCollisionProbeVecZ, nrm[0]);

        if (st->pitch != 0 || st->yaw != 0) {
          actor->rotZ = 0;
        }

        actor->subMode = 1;
        break;
      }
      case 1: {
        if (st->sub != 0) {

          if (st->sub == 1) {
            SnapActorToGround(actor);
            EncodeCachedVecToActorDirCode(actor);
            if (actor->type >= 83) {
              int *nrm = &g_anCollisionProbeVec;

              st->pitch = -ArcTan2_8bit(
                  IntegerSqrt((nrm[0] * nrm[0]) + (g_nCollisionProbeVecZ * g_nCollisionProbeVecZ)),
                  g_nCollisionProbeVecY);
              st->yaw = -ArcTan2_8bit(g_nCollisionProbeVecZ, nrm[0]);
            }

          } else if (st->sub == 2) {

            UnpackWorldCollisionTri(st->unk0C, acc[0]);

            AddVector(acc[0], acc[0], acc[1]);
            AddVector(acc[0], acc[0], acc[2]);

            actor->posX = acc[0][0] / 3;
            actor->posY = acc[0][1] / 3;
            actor->posZ = acc[0][2] / 3;

            RefreshActorDerivedState(actor, 2);
            SnapActorToGround(actor);
            EncodeCachedVecToActorDirCode(actor);

          } else if (st->sub == 3) {
            int colIndex;

            CopyVector(svF, &actor->posX);
            svF[2] += 0x400;

            RefreshActorDerivedState(actor, 2);

            if (FindGroundHeightBelow(svF, 0x1000) > 0) {
              colIndex = g_nLastCollisionHitChunkId;
              st->sub = 2;
              st->unk0C = colIndex;
            }
          }
        }

        if (dist < 1434) {
          int zDist = (SPY.posZ - 356) - actor->posZ;

          if (ABS2(zDist) < 512) {

            if (g_pLevelOverlayInitRecord != 0) {

              if (SPY.u09C == 0) {

                int res = CheckLineOfSightSegment(&g_pLevelOverlayInitRecord->posX, &actor->posX);

                if (res) {
                  ChaserState *cs = (ChaserState *)g_pLevelOverlayInitRecord->state;

                  if (cs->owner == 0) {
                    if (!GetSoundVoiceStatusByOwner(g_pLevelOverlayInitRecord,
                                       g_apActorMeshTable[g_pLevelOverlayInitRecord->type]->m_Sounds[0])) {
                      g_nSfxOverrideFlags = 1;
                      g_nSfxOverrideVolR = 0x2000;
                      g_nSfxOverrideVolL = 0x2000;
                      PlayActorMeshVoice(g_pLevelOverlayInitRecord, 0, 0);
                    }

                    g_pLevelOverlayInitRecord->subMode = 4;
                    cs->owner = actor;
                  }
                }
              }
            }
          }
        }

        break;
      }
      case 2: {
        int speed;

        ScaleVector3Sat(svG, st->vel, g_nFrameStep);
        RShiftVector3(svG, 1);

        speed = VectorLength(svG, 1);

        if (220 < speed) {
          ScaleVector3ByRatio(svG, speed, 220);
        }

        AddVector(svG, &actor->posX, svG);

        if (-220 < st->vel[2]) {
          st->vel[2] -= g_nFrameStep * 5;
        }

        if (svG[2] < 0) {
          if (actor->type != 14 && actor->type != 15) {
            CollectPickupActor(actor);
          }
          DespawnActorRecord(actor);
          continue;
        } else {
          svG[2] += 240;

          if (CollideSphereWithWorldAndActors(svG, 240, 240)) {
            if (GetGroundTriggerRegionType() == 0) {
              if (actor->type != 14 && actor->type != 15) {
                CollectPickupActor(actor);
              }
              DespawnActorRecord(actor);
              continue;
            }

            CopyVector(svH, &g_anCollisionProbeVec);
            CopyVector(&actor->posX, &g_anCollisionResolvedPos);
            actor->posZ -= 0xF0;

            if (st->count == 0) {
              int groundHeight = FindGroundHeightBelow(svG, 0x400);
              int groundAngle =
                  (signed char)ArcTan2_8bit(svH[2], VectorLength(svH, 0));

              if ((svG[2] - 400) < groundHeight && groundAngle < 24) {
                actor->subMode = 1;
                CopyVector(&actor->posX, svG);
                actor->posZ = groundHeight;

                if (actor->type != 0xF) {

                  st->pitch = -ArcTan2_8bit(
                      IntegerSqrt((svH[0] * svH[0]) + (svH[2] * svH[2])),
                      svH[1]);
                  st->yaw = -ArcTan2_8bit(svH[2], svH[0]);

                  if (st->pitch != 0 || st->yaw != 0) {
                    actor->rotZ = 0;
                  }
                }

                EncodeCachedVecToActorDirCode(actor);
              } else {
                st->count++;
              }
            }

            if (st->count != 0) {
              g_nSfxOverrideFlags = 1;
              g_nSfxOverrideVolL = 0x3CCC >> (3 - st->count);
              g_nSfxOverrideVolR = 0x3CCC >> (3 - st->count);

              PlaySoundEffect(g_pLevelSampleBankHeader[0x2F], actor, 8, &actor->sfxSlot);

              if (func_80017428(st->vel, svH, st->vel)) {
                st->count--;

                st->vel[0] = (st->vel[0] >> 3) + (GetRandomU32() & 0x3F) - 32;
                st->vel[1] = (st->vel[1] >> 3) + (GetRandomU32() & 0x3F) - 32;
                st->vel[2] = (st->vel[2] >> 2) + (GetRandomU32() & 0xF);
              }
            }

          } else {
            svG[2] -= 240;
            CopyVector(&actor->posX, svG);
            FindGroundHeightBelow(svG, 0x10000);
            EncodeCachedVecToActorDirCode(actor);
          }
        }

        if (dist < 1434) {
          int zDist = (((int *)&SPY.posX)[2] - 356) - actor->posZ;

          if (ABS2(zDist) < 512) {

            if (g_pLevelOverlayInitRecord != 0) {

              if (SPY.u09C == 0 && st->count < 3) {

                ChaserState *cs = (ChaserState *)g_pLevelOverlayInitRecord->state;

                if (cs->owner == 0) {
                  if (!GetSoundVoiceStatusByOwner(g_pLevelOverlayInitRecord,
                                     g_apActorMeshTable[g_pLevelOverlayInitRecord->type]->m_Sounds[0])) {
                    g_nSfxOverrideFlags = 1;
                    g_nSfxOverrideVolR = 0x2000;
                    g_nSfxOverrideVolL = 0x2000;
                    PlayActorMeshVoice(g_pLevelOverlayInitRecord, 0, 0);
                  }

                  g_pLevelOverlayInitRecord->subMode = 4;
                  cs->owner = actor;
                }
              }
            }
          }
        }

        break;
      }
      case 3: {
        if (st->timer >= 32) {
          CopyVector(&actor->posX, &SPY.posX);
        } else {

          SubtractVector(svI, &SPY.posX, st->vel);
          RShiftVector3(svI, 5);

          if (VectorLength(svI, 1) > 480) {
            CopyVector(&actor->posX, &SPY.posX);
            st->timer = 32;
          } else {
            ScaleVector3Sat(svI, svI, st->timer);
            AddVector(&actor->posX, st->vel, svI);
            actor->posZ += g_anSineLut[st->timer * 4] / 12;
          }
        }

        actor->rotX = actor->rotX - 7 + st->pitch;
        actor->rotY = actor->rotY - 7 + st->yaw;
        actor->rotZ = actor->rotZ - 7 + st->roll;
        break;
      }

      case 4: {
        continue;
      }
      }
      if (actor->subMode < 3) {
        if (actor->type == 14) {
          actor->rotX = (g_anCosineLut[st->roll] >> 9) + st->pitch;
          actor->rotY = (g_anSineLut[st->roll] >> 9) + st->yaw;
          st->roll += g_nFrameStep << 1;
        } else if (actor->type == 15) {
          actor->rotZ += 8;
          actor->rotY -= 6;
        } else {
          actor->rotX = (g_anCosineLut[st->roll] >> 7) + st->pitch;
          actor->rotY = (g_anSineLut[st->roll] >> 7) + st->yaw;
          st->roll += g_nFrameStep << 1;
        }
      }
      if ((st->timer > 32 && dist < 512 && (-384 < (SPY.posZ - actor->posZ)) &&
           ((SPY.posZ - actor->posZ) < 512)) ||
          (actor->subMode == 3 && st->timer > 64)) {

        if (g_pLevelOverlayInitRecord != 0) {
          ChaserState *cs = (ChaserState *)g_pLevelOverlayInitRecord->state;

          if (cs->owner == actor) {
            cs->owner = 0;
          }
        }

        if (actor->type == 14) {
          PlaySoundEffect(g_pLevelSampleBankHeader[0], actor, 16, 0);
          RefreshActorDerivedState(actor, 4);
          g_pfnLevelEmitParticles(16, 70, &actor->posX, (void *)8);
          g_pfnLevelEmitParticles(16, 70, &actor->posX, (void *)0x10);

          if (g_nExtraLives < 99) {
            g_nExtraLives++;
          }

          CommitActorKillGems(0, actor);
          DespawnActorRecord(actor);
          continue;
        } else if (actor->type == 15) {
          PlaySoundEffect(g_pLevelSampleBankHeader[0], actor, 16, 0);
          g_nSparxFoodCount++;

          DespawnActorRecord(actor);
        } else {
          CollectPickupActor(actor);
          DespawnActorRecord(actor);
          continue;
        }
      }

      break;
    }

    /* Type 109. */
    case 109: {
      L14State109 *st = actor->state;
      int d;
      int h;

      switch (st->mode & 3) {
      case 0:
        if (st->mode & 0x80) {
          if (g_nSpyroAirborneFrames != 0) {
            g_dwSpyroRequestMask = 0x80000200;
            if (g_nSpyroState == 8) {
              g_anCameraActiveTargetParams.w[0] = (g_anCameraParamMode2Front.w[0] - g_nSpyroMoveTargetYaw) & 0xFFF;
            } else {
              g_anCameraActiveTargetParams.w[0] = (g_anCameraParamMode2Front.w[0] - g_nSpyroBodyYaw) & 0xFFF;
            }
            {
              int *w1 = &g_anCameraActiveTargetParams.w[1];

              g_pSpyroRequestCameraTargetParams = &g_anCameraActiveTargetParams;
              *w1 = g_anCameraParamMode2Front.w[1];
            }
            D_80078670 = g_anCameraParamMode2Front.w[2];
            D_80078674 = g_anCameraParamMode2Front.w[3];
            D_80078678 = g_anCameraParamMode2Front.w[4];
            D_8007867C = g_anCameraParamMode2Front.w[5];
          } else {
            st->mode &= ~0x80;
          }
        }
        if (st->mode & 0x2C) {
          int *cf = &g_dwSpyroRequestMask;

          *cf = 0x80000327;
          SubtractVector(svF, &actor->posX, &SPY.posX);
          svF[2] -= 0x80;
          RShiftVector3(svF, 2);
          d = VectorLength(svF, 1);
          if (d > 0x200) {
            ScaleVector3ByRatio(svF, d, 0x200);
          }
          AddVector(cf + 1, svF, &SPY.posX);
          h = (actor->rotZ - SPY.bodyRotZ) & 0xFF;
          if (h > 0x80) {
            h -= 0x100;
          }
          if (h < -4) {
            h = -4;
          }
          if (h > 4) {
            h = 4;
          }
          D_80078C5E = SPY.bodyRotZ + h;
          g_nSpyroRequestState = 0x11;
          g_pSpyroRequestCameraAnchor = &SPY.posX;
          g_pScriptedCameraActor = 0;
          g_pSpyroRequestCameraTargetParams = &g_anCameraActiveTargetParams;
          g_anCameraActiveTargetParams.w[0] = (g_anCameraParamMode2Front.w[0] - g_nSpyroBodyYaw) & 0xFFF;
          g_anCameraActiveTargetParams.w[1] = g_anCameraParamMode2Front.w[1];
          g_anCameraActiveTargetParams.w[2] = g_anCameraParamMode2Front.w[2];
          g_anCameraActiveTargetParams.w[3] = g_anCameraParamMode2Front.w[3];
          g_anCameraActiveTargetParams.w[4] = g_anCameraParamMode2Front.w[4];
          g_anCameraActiveTargetParams.w[5] = g_anCameraParamMode2Front.w[5];
        } else if (st->mode & 0x10) {
          int *cf = &g_dwSpyroRequestMask;

          *cf = 0x80000327;
          CopyVector(cf + 1, &SPY.posX);
          h = (actor->rotZ - SPY.bodyRotZ) & 0xFF;
          if (h > 0x80) {
            h -= 0x100;
          }
          if (h < -3) {
            h = -3;
          }
          if (h > 3) {
            h = 3;
          }
          D_80078C5E = SPY.bodyRotZ + h;
          g_nSpyroRequestState = 0x11;
          g_pSpyroRequestCameraAnchor = &SPY.posX;
          g_pScriptedCameraActor = 0;
          g_pSpyroRequestCameraTargetParams = &g_anCameraActiveTargetParams;
          g_anCameraActiveTargetParams.w[0] = (g_anCameraParamMode2Front.w[0] - g_nSpyroBodyYaw) & 0xFFF;
          g_anCameraActiveTargetParams.w[1] = g_anCameraParamMode2Front.w[1];
          g_anCameraActiveTargetParams.w[2] = g_anCameraParamMode2Front.w[2];
          g_anCameraActiveTargetParams.w[3] = g_anCameraParamMode2Front.w[3];
          g_anCameraActiveTargetParams.w[4] = g_anCameraParamMode2Front.w[4];
          g_anCameraActiveTargetParams.w[5] = g_anCameraParamMode2Front.w[5];
        }

        if (st->mode & 4) {
          d = ApproxDist2D(&actor->posX, st->dest);
          if (d < 0x200) {
            st->mode = (st->mode & ~4) | 8;
            (st->cur = st->path0)->cur = 0;
            st->vz = 0;
            continue;
          }
          h = ArcTan2(st->dest[0] - actor->posX,
                            st->dest[1] - actor->posY, 0);
          d >>= 1;
          if (d > 0x140) {
            d = 0x140;
          }
          STEER(actor, h, d, 4, 0x20, 4);
          d = st->dest[2] - actor->posZ;
          if (d != 0) {
            d >>= 1;
            if (d < -0x40) {
              d = -0x40;
            }
            if (d > 0x40) {
              d = 0x40;
            }
            actor->posZ += d;
          }
          continue;
        }

        if (st->mode & 8) {
          d = 0x140;
          h = ArcTan2(NODE_PT(st->cur)[0] - actor->posX,
                            NODE_PT(st->cur)[1] - actor->posY, 0);
          if (TurnActorYawTowardAngle(actor, h, 8, 0x30, 1) != 0) {
            MoveActorAlongHeading(actor, 0x140, 0, 0x200, 0x20);
          } else {
            h = (h - actor->rotZ) & 0xFF;
            if (h > 0x80) {
              h -= 0x100;
            }
            d -= ABS2(h) << 4;
            if (d > 0) {
              MoveActorAlongHeading(actor, d, 0, 0x200, 0x20);
            }
          }
          d = (NODE_PT(st->cur)[2] - actor->posZ) >> 2;
          if (d > 0x60) {
            d = 0x60;
          }
          if (d < -0x60) {
            d = -0x60;
          }
          d = (d - st->vz) >> 1;
          if (d < -0x10) {
            d = -0x10;
          }
          if (d > 0x10) {
            d = 0x10;
          }
          st->vz += d;
          actor->posZ += st->vz;
          if (ApproxDist2D(&actor->posX, PATH_CUR(st->cur)) < 0x200 &&
              ++st->cur->cur >= st->cur->count) {
            st->cur->cur = st->cur->count - 2;
            st->mode = (st->mode & ~8) | 0xC0;
            do {
            } while (0);
            g_nSpyroRequestState = 8;
            g_nSpyroMoveTargetYaw = 0x290;
          }
          continue;
        }

        if (st->mode & 0x10) {
          int done = 1;

          PlaySoundEffect(g_pLevelSampleBankHeader[0x36], actor, 8, &actor->sfxSlot);
          if (actor->drawn == 0) {
            CopyVector(&actor->posX, &SPY.posX);
            actor->posZ += 0x400;
          } else {
            d = ApproxDist2D(&actor->posX, &SPY.posX);
            if (d > 0x80) {
              d >>= 1;
              if (d > 0x140) {
                d = 0x140;
              }
              h = ArcTan2(SPY.posX - actor->posX,
                                SPY.posY - actor->posY, 0);
              if (TurnActorYawTowardAngle(actor, h, 0x10, 0x30, 1) != 0) {
                MoveActorAlongHeading(actor, d, 0, 0, 0);
              } else {
                h = (h - actor->rotZ) & 0xFF;
                if (h > 0x80) {
                  h -= 0x100;
                }
                d -= ABS2(h) << 4;
                if (d > 0) {
                  MoveActorAlongHeading(actor, d, 0, 0, 0);
                }
              }
              done = 0;
            } else {
              d = SPY.posZ - actor->posZ + 0x100;

              if (ABS2(d) > 0x80) {
                if (d < -0x80) {
                  d = -0x80;
                }
                if (d > 0x80) {
                  d = 0x80;
                }
                actor->posZ += d;
                done = 0;
              }
            }
          }
          if (done) {
            int i;
            int which;

            st->mode &= ~0x10;
            d = ApproxDist2D(&actor->posX, st->dest);
            which = 0;
            for (i = 0; i < st->pathA->count; i++) {
              int e = ApproxDist2D(&actor->posX, &st->pathA->nodes[i].x);

              if (e < d) {
                d = e;
                which = 1;
                st->pathA->cur = i;
              }
            }
            for (i = 0; i < st->pathB->count; i++) {
              int e = ApproxDist2D(&actor->posX, &st->pathB->nodes[i].x);

              if (e < d) {
                d = e;
                which = 2;
                st->pathB->cur = i;
              }
            }
            for (i = 0; i < st->pathC->count; i++) {
              int e = ApproxDist2D(&actor->posX, &st->pathC->nodes[i].x);

              if (e < d) {
                d = e;
                which = 3;
                st->pathC->cur = i;
              }
            }
            switch (which) {
            case 0:
              st->mode |= 4;
              break;
            case 1:
              st->mode |= 0x20;
              st->cur = st->pathA;
              break;
            case 2:
              st->mode |= 0x20;
              st->cur = st->pathB;
              break;
            case 3:
              st->mode |= 0x20;
              st->cur = st->pathC;
              break;
            }
          }
          continue;
        }

        if (st->mode & 0x20) {
          d = 0x140;
          h = ArcTan2(NODE_PT(st->cur)[0] - actor->posX,
                            NODE_PT(st->cur)[1] - actor->posY, 0);
          if (TurnActorYawTowardAngle(actor, h, 8, 0x30, 1) != 0) {
            MoveActorAlongHeading(actor, 0x140, 0, 0x200, 0x20);
          } else {
            h = (h - actor->rotZ) & 0xFF;
            if (h > 0x80) {
              h -= 0x100;
            }
            d -= ABS2(h) << 4;
            if (d > 0) {
              MoveActorAlongHeading(actor, d, 0, 0x200, 0x20);
            }
          }
          d = (NODE_PT(st->cur)[2] - actor->posZ) >> 1;
          if (d > 0x40) {
            d = 0x40;
          }
          if (d < -0x40) {
            d = -0x40;
          }
          actor->posZ += d;
          if (ApproxDist2D(&actor->posX, PATH_CUR(st->cur)) < 0x200 &&
              ++st->cur->cur >= st->cur->count) {
            st->cur->cur = 0;
            st->mode = (st->mode & ~0x20) | 4;
          }
          continue;
        }

        if (st->mode & 0x40) {
          if (st->cur->cur > st->cur->count) {
            d = ApproxDist2D(&actor->posX, st->dest);
            if (d > 0x140) {
              d = 0x140;
            }
            h = ArcTan2(st->dest[0] - actor->posX,
                              st->dest[1] - actor->posY, 0);
            STEER(actor, h, d, 8, 0x30, 4);
            d = (st->dest[2] - actor->posZ) >> 1;
            if (ABS2(d) < 0x80) {
              st->cur->cur = 0;
              st->mode &= ~0x40;
            }
            if (d > 0x80) {
              d = 0x80;
            }
            if (d < -0x80) {
              d = -0x80;
            }
            actor->posZ += d;
            continue;
          }
          d = 0x140;
          h = ArcTan2(NODE_PT(st->cur)[0] - actor->posX,
                            NODE_PT(st->cur)[1] - actor->posY, 0);
          if (TurnActorYawTowardAngle(actor, h, 8, 0x30, 1) != 0) {
            MoveActorAlongHeading(actor, 0x140, 0, 0x200, 0x20);
          } else {
            h = (h - actor->rotZ) & 0xFF;
            if (h > 0x80) {
              h -= 0x100;
            }
            d -= ABS2(h) << 4;
            if (d > 0) {
              MoveActorAlongHeading(actor, d, 0, 0x200, 0x20);
            }
          }
          d = (NODE_PT(st->cur)[2] - actor->posZ) >> 1;
          if (d > 0x40) {
            d = 0x40;
          }
          if (d < -0x40) {
            d = -0x40;
          }
          actor->posZ += d;
          if (ApproxDist2D(&actor->posX, PATH_CUR(st->cur)) < 0x200) {
            st->cur->cur--;
          }
          continue;
        }

        {
          int i;

          d = ApproxDist2D(&SPY.posX, st->dest);
          st->cur = 0;
          for (i = 0; i < st->pathA->count; i++) {
            int e = ApproxDist2D(&SPY.posX, &st->pathA->nodes[i].x);

            if (e < d) {
              d = e;
              st->cur = st->pathA;
              st->cur->cur = i;
            }
          }
          for (i = 0; i < st->pathB->count; i++) {
            int e = ApproxDist2D(&SPY.posX, &st->pathB->nodes[i].x);

            if (e < d) {
              d = e;
              st->cur = st->pathB;
              st->cur->cur = i;
            }
          }
          for (i = 0; i < st->pathC->count; i++) {
            int e = ApproxDist2D(&SPY.posX, &st->pathC->nodes[i].x);

            if (e < d) {
              d = e;
              st->cur = st->pathC;
              st->cur->cur = i;
            }
          }
          if (d >= 0x3000) {
            continue;
          }
          if (st->cur != 0) {
            CopyVector(svF, PATH_CUR(st->cur));
          } else {
            CopyVector(svF, st->dest);
          }
          d = ApproxDist2D(&actor->posX, svF);
          if (d > 0x100) {
            d >>= 1;
            if (d > 0x140) {
              d = 0x140;
            }
            h = ArcTan2(svF[0] - actor->posX, svF[1] - actor->posY,
                              0);
            STEER(actor, h, d, 0x20, 0x30, 4);
          } else {
            StopSoundVoicesByOwner(actor, 1);
            TrackEntityYawTowardSpyro(actor, 0x10, 0x18, 1);
          }
          d = (SPY.posZ - actor->posZ - 0x400) >> 1;
          if (d < 0) {
            if (d < -0x80) {
              d = -0x80;
            }
            actor->posZ += d;
          } else {
            d = st->dest[2] - actor->posZ;
            if (d > 0x40) {
              d = 0x40;
            }
            actor->posZ += d;
          }
          if (SPY.posZ < 0x9358) {
            st->mode |= 0x10;
          }
          continue;
        }

      case 1:
      case 2: {
        if (*(int *)g_pActorListBase[st->rider].state & 0x2C) {
          int side = -0x40;

          if ((st->mode & 3) == 1) {
            side = 0x40;
          }
          CopyVector(svF, &SPY.posX);
          if (actor->mode != st->mode) {
            actor->mode = st->mode & 3;
            if (actor->nextAnim != st->mode) {
              ANIM_ADVANCE(actor, st->mode & 3);
            }
            continue;
          }
          h = (side << 4) + g_nSpyroBodyYaw;
          svF[0] += LookupCosine(h) >> 4;
          svF[1] += LookupSine(h) >> 4;
          CopyVector(&actor->posX, svF);
          TurnActorYawTowardAngle(actor, g_nSpyroBodyYaw >> 4, 8, 0x20, 1);
          st->bob = (st->bob + g_nFrameStep * 2) & 0xFF;
          EncodeCachedVecToActorDirCode(actor);
          RefreshActorDerivedState(actor, 2);
          continue;
        } else {
          int side = -0x50;

          if ((st->mode & 3) == 1) {
            side = 0x50;
          }
          CopyVector(svF, &g_pActorListBase[st->rider].posX);
          if (actor->mode != 0) {
            ENTER_POSE(actor, 0);
            continue;
          }
          h = ((side + g_pActorListBase[st->rider].rotZ) & 0xFF) << 4;
          svF[2] -= 0x180;
          svF[0] += LookupCosine(h) >> 2;
          svF[1] += LookupSine(h) >> 2;
          d = ApproxDist2D(svF, &actor->posX);
          SubtractVector(svF, svF, &actor->posX);
          if (d > 0x20) {
            h = ArcTan2(svF[0], svF[1], 0);
            d >>= 1;
            if (d > 0x190) {
              d = 0x190;
            }
            STEER(actor, h, d, 8, 0x20, 5);
            st->bob = g_pActorListBase[st->rider].rotZ;
          } else {
            TurnActorYawTowardAngle(actor, g_pActorListBase[st->rider].rotZ, 8, 0x20, 1);
          }
          if (svF[2] < -0x80) {
            svF[2] = -0x80;
          }
          if (svF[2] > 0x80) {
            svF[2] = 0x80;
          }
          actor->posZ += svF[2];
          continue;
        }
      }
      }
      break;
    }

    /* Dragon-pad fairy (PoleState). */
    case 110: {
      PoleState *st = (PoleState *)actor->state;

      Actor *tgtA = &g_pActorListBase[st->idxA];
      Actor *tgtB = &g_pActorListBase[st->idxB];

      switch (actor->mode) {
      case 0: {
        actor->unk08 = 0;
        if (ApproxDist2D(&tgtB->posX, &SPY.posX) > 2560) {
          actor->mode = 1;
        }
        break;
      }

      case 1: {
        if (tgtA->mode >= 0x80u) {
          actor->mode = 2;
          st->timer = 16;
          st->baseZ = actor->posZ;
        }
        break;
      }

      case 2: {
        if (st->timer != 0) {
          st->timer--;
        } else {
          actor->unk08 = 0;

          if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x2000) {
            if (ABS2(actor->posZ - actor->floorOffset - SPY.posZ) < 2048) {
              if (st->flag24 == 0 || tgtB->mode == 1) {
                st->flag24 = 0;
                actor->posZ = st->baseZ + 0x80;
                actor->rotZ = ArcTan2(SPY.posX - actor->posX,
                                             SPY.posY - actor->posY, 0) -
                               128;
                actor->unk50 = 0x20;
                actor->mode = 3;
                actor->unk57 = 0x60;

                g_pfnLevelEmitParticles(0x18, 0x50, &actor->posX, (void *)0xC);

                RefreshActorDerivedState(actor, 1);
                FindGroundHeightBelow(&actor->posX, 0x1000);
                EncodeCachedVecToActorDirCode(actor);
                actor->unk1C |= 0x80000000;
              }
            }
          }
        }
        break;
      }

      case 3: {
        if (actor->unk57 >= 0x21) {
          actor->unk57 -= 4;
          actor->rotZ = actor->rotZ + 8;
        } else {
          actor->mode = 4;
          st->timer = 0;
          st->t20 = 30;
        }
        break;
      }

      case 4: {
        st->timer++;
        actor->posZ = st->baseZ + (LookupCosine(st->timer << 7) >> 6);

        if ((tgtB->mode == 0 || (tgtB->mode == 2 && tgtB->subMode >= 0x10)) &&
            ApproxDist2D(&tgtB->posX, &SPY.posX) < 1024 &&
            ABS2(tgtB->posZ - tgtB->floorOffset - SPY.posZ) < 512) {

          if ((SPY.state == 0 || SPY.state == 0xD) && SPY.u080 > 0) {
            actor->mode = 6;
            st->flag24 = 1;
            BeginSaveMenuFromWorld(actor);
            break;
          }
        }

        if (ApproxDist2D(&actor->posX, &SPY.posX) > 0x2400 ||
            (ABS2(actor->posZ - actor->floorOffset - SPY.posZ) > 0xC00)) {
          actor->mode = 6;
        } else {
          st->t20--;
          if (st->t20 == 0) {
            int baseDistance = (GetRandomU32() & 0x3F) + 0x140;
            char finalAngle = (ArcTan2(SPY.posX - tgtB->posX,
                                             SPY.posY - tgtB->posY, 0) +
                               (GetRandomU32() & 0xF) - 8);

            CopyVector(st->tgt, &tgtB->posX);

            actor->mode = 5;

            st->tgt[0] -= (LookupCosine(finalAngle * 0x10) * baseDistance) >> 12;
            st->tgt[1] -= (LookupSine(finalAngle * 0x10) * baseDistance) >> 12;

            st->angle = ArcTan2(SPY.posX - st->tgt[0],
                                      SPY.posY - st->tgt[1], 0);
            st->t20 = 0;
          }
        }
        break;
      }
      case 5: {
        st->timer++;
        actor->posZ = st->baseZ + (LookupCosine(st->timer << 7) >> 6);

        if ((tgtB->mode == 0 || (tgtB->mode == 2 && tgtB->subMode >= 0x10)) &&
            ApproxDist2D(&tgtB->posX, &SPY.posX) < 0x400 &&
            ABS2(tgtB->posZ - tgtB->floorOffset - SPY.posZ) < 512) {

          if ((SPY.state == 0 || SPY.state == 0xD) && SPY.u080 > 0) {
            actor->mode = 6;
            st->flag24 = 1;
            BeginSaveMenuFromWorld(actor);
            break;
          }
        }

        if (ApproxDist2D(&actor->posX, &SPY.posX) > 0x2800 ||
            ABS2(actor->posZ - actor->floorOffset - SPY.posZ) >= 0xC01) {
          actor->mode = 6;
        } else {
          int angleDiff;
          int cosWeight1;
          int cosDelta;
          int totalWeight;

          st->t20++;

          SubtractVector(svA, st->tgt, &actor->posX);

          angleDiff = SignedAngleDelta8(st->angle, actor->rotZ);

          cosWeight1 = LookupCosine((st->t20 - 1) << 7);
          cosDelta = cosWeight1 - LookupCosine(st->t20 << 7);
          totalWeight = LookupCosine((st->t20 - 1) << 7) + 0x1000;

          actor->posX += (svA[0] * cosDelta) / totalWeight;
          actor->posY += ((svA[1] * cosDelta) / totalWeight);
          actor->rotZ += (angleDiff * cosDelta) / totalWeight;

          FindGroundHeightBelow(&actor->posX, 0x1000);
          EncodeCachedVecToActorDirCode(actor);

          if (st->t20 == 0x10) {
            actor->mode = 4;
            st->t20 = (GetRandomU32() & 0x1F) + 0x1E;
          }
        }

        break;
      }

      case 6: {
        if (actor->unk57 < 96) {
          actor->unk57 += 4;
          actor->rotZ = actor->rotZ + 8;
        } else {
          actor->unk50 = 0;
          actor->drawn = 0;
          actor->mode = 2;
          st->timer = 16;

          g_pfnLevelEmitParticles(0x18, 0x50, &actor->posX, (void *)0xC);

          actor->unk1C &= 0x7FFFFFFF;
        }
        break;
      }
      }

      break;
    }

    /* Sparx (FlierState). */
    case 120: {
      FlierState *st = (FlierState *)actor->state;
      char pitchAngle;

      if (SPY.u164 >= 2) {

        svA[0] = 0;
        svA[1] = 0x64;
        svA[2] = 0;
        RotateVectorByMatrix(actor->mtx, svA, svA);
        AddVector(svA, svA, &actor->posX);
        g_pfnLevelEmitParticles(2, 0x42, svA, 0);

        svA[0] = 0;
        svA[1] = -0x64;
        svA[2] = 0;
        ApplyActiveGteRotation(svA, svA);
        AddVector(svA, svA, &actor->posX);
        g_pfnLevelEmitParticles(2, 0x42, svA, 0);
      }

      if (SPY.u164 >= 3) {
        if (st->fx == 0) {
          FxNode *g = AllocWorldSparkle();
          st->fx = g;
          if (g != 0) {
            g->anchor = &actor->posX;
            st->fx->size = 0x40;
            st->fx->unk14 = 0;
            st->fx->unk18 = 0;
            st->fx->unk1C = 0;
            st->fx->r = 0xC0;
            st->fx->g = 0xC0;
            st->fx->b = 0x60;
            st->fx->kind = 9;
            st->fx->desc = &D_8006E390;
          }
        } else {
          st->fx->size += 0x20;
          if (st->fx->size > 0x400) {
            st->fx->size = 0x400;
          }
        }
      } else {
        if (st->fx != 0) {
          ClearWord(st->fx);
          st->fx = 0;
        }
      }

      if (SPY.u164 <= 0) {
        Actor *target = st->rider;
        if (target != 0 && target->type == 16) {
          target->mode = 0;
        }
        g_pLevelOverlayInitRecord = 0;
        DespawnActorRecord(actor);
        break;
      }

      if (st->rider != 0 && ApproxDist2D(&actor->posX, &SPY.posX) > 8192) {
        st->timer = 0;
      }

      if (actor->subMode == 99)
        break;

      if (st->rider != 0) {
        switch (actor->subMode) {
        case 0: {
          Actor *target = st->rider;
          ChaseState *bp = target->state;
          int isPickupAnimPhase = actor->nextAnim & 1;
          int dist;
          int targetSpeed;

          CopyVector(svJ1, &target->posX);
          if (isPickupAnimPhase == 0) {
            svJ1[0] -= g_anCosineLut[st->rider->rotZ] >> 4;
            svJ1[1] -= g_anSineLut[st->rider->rotZ] >> 4;
          } else {
            svJ1[0] -= g_anCosineLut[st->rider->rotZ] >> 6;
            svJ1[1] -= g_anSineLut[st->rider->rotZ] >> 6;
          }

          if (isPickupAnimPhase) {
            int delta;
            if (actor->nextFrame >= 7) {
              actor->subMode++;
            }

            if (2 < actor->rotY && actor->rotY < 254) {
              delta = -actor->rotY;
              if (delta > 2) {
                delta = 2;
              }
              if (delta < -2) {
                delta = -2;
              }
              actor->rotY = actor->rotY + delta;
            }
          }

          SubtractVector(svJ1, svJ1, &actor->posX);
          dist = VectorLength(svJ1, 1);

          if (isPickupAnimPhase == 0) {
            if (TickCountdownByFrameDelta(st, 4) != 0 ||
                (st->timer < 80 &&
                 AbsAngleDelta8((actor->rotZ + (bp->dir * 110)) & 0xFF,
                               ((CAM.rotZ >> 4) + 0x80) & 0xFF) < 8)) {
              if (actor->nextAnim != actor->nextAnim + 1) {
                g_nActorAnimDone = 0;
                ANIM_ADVANCE(actor, actor->nextAnim + 1);
              }
            }
          }
          targetSpeed = isPickupAnimPhase ? 150 : 130;
          if (targetSpeed + 5 < dist) {
            ScaleVector3ByRatio(svJ1, dist, targetSpeed + 5);
          } else {
            ScaleVector3ByRatio(svJ1, dist, targetSpeed - 5);
          }
          AddVector(&actor->posX, &actor->posX, svJ1);
          TurnActorYawTowardAngle(actor, ArcTan2(svJ1[0], svJ1[1], 0), 0xA, 0, 0);
          pitchAngle = ArcTan2(VectorLength(svJ1, 0), svJ1[2], 0);
          actor->rotY = pitchAngle;
          actor->rotY = StepAngle8Toward(pitchAngle, 0, 0x30);
          break;
        }
        case 1: {
          SPY.u164++;
          PlaySoundEffect(g_pLevelSampleBankHeader[0], actor, 0x10, 0);
          actor->subMode++;
          DespawnActorRecord(st->rider);
          break;
        }
        case 2: {
          if (g_nActorAnimDone != 0) {
            if (actor->nextAnim != actor->nextAnim - 1) {
              g_nActorAnimDone = 0;
              ANIM_ADVANCE(actor, actor->nextAnim - 1);
            }
            st->rider = 0;
            actor->subMode = 0;
          }
          break;
        }
        case 4: {
          PickupState *cp = st->rider->state;
          int dist;

          SubtractVector(svJ, &st->rider->posX, &actor->posX);
          dist = VectorLength(svJ, 1);

          if (dist < 512) {
            cp->timer = 0;
            cp->pitch = GetRandomU32() & 0xE;
            cp->yaw = GetRandomU32() & 0xE;
            cp->roll = GetRandomU32() & 0xE;

            CopyVector(cp->vel, &st->rider->posX);
            st->rider->subMode = 3;

            if (st->rider->type == 14)
              g_pfnLevelEmitParticles(1, 0xC, (int *)actor, D_8006E494);
            else if (st->rider->type == 15)
              g_pfnLevelEmitParticles(1, 0xC, (int *)actor, D_8006E490);
            else
              g_pfnLevelEmitParticles(1, 0xC, (int *)actor, g_adwActorBurstColors[st->rider->type]);

            st->rider = 0;
            if (!GetSoundVoiceStatusByOwner(actor, AS_PTRU8(g_pLevelSampleBankHeader)[0x2F])) {
              g_nSfxOverrideFlags = 1;
              g_nSfxOverrideVolR = 0x3600;
              g_nSfxOverrideVolL = 0x3600;
              PlaySoundEffect(g_pLevelSampleBankHeader[0x2F], actor, 8, &actor->sfxSlot);
            }
          } else {
            int dcheck = 310;

            if (dcheck < dist)
              ScaleVector3ByRatio(svJ, dist, 310);
            else
              ScaleVector3ByRatio(svJ, dist, 290);

            AddVector(&actor->posX, &actor->posX, svJ);
            TurnActorYawTowardAngle(actor, ArcTan2(svJ[0], svJ[1], 0), 0xA, 0, 0);
            pitchAngle = ArcTan2(VectorLength(svJ, 0), svJ[2], 0);
            actor->rotY = pitchAngle;
            actor->rotY = StepAngle8Toward(pitchAngle, 0, 0x30);
          }
          break;
        }
        }
      } else {

        actor->unk50 = 0x10;
        if (st->timer <= 0) {

          svA[0] = -0x258;
          svA[1] = (GetRandomU32() & 0x310) - 0x188;
          if (CAM.state == 3) {
            svA[2] = (GetRandomU32() & 0x7F) + 0x64;
          } else {
            svA[2] = (GetRandomU32() & 0x1FF) - 0xC8;
          }
          st->vx = svA[0];
          st->vy = svA[1];
          st->vz = svA[2];
          st->timer = GetRandomU32() & 0x7B;
        } else {
          st->timer -= g_nFrameStep;
          if (CAM.state == 3) {
            svA[0] = st->vx + (SPY.u118 >> 2);
            svA[1] = st->vy;
            svA[2] = st->vz;
            if (svA[2] < 0x64)
              svA[2] = 0x64;
          } else {
            if (CAM.state == 0x80000009)
              svA[0] = st->vx - 512;
            else {
              svA[0] = st->vx;
            }
            svA[1] = st->vy;
            svA[2] = st->vz;
          }
          RotateVectorByMatrix(&SPY.u034, svA, svA);
          AddVector(svA, svA, &SPY.posX);
          SubtractVector(svA, svA, &actor->posX);
          RShiftVector3(svA, 2);
          AddVector(&actor->posX, &actor->posX, svA);

          if (CollideSphereWithWorldAndActors(&actor->posX, 0x100, 0x100)) {
            CopyVector(&actor->posX, &g_anCollisionResolvedPos);
          }

          if (svA[2] > 32)
            svA[2] = 32;
          if (svA[2] < -32)
            svA[2] = -32;
          actor->rotX = 0;
          actor->rotY = svA[2];
          TurnActorYawTowardAngle(actor, SPY.bodyRotZ, 4, 0, 0);
        }

        switch (SPY.u164) {
        case 1:
          if (actor->nextAnim != 0) {
            if (actor->anim != 0) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, 0);
            }
          }
          break;
        case 2:
          if (actor->nextAnim != 2) {
            if (actor->anim != 2) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, 2);
            }
          }

          break;
        case 3:
          if (actor->nextAnim != 4) {
            if (actor->anim != 4) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, 4);
            }
          }
          break;
        }
      }
      RefreshActorDerivedState(actor, 4);
      break;
    }

    /* Type 227. */
    case 227: {
      L14State227 *st = actor->state;

      if (st->heading == -1) {
        st->heading = actor->rotZ;
      }
      switch (actor->mode) {
      case 0: {
        int range = 0xA8C;

        if (st->kind == 3) {
          range = 0xD48;
        }
        if (st->alert != 0) {
          actor->mode = 4;
          actor->posZ -= 0x1F4;
          continue;
        }
        if (actor->subMode != 0) {
          actor->subMode = 0;
          st->cool = 0;
          actor->mode = 3;
          continue;
        }
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if ((st->link == -1 || g_pActorListBase[st->link].mode >= 0x80) &&
            st->cool == 0 && g_nSpyroFlameBreathTimer == 0 &&
            ApproxDist2D(&actor->posX, &SPY.posX) < range &&
            ABS2(SPY_DZ(actor)) < 0x7D0 &&
            AbsAngleDelta8(g_bSpyroPersistentEulerZ,
                          ArcTan2(actor->posX - SPY.posX,
                                        actor->posY - SPY.posY, 0)) < 0x1E) {
          st->timer = 0x4B;
          actor->subMode = 0;
          actor->mode = 1;
          continue;
        }
        break;
      }

      case 1: {
        int dz;
        short *nv;

        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        CopyVector(x288, &SPY.posX);
        g_nGameplayBlocked = 1;
        x288[0] += (g_anCosineLut[SPY.bodyRotZ] * 0xE1) >> 10;
        x288[1] += (g_anSineLut[SPY.bodyRotZ] * 0xE1) >> 10;
        x288[2] -= 0x17C;
        dz = x288[2] - actor->posZ;
        SPY.controlFlags = 0x80002000;
        CopyVector(st->cam, &SPY.posX);
        st->cam[0] += (g_anCosineLut[SPY.bodyRotZ] * 0x4B) >> 10;
        st->cam[1] += (*(nv = &g_anSineLut[SPY.bodyRotZ]) * 0x4B) >> 10;
        st->cam[2] += 0x64;
        g_pSpyroRequestCameraAnchor = st->cam;
        g_pSpyroRequestCameraTargetParams = &g_anCameraActiveTargetParams;
        g_anCameraActiveTargetParams.w[2] = 0x400;
        g_anCameraActiveTargetParams.w[1] = 0;
        g_anCameraActiveTargetParams.w[3] = 0;
        g_anCameraActiveTargetParams.w[4] = 0;
        g_anCameraActiveTargetParams.w[5] = 0;
        SPY.controlFlags |= 0x80000200;
        g_anCameraActiveTargetParams.w[0] = -(SPY.bodyRotZ + 0x40) << 4;
        switch (actor->subMode) {
        case 0:
          if (ApproxDist2D(&actor->posX, x288) > 0x50) {
            MoveActorAlongAngle(actor,
                          ArcTan2(x288[0] - actor->posX,
                                        x288[1] - actor->posY, 0),
                          0x64, 0, 0, 0);
          } else {
            actor->posX = x288[0];
            actor->posY = x288[1];
          }
          if (ABS2(dz) > 0x28) {
            if (dz < 0) {
              if (dz < -0x32) {
                dz = -0x32;
              }
            } else if (dz > 0 && dz > 0x32) {
              dz = 0x32;
            }
            actor->posZ += dz;
          } else {
            actor->posZ = x288[2];
          }
          if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
            ANIM_GO(actor, 1);
            st->count = 0;
            actor->subMode = 1;
          }
          break;

        case 1: {
          int f;
          int *p;
          int *q;

          if (actor->nextFrame >= 6 &&
              (++st->count == 2 || actor->type == 0xF1)) {
            int r = 0x140;

            if (actor->type == 0xF1) {
              r = 0x190;
            }
            CopyVector(x288, &SPY.posX);
            x288[0] += (r * g_anCosineLut[SPY.bodyRotZ]) >> 12;
            x288[1] += (r * g_anSineLut[SPY.bodyRotZ]) >> 12;
            g_pfnLevelEmitParticles(1, 0x1F, x288, 0);
            st->count = 0;
          }
          if (g_nActorAnimDone != 0) {
            ANIM_GO(actor, 0);
            st->timer = 0xA;
            actor->subMode = 2;
            PlayActorMeshVoice(actor, 0, 0);
          }
          f = 7;
          if (actor->type == 0xF1) {
            f = 0xE;
          }
          if (actor->nextFrame >= f && *(p = &g_nSpyroFlameBreathTimer) == 0) {
            if (st->kind == 1 || st->kind == 3) {
              *p = 0x9450C00;
            } else if (st->kind == 2) {
              *p = 0x30C;
            } else {
              *p = 0x384;
            }
            *(q = &g_dwSpyroRequestMask) |= 0x80020000;
            D_80078C5E = st->heading;
          }
          break;
        }

        case 2:
          if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
            actor->mode = 2;
            continue;
          }
          break;
        }
        break;
      }

      case 2: {
        int dz;

        g_nGameplayBlocked = 0;
        dz = st->home[2] - actor->posZ;
        if (st->link != -1 && g_pActorListBase[st->link].mode < 0x80) {
          TurnActorYawTowardAngle(actor,
                        ArcTan2(st->home[0] - actor->posX,
                                      st->home[1] - actor->posY, 0),
                        6, 0, 0);
        } else {
          TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        }
        if (ApproxDist2D(&actor->posX, st->home) > 0x50) {
          MoveActorAlongAngle(actor,
                        ArcTan2(st->home[0] - actor->posX,
                                      st->home[1] - actor->posY, 0),
                        0x64, 0, 0, 0);
        } else {
          actor->posX = st->home[0];
          actor->posY = st->home[1];
        }
        if (ABS2(dz) > 0x28) {
          if (dz < 0) {
            if (dz < -0x32) {
              dz = -0x32;
            }
          } else if (dz > 0 && dz > 0x32) {
            dz = 0x32;
          }
          actor->posZ += dz;
        }
        if (ApproxDist2D(&actor->posX, st->home) < 0x51 && ABS2(dz) < 0x29) {
          actor->subMode = 0;
          actor->mode = 0;
          continue;
        }
        break;
      }

      case 3:
        if (WalkActorAlongWaypoints(actor, st->path, 0x200, 0x96, 0, 6, 0x10, 0) ==
            0x100) {
          actor->subMode = 0;
          CopyVector(st->home, &actor->posX);
          actor->mode = 0;
          continue;
        }
        break;

      case 4: {
        int h = (ArcTan2(actor->posX - st->path->nodes[0].x,
                               actor->posY - st->path->nodes[0].y, 0) +
                 8) &
                0xFF;

        ANIM_GO(actor, 2);
        if (actor->subMode != 0 || g_pActorListBase[st->link].mode >= 0x80) {
          actor->subMode = 0;
          st->alert = 0;
          ANIM_GO(actor, 0);
          actor->mode = 2;
          continue;
        }
        SteerActorAroundPoint(actor, &st->path->nodes[0].x, 0xBB8, h, 8, 0x78, 0xE,
                      0x80, 0xFF, 0xFF, 0, 0, 0);
        break;
      }
      }
      RefreshActorDerivedState(actor, 4);
      GetActorAttachPoint(actor, 0, x272);
      g_pfnLevelEmitParticles(2, 0x42, x272, (void *)2);
      break;
    }

    /* Types 152/153: sparking debris. Falls under gravity, spinning, and
     * pops when it runs out of life or reaches the floor. */
    case 152:
    case 153: {
      DebrisFallState *st = actor->state;

      if (actor->drawn == 0) {
        DespawnActorRecord(actor);
        continue;
      }
      if (--actor->subMode == 0 || actor->posZ < st->floor) {
        g_pfnLevelEmitParticles(8, 0x46, &actor->posX, (void *)0x10);
        DespawnActorRecord(actor);
        continue;
      }
      AddVector(&actor->posX, &actor->posX, st->vel);
      st->vel[2] -= 0xC;
      actor->rotX += st->spin[0];
      actor->rotY += st->spin[1];
      actor->rotZ += st->spin[2];
      if (!(actor->subMode & 1)) {
        CopyVector(fx, &actor->posX);
        fx[3] = GetRandomU32() & 3;
        fx[4] = GetRandomU32() & 3;
        fx[5] = 0x14;
        col[0] = 0x80;
        col[1] = 0x70;
        col[2] = 0x40;
        g_pfnLevelEmitParticles(1, 0x11, fx, col);
      }
      break;
    }

    /* Key (SpinState). */
    case 173: {
      SpinState *st;
      int dz;
      int ang;
      int i;

      st = (SpinState *)actor->state;
      if (actor->subMode != 0) {
        continue;
      }

      actor->rotX = (unsigned short)g_anCosineLut[st->angle] >> 7;
      actor->rotY = (unsigned short)g_anSineLut[st->angle] >> 7;
      st->angle = st->angle + g_nFrameStep * 2;

      if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x200) {
        dz = (actor->posZ - actor->floorOffset) - SPY.posZ;
        if (dz > 0 ? dz < 0x200
                   : SPY.posZ - (actor->posZ - actor->floorOffset) < 0x200) {
          PlaySoundEffect(g_pLevelSampleBankHeader[0], actor, 0x10, 0);
          RefreshActorDerivedState(actor, 4);

          for (i = 0; i < 6; i++) {
            g_pfnLevelEmitParticles(1, 0xC, (int *)actor, (void *)(0x8080 + 0x1000000 * i));
          }

          g_nLevelEggCount = 1;
          actor->unk57 = 0x40;
          actor->unk50 = 0;
          actor->updateDist = 0;
          actor->drawn = 0;
          actor->unk1C = 0;
          actor->unk4A = 0xFF;
          actor->subMode = 2;
        }
      }

      if (st->anim != 0xFF) {
        RefreshActorDerivedState(actor, 4);
        RotateVectorByMatrix(actor->mtx, &g_anRotBillboardOffset4, g_aRotBillboards[st->anim].pos);
        AddVector(g_aRotBillboards[st->anim].pos, g_aRotBillboards[st->anim].pos,
                      &actor->posX);
        if (g_aRotBillboards[st->anim].life < 5) {
          st->anim = 0xFF;
        }
      } else if (st->timer >= 0xF8) {
        ang = SpawnActorRotBillboard(actor, &g_anRotBillboardOffset4);
        if (ang >= 0 && ApproxDist2D(&actor->posX, &SPY.posX) < 0x4000) {
          st->anim = ang;
        }
        st->timer = (GetRandomU32() & 0x3F) + 0x18;
      }

      st->timer = st->timer + g_nFrameStep;
      continue;
    }

    /* Wooden chest (BreakState). */
    case 194: {
      BreakState *st;
      int i;

      st = (BreakState *)actor->state;
      if (st->unk04 != 0) {
        SnapActorToGround(actor);
      }
      if ((actor->flags & 0xB0000) == 0) {
        continue;
      }

      actor->unk55 = 0x20;
      PlayActorMeshVoice(actor, 0, 0);

      /* Two of each of the first pair, then six of the third. */
      for (i = 0; i < 2; i++) {
        if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
          break;
        }
        g_pfnLevelSpawnActor(0xFF, actor);
        g_pfnLevelSpawnActor(0x100, actor);
      }
      for (i = 0; i < 6; i++) {
        if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
          break;
        }
        g_pfnLevelSpawnActor(0x101, actor);
      }

      g_pfnLevelEmitParticles(5, 2, &actor->posX, 0);
      g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (void *)0x20);
      SpawnActorDeathDrop(actor, 3, 0, 0);
      MarkActorCommittedKill(actor);
      DespawnActorRecord(actor);
      continue;
    }

    /* Wobbling chest (WobbleState): plays a recoil pose, then breaks. */
    case 195: {
      WobbleState *st = actor->state;

      if (st->unk10 != 0) {
        SnapActorToGround(actor);
      }

      if (st->phase != 0) {
        int phase = st->phase + g_nFrameStep;

        st->phase = phase;

        if (phase < 0x40) {
          int dx;
          int dy;

          actor->rotX = g_aShakeRotOffsets[st->phase >> 1].a + st->rotX;
          actor->rotY = g_aShakeRotOffsets[st->phase >> 1].b + st->rotY;

          dx = (signed char)g_aShakeRotOffsets[st->phase >> 1].a;
          dy = (signed char)g_aShakeRotOffsets[st->phase >> 1].b;
          if (dx < 0) {
            dx = -dx;
          }
          if (dy < 0) {
            dy = -dy;
          }
          dx += dy;
          actor->posZ = st->restZ + dx * 6;
        } else {
          st->phase = 0;
          actor->rotX = st->rotX;
          actor->rotY = st->rotY;
          actor->posZ = st->restZ;
        }
      }

      if (actor->flags & 0xA0000) {
        int i;

        actor->unk55 = 0x20;
        PlayActorMeshVoice(actor, 0, 0);

        for (i = 0; i < 2; i++) {
          if ((g_nActorPoolCapacity - g_nActorPoolLiveCount) < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0x136, actor);
          g_pfnLevelSpawnActor(0x137, actor);
        }

        for (i = 0; i < 6; i++) {
          if ((g_nActorPoolCapacity - g_nActorPoolLiveCount) < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0x135, actor);
        }

        g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (int *)0x18);
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        DespawnActorRecord(actor);
      } else {
        st->handle = SelectActorLodBand(actor, st->handle);

        if ((actor->flags & 0x10000) && st->phase == 0) {
          st->phase = 1;
          st->rotX = actor->rotX;
          st->rotY = actor->rotY;
          st->restZ = actor->posZ;
        }
      }

      actor->flags = 0;
      break;
    }

    /* Crystal dragon (RideState). */
    case 250: {
      RideState *st = (RideState *)actor->state;

      if (actor->mode == 0) {
        int padIdx = st->unk20;
        if (padIdx != -1) {
          g_pActorListBase[padIdx].mode = 3;
        }
        actor->mode = 1;
        st->unk48 = actor->rotX;
        st->unk4C = actor->rotY;
        st->unk50 = actor->posZ;
      } else if (actor->mode == 1) {
        st->unk44 += g_nFrameStep;
        if (st->unk44 > 256) {
          st->unk44 = 0;
          actor->rotX = st->unk48;
          actor->rotY = st->unk4C;
          actor->posZ = st->unk50;
          actor->flags = 0;
          StopSoundVoicesByOwner(actor, 1);
        } else if (st->unk44 >= 192) {

          if (!GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[0])) {
            PlayActorMeshVoice(actor, 0, 0);
          }
          actor->rotX =
              (signed char)g_aShakeRotOffsets[(st->unk44 - 192) >> 1].a + st->unk48;
          actor->rotY =
              (signed char)g_aShakeRotOffsets[(st->unk44 - 192) >> 1].b + st->unk4C;
          actor->posZ =
              st->unk50 +
              (ABS2((signed char)g_aShakeRotOffsets[(st->unk44 - 192) >> 1].a) +
               ABS2((signed char)g_aShakeRotOffsets[(st->unk44 - 192) >> 1].b)) *
                  6;
        } else if (st->unk44 >= 188) {
          st->unk48 = actor->rotX;
          st->unk4C = actor->rotY;
          st->unk50 = actor->posZ;
        } else if (actor->flags != 0) {
          st->unk44 = 188;
          StopSoundVoicesByOwner(actor, 1);
        } else {
          if ((st->unk44 >> 1) == 16 && (GetRandomU32() & 3) == 0) {
            SpawnActorRotBillboard(actor, &g_anRotBillboardOffset0);
          } else if ((st->unk44 >> 1) == 48 && (GetRandomU32() & 3) == 0) {
            SpawnActorRotBillboard(actor, &g_anRotBillboardOffset1);
          } else if ((st->unk44 >> 1) == 80 && (GetRandomU32() & 3) == 0) {
            SpawnActorRotBillboard(actor, &g_anRotBillboardOffset2);
          }
        }

        if (ApproxDist2D(&actor->posX, &SPY.posX) < 2048) {

          SubtractVector(svK, &actor->posX, &SPY.posX);
          svK[2] = (svK[2] * 3) >> 2;
          if (VectorLength(svK, 1) < 1088) {
            int rotZ;
            int cutsceneId;
            if (st->unk20 != -1) {
              rotZ = g_pActorListBase[st->unk20].rotZ;
            } else {
              rotZ = st->unk1C;
            }
            CommitActorKillGems(0, actor);
            SetSpawnAnchorFromActor(actor, rotZ);

            if (st->unk24 != -1) {
              g_anLevelDragonsRescued[g_nLevelIntroIndex]++;
              g_nWorldDragonsRescued++;
              StubRewardHook(st->unk24, 0);
              if (st->unk20 != -1) {
                g_pActorListBase[st->unk20].mode = 1;
              }
              DespawnActorRecord(actor);
              continue;
            } else if (cutsceneId = st->unk18, cutsceneId == st->unk24) {
              g_anLevelDragonsRescued[g_nLevelIntroIndex]++;
              g_nWorldDragonsRescued++;
              if (st->unk20 != cutsceneId) {
                g_pActorListBase[st->unk20].mode = 1;
              }
              DespawnActorRecord(actor);
              continue;
            } else {
              actor->mode = 2;
              actor->rotX = st->unk48;
              actor->rotY = st->unk4C;
              actor->posZ = st->unk50;
              BeginGemPickupOverlay(actor);
              ZeroVector(&SPY.headLook);
              SPY.controlFlags = 0x80002147;
              if (SPY.u09C != 0) {
                SPY.u218 = 6;
                SPY.u208 = SPY.u10C >> 8;
                SPY.u20C = SPY.u110 >> 8;
                if (SPY.u114 > 0) {
                  SPY.u210 = 0;
                } else {
                  SPY.u210 = SPY.u114 >> 6;
                }
              } else {
                SPY.u218 = 3;
                CopyVector(&SPY.u208, &SPY.u10C);
                RShiftVector3(&SPY.u208, 6);
                SPY.u210 = 0;
                if (SPY.u208 != 0 || SPY.u20C != 0) {
                  ScaleVector3ByRatio(&SPY.u208, VectorLength(&SPY.u208, 0), 0x60);
                }
                SPY.u210 = 0;
              }
            }
          }
        }
      } else {
        SPY.controlFlags = 0x80002000;
      }
      break;
    }

    /* Dragon fragment, updated out of line. */
    case 251:
      TickGemPickupBurstEntity(actor);
      break;

    /* Floating digits (DebrisState). */
    case 260:
    case 261:
    case 262:
    case 263:
    case 264:
    case 265:
    case 266:
    case 267:
    case 268:
    case 269: {
      DebrisState *st = actor->state;

      if (!(actor->unk50 & 0x80) && (actor->unk50 != 0)) {

        if (st->life >= 1) {

          actor->rotZ += 1;

          st->vel[2] -= 6;
          if (st->vel[2] < -0x80) {
            st->vel[2] = -0x80;
          }

          actor->posX += st->vel[0];
          actor->posY += st->vel[1];
          actor->posZ += st->vel[2];

          if (actor->posZ > 1023) {
            if (CollideSphereWithWorldAndActors(&actor->posX, 0x100, 0x100)) {
              int *nrm = &g_anCollisionProbeVec;
              int dot;

              actor->posX = g_anCollisionResolvedPos;
              actor->posY = AS_I32(g_nCollisionResolvedY);
              actor->posZ = AS_I32(g_nCollisionResolvedZ);

              SetVector3Magnitude(nrm, 0x1000);

              dot = (st->vel[0] * nrm[0] + st->vel[1] * g_nCollisionProbeVecY +
                     st->vel[2] * g_nCollisionProbeVecZ) >>
                    11;

              if (dot < 0) {
                ScaleVector3ByRatio(nrm, 0x1000, -dot);
                st->vel[0] += nrm[0];
                st->vel[1] += AS_I32(g_nCollisionProbeVecY);
                st->vel[2] += AS_I32(g_nCollisionProbeVecZ);
              }
            }
            st->life--;
          } else {
            DespawnActorRecord(actor);
          }
        } else {
          g_pfnLevelEmitParticles(10, 0x47, &actor->posX, 0);
          DespawnActorRecord(actor);
        }
      }
      break;
    }

    /* Waypoint rider (RideState30): steps a closed path and calls out a voice
     * line per leg. */
    case 286: {
      RideState30 *st = actor->state;
      int voice = -1;

      actor->unk55 = st->volume;

      switch (st->pose) {
      case 0:
        voice = 6;
        break;

      case 1:
        if (TickCountdownByFrameDelta(&st->wait, 4) != 0) {
          st->wait = 0x258 - (GetRandomU32() & 0x7F);
          voice = (GetRandomU32() & 7) + 7;
        }
        break;

      case 2:
        switch (st->leg) {
        case 0:
          if (TickCountdownByFrameDelta(&st->wait, 4) != 0) {
            st->wait = 0x200 - (GetRandomU32() & 0x7F);
            voice = (GetRandomU32() & 1) + 0xD;
            st->leg = 1;
          }
          break;

        case 1: {
          int left = st->left;

          st->left = left - 1;

          if (left > 0) {
            actor->posX += st->step[0];
            actor->posY += st->step[1];
            actor->posZ += st->step[2];
          } else {
            PathData *path = st->path;
            int next = (path->cur + 1) % path->count;

            actor->posX = path->nodes[next].x;
            actor->posY =
                st->path->nodes[(st->path->cur + 1) % st->path->count].y;
            actor->posZ =
                st->path->nodes[(st->path->cur + 1) % st->path->count].z;
            st->leg = 0;
            st->path->cur = (st->path->cur + 1) % st->path->count;
            st->left = ComputeStepTowardPoint(
                &st->path->nodes[st->path->cur].x,
                &st->path->nodes[(st->path->cur + 1) % st->path->count].x,
                st->step, st->speed);
          }

          if (actor->sfxSlot == 0x7F && st->left == (st->left / 48) * 48) {
            voice = 0xE;
          }
          break;
        }
        }
        break;

      case 3:
        if (TickCountdownByFrameDelta(&st->wait, 4) != 0) {
          st->wait = 0x270 - (GetRandomU32() & 0x7F);
          voice = (GetRandomU32() & 1) + 0xF;
        }
        break;

      case 4:
        if (TickCountdownByFrameDelta(&st->wait, 4) != 0) {
          int r;

          st->wait = 0x258 - (GetRandomU32() & 0x7F);
          r = GetRandomU32();
          voice = (r % 4) + 0x15;
        }
        break;

      case 5:
        voice = 0x18;
        break;

      case 6:
        voice = 0x19;
        break;

      case 7:
        voice = 0x12;
        break;

      case 8:
        voice = 0x1A;
        break;

      case 9:
        voice = 0x36;
        break;
      }

      if (voice >= 0) {
        PlaySoundEffect(g_pLevelSampleBankHeader[voice], actor, 8, &actor->sfxSlot);
      }

      break;
    }

    /* Homing wisp (WispState). */
    case 295: {
      WispState *st = actor->state;
      int i;

      if (actor->posX < 0x400 || actor->posY < 0x400 || actor->posZ < 0x400) {
        DespawnActorRecord(actor);
        continue;
      }
      if (st->target != 0) {
        CopyVector(svK, &st->target->posX);
        svK[2] += 0x2BC;
      } else {
        CopyVector(svK, &SPY.posX);
      }
      {
        int d = (ArcTan2(svK[0] - actor->posX, svK[1] - actor->posY, 0) -
                 actor->rotZ) &
                0xFF;

        if (d > 0x80) {
          d -= 0x100;
        }
        if (d > 2) {
          d = 2;
        }
        if (d < -2) {
          d = -2;
        }
        actor->rotZ += d;
      }
      CopyVector(svL, &actor->posX);
      if (RaiseSpyroTriggerInRadius(&actor->posX, 0x64, 0x86) != 0) {
        for (i = 0; i < 3; i++) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0x13E, actor);
        }
        g_anSpyroRequestVelocity.x = g_anCosineLut[actor->rotZ] >> 5;
        g_nSpyroRequestVelocityY = g_anSineLut[actor->rotZ] >> 5;
        g_nSpyroRequestVelocityZ = 0;
        DespawnActorRecord(actor);
        continue;
      }
      if (TickCountdownByFrameDelta(&st->timer, 4) != 0 ||
          TryMoveActorForward(actor, 0x12C, 0, 0x100, 0) != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      if (st->target != 0) {
        if (ApproxDist2D(&actor->posX, svK) < 0x200) {
          int dz = actor->posZ - svK[2];

          if (ABS(dz) < 0x200) {
            for (i = 0; i < 3; i++) {
              if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
                break;
              }
              g_pfnLevelSpawnActor(0x13E, actor);
            }
            st->target->flags |= 1;
            DespawnActorRecord(actor);
            continue;
          }
        }
      } else {
        g_pfnLevelEmitParticles(1, 5, &actor->posX, 0);
      }
      if (CastRayWorldAndActors(svL, &actor->posX) != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      break;
    }

    /* Type 301. */
    case 301: {
      L14State301 *st = actor->state;

      if ((actor->flags & 0x90000) && actor->mode != 4) {
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        actor->mode = 4;
        ANIM_RESET(actor, 4);
        continue;
      }
      if ((actor->flags & 1) && actor->mode < 4) {
        actor->mode = 5;
        ANIM_RESET(actor, 5);
        continue;
      }
      switch (actor->mode) {
      case 0:
        if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x1000 &&
            ABS2(SPY_DZ(actor)) < 0xC00) {
          if (st->flags & 1) {
            st->flags |= 2;
            ENTER_POSE(actor, 2);
            continue;
          }
          ENTER_POSE(actor, 1);
          continue;
        }
        if (st->target >= 0) {
          if (g_pActorListBase[st->target].mode >= 0x80) {
            st->target = -1;
          } else if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
            ENTER_POSE(actor, 2);
            continue;
          } else {
            TurnActorYawTowardAngle(actor,
                          ArcTan2(g_pActorListBase[st->target].posX - actor->posX,
                                        g_pActorListBase[st->target].posY - actor->posY,
                                        0),
                          8, 0x10, 1);
          }
        }
        break;

      case 1:
        TrackEntityYawTowardSpyro(actor, 8, 0x10, 1);
        if (g_nActorAnimDone != 0) {
          st->flags |= 1;
          actor->mode = 0;
          ANIM_RESET(actor, 0);
          continue;
        }
        break;

      case 2:
        if (st->flags & 2) {
          if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x1800 &&
              ABS2(SPY_DZ(actor)) < 0xC00) {
            TrackEntityYawTowardSpyro(actor, 8, 0x10, 1);
          } else {
            st->flags &= ~2;
            ENTER_POSE(actor, 0);
            continue;
          }
        } else {
          TurnActorYawTowardAngle(actor,
                        ArcTan2(g_pActorListBase[st->target].posX - actor->posX,
                                      g_pActorListBase[st->target].posY - actor->posY,
                                      0),
                        8, 0x10, 1);
        }
        if (g_nActorAnimDone != 0) {
          st->timer = 0;
          st->count = 0;
          actor->mode = 3;
          ANIM_RESET(actor, 3);
          continue;
        }
        break;

      case 3:
        if (TickCountdownByFrameDelta(&st->timer, 4) && actor->nextFrame >= 4 &&
            st->count < 3) {
          Actor *b = g_pfnLevelSpawnActor(0x127, actor);

          if (st->flags & 2) {
            if (ApproxDist2D(&actor->posX, &SPY.posX) < 0xA00 &&
                ABS2(SPY_DZ(actor)) < 0x400) {
              CopyVector(&b->posX, &SPY.posX);
            }
          } else {
            Actor **bs = b->state;

            CopyVector(v376, &g_pActorListBase[st->target].posX);
            v376[2] += 0x2BC;
            b->rotY = ArcTan2(ApproxDist2D(&actor->posX, v376),
                                     v376[2] - actor->posZ, 0);
            b->rotZ = ArcTan2(v376[0] - actor->posX,
                                     v376[1] - actor->posY, 0);
            b->unk50 = 0x24;
            *bs = &g_pActorListBase[st->target];
          }
          st->timer = 4;
          st->count++;
        }
        if (g_nActorAnimDone != 0) {
          if ((st->flags & 2) &&
              ApproxDist2D(&actor->posX, &SPY.posX) < 0x1000 &&
              ABS2(SPY_DZ(actor)) < 0xC00) {
            actor->mode = 2;
            ANIM_RESET(actor, 2);
            continue;
          }
          st->flags &= ~2;
          st->timer = (GetRandomU32() & 0x1F) + 0x20;
          actor->mode = 0;
          ANIM_RESET(actor, 0);
          continue;
        }
        break;

      case 4:
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x30);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case 5:
        if (g_nActorAnimDone != 0) {
          actor->flags = 0;
          actor->mode = 0;
          ANIM_RESET(actor, 0);
          continue;
        }
        break;
      }
      RefreshActorDerivedState(actor, 1);
      break;
    }

    /* Type 302. */
    case 302: {
      L14State302 *st = actor->state;
      int i;

      if (st->riding != 0) {
        if (st->node >= 0) {
          UnpackWorldCollisionTri(st->node, t392[0]);
          ZeroVector(v376);
          for (i = 0; i < 3; i++) {
            SubtractVector(t392[i], t392[i], st->pts[i]);
            AddVector(v376, v376, t392[i]);
          }
          v376[0] /= 3;
          v376[1] /= 3;
          v376[2] /= 3;
          AddVector(&actor->posX, &actor->posX, v376);
        }
        actor->posZ += 0x400;
        RefreshActorDerivedState(actor, 2);
        i = FindGroundHeightBelow(&actor->posX, 0x1000);
        if (i > 0) {
          actor->posZ = i;
          st->node = g_nLastCollisionHitChunkId;
          UnpackWorldCollisionTri(g_nLastCollisionHitChunkId, st->pts[0]);
        } else {
          st->node = -1;
        }
      }
      if ((actor->flags & 0xB0000) && actor->mode != 3) {
        Actor *hit;

        StopSoundVoiceByIndex(actor->sfxSlot, 4);
        hit = SpawnActorDeathDrop(actor, 4, 0, 0);
        if (hit != 0) {
          ((unsigned char *)hit->state)[0xE] = 3;
        }
        MarkActorCommittedKill(actor);
        ENTER_POSE(actor, 3);
        continue;
      }
      if ((actor->flags & 1) && actor->mode < 3) {
        ENTER_POSE(actor, 4);
        continue;
      }
      switch (actor->mode) {
      case 0: {
        int d;
        int r;

        if (st->riding != 0) {
          ENTER_POSE(actor, 1);
          continue;
        }
        d = ApproxDist2D(&SPY.posX, (int *)(st->anchor + 0x18));
        if (d < 0x2000 && st->spark != 0 && (GetRandomU32() & 1)) {
          g_pfnLevelEmitParticles(1, 8, actor, &g_pActorListBase[st->spark]);
        }
        r = GetWorldChunkAnimState(st->snd);
        if (r & 2) {
          r = (r >> 8) & 0xFF;
          if (r == 0 && d < 0x2000) {
            ModifyWorldChunkAnimFlags(st->snd, 0xFC, 0);
            ENTER_POSE(actor, 2);
            continue;
          }
          if (r == 0xE && d > 0x3000) {
            ModifyWorldChunkAnimFlags(st->snd, 0xFC, 0);
            ENTER_POSE(actor, 2);
            continue;
          }
        }
        break;
      }

      case 1:
        if (st->spark >= 0 && (GetRandomU32() & 1)) {
          g_pfnLevelEmitParticles(1, 8, actor, &g_pActorListBase[st->spark]);
        }
        ModifyWorldChunkAnimFlags(st->snd, 0xFC, 0);
        break;

      case 2:
        if (g_nActorAnimDone != 0) {
          actor->mode = 0;
          ANIM_RESET(actor, 0);
          continue;
        }
        if (st->spark != 0 && (GetRandomU32() & 1) &&
            ApproxDist2D(&SPY.posX, (int *)(st->anchor + 0x18)) < 0x2000) {
          g_pfnLevelEmitParticles(1, 8, actor, &g_pActorListBase[st->spark]);
        }
        break;

      case 3:
        if (st->riding == 0) {
          int r = GetWorldChunkAnimState(st->snd);

          if (r & 2) {
            r = (r >> 8) & 0xFF;
            if (r == 0xE) {
              ModifyWorldChunkAnimFlags(st->snd, 0xFC, 0);
            }
          }
        }
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x18);
          DespawnActorRecord(actor);
          continue;
        }
        RefreshActorDerivedState(actor, 1);
        break;

      case 4:
        if (st->spark >= 0) {
          if (GetRandomU32() & 1) {
            g_pfnLevelEmitParticles(1, 8, actor, &g_pActorListBase[st->spark]);
          }
        }
        ModifyWorldChunkAnimFlags(st->snd, 0xFC, 0);
        if (g_nActorAnimDone != 0) {
          actor->flags = 0;
          actor->mode = 1;
          ANIM_RESET(actor, 1);
          continue;
        }
        break;
      }
      break;
    }

    /* Type 303. */
    case 303: {
      L14State303 *st = actor->state;

      if ((actor->flags & 0xB0000) && actor->mode != 4) {
        StopSoundVoiceByIndex(actor->sfxSlot, 4);
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        ENTER_POSE(actor, 4);
        continue;
      }
      if ((actor->flags & 1) && actor->mode != 2 && actor->mode != 4) {
        ENTER_POSE(actor, 2);
        continue;
      }
      TickCountdownByFrameDelta(&st->timer, 4);
      switch (actor->mode) {
      case 0:
        if (st->timer == 0 && !(st->flags & 1)) {
          st->timer = 0xB4;
          ENTER_POSE(actor, 3);
          continue;
        }
        if (TickCountdownByFrameDelta(&st->wait, 4) != 0) {
          ENTER_POSE(actor, 1);
          continue;
        }
        break;

      case 1:
        if (g_nActorAnimDone != 0) {
          st->wait = (GetRandomU32() & 0x1F) + 0x30;
          ANIM_QUEUE(actor, 0);
        }
        break;

      case 2:
        if (g_nActorAnimDone != 0) {
          actor->flags = 0;
          ANIM_QUEUE(actor, 0);
        }
        break;

      case 3:
        if (actor->nextFrame >= 5 && !(st->flags & 1)) {
          st->link[1] = 0;
          ((short *)g_pfnLevelSpawnActor(0x130, actor)->state)[10] = st->value;
          st->flags |= 1;
        }
        if (g_nActorAnimDone != 0) {
          ANIM_QUEUE(actor, 0);
        }
        break;

      case 4:
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x18);
          StopSoundVoicesByOwner(actor, 2);
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      break;
    }

    /* Type 304. */
    case 304: {
      L14State304 *st = actor->state;

      switch (actor->mode) {
      case 0:
        if (g_nActorAnimDone != 0) {
          actor->mode = 1;
          ANIM_RESET(actor, 1);
          continue;
        }
        if (st->link->mode >= 4) {
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case 1: {
        int *pos = &actor->posX;

        if (RaiseSpyroTriggerInRadius(pos, 0x280, 0x46) != 0) {
          ((int *)st->link->state)[1] &= ~1;
          ENTER_POSE(actor, 2);
          continue;
        }
        if (WalkActorAlongWaypoints(actor, st->path, 0x100, 0x40, 0, 0x20, 0xFF,
                          &st->hold) == 0x100) {
          ((int *)st->link->state)[1] &= ~1;
          ENTER_POSE(actor, 2);
          continue;
        }
        if (st->target >= 0 && g_pActorListBase[st->target].mode < 0x80 &&
            ApproxDist2D(pos, &g_pActorListBase[st->target].posX) < 0x280) {
          ((int *)st->link->state)[1] &= ~1;
          g_pActorListBase[st->target].flags |= 1;
          ENTER_POSE(actor, 2);
          continue;
        }
        break;
      }

      case 2:
        if (g_nActorAnimDone != 0) {
          StopSoundVoicesByOwner(actor, 2);
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      FindGroundHeightBelow(&actor->posX, 0x10000);
      EncodeCachedVecToActorDirCode(actor);
      break;
    }

    /* Type 305. */
    case 305: {
      L14State305 *st = actor->state;

      func_8003AA84(actor);
      if (actor->mode == 2) {
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x30);
          DespawnActorRecord(actor);
          continue;
        }
        if (st->speed < 0x40) {
          st->speed = 0x40;
        }
        actor->posX += (st->speed * LookupCosine(st->heading)) >> 12;
        actor->posY += (st->speed * LookupSine(st->heading)) >> 12;
        continue;
      }
      if (actor->flags & 0x80000) {
        st->heading = SPY.u11C;
        st->speed = SPY.u128 >> 5;
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        ENTER_POSE(actor, 2);
        continue;
      }
      actor->flags = 0;
      if (SPY.state == 0xE) {
        st->timer = 0x78;
        st->mode |= 0x10;
      }
      if (g_nSpyroAirborneFrames == 0) {
        if (ABS2(actor->posZ - SPY.posZ + 0x164) < 0x140 &&
            ApproxDist2D(&actor->posX, &SPY.posX) < st->range) {
          st->mode |= 8;
          ZeroVector(st->vel);
        } else {
          st->mode &= ~8;
        }
        switch (st->mode & 3) {
        case 0:
          if (ApproxDist2D(&SPY.posX, PATH_END(st->pathA)) < 0xC00) {
            st->pathA->cur = st->pathA->count - 1;
            st->cur = st->pathA;
            st->mode |= 4;
          }
          break;

        case 1:
          if (ApproxDist2D(&SPY.posX, PATH_END(st->pathA)) < 0xC00) {
            st->pathA->cur = st->pathA->count - 1;
            st->cur = st->pathA;
            st->mode = (st->mode | 4) & ~3;
          }
          break;

        case 2:
          if (ApproxDist2D(&SPY.posX, PATH_END(st->pathA)) < 0xC00) {
            st->pathA->cur = st->pathA->count - 1;
            st->cur = st->pathA;
            st->mode = (st->mode | 4) & ~3;
          } else if (ApproxDist2D(&SPY.posX, PATH_END(st->pathB)) < 0xC00) {
            st->cur = st->pathA;
            st->cur->cur = 0;
            st->mode = (st->mode & ~3) | 1;
          }
          break;

        case 3:
          if (ApproxDist2D(&SPY.posX, PATH_END(st->pathA)) < 0xC00) {
            st->pathA->cur = st->pathA->count - 1;
            st->cur = st->pathA;
            st->mode = (st->mode | 4) & ~3;
          } else if (ApproxDist2D(&SPY.posX, PATH_END(st->pathB)) < 0xC00) {
            st->cur = st->pathA;
            st->cur->cur = 0;
            st->mode = (st->mode & ~3) | 1;
          } else if (ApproxDist2D(&SPY.posX, PATH_END(st->pathC)) < 0xC00) {
            st->cur = st->pathB;
            st->cur->cur = 0;
            st->mode = (st->mode & ~3) | 2;
          }
          break;
        }
      }
      if (st->mode & 0x10) {
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          st->mode &= ~0x10;
        }
        if (actor->mode != 0) {
          ENTER_POSE(actor, 0);
          continue;
        }
      } else if (st->mode & 8) {
        RaiseSpyroTriggerInRadius(&actor->posX, 0x540, 7);
        if (actor->mode != 3) {
          ENTER_POSE(actor, 3);
          continue;
        }
        actor->frameRate = 0x10;
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          if (TrackEntityYawTowardSpyro(actor, 8, 0x30, 1) != 0 &&
              MoveActorAlongHeading(actor, st->speed, 0, 0x400, 0x15) == 2) {
            st->timer = 0x12;
            st->heading = ArcTan2(g_anCollisionContactPoint[0] - actor->posX,
                                        g_anCollisionContactPoint[1] - actor->posY, 0);
            if (((st->heading - actor->rotZ) & 0xFF) < 0x80) {
              st->heading = (st->heading - 0x40) & 0xFF;
            } else {
              st->heading = (st->heading + 0x40) & 0xFF;
            }
          }
        } else {
          TurnActorYawTowardAngle(actor, st->heading, 8, 0x30, 1);
          MoveActorAlongHeading(actor, st->speed, 0, 0x400, 0x15);
        }
      } else if (st->mode & 4) {
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          if (st->mode & 0x20) {
            st->cur->cur = 0;
          }
          if (ApproxDist2D(&actor->posX, PATH_CUR(st->cur)) > 0x100) {
            if (actor->mode != 0) {
              ENTER_POSE(actor, 0);
              continue;
            }
            actor->frameRate = 8;
            if (TurnActorYawTowardAngle(actor,
                              ArcTan2(NODE_PT(st->cur)[0] - actor->posX,
                                            NODE_PT(st->cur)[1] - actor->posY,
                                            0),
                              8, 0x30, 1) != 0 &&
                MoveActorAlongHeading(actor, 0x60, 0, 0x400, 0x15) == 2) {
              st->timer = 0x12;
              st->heading = ArcTan2(g_anCollisionContactPoint[0] - actor->posX,
                                          g_anCollisionContactPoint[1] - actor->posY, 0);
              if (((st->heading - actor->rotZ) & 0xFF) < 0x80) {
                st->heading = (st->heading - 0x40) & 0xFF;
              } else {
                st->heading = (st->heading + 0x40) & 0xFF;
              }
            }
          } else {
            if (actor->mode != 1) {
              ENTER_POSE(actor, 1);
              continue;
            }
            actor->frameRate = 0x10;
          }
        } else {
          TurnActorYawTowardAngle(actor, st->heading, 8, 0x30, 1);
          MoveActorAlongHeading(actor, 0x80, 0, 0x400, 0x15);
        }
      } else if (st->mode & 3) {
        if (st->mode & 0x40) {
          if (actor->mode != 1) {
            ENTER_POSE(actor, 1);
            continue;
          }
          if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
            st->mode &= ~0x40;
          }
        } else {
          if (actor->mode != 0) {
            ENTER_POSE(actor, 0);
            continue;
          }
          actor->frameRate = 8;
          if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
            if (ApproxDist2D(&actor->posX, PATH_CUR(st->cur)) < 0x100) {
              if (st->mode & 0x80) {
                if (st->cur->cur != 0) {
                  st->cur->cur--;
                } else {
                  st->mode = (st->mode & ~0x80) | 0x40;
                  st->timer = 0x3C;
                }
              } else if (st->cur->cur >= st->cur->count - 2) {
                st->mode |= 0xC0;
                st->timer = 0x3C;
              } else {
                st->cur->cur++;
              }
            }
            if (TurnActorYawTowardAngle(actor,
                              ArcTan2(NODE_PT(st->cur)[0] - actor->posX,
                                            NODE_PT(st->cur)[1] - actor->posY,
                                            0),
                              8, 0x30, 1) != 0 &&
                MoveActorAlongHeading(actor, 0x60, 0, 0x400, 0x15) == 2) {
              st->timer = 0x12;
              st->heading = ArcTan2(g_anCollisionContactPoint[0] - actor->posX,
                                          g_anCollisionContactPoint[1] - actor->posY, 0);
              if (((st->heading - actor->rotZ) & 0xFF) < 0x80) {
                st->heading = (st->heading - 0x40) & 0xFF;
              } else {
                st->heading = (st->heading + 0x40) & 0xFF;
              }
            }
          } else {
            TurnActorYawTowardAngle(actor, st->heading, 8, 0x30, 1);
            MoveActorAlongHeading(actor, 0x60, 0, 0x400, 0x15);
          }
        }
      } else {
        st->cur->cur = 0;
        if (ApproxDist2D(&actor->posX, PATH_CUR(st->cur)) > 0x100) {
          if (actor->mode != 0) {
            ENTER_POSE(actor, 0);
            continue;
          }
          actor->frameRate = 8;
          if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
            if (TurnActorYawTowardAngle(actor,
                              ArcTan2(NODE_PT(st->cur)[0] - actor->posX,
                                            NODE_PT(st->cur)[1] - actor->posY,
                                            0),
                              8, 0x30, 1) != 0 &&
                MoveActorAlongHeading(actor, 0x60, 0, 0x400, 0x15) == 2) {
              st->timer = 0x12;
              st->heading = ArcTan2(g_anCollisionContactPoint[0] - actor->posX,
                                          g_anCollisionContactPoint[1] - actor->posY, 0);
              if (((st->heading - actor->rotZ) & 0xFF) < 0x80) {
                st->heading = (st->heading - 0x40) & 0xFF;
              } else {
                st->heading = (st->heading + 0x40) & 0xFF;
              }
            }
          } else {
            TurnActorYawTowardAngle(actor, st->heading, 8, 0x30, 1);
            MoveActorAlongHeading(actor, 0x60, 0, 0x400, 0x15);
          }
        } else {
          if (actor->mode != 1) {
            ENTER_POSE(actor, 1);
            continue;
          }
          actor->frameRate = 0x10;
        }
      }
      RefreshActorDerivedState(actor, 1);
      break;
    }

    /* Sparkling shard: 1 puff per frame. */
    case 309: {
      ShardState *st = actor->state;

      if (st->life != 0 && actor->drawn != 0) {
        actor->posX += st->vel[0];
        actor->posY += st->vel[1];
        st->vel[2] -= 6;
        if (st->vel[2] < -0x80) {
          st->vel[2] = -0x80;
        }
        actor->posZ += st->vel[2];
        actor->rotX += st->spin[0];
        actor->rotY += st->spin[1];
        actor->rotZ += st->spin[2];

        svM[0] = (GetRandomU32() & 0xFE) - 0x7F;
        svM[1] = (GetRandomU32() & 0xFE) - 0x7F;
        svM[2] = (GetRandomU32() & 0xFE) - 0x40;
        AddVector(svM, svM, &actor->posX);
        g_pfnLevelEmitParticles(1, 0x42, svM, (int *)1);
        st->life -= 1;
      } else {
        DespawnActorRecord(actor);
        continue;
      }

      break;
    }

    /* Wood shard: bounces and throws three sparks a frame. */
    case 310:
    case 311: {
      ShardState *st = actor->state;
      register int i asm("$16");

      if (st->life > 0) {
        if (actor->drawn == 0) {
          DespawnActorRecord(actor);
          continue;
        }

        if (CollideSphereWithWorldAndActors(&actor->posX, 0x100, 0x100) != 0) {
          int *nrm = &g_anCollisionProbeVec;
          int along;

          SetVector3Magnitude(nrm, 0x1000);
          along = (st->vel[0] * nrm[0]) + (st->vel[1] * g_nCollisionProbeVecY) +
                  (st->vel[2] * g_nCollisionProbeVecZ);
          i = along >> 11;

          if (i < 0) {
            ScaleVector3ByRatio(nrm, 0x1000, (along >> 13) - i);
            st->vel[0] += nrm[0];
            st->vel[1] += AS_I32(g_nCollisionProbeVecY);
            st->vel[2] += AS_I32(g_nCollisionProbeVecZ);
          }
        }

        actor->posX += st->vel[0];
        actor->posY += st->vel[1];
        st->vel[2] -= 6;
        if (st->vel[2] < -0x80) {
          st->vel[2] = -0x80;
        }
        actor->posZ += st->vel[2];
        actor->rotX += st->spin[0];
        i = 0;
        {
          actor->rotY += st->spin[1];
          actor->rotZ += st->spin[2];

          do {
            i += 1;
            svT[0] = (GetRandomU32() & 0xFE) - 0x7F;
            svT[1] = (GetRandomU32() & 0xFE) - 0x7F;
            svT[2] = (GetRandomU32() & 0xFE) - 0x40;
            AddVector(svT, svT, &actor->posX);
            g_pfnLevelEmitParticles(1, 0x42, svT, (int *)1);
          } while (i < 3);
        }

        st->life -= 1;
        break;
      }

      if (actor->drawn != 0) {
        g_pfnLevelEmitParticles(8, 0x46, &actor->posX, (int *)0x10);
      }
      DespawnActorRecord(actor);
      continue;
    }

    /* Type 312: cracking boulder. Shakes and smokes when hit by a charge or
     * flame, then bursts into three rings of debris and knocks the camera
     * back. */
    case 312: {
      L16Boulder *st = actor->state;

      switch (actor->subMode) {
      case 0:
        *(int *)&actor->unk4C = D_8006E52C[actor->unk53];
        actor->subMode = 1;
        break;

      case 1:
        if (actor->flags & 0x40000) {
          actor->subMode = 2;
          actor->updateDist = 0xFF;
          actor->unk50 = 0x7F;
          if (st->timer < 0x5A) {
            st->timer = 0x5A;
          }
        } else if (actor->flags & 0x10000) {
          actor->subMode = 2;
          actor->updateDist = 0xFF;
          actor->unk50 = 0x7F;
          st->timer = 0;
        } else {
          goto hit;
        }
        st->rotX = actor->rotX;
        st->rotY = actor->rotY;
        st->restZ = actor->posZ;
        break;

      hit:
        if (actor->flags & 0xA0000) {
          actor->subMode = 3;
          actor->unk50 = 0;
          actor->drawn = 0;
          st->timer = 0;
        }
        break;

      case 2: {
        int t = st->timer + g_nFrameStep;
        int i;
        int n;

        st->timer = t;
        if (t < 0x78) {
        if ((actor->flags & 0x40000) && t < 0x5A) {
          st->timer = 0x5A;
        }
        for (i = 0; i < 6; i++) {
          ps[0] = D_8006E614[i >> 1].pos[0];
          ps[1] = D_8006E614[i >> 1].pos[1];
          ps[2] = D_8006E614[i >> 1].pos[2];
          RotateVectorByMatrix(actor->mtx, ps, ps);
          AddVector(ps, ps, &actor->posX);
          ps[3] = D_8006E614[i >> 1].vel[0];
          ps[4] = D_8006E614[i >> 1].vel[1];
          ps[5] = D_8006E614[i >> 1].vel[2];
          RotateVectorByMatrix(actor->mtx, ps + 3, ps + 3);
          ps[3] += (GetRandomU32() & 0xF) - 8;
          ps[4] += (GetRandomU32() & 0xF) - 8;
          g_pfnLevelEmitParticles(1, 0x4A, ps, ps + 3);
        }
        n = st->timer / 40 + 1;
        for (i = 0; i < n; i++) {
          ps[0] = (GetRandomU32() & 0x1FF) - 0xFF;
          ps[1] = (GetRandomU32() & 0x1FF) - 0xFF;
          ps[2] = (GetRandomU32() & 0x7F) + 0xC0;
          ps[3] = ps[0] >> 4;
          ps[4] = ps[4] >> 4;
          ps[5] = (GetRandomU32() & 0xF) + 8;
          RotateVectorByMatrix(actor->mtx, ps, ps);
          AddVector(ps, ps, &actor->posX);
          RotateVectorByMatrix(actor->mtx, ps + 3, ps + 3);
          s496[0] = (GetRandomU32() & 7) + 0x10;
          s496[1] = (GetRandomU32() & 7) + 6;
          g_pfnLevelEmitParticles(1, 0x10, ps, s496);
          if (actor->sfxSlot == 0x7F) {
            PlayActorMeshVoice(actor, 0, 0);
          }
        }
        if (st->timer >= 0x39) {
          actor->rotX = g_aShakeRotOffsets[(0x78 - st->timer) >> 1].a + st->rotX;
          actor->rotY = g_aShakeRotOffsets[(0x78 - st->timer) >> 1].b + st->rotY;
          actor->posZ =
              st->restZ +
              (ABS2((signed char)g_aShakeRotOffsets[(0x78 - st->timer) >> 1].a) +
               ABS2((signed char)g_aShakeRotOffsets[(0x78 - st->timer) >> 1].b)) *
                  6;
        }
        } else {
          actor->subMode = 3;
          actor->unk50 = 0;
          actor->drawn = 0;
          st->timer = 0;
        }
        break;
      }

      case 3:
        if (st->timer == 0) {
          int i;
          Actor *c;
          DebrisFallState *cs;

          PlayActorMeshVoice(actor, 1, 0);
          SpawnActorDeathDrop(actor, 6, 0, 0);
          for (i = 0; i < 8; i++) {
            short *p;

            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            p = g_pfnLevelSpawnActor(0x97, actor)->state;
            p[0] >>= 1;
            p[1] >>= 1;
          }
          for (i = 0; i < 16; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            c = g_pfnLevelSpawnActor(0x99, actor);
            cs = c->state;
            c->subMode = (GetRandomU32() & 1) + 0x12;
            CopyVector(&c->posX, &actor->posX);
            c->posX += (g_anCosineLut[i * 16] >> 4) + (GetRandomU32() & 0x1F) - 0xF;
            c->posY += (g_anSineLut[i * 16] >> 4) + (GetRandomU32() & 0x1F) - 0xF;
            c->posZ += (GetRandomU32() & 0xFF) + 0x80;
            cs->vel[0] = g_anCosineLut[i * 16] / 32 + (GetRandomU32() & 0x1F) - 0xF;
            cs->vel[1] = g_anSineLut[i * 16] / 32 + (GetRandomU32() & 0x1F) - 0xF;
            cs->vel[2] = (GetRandomU32() & 0x3F) + 0x30;
            c->rotX = GetRandomU32();
            c->rotY = GetRandomU32();
            c->rotZ = GetRandomU32();
            cs->spin[0] = GetRandomU32() & 0xF;
            cs->spin[1] = GetRandomU32() & 0xF;
            cs->spin[2] = GetRandomU32() & 0xF;
            cs->floor = actor->posZ;
          }
          for (i = 0; i < 12; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            c = g_pfnLevelSpawnActor(0x98, actor);
            cs = c->state;
            c->subMode = (GetRandomU32() & 3) + 0x19;
            CopyVector(&c->posX, &actor->posX);
            c->posX += (g_anCosineLut[(i << 8) / 12] >> 4) + (GetRandomU32() & 0x3F) - 0x1F;
            c->posY += (g_anSineLut[(i << 8) / 12] >> 4) + (GetRandomU32() & 0x3F) - 0x1F;
            c->posZ += (GetRandomU32() & 0xFF) + 0x80;
            cs->vel[0] = (short)(g_anCosineLut[(i << 8) / 12] / 56) + (GetRandomU32() & 0x1F) - 0xF;
            cs->vel[1] = (short)(g_anSineLut[(i << 8) / 12] / 56) + (GetRandomU32() & 0x1F) - 0xF;
            cs->vel[2] = (GetRandomU32() & 0x7F) + 0x60;
            c->rotX = GetRandomU32();
            c->rotY = GetRandomU32();
            c->rotZ = GetRandomU32();
            cs->spin[0] = GetRandomU32() & 0xF;
            cs->spin[1] = GetRandomU32() & 0xF;
            cs->spin[2] = GetRandomU32() & 0xF;
            cs->floor = actor->posZ;
          }
        }
        if (st->pushed == 0 &&
            RaiseSpyroTriggerInRadius(&actor->posX, (st->timer + 1) << 8, 0x86) != 0) {
          st->pushed = 1;
          g_anSpyroRequestVelocity.x = SPY.posX - actor->posX;
          g_nSpyroRequestVelocityZ = 0;
          g_nSpyroRequestVelocityY = SPY.posY - actor->posY;
          if (g_anSpyroRequestVelocity.x != 0 || g_nSpyroRequestVelocityY != 0) {
            int len = VectorLength(&g_anSpyroRequestVelocity.x, 1);

            ScaleVector3ByRatio(&g_anSpyroRequestVelocity.x, len,
                          (0x1000 - ApproxDist2D(&actor->posX, &SPY.posX)) /
                              48);
          }
          g_nSpyroRequestVelocityZ = 0x46;
        }
        DispatchActorContactAtSphere(&actor->posX, (st->timer + 1) << 8, 0, 0x50000, actor, 0);
        if (++st->timer >= 8) {
          MarkActorCommittedKill(actor);
          DespawnActorRecord(actor);
        }
        break;
      }
      actor->flags = 0;
      break;
    }

    /* Falling debris: tumbles along its velocity while on screen and is
     * removed once its timer runs out or it goes off screen. */
    case 318: {
      int *st = actor->state;

      if (TickCountdownByFrameDelta(&st[3], 4) != 0 || actor->drawn == 0) {
        DespawnActorRecord(actor);
        continue;
      }
      st[2] -= 6;
      if (st[2] < -0x80) {
        st[2] = -0x80;
      }
      AddVector(&actor->posX, &actor->posX, st);
      actor->rotX += 0xF;
      actor->rotZ += 9;
      break;
    }

    /* Respawner (RespawnState). */
    case 323: {
      RespawnState *st = actor->state;
      Actor *p;

      CopyVector(&actor->posX, &SPY.posX);
      if (TickCountdownByFrameDelta(&st->timer, 4) == 0) {
        break;
      }
      st->timer = st->period;

      for (p = g_pActorListBase; p < g_pActorPoolDynBase; p++) {
        if (p->type != st->type) {
          continue;
        }
        if (p->mode < 0x80) {
          continue;
        }
        if (ApproxDist2D(&SPY.posX, p->state) < 0x6001) {
          continue;
        }

        CopyVector(&p->posX, p->state);
        InsertActorIntoSpatialGrid(p);
        p->unk3A &= 0x7F;
        p->flags = 0;
        p->mode = 0;
        p->frameProgress = 0;
        p->frameRate = TYPE_DESC(p, 0);
        p->anim = 0;
        p->nextAnim = 0;
        p->frame = 0;
        p->nextFrame = 1;
      }
      break;
    }

    /* Type 328: floor-snapped marker. Drops onto the floor once, then sits at
     * the centroid of the triangle it landed on. */
    case 328: {
      int *tri = actor->state;

      if (*tri < 0) {
        int floor;
        int d;

        actor->posZ += 0x200;
        floor = FindGroundHeightBelow(&actor->posX, 0x400);
        d = actor->posZ - floor;
        if (ABS(d) < 0x400) {
          actor->posZ = floor;
          *tri = g_nLastCollisionHitChunkId;
        } else {
          actor->posZ -= 0x200;
        }
        if (*tri < 0) {
          break;
        }
      }
      UnpackWorldCollisionTri(*tri, mat2[0]);
      CopyVector(&actor->posX, mat2[0]);
      AddVector(&actor->posX, &actor->posX, mat2[1]);
      AddVector(&actor->posX, &actor->posX, mat2[2]);
      actor->posX /= 3;
      actor->posY /= 3;
      actor->posZ /= 3;
      break;
    }

    /* Type 329: seed pod. Wobbles when struck and holds a floating prize
     * above itself; Spyro takes the prize by touching it (or by being in a
     * grab state), otherwise the pod settles back after a while. */
    case 329: {
      L16Pod *st = actor->state;

      actor->unk55 = 0x20;
      switch (actor->mode) {
      case 0:
        if (actor->subMode == 0) {
          *(int *)&actor->unk4C = D_8006E52C[actor->unk53];
          actor->subMode = 1;
          actor->updateDist = 0x10;
        }
        if (actor->flags & 0xB0000) {
          st->timer = 0;
          st->rotX = actor->rotX;
          st->rotY = actor->rotY;
          st->restZ = actor->posZ;
          actor->updateDist = 0x40;
          st->prize = SpawnActorDeathDrop(actor, 5, 0, 0);
          st->prize->subMode = 4;
          st->prize->unk50 = 0;
          st->prize->drawn = 0;
          st->prize->rotX = 0;
          st->prize->rotY = 0;
          g_pfnLevelEmitParticles(8, 0x40, &actor->posX, (void *)(st->prize->type - 0x53));
          PlayActorMeshVoice(actor, 0, 0);
          actor->mode = 1;
          ANIM_RESET(actor, 1);
          continue;
        }
        if (st->timer >= 0xF8) {
          SpawnActorRotBillboard(actor, g_anRotBillboardOffsetsRand[GetRandomU32() & 3]);
          st->timer = (GetRandomU32() & 0x3F) + 0x18;
        }
        st->timer += g_nFrameStep;
        break;

      case 1: {
        int i;

        st->prize->unk50 = 0x18;
        CopyVector(&st->prize->posX, &actor->posX);
        st->prize->posZ += LookupSine((st->timer << 11) / 90) / 3 + 0x1A4;
        st->prize->rotZ += g_nFrameStep * 2;
        if (st->timer < 0x40) {
          int dx;
          int dy;

          actor->rotX = g_aShakeRotOffsets[st->timer >> 1].a + st->rotX;
          actor->rotY = g_aShakeRotOffsets[st->timer >> 1].b + st->rotY;
          dx = (signed char)g_aShakeRotOffsets[st->timer >> 1].a;
          dy = (signed char)g_aShakeRotOffsets[st->timer >> 1].b;
          if (dx < 0) {
            dx = -dx;
          }
          if (dy < 0) {
            dy = -dy;
          }
          dx += dy;
          actor->posZ = st->restZ + dx * 6;
        }
        if (st->timer < 0x20 &&
            (st->timer & 3) <= ((st->timer + g_nFrameStep) & 3)) {
          ZeroVector(s496);
          g_pfnLevelEmitParticles(1, 0, &st->prize->posX, s496);
        }
        if ((ApproxDist2D(&st->prize->posX, &spy->posX) < 0x1E0 &&
             (unsigned int)(SPY.posZ - st->prize->posZ + 0xDF) < 0x2BF) ||
            g_nSpyroState == 0x2C || g_nSpyroState == 0x18 ||
            (g_nSpyroState == 0x14 && (g_nSpyroStateFlags & 0x40))) {
          CollectPickupActor(st->prize);
          DespawnActorRecord(st->prize);
          PlayActorMeshVoice(actor, 2, 0);
          for (i = 0; i < 2; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(0x43, actor);
            g_pfnLevelSpawnActor(0x44, actor);
          }
          for (i = 0; i < 6; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(0x45, actor);
          }
          g_pfnLevelEmitParticles(5, 2, &actor->posX, 0);
          g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (void *)0x20);
          DespawnActorRecord(actor);
          continue;
        }
        if (actor->flags & 0x80000) {
          st->prize->subMode = 2;
          PlayActorMeshVoice(actor, 2, 0);
          for (i = 0; i < 2; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(0x43, actor);
            g_pfnLevelSpawnActor(0x44, actor);
          }
          for (i = 0; i < 6; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(0x45, actor);
          }
          g_pfnLevelEmitParticles(5, 2, &actor->posX, 0);
          g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (void *)0x20);
          DespawnActorRecord(actor);
          continue;
        }
        st->timer += g_nFrameStep;
        if (st->timer >= 0x56) {
          int j;

          for (j = 0; j < 6; j++) {
            CopyVector(s512, &st->prize->posX);
            s512[0] += g_anCosineLut[j * 43] >> 6;
            s512[1] += g_anSineLut[j * 43] >> 6;
            s512[2] += 0x28;
            s528[0] = g_anCosineLut[j * 43] >> 7;
            s528[1] = g_anSineLut[j * 43] >> 7;
            s528[2] = 0x10;
            g_pfnLevelEmitParticles(1, 0, s512, s528);
          }
          DespawnActorRecord(st->prize);
          actor->unk3A &= 0x7F;
          actor->updateDist = 0x10;
          actor->rotX = st->rotX;
          actor->rotY = st->rotY;
          actor->posZ = st->restZ;
          st->timer = 0;
          PlayActorMeshVoice(actor, 1, 0);
          actor->mode = 0;
          ANIM_RESET(actor, 0);
          continue;
        }
        break;
      }
      }
      actor->flags = 0;
      break;
    }

    /* Dragon pad: saves a checkpoint when Spyro lands on it. */
    case 333: {
      switch (actor->mode) {
      case 0: {
        if (ApproxDist2D(&actor->posX, &SPY.posX) > 2560) {
          actor->mode = 1;
        }
        break;
      }

      case 1: {
        if (ApproxDist2D(&actor->posX, &SPY.posX) < 896 &&
            ABS2(actor->posZ - actor->floorOffset - SPY.posZ) < 512) {

          SetSpawnAnchorFromActor(actor, actor->rotZ);
          actor->mode = 2;
          actor->anim = 1;
          actor->subMode = 0;
        }
        break;
      }

      case 2: {
        actor->subMode += g_nFrameStep;

        if (actor->subMode >= 0x30) {
          actor->mode = 0;
          actor->anim = 0;
        }
        break;
      }
      }

      break;
    }

    /* Types 363/364: a one-shot effect that is removed when its animation
     * ends. */
    case 405:
    case 477:      if (g_nActorAnimDone != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      break;

    /* Type 390: spinning chest. Each charge spins it faster (with a rising
     * voice); spun fast enough, or flamed, it bursts. */
    case 390: {
      L26Spinner *st = actor->state;
      int pitch = *(unsigned short *)(g_pLevelSampleBankEntries +
                                      g_apActorMeshTable[actor->type]->m_Sounds[1] *
                                          20 +
                                      0xA);
      int cur = g_anSpuVoiceRowPitch[actor->sfxSlot & 0x7F][0];
      int i;

      if ((actor->flags & 0x90000) == 0x10000) {
        actor->updateDist = 0xFF;
        PlaySoundEffect(g_apActorMeshTable[actor->type]->m_Sounds[1], actor, 8,
                      &actor->sfxSlot);
        if (actor->sfxSlot != 0x7F) {
          g_anSpuVoiceRowPitch[actor->sfxSlot & 0x7F][0] = pitch + st->speed * 4;
        }
      }
      if (st->speed < 0x19) {
        StopSoundVoicesByOwner(actor, 1);
      }
      if (actor->subMode == 0) {
        st->lid = g_pfnLevelSpawnActor(0x188, actor);
        st->lid->unk50 = 0x1A;
        actor->updateDist = 0x10;
        actor->subMode = 1;
        continue;
      }
      if (st->timer == 0 || --st->timer == 0) {
        if (actor->flags & 0x10000) {
          st->timer = 0x10;
          st->speed += 0x64;
        }
      }
      st->handle = SelectActorLodBand(actor, st->handle);
      st->speed -= 1;
      if (pitch < cur && actor->sfxSlot != 0x7F) {
        g_anSpuVoiceRowPitchStep[actor->sfxSlot & 0x7F][0] = -4;
      }
      if (st->speed < 0x18) {
        st->speed = 0x18;
        actor->updateDist = 0x10;
      } else {
        actor->updateDist = 0x40;
      }
      st->angle += (st->speed * g_nFrameStep) >> 1;
      st->lid->rotZ = st->angle >> 4;
      if (st->speed > 0xE0 || (actor->flags & 0x80000)) {
        st->lid->subMode = 1;
        PlayActorMeshVoice(actor, 0, 0);
        for (i = 0; i < 2; i++) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0x136, actor);
          g_pfnLevelSpawnActor(0x137, actor);
        }
        for (i = 0; i < 6; i++) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0x135, actor);
        }
        g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (int *)0x18);
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        DespawnActorRecord(actor);
      }
      actor->flags = 0;
      break;
    }

    /* Type 392: homing spark. Launched at the nearest enemy in front of
     * Spyro (or away from him when there is none), and bursts on what it
     * hits. */
    case 392: {
      L26Spark *st = actor->state;
      int i;

      if (actor->subMode == 1) {
        Actor *best = 0;
        int bestAng = 0x180;
        Actor *a;

        for (a = g_pActorListBase; a < g_pActorPoolDynBase; a++) {
          int d;

          if (a == actor || a->mode >= 0x80 || a->unk08 == 0 ||
              a->unk08 >= 0) {
            continue;
          }
          SubtractVector(s496, &a->posX, &actor->posX);
          d = ABS2(s496[0]) + ABS2(s496[1]);
          if (d < 0x6000 &&
              (unsigned int)(VectorLength(s496, 0) - 0x1000) <= 0x2000 &&
              (unsigned int)(s496[2] + 0x1000) <= 0x2000) {
            d = AbsAngleDelta12(ArcTan2(actor->posX - spy->posX,
                                            actor->posY - spy->posY, 1),
                              ArcTan2(s496[0], s496[1], 1));
            if (d < bestAng) {
              bestAng = d;
              best = a;
            }
          }
        }
        if (best != 0) {
          SubtractVector(st->vel, &best->posX, &actor->posX);
          st->vel[2] += 0x200;
          RShiftVector3(st->vel, 7);
        } else {
          SubtractVector(st->vel, &actor->posX, &SPY.posX);
          st->vel[2] = 0;
          ScaleVector3ByRatio(st->vel, VectorLength(st->vel, 0), 0x50);
        }
        st->vel[2] += 0xA0;
        st->timer = 0;
        actor->subMode = 2;
        actor->unk50 = 0x20;
        actor->updateDist = 0xFF;
        actor->rotX = 0xA;
        actor->rotY = 0;
        continue;
      }
      if (actor->subMode != 2) {
        continue;
      }
      st->timer += g_nFrameStep;
      actor->rotZ += g_nFrameStep * 8;
      if (st->vel[2] >= -0x9F) {
        st->vel[2] -= (g_nFrameStep * 5) >> 1;
      }
      for (i = 0; i < g_nFrameStep; i++) {
        AddVector(&actor->posX, &actor->posX, st->vel);
        if (actor->posZ < 0) {
          DespawnActorRecord(actor);
          continue;
        }
        actor->posZ += 0xB4;
        if (st->timer >= 5 &&
            (CollideSphereWithWorldAndActors(&actor->posX, 0xB4, 0xB4) != 0 ||
             DispatchActorContactAtSphere(&actor->posX, 0x100, 0, 0x30000, actor, 0) != 0)) {
          DispatchActorContactAtSphere(&actor->posX, 0x400, 0, 0x30000, actor, 0);
          for (i = 0; i < 2; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(0x136, actor);
            g_pfnLevelSpawnActor(0x137, actor);
          }
          for (i = 0; i < 6; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(0x135, actor);
          }
          g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (int *)0x18);
          PlayActorMeshVoice(actor, 0, 0);
          DespawnActorRecord(actor);
          continue;
        }
        actor->posZ -= 0xB4;
      }
      continue;
    }

    /* Floating-word letters (OrbitState). */
    case 76:
    case 426:
    case 427:
    case 428:
    case 429:
    case 430:
    case 431:
    case 432:
    case 433:
    case 434:
    case 435:
    case 436:
    case 437:
    case 438:
    case 439:
    case 440:
    case 441:
    case 442:
    case 443:
    case 444:
    case 445:
    case 446:
    case 447:
    case 448:
    case 449:
    case 450:
    case 451: {
      OrbitState *st = actor->state;

      if (st->owner->mode == 0 || st->owner->subMode != actor->mode) {
        DespawnActorRecord(actor);
      } else if (st->owner->type == 9) {
        if (ApproxDist2D(&st->owner->posX, &SPY.posX) < 0x400) {
          int oldval;
          oldval = g_anCosineLut[actor->subMode];
          actor->subMode += 8;
          actor->rotZ = actor->rotZ - (oldval * 3 >> 9) +
                         (g_anCosineLut[actor->subMode] * 3 >> 9);
        } else {
          int anglespyro;

          actor->subMode += 8;
          anglespyro = ArcTan2(SPY.posX - st->owner->posX,
                                     SPY.posY - st->owner->posY, 0);
          s512[0] = g_anCosineLut[anglespyro] * 3 >> 4;
          s512[1] = g_anSineLut[anglespyro] * 3 >> 4;
          s512[2] = 0;
          vecB[0] = g_anCosineLut[anglespyro + 64 & 0xFF] >> 5;
          vecB[1] = g_anSineLut[(anglespyro + 64) & 0xFF] >> 5;
          vecB[2] = 0;
          anglespyro = (anglespyro + 0x80) & 0xFF;
          actor->rotZ = anglespyro + (g_anCosineLut[actor->subMode] * 3 >> 9);
          ScaleVector3Sat(vecC, vecB, st->unk04 - 1);
          LShiftVector3(vecB, 1);
          anglespyro = 2;
          anglespyro = (st->unk04 - 1) * anglespyro;
          vecC[2] += g_anCosineLut[anglespyro] * 3 >> 3;
          vecD[0] = s512[0] * g_anCosineLut[anglespyro];
          vecD[1] = s512[1] * g_anCosineLut[anglespyro];
          vecD[2] = 0;
          anglespyro = ((anglespyro - st->unk06 * 4)) & 0xFF;
          ScaleVector3Sat(vecB, vecB, st->unk06);
          SubtractVector(vecC, vecC, vecB);
          actor->posX = s512[0] * g_anCosineLut[anglespyro];
          actor->posY = s512[1] * g_anCosineLut[anglespyro];
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, vecD);
          RShiftVector3(&actor->posX, 10);
          AddVector(&actor->posX, &actor->posX, s512);
          AddVector(&actor->posX, &actor->posX, &st->owner->posX);
          SubtractVector(&actor->posX, &actor->posX, vecC);
          actor->posZ = actor->posZ + (g_anCosineLut[anglespyro] * 3 >> 3) + 0x600;
        }
      } else {
        actor->subMode += 8;
        actor->rotZ = actor->floorOffset + (g_anCosineLut[actor->subMode] * 3 >> 9);
      }
    }
    break;

    default: /* 77..82, 88..109, 258..259 and everything unlisted */
      break;
    
}
  }
}
