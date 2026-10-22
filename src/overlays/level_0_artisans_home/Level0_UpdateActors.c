/* Level0_UpdateActors -- per-frame actor update for level 0.
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class: a small
 * state machine over the actor's state byte (->mode), often with a sub-state
 * (->subMode), and usually a class-specific state block behind ->state.
 *
 * Layout of this file:
 *   Core records      Actor, the per-type Model table, waypoint paths
 *   Per-class blocks  one state struct per actor class, each followed by the
 *                     globals and callees its case uses, in roughly case order
 *   Shared objects    the camera (CAM) and Spyro (SPY), each declared as one
 *                     aggregate over its whole address range
 *   Helper macros     absolute value and the three animation idioms
 *   The function      update-list preamble, then the per-class dispatch
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
  short m_NumFrames;              /* 0x00 */
  unsigned short m_NumColors;     /* 0x02 */
  unsigned char m_IsSpyroAnimation; /* 0x04 */
  unsigned char m_Scale;
  unsigned char m_ShortEncodeShift;
  unsigned char m_Radius;
  unsigned char m_VertCountHigh;  /* 0x08 */
  unsigned char m_VertCountLow;
  unsigned char m_Padding2;
  unsigned char m_DepthScale;
  unsigned char m_ProgressPerTick; /* 0x0C -- the byte TYPE_DESC reads */
  unsigned char m_Padding3;
  unsigned short m_Padding4;
  void *m_AnimationVertices;      /* 0x10 */
  void *m_Faces;
  void *m_Colors;
  void *m_LpFaces;
  void *m_LpColors;               /* 0x20 */
} AnimationHeader;

typedef struct Model {
  int m_NumAnimations;            /* 0x00 */
  unsigned char m_Sounds[16];     /* 0x04 */
  void *m_CollisionModels[8];     /* 0x14 */
  void *m_Data;                   /* 0x34 */
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
  short reversed;      /* 0x06 */
  struct {
    int x;
    int y;
    int z;
    int unk0C;
  } nodes[1];          /* 0x08 */
} PathData;

/* Type 10: path patroller. */
typedef struct PathState {
  int unk00;
  int unk04;
  int unk08;
  PathData *path;      /* 0x0C */
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

/* Type 286: ambient sound source; mode 2 also coasts along a path. */
typedef struct RoamState {
  int mode;            /* 0x00 inner jump-table discriminant, 0..9 */
  int unk04;           /* 0x04 republished into actor->soundRange every frame */
  unsigned char *path; /* 0x08 [0]=waypoint count, [1]=current index */
  int timer;           /* 0x0C */
  int sub;             /* 0x10 sub-state inside mode 2 */
  int unk14;           /* 0x14 4th argument to ComputeStepTowardPoint */
  int vel[3];          /* 0x18 current leg velocity, written by ComputeStepTowardPoint */
  int count;           /* 0x24 frames left on the current leg */
} RoamState;

extern int ComputeStepTowardPoint(int *from, int *to, int *vel, int a);

/* Progress-per-tick byte of animation `anim` in the actor's model. */
#define TYPE_DESC(a, id)                                                     \
  (g_apActorMeshTable[(a)->type]->m_Animations[(id)]->m_ProgressPerTick)

/* Type 34: dragon egg. Spirals up, flies to Spyro and is collected. */
typedef struct ShotState {
  void *unk00;           /* 0x00 egg index */
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
extern int D_80077AE8;            /* bit 0: switch sounds enabled */
extern struct {
  short v;
} g_nSfxOverridePitch;
extern unsigned short D_80073094[]; /* indexed by the switch's bit position */

/* Egg sequence flags, then the vector library: copy, add, subtract, scale,
 * shift, sine and cosine. */
extern struct {
  int v;
} g_nHudWorldEggRollRequest;

extern void AimCameraStickTowardPoint(int *pos);
extern void SVectorToVectorShl2(int *d, short *s);

/* Mark an item collected; the second argument is the actor or its id. */
extern void CommitActorKillGems(int a, void *b);

extern void SteerActorAroundPoint(Actor *actor, int *target, int a, int heading, int b,
                          int c, int d, int e, int f, int g, int h, int i,
                          int j);

/* Type 49: sequenced prop driven by the 0x8002Bxxx player. Starts once any
 * exit vortex flag is set and its linked actor is dormant. */
typedef struct AnimState {
  int handle;  /* 0x00 */
  int started; /* 0x04 one-shot init latch */
  int idx;     /* 0x08 pool index of the actor this one watches */
} AnimState;

extern unsigned char D_80078E7C; /* visited flag, read once */

extern unsigned char D_8007A6A9;
extern unsigned char D_8007A6AA;
extern unsigned char D_8007A6AB;
extern unsigned char D_8007A6AC;

extern void ModifyWorldChunkAnimFlags(int handle, int a, int b);
extern void SetWorldChunkAnimStep(int handle, int a, int b);
extern int GetWorldChunkAnimState(int handle); /* bits 8..15 are the anim marker */

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

typedef struct {
  unsigned char *v;
} PtrU8;
#define AS_PTRU8(sym) (((PtrU8 *)&(sym))->v)

extern int CollideSphereWithWorldAndActors(int *pos, int a, int b); /* ground/wall probe */

/* Types 14/15/83..87: collectibles (life statue, life orb, gems). ->vel
 * doubles as the pickup origin once Sparx has fetched the item. */
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

/* g_aRotBillboards records (SpawnActorRotBillboard) used as sparkles around items. */
typedef struct RotBillboard {
  int pos[3];         /* 0x00 */
  unsigned char life; /* 0x0C frames left; 0 = slot free */
  unsigned char pad[11];
} RotBillboard;

extern RotBillboard g_aRotBillboards[];

/* Type 173: key. Rocks in place, sparkles, and is collected on contact. */
typedef struct SpinState {
  short timer;         /* 0x00 */
  unsigned char angle; /* 0x02 */
  unsigned char anim;  /* 0x03 0xFF == none */
} SpinState;

/* Type 187: talking dragon. Dragon count, dialogue selection, and the latch
 * in the save-options block. */
extern void ResetSpyroLinearMotion(void);

/* Type 194: wooden chest. Breaks into fragments when damaged. */
typedef struct BreakState {
  int unk00; /* 0x00 */
  int unk04; /* 0x04 */
} BreakState;

/* Type 250: crystal dragon. Shakes and sparkles; when Spyro walks into it,
 * saves a checkpoint and starts the rescue. */
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
  int posX;      /* 0x28 m_Position */
  int posY;      /* 0x2C */
  int posZ;      /* 0x30 */
  char pad034[0x18];
  short rotX;    /* 0x4C m_Rotation */
  short rotY;    /* 0x4E */
  short rotZ;    /* 0x50 */
  short unk52;   /* 0x52 */
  int occlusion; /* 0x54 */
  unsigned int state; /* 0x58 m_State */
  int unk5C;     /* 0x5C */
  Xform lastSim; /* 0x60 */
  Xform sphere;  /* 0x78 m_Sphere */
  Xform sim;     /* 0x90 m_Simulation */
  Xform unkA8;   /* 0xA8 */
  int unkC0;     /* 0xC0 */
  char padC4[0x14];
  Xform *preset; /* 0xD8 m_SphericalPreset */
} CamObj;
extern CamObj g_anWorldToCameraRotMtx;
#define CAM g_anWorldToCameraRotMtx

extern Xform g_aCameraXformPresets[];

extern void ComputeCameraOrbitOffset(int *out);
extern void UpdateCameraEulerAngles(void);

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
extern Xform6 g_anCameraParamMode2Front; /* template it is seeded from */

/* Type 300 state block: activation box, mode flags, timer, linked actor. */
typedef struct TriggerState {
  int unk00; /* 0x00 vertical span; also the offset added to the target Z */
  int unk04; /* 0x04 horizontal radius */
  int flags; /* 0x08 bit0 style, bit1 armed, bit2 held, bit3 owner-gated,
              * bit4 also stamps actor->subMode */
  int timer; /* 0x0C */
  int idx;   /* 0x10 index into the actor pool at g_pActorListBase */
} TriggerState;

/* Type 323: respawner. Revives dormant actors of one class out of view. */
typedef struct RespawnState {
  int type;   /* 0x00 the actor type this manager owns */
  int timer;  /* 0x04 */
  int period; /* 0x08 reload for ->timer */
} RespawnState;

extern void StubRewardHook(int script, int a);

/* Callees used by the collectible and fairy behaviours. */
extern void UnpackWorldCollisionTri(int a, int *out);
extern int FindGroundHeightBelow(int *v, int k);
extern int GetGroundTriggerRegionType(void);
extern int CheckLineOfSightSegment(int *a, int *b);
extern int func_80017428(int *v, int *n, int *out);

/* Type 110: dragon-pad fairy. Appears near her pad, hovers, and starts the
 * fairy cutscene when Spyro stands still on the pad. */
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

/* Type 114: path-following enemy that runs from Spyro and can be knocked
 * back. */
typedef struct WalkerState {
  PathData *path;      /* 0x00 */
  int timer;           /* 0x04 */
  int tgtIdx;          /* 0x08 index into g_pActorListBase, -1 when unclaimed */
  int heading;         /* 0x0C */
  int unk10;           /* 0x10 passed by address to DriveActorTrackedMove */
  int unk14;           /* 0x14 likewise */
} WalkerState;

extern void TrackEntityYawTowardSpyro(Actor *actor, int a, int b, int c);

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

/* Type 120: Sparx. Idle hover offset, glow node, and the actor he is
 * fetching. */
typedef struct FlierState {
  int timer;    /* 0x00 idle / fetch timer */
  short vx;     /* 0x04 */
  short vy;     /* 0x06 */
  short vz;     /* 0x08 */
  FxNode *fx;   /* 0x0C */
  Actor *rider; /* 0x10 */
} FlierState;

extern FxNode *AllocWorldSparkle(void);
extern void ClearWord(FxNode *node);
extern int StepAngle8Toward(int a, int b, int c);

/* Type 339: flock leader. Two linked chains of follower actors, a path, and
 * one formation slot per follower plus one for the leader. */
typedef struct FlockState {
  int unk00;           /* 0x00 knockback angle */
  int unk04;           /* 0x04 knockback speed */
  PathData *path;      /* 0x08 */
  int unk0C;
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

extern int CastRayWorldAndActors(int *a, int *b);

/* Type 350: sequenced prop driven by the 0x8002Bxxx player, unlocked once. */
typedef struct PropAnimState {
  int anim;   /* 0x00 handle for ModifyWorldChunkAnimFlags / B3F4 / B444 */
  int timer;  /* 0x04 */
  int primed; /* 0x08 */
} PropAnimState;

extern unsigned char D_80078E7D; /* set once the prop has been unlocked */

/* Types 76/426..451: letters of a floating word, placed relative to their
 * parent actor. */
typedef struct OrbitState {
  Actor *owner; /* 0x00 */
  short unk04;  /* 0x04 ring index; the arm always uses it as unk04 - 1 */
  short unk06;  /* 0x06 phase offset within the ring */
} OrbitState;

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
  (a)->frameProgress = 0x10;                                                           \
  (a)->frameRate = 0x10;                                                           \
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
    (a)->frameProgress = 0x10;                                                         \
    (a)->frameRate = 0x10;                                                         \
    (a)->anim = (a)->nextAnim;                                                   \
    (a)->nextAnim = (n);                                                          \
    (a)->frame = (a)->nextFrame;                                                   \
    (a)->nextFrame = 0;                                                            \
    StubActorHook(a);                                                          \
  }                                                                            \
  continue

/* Spyro as one object at 0x80078A58, with the fields this function touches. */
typedef struct {
  int posX;                       /* 0x000 */
  int posY;                       /* 0x004 */
  int posZ;                       /* 0x008 */
  char pad00C[0x2];
  unsigned char bodyRotZ;         /* 0x00E */
  char pad00F[0x18];
  unsigned char u027;             /* 0x027 */
  char pad028[0x4];
  int damageFlags;                /* 0x02C */
  char pad030[0x4];
  int u034;                       /* 0x034 */
  char pad038[0x40];
  int state;                      /* 0x078 */
  char pad07C[0x4];
  int u080;                       /* 0x080 */
  char pad084[0x18];
  int u09C;                       /* 0x09C */
  char pad0A0[0x6C];
  int u10C;                       /* 0x10C */
  int u110;                       /* 0x110 */
  int u114;                       /* 0x114 */
  int u118;                       /* 0x118 */
  int u11C;                       /* 0x11C */
  char pad120[0x44];
  int u164;                       /* 0x164 */
  char pad168[0x14];
  int contact;                    /* 0x17C */
  char pad180[0x24];
  int headLook;                   /* 0x1A4 */
  char pad1A8[0x4C];
  int controlFlags;               /* 0x1F4 */
  char pad1F8[0x8];
  int u200;                       /* 0x200 */
  char pad204[0x2];
  unsigned char u206;             /* 0x206 */
  char pad207[0x1];
  int u208;                       /* 0x208 */
  int u20C;                       /* 0x20C */
  int u210;                       /* 0x210 */
  char pad214[0x4];
  int u218;                       /* 0x218 */
  int *pCamAnchor;                /* 0x21C */
  void *pCamTarget;               /* 0x220 */
  Actor *pScriptedActor;          /* 0x224 */
  char pad228[0x18];
  PortalDesc *pPortal;            /* 0x240 */
  int u244;                       /* 0x244 */
  int u248;                       /* 0x248 */
} SpyroObj;
extern SpyroObj g_anSpyroWorldPosBlock;
#define SPY g_anSpyroWorldPosBlock

/* ==== The function ==== */
void Level0_UpdateActors(void) {
  Actor **actorList;
  Actor *actor;
  int flag1;

  /* Scratch vectors shared by several cases. */
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
  int svJ[3];
  int svK[3];

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

    /* Publish this actor's animation flags and the frame delta for the
     * callees. */
    g_nActorAnimDone = actor->animFlags & 2;
    flag1 = actor->animFlags & 1;
    g_nActorFrameDelta = g_nFrameStep;
    g_nActorAnimNewFrame = flag1;

    /* One case per actor class. `break` and `continue` both end this actor's
     * update; `continue` also skips any shared tail inside the case. */
    switch (actor->type) {
    /* Portal name text: shown while the camera faces the portal. */
    case 1: {
      int shouldSpawnText, cameraAngleDiff;
      int distance = ApproxDist2D(&actor->posX, &CAM.posX);

      if (distance < (actor->mode ? 12 : 11) * 1024) {
        shouldSpawnText = actor->mode == 0;

        actor->mode = 1;

        cameraAngleDiff = AbsAngleDelta8(
            actor->rotZ, ArcTan2(CAM.posX - actor->posX,
                                        CAM.posY - actor->posY, 0));

        if (cameraAngleDiff <= 56) {
          if (actor->subMode == 1) {
            shouldSpawnText = 1;
          }
          actor->subMode = 0;
        } else if (cameraAngleDiff > 71) {
          if (actor->subMode == 0) {
            shouldSpawnText = 1;
          }
          actor->subMode = 1;
        } else {
          actor->mode = 0;
        }

        if (actor->mode && shouldSpawnText) {
          SpawnNameLetterActors(actor, 1);
        }
      } else {
        actor->mode = 0;
      }

      break;
    }

    /* Path patroller (PathState). */
    case 10: {
      PathState *st = (PathState *)actor->state;

      if ((actor->flags & 0xB0000) != 0 && actor->mode != 3) {
        actor->flags = 0;
        st->heading = ArcTan2(actor->posX - SPY.posX,
                                    actor->posY - SPY.posY, 0);

        if ((actor->flags & 0x10000) != 0) {
          st->unk20 = 0xC8;
        } else {
          st->unk20 = 0x190;
        }

        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        actor->mode = 3;
        ANIM_RESET(actor, 3);
        continue;
      }

      switch (actor->mode) {
      case 0: {
        SnapActorToGround(actor);

        if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x1400 &&
            ABS2(actor->posZ - SPY.posZ) < 0x400) {
          if (actor->group != 0xFF) {
            Actor **scan = g_apSaveMenuAnimActorList;
            Actor *other;

            while ((other = *scan++) != 0) {
              PathState *ost = (PathState *)other->state;

              if (other->group == actor->group && other->mode == 0) {
                if (ost->path->cur == st->path->cur) {
                  other->mode = 1;
                  ost->unk30 = RandomIntInRange(6, 0x28);
                }
              }
            }
          }

          actor->mode = 1;
          continue;
        }

        if (actor->nextAnim != 0) {
          g_nActorAnimDone = 0;
          ANIM_ADVANCE(actor, 0);
        }

        break;
      }
      case 1: {
        int spyroAngle;
        int angle;
        int angle2;
        int previousAngle;
        int nextNode;
        int previousNode;
        int distance;
        int angleScale;
        int nextAngle;

        spyroAngle =
            ArcTan2(SPY.posX - actor->posX, SPY.posY - actor->posY, 0);
        nextNode = (st->path->cur + 1) % st->path->count;
        previousNode = (st->path->cur - 1 + st->path->count) % st->path->count;

        nextAngle = ArcTan2(st->path->nodes[nextNode].x - actor->posX,
                                  st->path->nodes[nextNode].y - actor->posY, 0),
        previousAngle =
            ArcTan2(st->path->nodes[previousNode].x - actor->posX,
                          st->path->nodes[previousNode].y - actor->posY, 0);

        if (AbsAngleDelta8(spyroAngle, nextAngle) >
            AbsAngleDelta8(spyroAngle, previousAngle)) {
          st->path->cur = nextNode;
        } else {
          st->path->cur = previousNode;
        }

        angle = ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                              st->path->nodes[st->path->cur].y - actor->posY, 0);
        angle = (angle + RandomIntInRange(-0x20, 0x20)) & 0xFF;
        angle2 =
            ArcTan2(actor->posX - SPY.posX, actor->posY - SPY.posY, 0);
        angleScale = 108;
        angle2 += (WrapAngleDelta8(angle2, angle) * angleScale) >> 7;
        angle2 &= 0xFF;
        st->heading = angle2;
        st->unk24 = RandomIntInRange(0x6E, 0xA0);
        st->unk20 = 0;
        distance = ApproxDist2D(&actor->posX,
                                 (int *)&st->path->nodes[st->path->cur]);
        st->unk28 = (distance / st->unk24) >> 1;

        if (st->unk28 >= 0x5B) {
          st->unk28 = 0x5A;
        }

        actor->mode = 2;
        continue;
      }
      case 2: {
        int distance = ApproxDist2D(&actor->posX,
                                     (int *)&st->path->nodes[st->path->cur]);

        if (TickCountdownByFrameDelta(&st->unk30, 4) == 0) {
          break;
        }

        if (st->unk20 < st->unk24) {
          st->unk20 += 0xA;
        }

        if (actor->nextAnim != 1) {
          g_nActorAnimDone = 0;
          ANIM_ADVANCE(actor, 1);
        }

        if (TickCountdownByFrameDelta(&st->unk28, 4) != 0) {
          int angleDelta;
          int turn;

          angleDelta = WrapAngleDelta8(
              ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                            st->path->nodes[st->path->cur].y - actor->posY, 0),
              st->heading);
          turn = 1;

          if (ABS2(angleDelta) >= 0x1F) {
            turn = 2;
          }

          if (angleDelta > 0) {
            st->heading -= turn;
            st->heading &= 0xFF;
          } else if (angleDelta < 0) {
            st->heading += turn;
            st->heading &= 0xFF;
          }
        }

        if (TurnActorYawTowardAngle(actor, st->heading, 6, 0x14, 1) != 0) {
          if (MoveActorAlongHeading(actor, st->unk20, 0x12C, 0x12C, 0x15) != 0) {
            st->unk2C++;
          } else {
            st->unk2C = 0;
          }
          if (st->unk2C >= 4 && g_nActorAnimDone != 0) {
            if (distance < 0x1400) {
              actor->mode = 0;
              continue;
            }
            st->heading = WrapAddAngle8(st->heading, RandomSignedInRange(0x3C, 0x64));
          }
        }
        if (distance < 0xC00 && g_nActorAnimDone != 0) {
          actor->mode = 0;
          continue;
        }

        break;
      }

      case 3: {
        if (st->unk20 >= 0x10) {
          st->unk20 -= 0xF;
          MoveActorAlongAngle(actor, st->heading, st->unk20, 0, 0x2BC, 5);
        }

        if (g_nActorAnimDone != 0) {
          DespawnActorRecord(actor);
          continue;
        }

        break;
      }
      }

      RefreshActorDerivedState(actor, 1);
      break;
    }

    /* Particle emitter on a random timer. */
    case 11: {
      int *st;

      st = (int *)actor->state;
      if (TickCountdownByFrameDelta(st, 4) == 0) {
        break;
      }
      g_pfnLevelEmitParticles(1, 0x16, &actor->posX, 0);
      *st = GetRandomU32() & 0xE;
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

    /* Pressure switch (SwitchState). */
    case 18: {
      SwitchState *st = (SwitchState *)actor->state;
      int active = D_80077AE8 & 1;

      switch (actor->subMode) {
      case 0: {
        if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x500 &&
            ABS2((actor->posZ - actor->floorOffset) - SPY.posZ) < 0x280) {
          if ((g_pActorListBase[st->idx].subMode >> st->shift) & 1) {
            g_pActorListBase[st->idx].subMode = 0;
          } else {
            g_pActorListBase[st->idx].subMode |= 1 << st->shift;
          }
          actor->subMode = 1;
        }
        break;
      }

      case 1: {
        if (ApproxDist2D(&actor->posX, &SPY.posX) >= 0x601 ||
            ABS2((actor->posZ - actor->floorOffset) - SPY.posZ) >= 0x2C1) {
          actor->subMode = 0;
        }
        break;
      }
      }

      if (g_pActorListBase[st->idx].mode == 10) {
        if ((actor->mode & 1) == 0) {
          actor->mode = 1;
          ANIM_RESET(actor, 1);
        }
        break;
      }

      if (g_pActorListBase[st->idx].mode == 11) {
        if ((actor->mode & 1) != 0) {
          actor->mode = 0;
          ANIM_RESET(actor, 0);
        }
        break;
      }

      if (((g_pActorListBase[st->idx].subMode >> st->shift) & 1) != 0 && active != 0 &&
          g_pActorListBase[st->idx].mode < 0x63) {
        if ((actor->mode & 1) == 0) {
          g_nSfxOverrideFlags = 2;
          g_nSfxOverridePitch.v =
              *(unsigned short *)(g_pLevelSampleBankEntries + g_pLevelSampleBankHeader[0x37] * 20 + 0xA) +
              D_80073094[st->shift];
          PlaySoundEffect(g_apActorMeshTable[actor->type]->m_Sounds[0], actor, 8,
                        &actor->sfxSlot);
        }
        actor->mode = 1;
        ANIM_RESET(actor, 1);
      } else if ((actor->mode & 1) != 0) {
        actor->mode = 0;
        ANIM_RESET(actor, 0);
      }
      break;
    }

    /* Dragon egg (ShotState). */
    case 34: {
      ShotState *st;

      st = (ShotState *)actor->state;

      if (actor->mode != 0 && actor->mode < 5) {
        SPY.controlFlags = 0x80002000;
        if (SPY.state != 0 && SPY.state != 0xC) {
          actor->mode = 5;
        }
      }
      if ((unsigned int)(actor->mode - 2) < 3) {
        AimCameraStickTowardPoint(&actor->posX);
        g_pfnLevelEmitParticles(1, 0xC, (int *)actor, (void *)0x602080);
      }

      switch (actor->mode) {
      case 1:
        g_nGameplayBlocked = 1;
        VectorToSVectorShr2(st->from, &actor->posX);
        st->heading =
            ArcTan2_8bit(actor->posX - SPY.posX, actor->posY - SPY.posY) +
            0x40;
        st->speed = 0;
        st->phase = 0;
        g_nHudWorldEggRollRequest.v = 1;
        actor->rotX = 0;
        actor->rotY = 8;
        actor->mode = actor->mode + 1;
        break;

      case 2:
        st->phase = st->phase + g_nFrameStep;
        if (st->phase < 0x80) {
          int step = g_nFrameStep << 2;
          st->speed = st->speed + step;
          st->heading = st->heading + step;
          SVectorToVectorShl2(&actor->posX, st->from);
          actor->posX += (LookupCosine(st->heading << 4) * st->speed) >> 12;
          actor->posY += (LookupSine(st->heading << 4) * st->speed) >> 12;
          {
            int ph = st->phase;
            actor->rotZ = actor->rotZ + (g_nFrameStep << 2);
            actor->posZ = actor->posZ - (ph << 3);
          }
        } else {
          CopyVector(svA, &SPY.posX);
          {
            int c = LookupCosine(SPY.bodyRotZ << 4);
            int s = LookupSine(SPY.bodyRotZ << 4);
            int cr = c * 724;
            int sr = s * 724;
            svA[0] = svA[0] + ((sr + cr) >> 12);
          }
          {
            int s = LookupSine(SPY.bodyRotZ << 4);
            int c = LookupCosine(SPY.bodyRotZ << 4);
            int sr = s * 724;
            int cr = c * 724;
            svA[1] = svA[1] + ((sr - cr) >> 12);
          }
          svA[2] = svA[2] + 0x300;
          VectorToSVectorShr2(st->to, svA);
          VectorToSVectorShr2(st->from, &actor->posX);
          st->phase = 0;
          actor->mode = actor->mode + 1;
        }
        if ((g_dwPadPressed & 0x40) != 0) {
          actor->mode = 5;
        }
        break;

      case 3:
        st->phase = st->phase + g_nFrameStep;
        if (st->phase < 0x40) {
          SVectorToVectorShl2(svB, st->from);
          SVectorToVectorShl2(svC, st->to);
          SubtractVector(svC, svC, svB);
          ScaleVector3Sat(svC, svC, st->phase);
          RShiftVector3(svC, 6);
          AddVector(&actor->posX, svB, svC);
          actor->rotZ = actor->rotZ + (g_nFrameStep << 2);
        } else {
          CopyVector(svD, &SPY.posX);
          svD[0] = svD[0] + (LookupCosine(SPY.bodyRotZ << 4) >> 3);
          svD[1] = svD[1] + (LookupSine(SPY.bodyRotZ << 4) >> 3);
          svD[2] = svD[2] + 0x300;
          VectorToSVectorShr2(st->from, svD);
          st->heading = SPY.bodyRotZ - 0x40;
          st->speed = 0x200;
          st->phase = 0;
          actor->mode = actor->mode + 1;
        }
        if ((g_dwPadPressed & 0x40) != 0) {
          actor->mode = 5;
        }
        break;

      case 4:
        st->phase = st->phase + g_nFrameStep;
        if (st->phase < 0x80) {
          int step = g_nFrameStep << 2;
          st->speed = st->speed - step;
          st->heading = st->heading - step;
          SVectorToVectorShl2(&actor->posX, st->from);
          actor->posX += (LookupCosine(st->heading << 4) * st->speed) >> 12;
          actor->posY += (LookupSine(st->heading << 4) * st->speed) >> 12;
          {
            int ph = st->phase;
            actor->rotZ = actor->rotZ + (g_nFrameStep << 2);
            actor->posZ = actor->posZ - (ph << 2);
          }
        } else {
          actor->mode = actor->mode + 1;
        }
        if ((g_dwPadPressed & 0x40) != 0) {
          actor->mode = 5;
        }
        break;

      case 5:
        SubtractVector(svE, &actor->posX, &CAM.posX);
        WorldToCameraRotate(svE, svE);
        g_pfnLevelEmitParticles(0x10, 0x4D, svE, 0);
        g_nGameplayBlocked = 0;
        ZeroVector(&SPY.headLook);
        CommitActorKillGems(0, st->unk00);
        g_nWorldEggsRecovered = g_nWorldEggsRecovered + 1;
        g_anLevelEggsRecovered[g_nLevelIntroIndex] += 1;
        g_nHudWorldEggRollRequest.v = 0;
        DespawnActorRecord(actor);
        break;

      default:
        break;
      }
      break;
    }

    /* Sequenced prop gated on the exit vortices (AnimState). */
    case 49: {
      AnimState *st = actor->state;
      int marker;

      if (st->started == 0) {
        if (D_80078E7C != 0) {
          ModifyWorldChunkAnimFlags(st->handle, 0xFC, 0);
          SetWorldChunkAnimStep(st->handle, 0x3B, 0);
        }
        st->started = 1;
      }

      marker = (GetWorldChunkAnimState(st->handle) >> 8) & 0xFF;

      switch (marker) {
      case 0:
        if (D_8007A6A9 || D_8007A6AA || D_8007A6AB || D_8007A6AC) {
          /* Wake only once the linked actor is dormant and Spyro is near. */
          if (g_pActorListBase[st->idx].mode >= 0x80 &&
              ApproxDist2D(&actor->posX, &SPY.posX) < 0x4000) {
            ModifyWorldChunkAnimFlags(st->handle, 0xFC, 0);
            actor->soundRange = 0x30;
            PlaySoundEffect(g_pLevelSampleBankHeader[0x41], actor, 8, &actor->sfxSlot);
          }
        }
        break;

      case 0x3C:
        StopSoundVoicesByOwner(actor, 1);
        if (ApproxDist2D(&actor->posX, &SPY.posX) >= 0x4801) {
          ModifyWorldChunkAnimFlags(st->handle, 0xFC, 0);
        }
        break;
      }
      break;
    }

    /* Bouncing chest fragments (SparkState). */
    case 255:
    case 256:
    case 423:
    case 424: {
      SparkState *st = actor->state;

      if (st->life > 0 && actor->drawn) {
        if (CollideSphereWithWorldAndActors(&actor->posX, 0x100, 0x100)) {
          int *nrm = &g_anCollisionProbeVec;
          int dot;

          SetVector3Magnitude(nrm, 0x1000);
          dot = st->vel[0] * nrm[0] + st->vel[1] * g_nCollisionProbeVecY +
                    st->vel[2] * g_nCollisionProbeVecZ >>
                11;
          if (dot < 0) {
            ScaleVector3ByRatio(nrm, 0x1000, (dot >> 2) - (dot));
            st->vel[0] += nrm[0];
            st->vel[1] += AS_I32(g_nCollisionProbeVecY);
            st->vel[2] += AS_I32(g_nCollisionProbeVecZ);
          }
        }

        actor->posX += st->vel[0];
        actor->posY += st->vel[1];
        st->vel[2] -= 6;
        if (st->vel[2] < -128)
          st->vel[2] = -128;
        actor->posZ += st->vel[2];

        actor->rotX += st->spin0;
        actor->rotY += st->spin1;
        actor->rotZ += st->spin2;
        if ((st->life & 3) == 0) {

          svD[0] = GetRandomU32() & 3;
          svD[1] = GetRandomU32() & 3;
          svD[2] = 0x14;
          g_pfnLevelEmitParticles(1, 1, &actor->posX, svD);
        }
        st->life--;
      } else {
        g_pfnLevelEmitParticles(8, 0x46, &actor->posX, (void *)0x10);
        DespawnActorRecord(actor);
        continue;
      }
      break;
    }

    /* Falling chest fragments (TumbleState). */
    case 257:
    case 425: {
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

    /* Dragon-pad fairy (PoleState). */
    case 110: {
      PoleState *st = (PoleState *)actor->state;

      Actor *tgtA = &g_pActorListBase[st->idxA];
      Actor *tgtB = &g_pActorListBase[st->idxB];

      switch (actor->mode) {
      case 0: {
        actor->collision = 0;
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
          actor->collision = 0;

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
                actor->scale = 0x60;

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
        if (actor->scale >= 0x21) {
          actor->scale -= 4;
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

          SubtractVector(svF, st->tgt, &actor->posX);

          angleDiff = SignedAngleDelta8(st->angle, actor->rotZ);

          cosWeight1 = LookupCosine((st->t20 - 1) << 7);
          cosDelta = cosWeight1 - LookupCosine(st->t20 << 7);
          totalWeight = LookupCosine((st->t20 - 1) << 7) + 0x1000;

          actor->posX += (svF[0] * cosDelta) / totalWeight;
          actor->posY += ((svF[1] * cosDelta) / totalWeight);
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
        if (actor->scale < 96) {
          actor->scale += 4;
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

    /* Path-following enemy (WalkerState). */
    case 114: {
      WalkerState *st = (WalkerState *)actor->state;
      int distance;

      if ((actor->flags & 0xB0000) != 0 &&
          (unsigned int)(actor->mode - 7) >= 3) {
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);

        if ((actor->flags & 0x20000) != 0) {
          st->unk10 = 0x190;
          st->unk14 = 0x46;
          st->heading = StepAndLerpAngle8(
              ArcTan2(actor->posX - SPY.posX, actor->posY - SPY.posY, 0),
              SPY.bodyRotZ, 0x28, 0x80);
          ENTER_POSE(actor, 8);
        }

        PlaySoundEffect(g_pLevelSampleBankHeader[0x27], actor, 8, &actor->sfxSlot);
        st->unk14 = 0xB4;
        st->unk10 = 0x118;
        st->heading = StepAndLerpAngle8(
            ArcTan2(actor->posX - SPY.posX, actor->posY - SPY.posY, 0),
            SPY.bodyRotZ, 0x20, 0x40);
        ENTER_POSE(actor, 7);
      }

      if (st->tgtIdx >= 0 && g_pActorListBase[st->tgtIdx].mode >= 0x80) {
        st->tgtIdx = -1;
      }

      switch (actor->mode) {
      case 0: {
        int xyDistance;
        int deltaY;
        int deltaZ;

        xyDistance = SPY.posX - actor->posX;
        deltaY = SPY.posY - actor->posY;

        xyDistance = ABS(xyDistance);
        xyDistance += ABS(deltaY);
        deltaZ = SPY.posZ - actor->posZ;
        deltaZ = ABS(deltaZ);
        distance = xyDistance + deltaZ;
        if (distance < 0x4000 &&
            ApproxDist2D(&actor->posX, &SPY.posX) < 0x2800 &&
            ABS2((actor->posZ - actor->floorOffset) - SPY.posZ) < 0x800) {
          ENTER_POSE(actor, 4);
        }

        if (st->tgtIdx >= 0 && TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          if (ApproxDist2D(&actor->posX, &g_pActorListBase[st->tgtIdx].posX) <
              0x1000) {
            ENTER_POSE(actor, 2);
          }

          ENTER_POSE(actor, 1);
        }

        SnapActorToGround(actor);
        break;
      }
      case 1: {
        int xyDistance;
        int deltaY;
        int deltaZ;

        xyDistance = g_pActorListBase[st->tgtIdx].posX - actor->posX;
        deltaY = g_pActorListBase[st->tgtIdx].posY - actor->posY;

        xyDistance = ABS(xyDistance);
        xyDistance += ABS(deltaY);
        deltaZ = g_pActorListBase[st->tgtIdx].posZ - actor->posZ;
        deltaZ = ABS(deltaZ);
        distance = xyDistance + deltaZ;

        if (distance < 0x1800 &&
            ApproxDist2D(&actor->posX, &g_pActorListBase[st->tgtIdx].posX) <
                0x1000) {
          ENTER_POSE(actor, 2);
        }

        if (g_nActorAnimDone != 0) {
          st->timer = (GetRandomU32() & 0x3F) + 0x20;
          ENTER_POSE(actor, 0);
        }

        SnapActorToGround(actor);
        break;
      }

      case 2: {
        int xyDistance;
        int deltaY;
        int deltaZ;

        xyDistance = SPY.posX - actor->posX;
        deltaY = SPY.posY - actor->posY;

        xyDistance = ABS(xyDistance);
        xyDistance += ABS(deltaY);
        deltaZ = SPY.posZ - actor->posZ;
        deltaZ = ABS(deltaZ);
        distance = xyDistance + deltaZ;
        if (distance < 0x4000 &&
            ApproxDist2D(&actor->posX, &SPY.posX) < 0x2800 &&
            ABS2((actor->posZ - actor->floorOffset) - SPY.posZ) < 0x800) {
          ENTER_POSE(actor, 4);
        }

        TurnActorYawTowardAngle(actor,
                      ArcTan2(g_pActorListBase[st->tgtIdx].posX - actor->posX,
                                    g_pActorListBase[st->tgtIdx].posY - actor->posY,
                                    0),
                      8, 0x10, 1);

        if (g_nActorAnimDone != 0) {
          st->timer = (GetRandomU32() & 0x3F) + 0x20;
          ENTER_POSE(actor, 0);
        }

        SnapActorToGround(actor);
        break;
      }

      case 3: {
        int angle;
        int angleDelta;
        int xyDistance;
        int deltaY;
        int deltaZ;

        xyDistance = SPY.posX - actor->posX;
        deltaY = SPY.posY - actor->posY;

        xyDistance = ABS(xyDistance);
        xyDistance += ABS(deltaY);
        deltaZ = SPY.posZ - actor->posZ;
        deltaZ = ABS(deltaZ);
        distance = xyDistance + deltaZ;
        if (distance < 0x4000 &&
            ApproxDist2D(&actor->posX, &SPY.posX) < 0x2800 &&
            ABS2((actor->posZ - actor->floorOffset) - SPY.posZ) < 0x800) {
          if (st->path->cur < st->path->count - 1) {
            st->path->cur++;
          }
          ENTER_POSE(actor, 4);
        }

        if (ApproxDist2D(&actor->posX,
                          (int *)&st->path->nodes[st->path->cur]) < 0x200) {
          if (st->path->cur == 0) {
            ENTER_POSE(actor, 0);
          }
          st->path->cur--;
        }

        angle = ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                              st->path->nodes[st->path->cur].y - actor->posY, 0);
        angleDelta = (angle - actor->rotZ) & 0xFF;
        if (angleDelta > 0x80) {
          angleDelta -= 0x100;
        }

        if (ABS2(angleDelta) < 0x20) {
          MoveActorAlongHeading(actor, 0x40, 0x200, 0x200, 0x27);
        }

        if (angleDelta < -8) {
          angleDelta = -8;
        }
        if (angleDelta > 8) {
          angleDelta = 8;
        }

        actor->rotZ += angleDelta;
        break;
      }

      case 4: {
        if (g_nActorAnimDone != 0) {
          if (SPY.state == 0xB || SPY.state == 0x14) {
            if (actor->anim != 6) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, 6);
            }
            actor->mode = 0x14;
            continue;
          }

          ENTER_POSE(actor, 5);
        }

        TrackEntityYawTowardSpyro(actor, 8, 0x10, 1);
        break;
      }

      case 5: {
        int angle;
        int angleDelta;

        if (ApproxDist2D(&actor->posX,
                          (int *)&st->path->nodes[st->path->cur]) < 0x200) {
          if (st->path->cur < st->path->count - 1) {
            st->path->cur++;
          } else {
            st->timer = 0x78;
            ENTER_POSE(actor, 6);
          }
        }

        angle = ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                              st->path->nodes[st->path->cur].y - actor->posY, 0);
        angleDelta = (angle - actor->rotZ) & 0xFF;
        if (angleDelta > 0x80) {
          angleDelta -= 0x100;
        }

        if (ABS2(angleDelta) < 0x20) {
          MoveActorAlongHeading(actor, 0x82, 0x200, 0x200, 0x27);
        }

        if (angleDelta < -8) {
          angleDelta = -8;
        }
        if (angleDelta > 8) {
          angleDelta = 8;
        }

        actor->rotZ += angleDelta;
        break;
      }

      case 6: {
        int xyDistance;
        int deltaY;
        int deltaZ;

        xyDistance = SPY.posX - actor->posX;
        deltaY = SPY.posY - actor->posY;

        xyDistance = ABS(xyDistance);
        xyDistance += ABS(deltaY);
        deltaZ = SPY.posZ - actor->posZ;
        deltaZ = ABS(deltaZ);
        distance = xyDistance + deltaZ;
        if (distance < 0x4000 &&
            ApproxDist2D(&actor->posX, &SPY.posX) < 0x2800 &&
            ABS2((actor->posZ - actor->floorOffset) - SPY.posZ) < 0x800) {
          TrackEntityYawTowardSpyro(actor, 8, 0x10, 1);
          st->timer = 0x78;
          break;
        }

        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          st->path->cur = st->path->count - 1;
          ENTER_POSE(actor, 3);
        }

        break;
      }
      case 7: {
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, 9);
        }

        actor->rotY += 4;
        TurnActorYawTowardAngle(actor, (st->heading + 0x80) & 0xFF, 0x10, 0x20, 1);
        DriveActorTrackedMove(actor, &st->unk10, st->heading, &st->unk14, 0xC, 0xC);
        break;
      }

      case 8: {
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x1C);
          DespawnActorRecord(actor);
          continue;
        }

        DriveActorTrackedMove(actor, &st->unk10, st->heading, &st->unk14, 0xC, 0xC);
        TurnActorYawTowardAngle(actor, (st->heading + 0x80) & 0xFF, 0x10, 0x20, 1);
        break;
      }

      case 9: {
        if (DriveActorTrackedMove(actor, &st->unk10, st->heading, &st->unk14, 0xC,
                          0x10) == 3) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x10);
          DespawnActorRecord(actor);
          continue;
        }

        TurnActorYawTowardAngle(actor, (st->heading + 0x80) & 0xFF, 0x10, 0x20, 1);
        break;
      }

      case 0x14: {
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);

        if (SPY.state != 0xB && SPY.state != 0x14) {
          ENTER_POSE(actor, 5);
        }

        break;
      }

      default: {
        break;
      }
      }

      RefreshActorDerivedState(actor, 1);
      break;
    }

    /* Sparx (FlierState). */
    case 120: {
      FlierState *st = (FlierState *)actor->state;
      char pitchAngle;

      if (SPY.u164 >= 2) {

        svF[0] = 0;
        svF[1] = 0x64;
        svF[2] = 0;
        RotateVectorByMatrix(actor->mtx, svF, svF);
        AddVector(svF, svF, &actor->posX);
        g_pfnLevelEmitParticles(2, 0x42, svF, 0);

        svF[0] = 0;
        svF[1] = -0x64;
        svF[2] = 0;
        ApplyActiveGteRotation(svF, svF);
        AddVector(svF, svF, &actor->posX);
        g_pfnLevelEmitParticles(2, 0x42, svF, 0);
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
            st->fx->desc = &g_anSparkleGlowDesc;
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

          CopyVector(svA, &target->posX);
          if (isPickupAnimPhase == 0) {
            svA[0] -= g_anCosineLut[st->rider->rotZ] >> 4;
            svA[1] -= g_anSineLut[st->rider->rotZ] >> 4;
          } else {
            svA[0] -= g_anCosineLut[st->rider->rotZ] >> 6;
            svA[1] -= g_anSineLut[st->rider->rotZ] >> 6;
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

          SubtractVector(svA, svA, &actor->posX);
          dist = VectorLength(svA, 1);

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
            ScaleVector3ByRatio(svA, dist, targetSpeed + 5);
          } else {
            ScaleVector3ByRatio(svA, dist, targetSpeed - 5);
          }
          AddVector(&actor->posX, &actor->posX, svA);
          TurnActorYawTowardAngle(actor, ArcTan2(svA[0], svA[1], 0), 0xA, 0, 0);
          pitchAngle = ArcTan2(VectorLength(svA, 0), svA[2], 0);
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
              g_pfnLevelEmitParticles(1, 0xC, (int *)actor, g_dwBurstColorType14);
            else if (st->rider->type == 15)
              g_pfnLevelEmitParticles(1, 0xC, (int *)actor, g_dwBurstColorType15);
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

          svF[0] = -0x258;
          svF[1] = (GetRandomU32() & 0x310) - 0x188;
          if (CAM.state == 3) {
            svF[2] = (GetRandomU32() & 0x7F) + 0x64;
          } else {
            svF[2] = (GetRandomU32() & 0x1FF) - 0xC8;
          }
          st->vx = svF[0];
          st->vy = svF[1];
          st->vz = svF[2];
          st->timer = GetRandomU32() & 0x7B;
        } else {
          st->timer -= g_nFrameStep;
          if (CAM.state == 3) {
            svF[0] = st->vx + (SPY.u118 >> 2);
            svF[1] = st->vy;
            svF[2] = st->vz;
            if (svF[2] < 0x64)
              svF[2] = 0x64;
          } else {
            if (CAM.state == 0x80000009)
              svF[0] = st->vx - 512;
            else {
              svF[0] = st->vx;
            }
            svF[1] = st->vy;
            svF[2] = st->vz;
          }
          RotateVectorByMatrix(&SPY.u034, svF, svF);
          AddVector(svF, svF, &SPY.posX);
          SubtractVector(svF, svF, &actor->posX);
          RShiftVector3(svF, 2);
          AddVector(&actor->posX, &actor->posX, svF);

          if (CollideSphereWithWorldAndActors(&actor->posX, 0x100, 0x100)) {
            CopyVector(&actor->posX, &g_anCollisionResolvedPos);
          }

          if (svF[2] > 32)
            svF[2] = 32;
          if (svF[2] < -32)
            svF[2] = -32;
          actor->rotX = 0;
          actor->rotY = svF[2];
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
          actor->scale = 0x40;
          actor->unk50 = 0;
          actor->updateDist = 0;
          actor->drawn = 0;
          actor->unk1C = 0;
          actor->visZone = 0xFF;
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

    /* Talking dragon: when Spyro faces it up close, start its dialogue. */
    case 187: {
      if (actor->mode == 0) {
        if (ApproxDist2D(&actor->posX, &SPY.posX) >= 0x1000) {
          actor->mode = 1;
        }
      } else if ((SPY.state == 0 || SPY.state == 1 || SPY.state == 0x15 ||
                  SPY.state == 2 || SPY.state == 0xB) &&
                 ApproxDist2D(&actor->posX, &SPY.posX) < 0x780 &&
                 AbsAngleDelta8(SPY.bodyRotZ,
                               ArcTan2(actor->posX - SPY.posX,
                                             actor->posY - SPY.posY, 0)) <
                     0x20 &&
                 AbsAngleDelta8(actor->rotZ,
                               ArcTan2(SPY.posX - actor->posX,
                                             SPY.posY - actor->posY, 0)) <
                     0x28) {
        actor->mode = 0;
        SPY.controlFlags = 0x80002000;
        ResetSpyroLinearMotion();
        ZeroVector(&SPY.headLook);
        g_pfnLevelBeginActorCutscene(actor);

        if (g_nWorldDragonsRescued < 10) {
          if (g_abSaveOptionsBlock[1] == 0) {
            g_nGamestate0cDialogPage = 0;
            g_abSaveOptionsBlock[1] = 1;
          } else {
            g_nGamestate0cDialogPage = 2;
          }
        } else if (g_abSaveOptionsBlock[1] < 2) {
          if (g_abSaveOptionsBlock[1] == 0) {
            g_nGamestate0cDialogPage = 3;
          } else {
            g_nGamestate0cDialogPage = 4;
          }
          g_abSaveOptionsBlock[1] = 2;
        } else {
          g_nGamestate0cDialogPage = 0x1E;
          g_nGamestate0cDialogVariant = (int)GetRandomU32() % 3;
        }
      }
      break;
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

      actor->soundRange = 0x20;
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

    /* Ambient sound source (RoamState). */
    case 286: {
      RoamState *st = actor->state;
      int snd;

      actor->soundRange = st->unk04;
      snd = -1;

      switch (st->mode) {
      case 0:
        snd = 6;
        break;

      case 1:
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          st->timer = 0x258 - (GetRandomU32() & 0x7F);
          snd = (GetRandomU32() & 7) + 7;
        }
        break;

      case 2:
        switch (st->sub) {
        case 0:
          if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
            st->timer = 0x200 - (GetRandomU32() & 0x7F);
            snd = (GetRandomU32() & 1) + 0xD;
            st->sub = 1;
          }
          break;

        case 1:
          if (st->count-- > 0) {
            /* Coasting along the current leg. */
            actor->posX += st->vel[0];
            actor->posY += st->vel[1];
            actor->posZ += st->vel[2];
          } else {
            /* Leg done: snap to the next node, then start the next leg. */
            actor->posX = PATH_X(st->path, (st->path[1] + 1) % st->path[0]);
            actor->posY = PATH_Y(st->path, (st->path[1] + 1) % st->path[0]);
            actor->posZ = PATH_Z(st->path, (st->path[1] + 1) % st->path[0]);
            st->sub = 0;
            st->path[1] = (st->path[1] + 1) % st->path[0];
            {
              int offA = (st->path[1] << 4) + 8;
              int offB = (((st->path[1] + 1) % st->path[0]) << 4) + 8;

              st->count =
                  ComputeStepTowardPoint((int *)(st->path + offA),
                                (int *)(st->path + offB), st->vel, st->unk14);
            }
          }

          if (actor->sfxSlot == 0x7F && st->count % 48 == 0) {
            snd = 0xE;
          }
          break;
        }
        break;

      case 3:
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          st->timer = 0x270 - (GetRandomU32() & 0x7F);
          snd = (GetRandomU32() & 1) + 0xF;
        }
        break;

      case 4:
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          st->timer = 0x258 - (GetRandomU32() & 0x7F);
          snd = (int)GetRandomU32() % 4 + 0x15;
        }
        break;

      case 5:
        snd = 0x18;
        break;

      case 6:
        snd = 0x19;
        break;

      case 7:
        snd = 0x12;
        break;

      case 8:
        snd = 0x1A;
        break;

      case 9:
        snd = 0x36;
        break;
      }

      if (snd < 0) {
        break;
      }
      PlaySoundEffect(g_pLevelSampleBankHeader[snd], actor, 8, &actor->sfxSlot);
      break;
    }

    /* Camera trigger volume (TriggerState). */
    case 300: {
      TriggerState *st;
      SpyroObj *spyro;
      int distance;
      int flags;
      st = (TriggerState *)actor->state;
      if (st->flags & 8) {
        if (g_pActorListBase[st->idx].type == 0xAD) {
          if (g_pActorListBase[st->idx].subMode == 0)
            break;
          if (ApproxDist2D(&actor->posX, &SPY.posX) >= 0x1C01)
            break;
        } else if (g_pActorListBase[st->idx].type == 0xFA ||
                   g_pActorListBase[st->idx].type == 0x53) {
          if (g_pActorListBase[st->idx].mode < 0x80)
            break;
        } else {
          break;
        }
      }
      flags = st->flags;
      if (flags & 2) {
        if (st->timer != 0) {
          if (TickCountdownByFrameDelta(&st->timer, 4) != 0)
            st->flags = (st->flags & ~2) | 4;
          break;
        }
        if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x1000 &&
            ABS2((actor->posZ - actor->floorOffset) - SPY.posZ) < 0x400) {
          st->timer = 0xB4;
        }
        break;
      }
      if (flags & 0x10)
        actor->subMode = 1;
      PlaySoundEffect(g_pLevelSampleBankHeader[54], actor, 8, &actor->sfxSlot);
      if ((GetRandomU32() & 1) == 0)
        g_pfnLevelEmitParticles(1, 6, (int *)actor, 0);
      SubtractVector(svK, &SPY.posX, &actor->posX);
      distance = VectorLength(svK, 0);
      if (distance < st->unk04 && svK[2] >= -0x3FF && svK[2] < st->unk00 &&
          (SPY.state == 0x11 || SPY.posZ + 0xC00 < actor->posZ + st->unk00)) {
        spyro = &SPY;
        spyro->controlFlags = 0x80000000;
        SPY.pScriptedActor = actor;
        SPY.damageFlags |= 0x800;
        SPY.u200 = actor->posZ + st->unk00;
        CopyVector(&SPY.contact, &actor->posX);
        if (st->flags & 1) {
          spyro->controlFlags |= 0x8000;
        } else {
          g_anCameraActiveTargetParams = g_anCameraParamMode2Front;
          SPY.pCamAnchor = &SPY.posX;
          SPY.pCamTarget = &g_anCameraActiveTargetParams;
          spyro->controlFlags |= 0x200;
          g_anCameraActiveTargetParams.w[0] = ((0x80 - actor->rotZ) << 4) & 0xFFF;
        }
        st->timer = 0x78;
        break;
      }
      if ((st->flags & ~1) == 4 && st->timer != 0 &&
          TickCountdownByFrameDelta(&st->timer, 4) != 0) {
        st->flags = (st->flags & ~4) | 2;
      }
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
        p->killGroup &= 0x7F;
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

    /* Dragon pad: saves a checkpoint when Spyro lands on it. */
    case 331: {
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

    /* Animated scenery: re-randomised speed; plays its hit animation when
     * damaged. */
    case 336:
    case 342:
      if (g_nActorAnimDone != 0) {
        /* Base speed comes from the animation that is playing. */
        int poseBase = TYPE_DESC(actor, actor->nextAnim);

        actor->frameRate = poseBase + RandomIntInRange(-4, 4);
      }

      if ((actor->flags & 0x10000) != 0 && actor->nextAnim == 0) {
        if (actor->anim != 1) {
          g_nActorAnimDone = 0;
          ANIM_RESET(actor, 1);
        }
        break;
      }

      if (actor->nextAnim != 1) {
        break;
      }

      RefreshActorDerivedState(actor, 1);

      if (g_nActorAnimDone != 0 && actor->anim != 2) {
        g_nActorAnimDone = 0;
        ANIM_RESET(actor, 2);
      }
      break;

    /* Flock leader (FlockState). */
    case 339: {
      FlockState *st = actor->state;

      if ((actor->flags & 0xB0000) != 0 && actor->mode < 4) {
        while (st->chainA >= 0 && g_pActorListBase[st->chainA].mode >= 0x80) {
          st->chainA = *(int *)g_pActorListBase[st->chainA].state;
        }

        if (st->chainA >= 0) {
          Actor *p = &g_pActorListBase[st->chainA];
          int *link = p->state;

          CopyVector(&p->posX, &actor->posX);
          SpawnActorDeathDrop(p, 1, 0, 0);
          MarkActorCommittedKill(p);
          st->chainA = *link;
          DespawnActorRecord(p);
          actor->flags = 0;
          ENTER_POSE(actor, 4);
        } else {
          int n = 0;

          while (st->chainB >= 0) {
            Actor *p = &g_pActorListBase[st->chainB];
            int *link = p->state;

            if (p->mode < 0x80) {
              CopyVector(&p->posX, &actor->posX);
              st->slots[n][0] = actor->posX + (LookupCosine(n * 0x555) >> 2);
              st->slots[n][1] = actor->posY + (LookupSine(n * 0x555) >> 2);
              st->slots[n][2] = actor->posZ;
              SpawnActorDeathDrop(p, 2, 0, st->slots[n]);
              n++;
              MarkActorCommittedKill(p);
              DespawnActorRecord(p);
            }

            st->chainB = *link;
          }

          st->slots[n][0] = actor->posX + (LookupCosine(n * 0x555) >> 2);
          st->slots[n][1] = actor->posY + (LookupSine(n * 0x555) >> 2);
          st->slots[n][2] = actor->posZ;
          SpawnActorDeathDrop(actor, 2, 0, st->slots[n]);
          MarkActorCommittedKill(actor);
          st->unk04 = 0x118;
          st->unk00 = StepAndLerpAngle8(
              ArcTan2(actor->posX - SPY.posX, actor->posY - SPY.posY, 0),
              SPY.bodyRotZ, 0x20, 0x40);
          actor->mode = 5;
          ANIM_RESET(actor, 5);
          continue;
        }
      }

      switch (actor->mode) {
      case 0: {
        int distance = ApproxDist2D(&actor->posX, &SPY.posX);

        if (distance < 0x1400 &&
            ABS2((actor->posZ - actor->floorOffset) - SPY.posZ) < 0x400) {
          ENTER_POSE(actor, 1);
        }

        if (distance < 0x2800) {
          TrackEntityYawTowardSpyro(actor, 8, 0x10, 1);
        }
        break;
      }

      case 1: {
        TrackEntityYawTowardSpyro(actor, 8, 0x10, 1);

        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, 2);
        }
        break;
      }

      case 2:
      case 3:
      case 4: {
        int spyroAngle;
        int angle;
        int previousAngle;
        int nextNode;
        int previousNode;
        int nextAngle;

        if (actor->mode == 4) {
          actor->flags = 0;
        }

        if (ApproxDist2D(&actor->posX,
                          (int *)&st->path->nodes[st->path->cur]) < 0x400) {
          if (ApproxDist2D(&actor->posX, &SPY.posX) > 0x1400 ||
              ABS2((actor->posZ - actor->floorOffset) - SPY.posZ) > 0x800) {
            ENTER_POSE(actor, 0);
          } else {
            spyroAngle =
                ArcTan2(SPY.posX - actor->posX, SPY.posY - actor->posY, 0);
            nextNode = (st->path->cur + 1) % st->path->count;
            previousNode =
                (st->path->cur - 1 + st->path->count) % st->path->count;

            nextAngle =
                ArcTan2(st->path->nodes[nextNode].x - actor->posX,
                              st->path->nodes[nextNode].y - actor->posY, 0),
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

        angle = ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                              st->path->nodes[st->path->cur].y - actor->posY, 0);
        previousAngle = (angle - actor->rotZ) & 0xFF;
        if (previousAngle > 0x80) {
          previousAngle -= 0x100;
        }
        nextAngle = previousAngle;

        if (previousAngle > 8) {
          previousAngle = 8;
        }
        if (previousAngle < -8) {
          previousAngle = -8;
        }
        actor->rotZ += previousAngle;

        if (ABS2(nextAngle) < 0x20) {
          if (actor->mode == 4) {
            MoveActorAlongHeading(actor, 0xD2, 0x200, 0x200, 0x27);
          } else {
            MoveActorAlongHeading(actor, 0x60, 0x200, 0x200, 0x27);
          }
        }

        if (g_nActorAnimDone != 0) {
          if (actor->mode == 2) {
            actor->mode = 3;
            ANIM_RESET(actor, 3);
          } else {
            actor->mode = 2;
            if (actor->nextAnim != 2) {
              ANIM_ADVANCE(actor, 2);
            }
          }
          continue;
        }
        break;
      }
      case 5: {
        RefreshActorDerivedState(actor, 1);

        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x20);
          DespawnActorRecord(actor);
          continue;
        }

        DriveActorTrackedMove(actor, &st->unk04, st->unk00, 0, 0xC, 0);
        break;
      }
      }
      break;
    }

    /* Sequenced prop (PropAnimState). */
    case 350: {
      PropAnimState *st = actor->state;

      /* First tick: if already unlocked, skip to the finished state. */
      if (st->primed == 0) {
        if (D_80078E7D != 0) {
          ModifyWorldChunkAnimFlags(st->anim, 0xFC, 0);
          SetWorldChunkAnimStep(st->anim, 0x77, 0);
          actor->mode = 0x63;
        }
        st->primed = 1;
      }

      switch (actor->mode) {
      case 0:
        if (actor->subMode == 0x1F) {
          actor->mode = 5;
        }
        break;

      case 5:
        ModifyWorldChunkAnimFlags(st->anim, 0xFC, 0);
        actor->mode = 0xA;
        st->timer = 0x1E;
        actor->soundRange = 0x30;
        PlaySoundEffect(g_pLevelSampleBankHeader[0x41], actor, 8, &actor->sfxSlot);
        break;

      case 0xA:
        if ((GetWorldChunkAnimState(st->anim) & 2) != 0) {
          actor->mode = 0x63;
          break;
        }
        if (TickCountdownByFrameDelta(&st->timer, 4) == 0) {
          break;
        }
        actor->mode = 0xB;
        st->timer = 0x1E;
        break;

      case 0xB:
        if ((GetWorldChunkAnimState(st->anim) & 2) != 0) {
          actor->mode = 0x63;
          break;
        }
        if (TickCountdownByFrameDelta(&st->timer, 4) == 0) {
          break;
        }
        actor->mode = 0xA;
        st->timer = 0x1E;
        break;

      case 0x63:
        StopSoundVoicesByOwner(actor, 1);
        break;
      }
      break;
    }

    /* Water bubbles and splash. */
    case 405:
    case 477:
      /* Removed as soon as the animation finishes. */
      if (g_nActorAnimDone == 0) {
        break;
      }
      DespawnActorRecord(actor);
      continue;

    /* Collision-triangle rider (PatchRideState). */
    case 391: {
      PatchRideState *st = actor->state;
      int tri[3][3];

      if (st->handle < 0) {
        /* Unattached: probe just ahead and attach to that triangle. */
        tri[0][0] = (short)g_anCosineLut[actor->rotZ] >> 1;
        tri[0][1] = (short)g_anSineLut[actor->rotZ] >> 1;
        tri[0][2] = 0;
        AddVector(tri[0], tri[0], &actor->posX);

        if (CastRayWorldAndActors(tri[0], &actor->posX) == 0) {
          break;
        }

        st->handle = g_nLastCollisionHitChunkId;
        UnpackWorldCollisionTri(st->handle, tri[0]);
        AddVector(tri[0], tri[0], tri[1]);
        AddVector(tri[0], tri[0], tri[2]);
        tri[0][0] /= 3;
        tri[0][1] /= 3;
        tri[0][2] /= 3;
        SubtractVector(st->offset, &actor->posX, tri[0]);
        break;
      }

      UnpackWorldCollisionTri(st->handle, tri[0]);
      AddVector(tri[0], tri[0], tri[1]);
      AddVector(tri[0], tri[0], tri[2]);
      tri[0][0] /= 3;
      tri[0][1] /= 3;
      tri[0][2] /= 3;
      AddVector(&actor->posX, tri[0], st->offset);

      if ((g_dwGamestateFrames & 3) != 0) {
        break;
      }
      tri[0][0] = (short)g_anCosineLut[actor->rotZ] >> 5;
      tri[0][1] = (short)g_anSineLut[actor->rotZ] >> 5;
      tri[0][2] = 0x20;
      g_pfnLevelEmitParticles(1, 0xE, &actor->posX, tri[0]);
      break;
    }

    /* Exit vortex (PortalState). */
    case 398: {
      PortalState *st = actor->state;
      PortalDesc *p;
      int mag;
      short progressed;
      switch (actor->mode) {
      case 1: {
        int vec[3];

        SPY.controlFlags =
            (0x80000000 | 0x2000 | 0x4000 | 0x100 | 0x40 | 0x4 | 0x2 | 0x1);
        SPY.u248 = actor->subMode;
        SPY.pPortal = st->desc;
        SPY.u244 = 0x60;
        SPY.u218 = 5;
        SPY.u027 = 0x7F;

        p = st->desc;
        AddVector(vec, p->posA, p->posB);
        RShiftVector3(vec, 1);

        SubtractVector(&SPY.u208, vec, &SPY.posX);
        AddVector(&SPY.u208, &SPY.u208, st->offset);

        mag = VectorLength(&SPY.u208, 1);
        if (mag > 128) {
          ScaleVector3ByRatio(&SPY.u208, mag, 128);
        } else {
          if (actor->subMode != 0) {
            progressed = (st->desc->index < (st->desc->count - 1));
          } else {
            progressed = st->desc->index;
          }

          if (progressed) {
            actor->mode = 2;
          }
        }

        if (CollideSphereWithWorldAndActors(&CAM.posX, 0x300, 0x300) != 0 &&
            (g_nGroundSurfaceType & 0x3F) < 0x3F && g_pTriggerRegionTable[g_nGroundSurfaceType][0] == 6) {
          actor->mode = 0;
          CAM.unkC0 = (0x80000000 | 0x10 | 0x2);
          SPY.u218 = 0xF;
          g_nCdStreamState = 0;
          g_nLevelTransitionTallyActive = 1;
          g_nLevelTransitionPhase = 0;
          g_nGamestate = 1;
          g_nGamePaused = 1;
          SPY.u206 = st->unk14;
          ComputeCameraToTargetAngle(&CAM.posX);

          CAM.sim.v[0] += SPY.u11C;

          SPY.u11C += g_nScriptedMobViewYawSpin;
          SPY.bodyRotZ = SPY.u11C >> 4;
          CAM.sphere = CAM.sim;
          CAM.sphere.v[0] -= (SPY.bodyRotZ) << 4;
          ComputeCameraOrbitOffset(&CAM.posX);
          AddVector(&CAM.posX, &CAM.posX, &SPY.posX);
          UpdateCameraEulerAngles();
          g_nLevelIntroParallaxCountdown = g_nScriptedMobViewPitchTilt;
          if (st->unk18 >= 0) {
            CAM.preset = &g_aCameraXformPresets[st->unk18];
          } else {
            CAM.preset = 0;
          }
        } else {
          SPY.controlFlags |= 0x400;
        }
        break;
      }
      case 2: {
        SPY.controlFlags = 0x80000000 | 0x4000 | 0x2000 | 0x100;
        SPY.pPortal = st->desc;
        SPY.u244 = 0x60;
        SPY.u218 = 0xF;
        SPY.u027 = 0x7F;

        if (CollideSphereWithWorldAndActors(&CAM.posX, 0x300, 0x300) != 0 &&
            (g_nGroundSurfaceType & 0x3F) < 0x3F && g_pTriggerRegionTable[g_nGroundSurfaceType][0] == 6) {
          actor->mode = 0;
          CAM.unkC0 = (0x80000000 | 0x10 | 0x2);
          g_nCdStreamState = 0;
          g_nLevelTransitionTallyActive = 1;
          g_nLevelTransitionPhase = 0;
          g_nGamestate = 1;
          g_nGamePaused = 1;
          SPY.u206 = st->unk14;
          ComputeCameraToTargetAngle(&CAM.posX);

          CAM.sim.v[0] += SPY.u11C;

          SPY.u11C += g_nScriptedMobViewYawSpin;
          SPY.bodyRotZ = SPY.u11C >> 4;
          CAM.sphere = CAM.sim;
          CAM.sphere.v[0] -= (SPY.bodyRotZ) << 4;
          ComputeCameraOrbitOffset(&CAM.posX);
          AddVector(&CAM.posX, &CAM.posX, &SPY.posX);
          UpdateCameraEulerAngles();
          g_nLevelIntroParallaxCountdown = g_nScriptedMobViewPitchTilt;
          if (st->unk18 >= 0) {
            CAM.preset = &g_aCameraXformPresets[st->unk18];
          } else {
            CAM.preset = 0;
          }
        } else {
          SPY.controlFlags |= 0x400;
        }
        break;
      }
      }
      break;
    }

    /* Balloon: bobs about its start height. */
    case 416: {
      int *base = actor->state;

      if (actor->mode == 0) {
        *base = actor->posZ;
        actor->mode = 1;
        break;
      }

      actor->posZ = *base + (g_anSineLut[actor->subMode] >> 4);
      actor->subMode += g_nFrameStep;
      break;
    }

    /* Life chest (LiftState). */
    case 421: {
      LiftState *st = actor->state;
      Actor *rider;
      int i;
      int a;
      int b;

      if (st->unk14 == 1) {
        SnapActorToGround(actor);
        EncodeCachedVecToActorDirCode(actor);
      }

      if (actor->mode == 0) {
        st->unk04 += g_nFrameStep;

        if (st->unk04 >= 0x1E1) {
          st->unk04 = 0;
          actor->rotX = st->unk08;
          actor->rotY = st->unk0C;
          actor->posZ = st->unk10;
          actor->mode = 1;
          actor->frameProgress = 8;
          actor->frameRate = 8;
          actor->nextAnim = 1;

          st->unk00 = g_pfnLevelSpawnActor(0x1A6, actor);
          st->unk00->rotX = actor->rotX;
          st->unk00->rotY = actor->rotY;
          st->unk00->rotZ = actor->rotZ;
          st->unk00->frameRate = 0x10;
          st->unk00->updateDist = 0;
          st->unk00->unk50 = 0;
          st->unk00->drawn = 0;
          st->unk00->unk47 = 5;

          PlayActorMeshVoice(actor, 0, 0);
        } else if (st->unk04 >= 0x1A0) {
          actor->rotX = g_aShakeRotOffsets[LIFT_STEP(st)].a + st->unk08;
          actor->rotY = g_aShakeRotOffsets[LIFT_STEP(st)].b + st->unk0C;
          a = (signed char)g_aShakeRotOffsets[LIFT_STEP(st)].a;
          b = (signed char)g_aShakeRotOffsets[LIFT_STEP(st)].b;
          if (a < 0) {
            a = -a;
          }
          if (b < 0) {
            b = -b;
          }
          actor->posZ = st->unk10 + (a + b) * 6;

          if (actor->sfxSlot == 0x7F) {
            PlayActorMeshVoice(actor, 1, 0);
          }
        } else if (st->unk04 >= 0x19C) {
          /* Just before the shake: remember the rest pose. */
          st->unk08 = actor->rotX;
          st->unk0C = actor->rotY;
          st->unk10 = actor->posZ;
          if (st->unk14 == 1) {
            st->unk04 = 0;
          }
        }
      } else {
        if (actor->anim == 1) {
          st->unk04 += g_nFrameStep;
          st->unk00->unk50 = 0x10;
          st->unk00->updateDist = 0xFF;
        }

        if (st->unk04 >= 0xE6) {
          DespawnActorRecord(st->unk00);
          st->unk04 = GetRandomU32() & 0xFF;
          actor->frameProgress = 8;
          actor->frameRate = 8;
          actor->mode = 0;
          actor->nextAnim = 0;
          if (actor->sfxSlot == 0x7F) {
            PlayActorMeshVoice(actor, 3, 0);
          }
        }
      }

      if ((actor->flags & 0xB0000) == 0) {
        break;
      }

      rider = SpawnActorDeathDrop(actor, 5, 0, 0);
      if (st->unk14 == 1) {
        ((unsigned char *)rider->state)[0xE] = 1;
      }

      actor->soundRange = 0x20;
      PlayActorMeshVoice(actor, 4, 0);

      /* Fragment bursts; each stops early once the dynamic pool is full. */
      for (i = 0; i < 2; i++) {
        if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
          break;
        }
        g_pfnLevelSpawnActor(0x1A7, actor);
        g_pfnLevelSpawnActor(0x1A8, actor);
      }

      for (i = 0; i < 6; i++) {
        if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
          break;
        }
        g_pfnLevelSpawnActor(0x1A9, actor);
      }

      for (i = 0; i < 8; i++) {
        int vec[3];

        vec[0] = ((short)g_anCosineLut[i * 32] >> 8) * 3;
        vec[1] = ((short)g_anSineLut[i * 32] >> 8) * 3;
        vec[2] = 0x18;
        g_pfnLevelEmitParticles(1, 0, &actor->posX, vec);
      }

      g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (void *)0x18);

      if (actor->mode == 1) {
        DespawnActorRecord(st->unk00);
      }

      PlaySoundEffect(g_apActorMeshTable[actor->type]->m_Sounds[2], actor, 8,
                    (void *)(long)actor->sfxSlot);
      DespawnActorRecord(actor);
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
          int vecA[3];
          int vecB[3];
          int vecC[3];
          int vecD[3];

          actor->subMode += 8;
          anglespyro = ArcTan2(SPY.posX - st->owner->posX,
                                     SPY.posY - st->owner->posY, 0);
          vecA[0] = g_anCosineLut[anglespyro] * 3 >> 4;
          vecA[1] = g_anSineLut[anglespyro] * 3 >> 4;
          vecA[2] = 0;
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
          vecD[0] = vecA[0] * g_anCosineLut[anglespyro];
          vecD[1] = vecA[1] * g_anCosineLut[anglespyro];
          vecD[2] = 0;
          anglespyro = ((anglespyro - st->unk06 * 4)) & 0xFF;
          ScaleVector3Sat(vecB, vecB, st->unk06);
          SubtractVector(vecC, vecC, vecB);
          actor->posX = vecA[0] * g_anCosineLut[anglespyro];
          actor->posY = vecA[1] * g_anCosineLut[anglespyro];
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, vecD);
          RShiftVector3(&actor->posX, 10);
          AddVector(&actor->posX, &actor->posX, vecA);
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
