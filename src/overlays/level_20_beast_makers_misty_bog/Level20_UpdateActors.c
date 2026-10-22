/* Level20_UpdateActors -- per-frame actor update for level 20 (Misty
 * Bog). 0x8007AFBC, 38,748 bytes.
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class. Most
 * classes are shared with levels 1/3/7/10/13/18/26/30; the level-20 classes
 * are the gunner and its partner (28), the lurker with its tether (403), the
 * grabber that carries Spyro off (476) and a small animated prop (487). The
 * guard (137) and roaming ram (466) are shared with level 18.
 *
 * Load-bearing source forms:
 *   - Carriers: `sixteen` (s7) and, at the loop top, `base = D_80078BBC` with
 *     `spy = base - 0x164` (fp). Making `base` a loop-top movable is what
 *     keeps 5, 8 and g_apActorMeshTable out of loop.c's hoist set; case 28's pose-5
 *     and pose-8 sites are ENTER_POSE_B (A285).
 *   - The hit blocks of 28/137/403/466 open with an empty do/while(0) so cse
 *     keeps spy->x fp-relative (A200).
 *   - Case 28 reads its partner through a pointer in one place and by index
 *     in the other; the two spellings give the original's two addu orders.
 */

#include "actor.h"
#include "vecmath.h"

/* The update list, and the per-actor values the loop publishes before it
 * dispatches. */
extern Actor *g_apSaveMenuAnimActorList[]; /* live actor list, 0-terminated */
extern int g_nFrameStep;      /* frame delta in ticks */
extern int g_nActorFrameDelta;      /* frame delta, as seen by the callees */
extern int g_nActorAnimDone;      /* current actor's animation finished */
extern int g_nActorAnimNewFrame;      /* current actor's new-frame flag */

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
extern Actor *g_pLevelOverlayInitRecord;   /* Sparx, or null when absent */

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

/* Type 286: ambient sound source; mode 2 also coasts along a path. */
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
extern int RandomIntInRange(int lo, int hi); /* random in [lo, hi] */
extern int RandomSignedInRange(int a, int b);

/* Step a countdown timer; non-zero when it fires. */
extern int TickCountdownByFrameDelta(void *timer, int step);
extern int WrapAddAngle8(int a, int b);
extern int WrapAngleDelta8(int a, int b); /* signed angle delta */

/* Particle spawn hook. The last argument is either a packed parameter word
 * or a pointer to a velocity vector, depending on the effect. */
extern void (*g_pfnLevelEmitParticles)(int a, int effect, int *pos, void *c);

/* Actor spawn hook: (type, parent) -> new actor. */
extern Actor *(*g_pfnLevelSpawnActor)(int type, Actor *parent);

/* Level actor pool, switch flags, and the sound tables and override
 * globals used when a behaviour plays a sound. */
extern Actor *g_pActorListBase;         /* level actor pool */
extern Actor *g_pActorPoolDynBase;         /* end of the level actor pool */
extern unsigned char *g_pLevelSampleBankEntries; /* sound definitions, 20 bytes each */
extern unsigned char *g_pLevelSampleBankHeader; /* sound id table */
extern int g_nSfxOverrideFlags;
extern struct {
  short v;
} g_nSfxOverridePitch;

/* Egg sequence flags, then the vector library: copy, add, subtract, scale,
 * shift, sine and cosine. */
extern int g_nGameplayBlocked;
extern struct {
  int v;
} g_nHudWorldEggRollRequest;
extern int g_dwPadPressed; /* pad buttons pressed; 0x40 skips the egg */
extern int g_nLevelIntroIndex;
extern int g_anLevelEggsRecovered[];
extern int g_nWorldEggsRecovered;

extern void AimCameraStickTowardPoint(int *pos);
extern void VectorToSVectorShr2(short *d, int *s);
extern void SVectorToVectorShl2(int *d, int *s);

/* Mark an item collected; the second argument is the actor or its id. */
extern void CommitActorKillGems(int a, void *b);

extern int SteerActorAroundPoint(Actor *actor, int *target, int a, int heading, int b,
                          int c, int d, int e, int f, int g, int h, int i,
                          int j);

/* Type 49: sequenced prop driven by the 0x8002Bxxx player. Starts once any
 * exit vortex flag is set and its linked actor is dormant. */
typedef struct AnimState {
  int handle;  /* 0x00 */
  int started; /* 0x04 one-shot init latch */
  int idx;     /* 0x08 pool index of the actor this one watches */
} AnimState;

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
extern int g_nCollisionProbeVecY;
extern int g_nCollisionProbeVecZ;

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

/* SpawnActorRotBillboard spawn offsets (one 0xC-byte table from 0x8006e57c), per model family. */
extern int g_anRotBillboardOffset3;
extern int g_anRotBillboardOffset5;
extern int g_anRotBillboardOffset4;

/* Type 173: key. Rocks in place, sparkles, and is collected on contact. */
typedef struct SpinState {
  short timer;         /* 0x00 */
  unsigned char angle; /* 0x02 */
  unsigned char anim;  /* 0x03 0xFF == none */
} SpinState;

extern int g_nLevelEggCount; /* key collected */

/* Type 187: talking dragon. Dragon count, dialogue selection, and the latch
 * in the save-options block. */
extern int g_nWorldDragonsRescued;
extern void (*g_pfnLevelBeginActorCutscene)(Actor *actor);

/* Dynamic actor pool counters; the difference is the free slot count. */
extern int g_nActorPoolLiveCount;
extern int g_nActorPoolCapacity;

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

/* Type 398: exit vortex. Surface-below flags and the level transition
 * globals. */
extern int g_nGroundSurfaceType;             /* index of the surface under the probe */
extern int g_nCdStreamState;
extern int g_nLevelTransitionPhase;
extern int g_nLevelTransitionTallyActive;
extern int g_nGamePaused;
extern int g_nGamestate;

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

/* SpawnActorRotBillboard spawn offsets for the crystal dragon, one per sparkle phase. */
extern int g_anRotBillboardOffset0;
extern int g_anRotBillboardOffset1;
extern int g_anRotBillboardOffset2;

extern int g_anLevelDragonsRescued[]; /* per-g_nLevelIntroIndex completion counters */

/* Type 300: camera trigger volume. Live camera sphere and its template. */
typedef struct Xform6 {
  int w[6];
} Xform6;
extern Xform6 g_anCameraActiveTargetParams; /* live camera transform */

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

/* Collision contact point, pickup counters and sound volume override. */
extern int g_anCollisionResolvedPos;   /* collision contact point */
extern int g_nCollisionResolvedY;   /* ..y */
extern int g_nCollisionResolvedZ;   /* ..z */
extern int g_nLastCollisionHitChunkId;   /* collision triangle index */
extern int g_nExtraLives;   /* life count, capped at 99 */
extern int g_nSparxFoodCount;   /* life orb count */
extern short g_nSfxOverrideVolL; /* sound volume override, left/right */
extern short g_nSfxOverrideVolR;

/* 256-entry sine and cosine tables. */
extern short g_anSineLut[];
extern short g_anCosineLut[];

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
  PathData *path; /* 0x00 */
  int timer;      /* 0x04 */
  int tgtIdx;     /* 0x08 index into g_pActorListBase, -1 when unclaimed */
  int heading;    /* 0x0C */
  int unk10;      /* 0x10 passed by address to DriveActorTrackedMove */
  int unk14;      /* 0x14 likewise */
} WalkerState;

extern int TrackEntityYawTowardSpyro(Actor *actor, int a, int b, int c);
extern int StepAndLerpAngle8(int ang, int facing, int a, int b);

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

extern int D_8006E390;
extern void *D_8006E490;
extern void *D_8006E494;
extern void *g_adwActorBurstColors[];

extern FxNode *AllocWorldSparkle(void);
extern void ClearWord(FxNode *node);
extern int StepAngle8Toward(int a, int b, int c);

/* Type 339: flock leader. Two linked chains of follower actors, a path, and
 * one formation slot per follower plus one for the leader. */
typedef struct FlockState {
  int unk00;      /* 0x00 knockback angle */
  int unk04;      /* 0x04 knockback speed */
  PathData *path; /* 0x08 */
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

extern int g_dwGamestateFrames; /* frame counter; the arm acts on every 4th frame */

/* Type 350: sequenced prop driven by the 0x8002Bxxx player, unlocked once. */
typedef struct PropAnimState {
  int anim;   /* 0x00 handle for ModifyWorldChunkAnimFlags / B3F4 / B444 */
  int timer;  /* 0x04 */
  int primed; /* 0x08 */
} PropAnimState;

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
  char pad120[0x44];
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

/* Case 16 — the balloonist's gnorc: wanders until the balloonist is up, then
 * hands itself to the flight cutscene. */
typedef struct WanderState {
  int turn;              /* 0x00 turn direction, +1 / -1 */
  int home[3];           /* 0x04 spawn position it stays near */
  unsigned char speed;   /* 0x10 forward speed, signed when >= 0x80 */
  unsigned char heading; /* 0x11 */
  unsigned char reTurn;  /* 0x12 frames until the next heading change */
  unsigned char reStep;  /* 0x13 frames until the next speed change */
  unsigned char unk14;   /* 0x14 */
} WanderState;

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

/* Case 194 — breakable chest: one hit spawns its gem shower. */
typedef struct ChestState {
  int unk0;
  int shake; /* 0x04 */
} ChestState;

/* Case 195 — the same chest with a wobble pose played before it breaks. */
typedef struct WobbleState {
  int phase;  /* 0x00 wobble phase, 0 when at rest */
  int rotX;   /* 0x04 rest pose */
  int rotY;   /* 0x08 */
  int restZ;  /* 0x0C */
  int unk10;  /* 0x10 */
  int handle; /* 0x14 */
} WobbleState;

/* Case 208 — the hub's music/voice trigger. */
typedef struct MusicState {
  int started; /* 0x00 */
} MusicState;

/* Case 236 — the charging gnorc: winds up, dashes, recovers. */
typedef struct ChargeState {
  char pad00[0x20];
  int speed;   /* 0x20 */
  int heading; /* 0x24 */
  char pad28[0x4];
  int rest; /* 0x2C frames before the next charge */
} ChargeState;

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

/* Cases 260..269 — the thrown projectile: flies until it lands. */
typedef struct ShotState30 {
  int life; /* 0x00 */
  char pad04[0x4];
  int vel[2]; /* 0x08 */
  int velZ;   /* 0x10 */
} ShotState30;

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

/* Case 323 — the respawn warden: puts every dormant actor of its group back
 * once Spyro is far enough away. */
typedef struct WardenState {
  int group;  /* 0x00 */
  int timer;  /* 0x04 */
  int period; /* 0x08 */
} WardenState;

/* Case 398 — the level-exit vortex: drives the scripted camera and hands the
 * game over to the world map. */
typedef struct ExitState {
  PortalDesc *portal; /* 0x00 */
  int centre[3];      /* 0x04 */
  int unk10;          /* 0x10 */
  int dest;           /* 0x14 */
  int preset;         /* 0x18 */
} ExitState;

/* Case 416 — the bobbing platform. */
typedef struct BobState {
  int restZ; /* 0x00 */
} BobState;

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

extern short g_nCameraEulerYaw; /* camera yaw, as the balloonist reads it */
extern int g_nCameraCurrentMode;   /* camera mode */
extern int g_nCameraNextMode;

extern int g_nSpyroState;   /* Spyro's state */
extern int g_nSpyroStateFlags;
extern int g_nSpyroStateTimer;
extern int g_nSpyroAirborneFrames;
extern int g_anSpyroMotionVec; /* look-at offset */
extern int D_80078B68;
extern int g_nSpyroMotionZ;
extern int g_nSpyroMotionSpeed;
extern int g_nSpyroBodyYaw; /* camera yaw accumulator */
extern volatile unsigned char
    g_bSpyroPersistentEulerZ;          /* SPY.bodyRotZ, absolute form (A240) */
extern int D_80078BBC[]; /* balloon sequence stage (== SPY.u164) */
extern int g_nSpyroSwingEulerPitchTarget;

extern int g_dwSpyroRequestMask;         /* scripted camera control word */
extern struct {
  int x;
  int y;
} g_anSpyroRequestVelocity; /* scripted camera look-at */
#define g_nSpyroRequestVelocityY g_anSpyroRequestVelocity.y
extern int g_nSpyroRequestVelocityZ;
extern int g_nSpyroRequestState; /* scripted camera mode */

extern void ModifyWorldChunkAnimFlags(int ch, int vol, int a);
extern void SetWorldChunkAnimStep(int ch, int n, int a);
extern int GetWorldChunkAnimState(int ch);
extern int ComputeStepTowardPoint(int *from, int *to, int *step, int speed);
extern int WrapAngleDelta8(int a, int b);

/* ==== Globals shared with level 4 ==== */
extern short g_nCameraEulerPitch; /* camera yaw */
extern unsigned char g_abHudCounterDisplayState;
extern int g_dwSpyroTriggerEventFlags; /* SPY.damageFlags, absolute form */
extern int D_80078C58;
extern Actor *g_pScriptedCameraActor;
extern unsigned char g_abLevelGemPercent[]; /* per-level "left through the exit" flags */
extern int g_nPreserveMobIdx;
extern int g_nScriptedRespawnFlag;
extern int g_nScriptedMobTag;
extern int g_nCurrentLevelId; /* current level id */
extern int g_nActiveLevelId;
extern int g_nSpyroFallTrackingFlag;
extern int g_nSpyroFlameBreathTimer;
extern int g_nSpyroWorldPosZ; /* SPY.posZ, absolute form */
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

/* Case 349 -- spinning enemy. */
typedef struct SpinnerState {
  int count; /* 0x00 */
  int timer; /* 0x04 */
  int angle; /* 0x08 knock-back heading */
  int speed; /* 0x0C */
  int spin;  /* 0x10 */
} SpinnerState;

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

/* ==== Level 16 classes ==== */

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

/* Cases 152/153 -- sparking debris. */
typedef struct DebrisFallState {
  int vel[3];             /* 0x00 */
  int floor;              /* 0x0C */
  unsigned char spin[3];  /* 0x10 */
} DebrisFallState;

/* Case 283 -- path guard. */
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
extern int D_8006E52C[];
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
extern int D_8006E5C4[][3];

/* Case 497 -- humming bell. */
typedef struct L16Bell {
  char pad00[0x10];
  int chan;   /* 0x10 voice channel */
  int timer;  /* 0x14 */
  int period; /* 0x18 */
  char pad1C[0x14];
  int target; /* 0x30 index into g_pActorListBase */
} L16Bell;
extern int g_dwGamestateFrames;

/* Case 495 -- the level boss. */
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
extern int IsPointInRotatedBox(int *a, int *b);
extern int TweenActorAlongPath(Actor *actor, PathData *path, int *a, int *b, int c,
                         int d);

/* ENTER_POSE with the already-there test first. */
/* ==== Level 2 classes ==== */

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
/* Case 174 -- the cage that carries Spyro. */
typedef struct CageState {
  int idx;    /* 0x00 index of its flyer in g_pActorListBase */
  int shake;  /* 0x04 shake timer, then ride timer */
  int rotX;   /* 0x08 rest pose */
  int rotY;   /* 0x0C */
  int restZ;  /* 0x10 */
  int handle; /* 0x14 for SelectActorLodBand */
} CageState;
/* ENTER_POSE with an empty loop at the head of the change: cse starts a new
 * block there, so the `unk3D = n` store takes its own copy of the constant
 * (merged again after loop.c). Keeps the constant's lifetime short enough
 * that loop.c does not hoist it (see case 115). */
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

/* ==== Level 9 classes ==== */

/* Cases 205/209 -- the snowball thrower and its snowball. */
typedef struct L9Snowball {
  int vx;             /* 0x00 */
  int vy;             /* 0x04 */
  int vz;             /* 0x08 */
  int timer;          /* 0x0C */
  unsigned char kind; /* 0x10 */
} L9Snowball;

/* Case 213 -- circling bird. */
typedef struct L9Bird {
  int home[3]; /* 0x00 perch */
  int timer;   /* 0x0C */
  int radius;  /* 0x10 */
  int angle;   /* 0x14 */
  int height;  /* 0x18 */
  int call;    /* 0x1C */
} L9Bird;
extern void SteerActorTowardPoint(Actor *actor, int *target, int a, int b, int c,
                          int d);

/* Case 198 -- charging yak. */
typedef struct L9Yak {
  int heading; /* 0x00 knock-back heading */
  int speed;   /* 0x04 */
  int rest;    /* 0x08 rest countdown after a charge */
  int unk0C;
  int timer;   /* 0x10 grazing timer */
} L9Yak;

/* Case 231 -- ram. */
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

/* Cases 211/212 -- snowball thrower. */
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
extern void GetActorAttachPoint(Actor *actor, int a, int *pos);

/* ==== level_10 per-class state blocks ==== */
/* Type 370: Doctor Shemp's guard. */
typedef struct {
  char pad00[0x38];
  int posX; /* 0x38 */
  int posY; /* 0x3C */
} GuardTarget;
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
/* Type 371: Doctor Shemp. */
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
extern int g_nSpyroState;
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
extern int WrapAddAngle8(int heading, int step);
extern int g_nActorAnimNewFrame;
extern int g_anSpyroWorldPos; /* Spyro, as a plain symbol (the loop's spy carrier) */
extern int D_8007866C, D_80078670, D_80078674, D_80078678, D_8007867C;
typedef struct {
  int x;
  int y;
  int z;
} CamLook;
#define CAM_LOOK (*(CamLook *)&g_anSpyroRequestVelocity)

/* from level_7 */
#define SPY_DZ(a) ((a)->posZ - (a)->floorOffset - SPY.posZ)
/* from level_7 */
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
/* from level_7 */
/* from level_7 */
typedef struct L26Spinner {
  Actor *lid; /* 0x00 */
  int angle;  /* 0x04 */
  int speed;  /* 0x08 */
  int timer;  /* 0x0C */
  int handle; /* 0x10 for SelectActorLodBand */
} L26Spinner;
/* from level_7 */
extern int D_80075F38[][7]; /* voice table */
/* from level_7 */
extern int D_80075F44[][7]; /* its word 3 */
/* from level_7 */
typedef struct L26Spark {
  int vel[3]; /* 0x00 */
  int timer;  /* 0x0C */
} L26Spark;
/* from level_7 */
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
/* from level_7 */
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
/* from level_7 */
typedef struct L26Critter {
  char pad00[0x38];
  int knock; /* 0x38 knock-back heading */
  int speed; /* 0x3C knock-back speed */
} L26Critter;
/* from level_7 */
typedef struct L7State184 {
  int link;    /* 0x00 handed to the spawned bolt */
  int flag;    /* 0x04 */
  int u08;     /* 0x08 */
  int heading; /* 0x0C */
  int speed;   /* 0x10 */
  int timer;   /* 0x14 */
} L7State184;
/* from level_7 */
typedef struct L7State183 {
  int link;    /* 0x00 */
  int u04;     /* 0x04 */
  int u08;     /* 0x08 */
  int heading; /* 0x0C */
  int u10;     /* 0x10 */
  int u14;     /* 0x14 */
} L7State183;
/* from level_7 */
typedef struct L7State186 {
  int home[3];   /* 0x00 */
  void *path;    /* 0x0C */
  int triggered; /* 0x10 */
  int mode;      /* 0x14 */
  int timer;     /* 0x18 */
  int heading;   /* 0x1C */
  int speed;     /* 0x20 */
  int blinks;    /* 0x24 */
  int u28;       /* 0x28 */
  int turn;      /* 0x2C */
  int range;     /* 0x30 */
  int spawned;   /* 0x34 */
} L7State186;
/* from level_7 */
typedef struct L7State197 {
  int vel[3];         /* 0x00 */
  short u0C;          /* 0x0C */
  short u0E;          /* 0x0E */
} L7State197;
/* from level_34 */
typedef struct RearState {
  int timer;   /* 0x00 idle-hop countdown */
  int t04;     /* 0x04 */
  int heading; /* 0x08 knockback heading */
  int speed;   /* 0x0C knockback speed */
  int spin;    /* 0x10 per-frame yaw while tumbling */
} RearState;
/* from level_34 */
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
/* from level_34 */
typedef struct {
  void *v;
} PtrV;
/* from level_34 */
#define AS_PTRV(sym) (((PtrV *)&(sym))->v)
/* from level_34 */
typedef struct L31Rock {
  int shake;  /* 0x00 shake timer, 0 = still */
  int rotX;   /* 0x04 rest rotation */
  int rotY;   /* 0x08 */
  int restZ;  /* 0x0C */
  int handle; /* 0x10 for SelectActorLodBand */
} L31Rock;
/* from level_34 */
typedef struct WispState {
  Actor *target; /* 0x00 actor to reach, or null to chase Spyro */
  int timer;     /* 0x04 lifetime */
} WispState;
/* from level_34 */
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
/* from level_34 */
typedef struct TetherState {
  int timer;   /* 0x00 */
  int speed;   /* 0x04 knockback speed */
  int heading; /* 0x08 knockback heading */
  int link;    /* 0x0C walker to follow (index into g_pActorListBase), or -1 */
  int owner;   /* 0x10 actor whose loss frees this one */
  int mode;    /* 0x14 */
} TetherState;
/* from level_34 */
typedef struct LeadState {
  PathData *path; /* 0x00 */
  int moving;     /* 0x04 */
} LeadState;
/* from level_34 */
typedef struct KeeperState {
  int timer;   /* 0x00 knockback timer */
  int heading; /* 0x04 knockback heading */
  int count;   /* 0x08 idle countdown */
  int link;    /* 0x0C linked guard (index into g_pActorListBase), or -1 */
  int mode;    /* 0x10 command passed to the guard */
  int baseX;   /* 0x14 post position */
  int baseY;   /* 0x18 */
} KeeperState;
/* from level_34 */
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
/* from level_34 */
typedef struct ChainState {
  int pad00[3];
  int link;  /* 0x0C next in chain */
  int pad10[2];
  int stop;  /* 0x18 path node where it waits */
} ChainState;
/* from level_34 */
extern int WalkActorAlongWaypoints(Actor *actor, PathData *path, int a, int b, int c,
                         int d, int e, int f);
/* from level_34 */
extern int SteerActorAlongWaypoints(Actor *actor, PathData *path, int a, int b, int c,
                         int d, int e, int *f);
/* from level_34 */
/* from level_34 */
#define FACE4(a, h, s, t)                                                      \
  ((int (*)(Actor *, int, int, int))TurnActorYawTowardAngle)(a, h, s, t)
/* from level_34 */
#define RESET_IDLE(a)                                                          \
  (a)->mode = 0;                                                              \
  ANIM_RESET(a, 0);                                                            \
  continue
/* from level_34 */
typedef struct L25Shot {
  int link;            /* 0x00 index of the actor it wakes on arrival */
  int vel[3];          /* 0x04 */
  int timer;           /* 0x10 */
  unsigned char *path; /* 0x14 */
  int init;            /* 0x18 1 = aim the barrel along the path */
} L25Shot;
/* from level_34 */
#define PATH_PT(p, i) ((int *)((p) + (((i) << 4) + 0x8)))
/* from level_34 */
/* from level_34 */
/* from level_34 */
typedef struct CatapultState {
  int angle;     /* 0x00 0..0x120 */
  int vel;       /* 0x04 */
  int link;      /* 0x08 actor to fling (index into g_pActorListBase), or -1 */
  Actor *flung;  /* 0x0C actor in flight */
  int timer;     /* 0x10 */
} CatapultState;
/* from level_34 */
typedef struct FlightState {
  int pos[3];          /* 0x00 landing point */
  unsigned char pad0C[2];
  unsigned char unk0E; /* 0x0E */
  unsigned char unk0F; /* 0x0F */
  unsigned char mode;  /* 0x10 */
} FlightState;
/* from level_34 */
typedef struct L31Critter {
  char pad00[0x20];
  int speed; /* 0x20 knock-back speed */
  int knock; /* 0x24 knock-back heading */
  char pad28[0x4];
  int wait;  /* 0x2C pause timer */
} L31Critter;
/* from level_34 */
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
/* from level_34 */
/* from level_34 */
/* from level_34 */
#define ANIM_WORD(a) (*(int *)&(a)->anim)
/* from level_34 */
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
/* from level_34 */
typedef struct LeaderState {
  int mode;               /* 0x00 */
  int unk04[2];
  unsigned char *script;  /* 0x0C */
  int unk10[4];
  int cue;                /* 0x20 */
} LeaderState;
/* from level_34 */
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
/* from level_34 */
typedef struct CrookState {
  Actor *owner; /* 0x00 */
  int live;     /* 0x04 */
  int heading;  /* 0x08 */
  int life;     /* 0x0C */
  int speed;    /* 0x10 */
} CrookState;
/* from level_34 */
#define SPY_FACING(a)                                                          \
  AbsAngleDelta8(spy->bodyRotZ, ArcTan2((a)->posX - spy->posX,            \
                                             (a)->posY - spy->posY, 0))
/* from level_34 */
typedef struct FallState {
  int unk00;
  int live;    /* 0x04 */
  int heading; /* 0x08 */
  int timer;   /* 0x0C */
  int vz;      /* 0x10 */
} FallState;
/* from level_34 */
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
/* from level_34 */
typedef struct BoltState {
  Actor *target; /* 0x00 */
  int life;      /* 0x04 */
} BoltState;
/* from level_34 */
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
/* from level_34 */
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
/* from level_34 */
#define ANIM_RETIME(a)                                                         \
  if (ABS2((a)->nextFrame - (a)->frame) < 3) {                                     \
    (a)->frameRate = TYPE_DESC(a, (a)->anim);                                     \
  }
/* from level_34 */
#define ANIM_JUMP(a, n, f)                                                     \
  g_nActorAnimDone = 0;                                                              \
  (a)->frameProgress = sixteen;                                                        \
  (a)->frameRate = sixteen;                                                        \
  (a)->anim = (a)->nextAnim;                                                     \
  (a)->nextAnim = (n);                                                            \
  (a)->frame = (a)->nextFrame;                                                     \
  (a)->nextFrame = (f);                                                            \
  StubActorHook(a)
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
extern int FindNearestPathPointToSpyro(FlyPath *path, int *nearest);
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
/* from level_31 */
typedef struct L26Lobber {
  int timer;   /* 0x00 */
  int heading; /* 0x04 knock-back heading */
  int speed;   /* 0x08 knock-back speed */
  int reload;  /* 0x0C */
} L26Lobber;
/* from level_31 */
typedef struct L26Sink {
  int partner; /* 0x00 index into g_pActorListBase */
  int depth;   /* 0x04 */
} L26Sink;
/* from level_31 */
typedef struct L26Bob {
  int partner;        /* 0x00 index into g_pActorListBase */
  unsigned int phase; /* 0x04 */
  int unk08;
  int unk0C;
  int baseZ;          /* 0x10 */
  int big;            /* 0x14 */
} L26Bob;
/* from level_31 */
typedef struct L26Rider {
  int partner; /* 0x00 index into g_pActorListBase */
  int speed;   /* 0x04 */
  int lift;    /* 0x08 */
  int heading; /* 0x0C */
} L26Rider;
/* from level_31 */
typedef struct L26Escort {
  int leader;     /* 0x00 index into g_pActorListBase */
  int partner;    /* 0x04 index into g_pActorListBase, -1 = none */
  PathData *path; /* 0x08 */
  int p0C[3];     /* 0x0C */
  int timer;      /* 0x18 */
  int slot;       /* 0x1C place in the ring */
  int speed;      /* 0x20 */
} L26Escort;
/* from level_31 */
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
/* from level_31 */
typedef struct L26Rot {
  unsigned char r[3];
} L26Rot;
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
/* from level_25 */
extern int g_nSpyroDrawSuppressed;
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

/* ==== Level 20 classes ==== */
typedef struct L18Ram {
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
} L18Ram;

/* Case 137 -- guard. */
typedef struct L18Guard {
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
} L18Guard;
extern int g_anLevelTriggerStateSlots[]; /* per-channel busy flags */
extern int g_nLevelReadyFlag; /* == D_80078BBC, scalar view */

/* Type 28 state. */
typedef struct L20State28 {
  void *path;  /* 0x00 */
  int partner; /* 0x04 index into g_pActorListBase */
  int u08;     /* 0x08 knock-back speed */
  int u0C;     /* 0x0C */
  int u10;     /* 0x10 knock-back heading */
  int timer;   /* 0x14 */
  int mode;    /* 0x18 0 guard, 1 patrol, 2 lookout */
  int hit;     /* 0x1C */
  int range;   /* 0x20 sight range (< 0x20: in units of 0x400) */
} L20State28;

#define FLY_X(p, i) (*(int *)((char *)(p) + ((i) << 4) + 0x8))
#define FLY_Y(p, i) (*(int *)((char *)(p) + ((i) << 4) + 0xC))

/* Type 403 state. */
typedef struct L20State403 {
  int mode;      /* 0x00 0 roams its path */
  FlyPath *path; /* 0x04 */
  int u08;       /* 0x08 knock-back heading */
  int u0C;       /* 0x0C knock-back speed */
  int u10;       /* 0x10 */
  int u14;       /* 0x14 */
  int u18;       /* 0x18 roam timer */
  int u1C;       /* 0x1C hop speed */
  Actor *tether; /* 0x20 */
  int u24;       /* 0x24 cool-down */
  int u28;       /* 0x28 holds Spyro */
  int u2C[3];    /* 0x2C */
  int u38;       /* 0x38 stun timer */
} L20State403;

extern int g_anCameraPos[]; /* camera position */

/* Type 476 state. */
typedef struct L20State476 {
  int u00[2];   /* 0x00 */
  int home[3];  /* 0x08 drop-off point */
  int u14;      /* 0x14 grab chance */
  int u18;      /* 0x18 timer */
  int u1C;      /* 0x1C */
  int u20;      /* 0x20 knock-back heading */
  int u24;      /* 0x24 knock-back speed */
  int u28;      /* 0x28 */
  int *held;    /* 0x2C shared "Spyro is held" latch */
  int u30;      /* 0x30 has Spyro */
  int u34;      /* 0x34 grab cool-down */
  int u38;      /* 0x38 */
} L20State476;

extern int g_nSpyroDrawSuppressed;   /* Spyro is being carried */
extern int D_8007577C;
extern int g_nSpyroBodyYaw;

/* ==== The function ==== */
void Level20_UpdateActors(void) {
  Actor **actorList;
  Actor *actor;
  SpyroObj *spy;
  char *base;
  int sixteen;
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
  int svJ[3];
  int A256[3];
  int A272[3];
  int A288[3];
  int A304[3];
  int A320[3];
  int A336[3];
  int A352[3];
  int A368[2];
  int A376[3];
  int A392[3];
  int A408[3];

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
    sixteen = 0x10;
    base = (char *)D_80078BBC;
    spy = (SpyroObj *)(base - 0x164);

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
        actor->mode = 1;
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
            D_80078C58 = *(int *)((char *)actor + 0x14) + 0x4000;
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
                g_nScriptedRespawnFlag = 1;
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

    /* Balloonist's gnorc (WanderState): wanders around the pad until the
     * balloonist is ready, then walks into the flight cutscene. */
    case 16: {
      WanderState *st;

      do {
      } while (0);
      st = actor->state;
      if (SPY.u164 < 3 && ApproxDist2D(&actor->posX, (int *)(base - 0x164)) < 0x960) {
        int height = actor->posZ - actor->floorOffset;
        int drop = height - SPY.posZ;

        if (drop > 0 ? drop < 0x708 : (SPY.posZ - height) < 0x708) {
          if (g_pLevelOverlayInitRecord == 0) {
            if (g_nLevelReadyFlag >= 0) {
              Actor *puff = g_pfnLevelSpawnActor(0x78, actor);

              g_pLevelOverlayInitRecord = puff;
              PlayActorMeshVoice(puff, 0, 0);
              g_nLevelReadyFlag = 1;
              DespawnActorRecord(actor);
              continue;
            }
          } else if (g_pLevelOverlayInitRecord->subMode != 0x63) {
            BalloonState *bal = g_pLevelOverlayInitRecord->state;

            if (bal->gnorc == 0) {
              if (WrapAngleDelta8(spy->bodyRotZ,
                                ArcTan2(actor->posX - spy->posX,
                                              actor->posY - spy->posY, 0)) <
                  0) {
                st->turn = 1;
              } else {
                st->turn = -1;
              }
              g_pLevelOverlayInitRecord->subMode = 0;
              bal->gnorc = actor;
              bal->timer = RandomIntInRange(0x96, 0xD2);
              actor->mode = 1;
            }
          }
        }
      }

      if (!(actor->unk50 & 0x80)) {
        switch (actor->mode) {
        case 0:
          if (TickCountdownByFrameDelta(&st->unk14, 2) == 0 || actor->drawn != 0) {
            int moved;
            int blocked;
            int step;
            int z;
            unsigned int spd;
            int *at;

            if (TickCountdownByFrameDelta(&st->reTurn, 1) != 0) {
              int turn = RandomSignedInRange(0x1E, 0x5A);

              if (GetRandomU32() & 1) {
                turn = -turn;
              }
              st->heading = WrapAddAngle8(st->heading, turn);
              st->reTurn = RandomIntInRange(0x3C, 0x8C);
            }

            if (TickCountdownByFrameDelta(&st->reStep, 1) != 0) {
              st->speed = RandomSignedInRange(0xA, 0x19);
              st->reStep = RandomIntInRange(0x50, 0x8C);
            }

            spd = st->speed;
            if (spd >= 0x80) {
              z = actor->posZ - 0x100;
            } else {
              z = actor->posZ;
            }
            actor->posZ = z + spd;
            do {
            } while (0);
            step = actor->posZ - ProbeActorGroundHeight(actor);

            if (step < 0x12C) {
              st->speed = RandomIntInRange(0xA, 0x19);
              st->reStep = RandomIntInRange(0x50, 0x8C);
            }

            if ((unsigned int)(step - 0x401) < 0x3FF) {
              st->speed = -RandomIntInRange(0xA, 0x19);
              st->reStep = RandomIntInRange(0x50, 0x8C);
            }

            moved = TurnActorYawTowardAngle(actor, st->heading, 4, 0xA, 1);
            blocked = MoveActorAlongHeading(actor, 0x1E, 0, 0x32, 1);

            if (moved != 0) {
              if (blocked != 0) {
                st->heading =
                    WrapAddAngle8(st->heading, RandomIntInRange(0x62, 0x9E));
                st->reTurn = RandomIntInRange(0x1E, 0x50);
              }
            }

            if (ApproxDist2D(&actor->posX, st->home) >= 0x7D1) {
              st->heading = ArcTan2(st->home[0] - actor->posX,
                                          st->home[1] - actor->posY, 0);
              st->reTurn = 0x28;
            }
          } else {
            DespawnActorRecord(actor);
            continue;
          }
          break;

        case 1:
          SteerActorAroundPoint(actor, &spy->posX, 0x320,
                        WrapAddAngle8(ArcTan2(actor->posX - spy->posX,
                                                    actor->posY - spy->posY, 0),
                                      st->turn * 8),
                        8, 0x82, 0xE, 0x80, 0xFF, 0xFF, 0, 0, 0);

          if (g_pLevelOverlayInitRecord == 0 || g_pLevelOverlayInitRecord->subMode == 0x63) {
            actor->mode = 0;
            break;
          }

          if (TickCountdownByFrameDelta(&st->reStep, 1) != 0) {
            st->speed = RandomSignedInRange(5, 0x14);
            st->reStep = RandomIntInRange(0x28, 0x50);
          }

          {
            int step;
            int z;
            unsigned int spd;

            spd = st->speed;
            if (spd >= 0x80) {
              z = actor->posZ - 0x100;
            } else {
              z = actor->posZ;
            }
            actor->posZ = z + spd;
            do {
            } while (0);
            step = actor->posZ - ProbeActorGroundHeight(actor);

            if (step < 0xC8) {
              st->speed = RandomIntInRange(5, 0x14);
              st->reStep = RandomIntInRange(0x28, 0x50);
            }

            if ((unsigned int)(step - 0x3E9) < 0x417) {
              st->speed = -RandomIntInRange(5, 0x14);
              st->reStep = RandomIntInRange(0x28, 0x50);
            }
          }
          break;
        }
      }

      break;
    }

    /* Type 28: gunner. Wakes its partner, turns to it, then fires. */
    case 28: {
      L20State28 *st = actor->state;

      func_8003AA84(actor);
      do {
      } while (0);
      if ((actor->flags & 0xA0000) && actor->mode != 5) {
        if (actor->flags & 0x80000) {
          st->u08 = 0;
          st->u0C = 0x104;
        } else {
          st->u08 = 0x190;
          st->u0C = 0xAA;
          st->u10 = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                                actor->posY - spy->posY, 0),
                                  spy->bodyRotZ, 0x20, 0x40);
        }
        actor->flags = 0;
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        ENTER_POSE_B(actor, 5);
      }
      actor->flags = 0;

      switch (actor->mode) {
      case 0:
        if (st->mode == 0) {
          ENTER_POSE(actor, 4);
        } else if (st->mode == 1) {
          Actor *p = &g_pActorListBase[st->partner];

          if (p->mode != 0) {
            st->mode = 0;
            ENTER_POSE(actor, 4);
          }
          ANIM_GO(actor, 7);
          PivotActorAlongPath(actor, st->path, 0x100, 0xA0, 0, 8, 0x28, 0xFF, 5);
        } else if (st->mode == 2) {
          ANIM_GO(actor, 0);
          if (ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) < 0x640 &&
              g_pActorListBase[st->partner].mode == 0) {
            ENTER_POSE(actor, 9);
          }
          if (actor->subMode != 0) {
            ENTER_POSE(actor, 1);
          }
        }
        break;

      case 1:
        if (actor->nextFrame >= 4 && g_pActorListBase[st->partner].mode == 0) {
          g_pActorListBase[st->partner].mode = 1;
        }
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, 2);
        }
        break;

      case 2:
        TurnActorYawTowardAngle(
            actor,
            ArcTan2(g_pActorListBase[st->partner].posX - actor->posX,
                          g_pActorListBase[st->partner].posY - actor->posY, 0),
            4, 0, 0);
        if (g_nActorAnimDone != 0) {
          st->timer = 0x78;
          ENTER_POSE(actor, 3);
        }
        break;

      case 3:
        TurnActorYawTowardAngle(
            actor,
            ArcTan2(g_pActorListBase[st->partner].posX - actor->posX,
                          g_pActorListBase[st->partner].posY - actor->posY, 0),
            6, 0, 0);
        if ((TickCountdownByFrameDelta(&st->timer, 4) != 0 ||
             ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) < 0x514) &&
            g_nActorAnimDone != 0) {
          if (actor->anim != 4) {
            actor->frameProgress = 8;
            actor->frameRate = 8;
            actor->anim = actor->nextAnim;
            actor->nextAnim = 4;
            actor->frame = actor->nextFrame;
            actor->nextFrame = 0;
            StubActorHook(actor);
          }
          actor->mode = 4;
          continue;
        }
        break;

      case 4: {
        int d = ApproxDist2D(&actor->posX, &spy->posX);
        int r = st->range;

        if (r < 0x20) {
          r <<= 10;
        }
        if (d < r && g_nSpyroState != 0xB && g_nSpyroState != 0x14) {
          st->timer = 0x78;
          ENTER_POSE(actor, 7);
        }
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0 && d < 0x514) {
          st->timer = 0x78;
          ENTER_POSE_B(actor, 8);
        }
        break;
      }

      case 5: {
        int r = DriveActorTrackedMove(actor, &st->u08, st->u10, &st->u0C, 0xC, sixteen);

        if ((g_nGroundSurfaceType & 0x3F) == 0) {
          int h = ProbeActorGroundHeight(actor);
          int z = actor->posZ - actor->floorOffset;

          if (z - h > 0 ? z - h < 0x64 : h - z < 0x64) {
            PlaySoundEffect(g_pLevelSampleBankHeader[0x1D], actor, 8, &actor->sfxSlot);
            g_pfnLevelSpawnActor(0x190, actor);
            st->timer = 0xA;
            actor->mode = 0xA;
            continue;
          }
        }
        if (r == 3 && st->hit == 0) {
          st->hit = 1;
          PlayActorMeshVoice(actor, 0, 0);
        }
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x20);
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }

      case 7:
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0 ||
            ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) < 0x3E8) {
          st->range = 0;
          st->timer = 0;
          ENTER_POSE_B(actor, 8);
        }
        if (TrackEntityYawTowardSpyro(actor, 6, 0x14, 1) != 0 &&
            MoveActorAlongHeading(actor, 0xA0, 0x12C, 0x12C, 0x15) != 0) {
          st->timer = 0;
          ENTER_POSE(actor, 4);
        }
        break;

      case 8:
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, 4);
        }
        break;

      case 9:
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, 4);
        }
        break;

      case 10:
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        actor->posZ -= 0x64;
        break;
      }
      RefreshActorDerivedState(actor, 1);
      continue;
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
            ArcTan2_8bit(actor->posX - spy->posX, actor->posY - spy->posY) +
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

          CopyVector(svA, &spy->posX);
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
          CopyVector(svD, &spy->posX);
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

    /* Bouncing chest fragments (SparkState). */
    case 67:
    case 68:
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
    case 69:
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

    /* The generic enemy: patrols, notices Spyro, charges, gets knocked back.
     * ->subMode is the outer state and st->mode the sub-state. */
    case 14:
    case 15:
    case 83:
    case 84:
    case 85:
    case 86:
    case 87: {
      EnemyState *st = actor->state;
      int toSpyro;

      toSpyro = ApproxDist2D(&actor->posX, &SPY.posX);

      if (st->timer < 0xFA) {
        st->timer += g_nFrameStep;
      }

      if (actor->type != 15) {
        if (st->attach != 0xFF) {
          RefreshActorDerivedState(actor, 4);
          if (actor->type == 14) {
            RotateVectorByMatrix(&actor->mtx[0], &g_anRotBillboardOffset5,
                          g_aRotBillboards[st->attach].pos);
          } else {
            RotateVectorByMatrix(&actor->mtx[0], &g_anRotBillboardOffset3,
                          g_aRotBillboards[st->attach].pos);
          }
          AddVector(g_aRotBillboards[st->attach].pos, g_aRotBillboards[st->attach].pos,
                        &actor->posX);

          if (g_aRotBillboards[st->attach].life < 5) {
            st->attach = 0xFF;
          }
        } else if (st->timer >= 0xF8) {
          int handle = SpawnActorRotBillboard(actor, &g_anRotBillboardOffset3);

          if (handle >= 0 && toSpyro < 0x4000) {
            st->attach = handle;
          }
          st->timer = (GetRandomU32() & 0x1F) + 0x21;
        }
      }

      if (actor->drawn != 0 || toSpyro <= 0x2000 || actor->subMode != 1 ||
          st->mode != 0) {
        switch (actor->subMode) {
        case 0:
          /* Idle: sample the floor normal and face along it. */
          CopyVector(svA, &actor->posX);
          svA[2] += 0x400;
          FindGroundHeightBelow(svA, 0x10000);
          {
            int *nrm = &g_anCollisionProbeVec;

            st->lean = -ArcTan2_8bit(
                IntegerSqrt((nrm[0] * nrm[0]) + (g_nCollisionProbeVecZ * g_nCollisionProbeVecZ)),
                g_nCollisionProbeVecY);
            st->roll = -ArcTan2_8bit(g_nCollisionProbeVecZ, nrm[0]);
          }

          if (st->lean != 0 || st->roll != 0) {
            actor->rotZ = 0;
          }
          actor->subMode = 1;
          goto enemy_pose;

        case 1: {

          if (st->mode == 0) {
          } else if (st->mode == 1) {
            SnapActorToGround(actor);
            EncodeCachedVecToActorDirCode(actor);
            if (actor->type >= 83) {
              {
                int *nrm = &g_anCollisionProbeVec;

                st->lean =
                    -ArcTan2_8bit(IntegerSqrt((nrm[0] * nrm[0]) +
                                                 (g_nCollisionProbeVecZ * g_nCollisionProbeVecZ)),
                                   g_nCollisionProbeVecY);
                st->roll = -ArcTan2_8bit(g_nCollisionProbeVecZ, nrm[0]);
              }
            }
          } else if (st->mode == 2) {
            UnpackWorldCollisionTri(st->node, acc[0]);
            AddVector(acc[0], acc[0], acc[1]);
            AddVector(acc[0], acc[0], acc[2]);
            actor->posX = acc[0][0] / 3;
            actor->posY = acc[0][1] / 3;
            actor->posZ = acc[0][2] / 3;
            RefreshActorDerivedState(actor, 2);
            SnapActorToGround(actor);
            EncodeCachedVecToActorDirCode(actor);
          } else if (st->mode == 3) {
            CopyVector(svF, &actor->posX);
            svF[2] += 0x400;
            RefreshActorDerivedState(actor, 2);
            if (FindGroundHeightBelow(svF, 0x1000) > 0) {
              st->node = g_nLastCollisionHitChunkId;
              st->mode = 2;
            }
          }

          if (toSpyro < 0x59A) {
            int top = actor->posZ + 0x164;
            register int py asm("$2") = SPY.posZ;
            int dz = py - top;

            if (dz < 0) {
              dz = -dz;
            }

            if (dz < 0x200 && g_pLevelOverlayInitRecord != 0 && g_nSpyroAirborneFrames == 0 &&
                CheckLineOfSightSegment(&g_pLevelOverlayInitRecord->posX, &actor->posX) != 0) {
              {
                BalloonState *bal = g_pLevelOverlayInitRecord->state;

                if (bal->gnorc == 0) {
                  if (GetSoundVoiceStatusByOwner(
                          g_pLevelOverlayInitRecord,
                          g_apActorMeshTable[g_pLevelOverlayInitRecord->type]->m_Sounds[0]) == 0) {
                    g_nSfxOverrideFlags = 1;
                    g_nSfxOverrideVolR = 0x2000;
                    g_nSfxOverrideVolL = 0x2000;
                    PlayActorMeshVoice(g_pLevelOverlayInitRecord, 0, 0);
                  }
                  g_pLevelOverlayInitRecord->subMode = 4;
                  bal->gnorc = actor;
                }
              }
              goto enemy_pose;
            }
          }
          goto enemy_pose;
        }

        case 2: {
          /* Knocked back: arc away, bounce, then settle. */
          int *at;
          int dist;

          ScaleVector3Sat(svG, (int *)st, g_nFrameStep);
          RShiftVector3(svG, 1);
          dist = VectorLength(svG, 1);

          if (dist >= 0xDD) {
            ScaleVector3ByRatio(svG, dist, 0xDC);
          }

          at = &actor->posX;
          AddVector(svG, at, svG);

          if (st->vel[2] >= -0xDB) {
            st->vel[2] -= g_nFrameStep * 5;
          }

          if (svG[2] < 0) {
            if (actor->type != 14 && actor->type != 15) {
              CollectPickupActor(actor);
            }
            DespawnActorRecord(actor);
            continue;
          }

          svG[2] += 0xF0;

          if (CollideSphereWithWorldAndActors(svG, 0xF0, 0xF0) != 0) {
            if (GetGroundTriggerRegionType() == 0) {
              if (actor->type != 14 && actor->type != 15) {
                CollectPickupActor(actor);
              }
              DespawnActorRecord(actor);
              continue;
            }

            CopyVector(svH, &g_anCollisionProbeVec);
            CopyVector(at, &g_anCollisionResolvedPos);
            actor->posZ -= 0xF0;

            if (st->bounce == 0) {
              int floor = FindGroundHeightBelow(svG, 0x400);
              int slope =
                  (signed char)ArcTan2_8bit(svH[2], VectorLength(svH, 0));

              if ((svG[2] - 0x190) < floor && slope < 0x18) {
                actor->subMode = 1;
                CopyVector(at, svG);
                actor->posZ = floor;
                if (actor->type != 15) {
                  st->lean = -ArcTan2_8bit(
                      IntegerSqrt((svH[0] * svH[0]) + (svH[2] * svH[2])),
                      svH[1]);
                  st->roll = -ArcTan2_8bit(svH[2], svH[0]);
                  if (st->lean != 0 || st->roll != 0) {
                    actor->rotZ = 0;
                  }
                }
                EncodeCachedVecToActorDirCode(actor);
              } else {
                st->bounce += 1;
              }

              if (st->bounce == 0) {
                goto enemy_settle;
              }
            }

            g_nSfxOverrideFlags = 1;
            g_nSfxOverrideVolL = 0x3CCC >> (3 - st->bounce);
            g_nSfxOverrideVolR = 0x3CCC >> (3 - st->bounce);
            PlaySoundEffect(g_pLevelSampleBankHeader[0x2F], actor, 8, &actor->sfxSlot);

            if (func_80017428((int *)st, svH, (int *)st) != 0) {
              st->bounce -= 1;
              st->vel[0] = (st->vel[0] >> 3) + (GetRandomU32() & 0x3F) - 0x20;
              st->vel[1] = (st->vel[1] >> 3) + (GetRandomU32() & 0x3F) - 0x20;
              st->vel[2] = (st->vel[2] >> 2) + (GetRandomU32() & 0xF);
            }
          } else {
            svG[2] -= 0xF0;
            CopyVector(at, svG);
            FindGroundHeightBelow(svG, 0x10000);
            EncodeCachedVecToActorDirCode(actor);
          }

        enemy_settle:
          if (toSpyro < 0x59A) {
            int top = actor->posZ + 0x164;
            register int py asm("$2") = SPY.posZ;
            int dz = py - top;

            if (dz < 0) {
              dz = -dz;
            }

            if (dz < 0x200 && g_pLevelOverlayInitRecord != 0 && g_nSpyroAirborneFrames == 0 &&
                st->bounce < 3) {
              {
                BalloonState *bal = g_pLevelOverlayInitRecord->state;

                if (bal->gnorc == 0) {
                  if (GetSoundVoiceStatusByOwner(
                          g_pLevelOverlayInitRecord,
                          g_apActorMeshTable[g_pLevelOverlayInitRecord->type]->m_Sounds[0]) == 0) {
                    g_nSfxOverrideFlags = 1;
                    g_nSfxOverrideVolR = 0x2000;
                    g_nSfxOverrideVolL = 0x2000;
                    PlayActorMeshVoice(g_pLevelOverlayInitRecord, 0, 0);
                  }
                  g_pLevelOverlayInitRecord->subMode = 4;
                  bal->gnorc = actor;
                }
              }
              goto enemy_pose;
            }
          }
          goto enemy_pose;
        }

          goto enemy_pose;

        case 3:
          /* Carried by the balloonist: pulled toward Spyro, then dropped. */
          if (st->timer >= 0x20) {
            CopyVector(&actor->posX, &SPY.posX);
          } else {
            SubtractVector(svI, &spy->posX, (int *)st);
            RShiftVector3(svI, 5);

            if (VectorLength(svI, 1) >= 0x1E1) {
              CopyVector(&actor->posX, &spy->posX);
              st->timer = 0x20;
            } else {
              ScaleVector3Sat(svI, svI, st->timer);
              AddVector(&actor->posX, (int *)st, svI);
              actor->posZ += (short)(g_anSineLut[st->timer * 4] / 12);
            }
          }

          actor->rotX += (unsigned char)(st->lean - 7);
          do {
          } while (0);
          actor->rotY += (unsigned char)(st->roll - 7);
          do {
          } while (0);
          actor->rotZ += (unsigned char)(st->spin - 7);
          goto enemy_pose;

        case 4:
          continue;

        default:
        enemy_pose:
          if (actor->subMode < 3) {
            if (actor->type == 14) {
              int v;

              v = ((unsigned short)g_anCosineLut[st->spin] << 16 >> 25) +
                  (unsigned char)st->lean;
              actor->rotX = v;
              v = ((unsigned short)g_anSineLut[st->spin] << 16 >> 25) +
                  (unsigned char)st->roll;
              actor->rotY = v;
              st->spin += g_nFrameStep * 2;
            } else if (actor->type == 15) {
              actor->rotZ += 8;
              actor->rotY -= 6;
            } else {
              int v;

              v = ((unsigned short)g_anCosineLut[st->spin] << 16 >> 23) +
                  (unsigned char)st->lean;
              actor->rotX = v;
              v = ((unsigned short)g_anSineLut[st->spin] << 16 >> 23) +
                  (unsigned char)st->roll;
              actor->rotY = v;
              st->spin += g_nFrameStep * 2;
            }
          }

          if ((st->timer >= 0x21 && toSpyro < 0x200 &&
               (unsigned int)((SPY.posZ - actor->posZ) + 0x17F) < 0x37F) ||
              (actor->subMode == 3 && st->timer >= 0x41)) {
            if (g_pLevelOverlayInitRecord != 0) {
              BalloonState *bal = g_pLevelOverlayInitRecord->state;

              if (bal->gnorc == actor) {
                bal->gnorc = 0;
              }
            }

            if (actor->type == 14) {
              PlaySoundEffect(g_pLevelSampleBankHeader[0], actor, 0x10, 0);
              RefreshActorDerivedState(actor, 4);
              g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (int *)8);
              g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (int *)0x10);
              if (g_nExtraLives < 0x63) {
                g_nExtraLives += 1;
              }
              CommitActorKillGems(0, actor);
              DespawnActorRecord(actor);
              continue;
            }

            if (actor->type == 15) {
              PlaySoundEffect(g_pLevelSampleBankHeader[0], actor, 0x10, 0);
              g_nSparxFoodCount += 1;
              DespawnActorRecord(actor);
            } else {

              CollectPickupActor(actor);
              DespawnActorRecord(actor);
              continue;
            }
          }
          break;
        }
      }

      break;
    }

    /* Lift plate (PlateState): rises once its trigger actor is gone, waits,
     * then drops back. */
    case 110: {
      PlateState *st = actor->state;
      Actor *trigger = &g_pActorListBase[st->trigger];
      Actor *rider = &g_pActorListBase[st->rider];

      switch (actor->mode) {
      case 0:
        actor->unk08 = 0;
        if (ApproxDist2D(&rider->posX, &SPY.posX) < 0xA01) {
          break;
        }
        actor->mode = 1;
        continue;

      case 1:
        if (trigger->mode >= 0x80) {
          actor->mode = 2;
          st->timer = 0x10;
          st->baseZ = actor->posZ;
        }
        break;

      case 2:
        if (st->timer != 0) {
          st->timer -= 1;
        } else {
          actor->unk08 = 0;
          if (ApproxDist2D(&actor->posX, &spy->posX) < 0x2000) {
            int height = actor->posZ - actor->floorOffset;
            int drop = height - SPY.posZ;

            if (drop > 0 ? drop < 0x800 : (SPY.posZ - height) < 0x800) {
              if (st->locked == 0 || rider->mode == 1) {
                int *at = &actor->posX;

                st->locked = 0;
                actor->posZ = st->baseZ + 0x80;
                actor->rotZ = ArcTan2(spy->posX - actor->posX,
                                             spy->posY - actor->posY, 0) -
                               0x80;
                actor->unk50 = 0x20;
                actor->mode = 3;
                actor->unk57 = 0x60;
                g_pfnLevelEmitParticles(0x18, 0x50, at, (int *)0xC);
                RefreshActorDerivedState(actor, 1);
                FindGroundHeightBelow(at, 0x1000);
                EncodeCachedVecToActorDirCode(actor);
                actor->unk1C |= 0x80000000;
              }
            }
          }
        }
        break;

      case 3:
        if (actor->unk57 >= 0x21) {
          actor->unk57 -= 4;
          actor->rotZ += 8;
        } else {
          actor->mode = 4;
          st->timer = 0;
          st->step = 0x1E;
        }
        break;

      case 4: {
        int height;
        int drop;
        int close;

        st->timer += 1;
        actor->posZ = st->baseZ + (LookupCosine(st->timer << 7) >> 6);

        if ((rider->mode == 0 ||
             (rider->mode == 2 && rider->subMode >= 0x10)) &&
            ApproxDist2D(&rider->posX, &spy->posX) < 0x400 &&
            (height = rider->posZ - rider->floorOffset, drop = height - SPY.posZ,
             drop > 0 ? drop < 0x200 : (SPY.posZ - height) < 0x200) &&
            (g_nSpyroState == 0 || g_nSpyroState == 0xD) && g_nSpyroStateTimer > 0) {
          actor->mode = 6;
          st->locked = 1;
          BeginSaveMenuFromWorld(actor);
          break;
        }

        if (ApproxDist2D(&actor->posX, &spy->posX) >= 0x2401 ||
            (height = actor->posZ - actor->floorOffset, drop = height - SPY.posZ,
             close = drop > 0 ? drop < 0xC01 : (SPY.posZ - height) < 0xC01,
             close == 0)) {
          actor->mode = 6;
          break;
        }

        st->step -= 1;
        if (st->step == 0) {
          int reach = (GetRandomU32() & 0x3F) + 0x140;
          int bearing = ArcTan2(spy->posX - rider->posX,
                                      spy->posY - rider->posY, 0);
          int aim = (bearing + (GetRandomU32() & 0xF)) - 8;

          do {
          } while (0);
          CopyVector(st->from, &rider->posX);
          aim &= 0xFF;
          aim *= 0x10;
          actor->mode = 5;
          st->from[0] -= (LookupCosine(aim) * reach) >> 12;
          st->from[1] -= (LookupSine(aim) * reach) >> 12;
          st->yaw = ArcTan2(spy->posX - st->from[0],
                                  spy->posY - st->from[1], 0);
          st->step = 0;
        }
        break;
      }

      case 5: {
        int height;
        int drop;

        st->timer += 1;
        actor->posZ = st->baseZ + (LookupCosine(st->timer << 7) >> 6);

        if ((rider->mode == 0 ||
             (rider->mode == 2 && rider->subMode >= 0x10)) &&
            ApproxDist2D(&rider->posX, &spy->posX) < 0x400 &&
            (height = rider->posZ - rider->floorOffset, drop = height - SPY.posZ,
             drop > 0 ? drop < 0x200 : (SPY.posZ - height) < 0x200) &&
            (g_nSpyroState == 0 || g_nSpyroState == 0xD) && g_nSpyroStateTimer > 0) {
          actor->mode = 6;
          st->locked = 1;
          BeginSaveMenuFromWorld(actor);
          break;
        }

        if (ApproxDist2D(&actor->posX, &spy->posX) >= 0x2801 ||
            (height = actor->posZ - actor->floorOffset, drop = height - SPY.posZ,
             drop > 0 ? drop >= 0xC01 : (SPY.posZ - height) >= 0xC01)) {
          actor->mode = 6;
          break;
        }
        {
          int *here = &actor->posX;
          int turn;
          int w0;
          int wSpan;
          int wDen;

          st->step += 1;
          SubtractVector(svF, st->from, here);
          turn = SignedAngleDelta8(st->yaw, actor->rotZ);
          w0 = LookupCosine((st->step - 1) << 7);
          wSpan = w0 - LookupCosine(st->step << 7);
          wDen = LookupCosine((st->step - 1) << 7) + 0x1000;
          actor->posX += (svF[0] * wSpan) / wDen;
          actor->posY += (svF[1] * wSpan) / wDen;
          actor->rotZ += (turn * wSpan) / wDen;
          FindGroundHeightBelow(here, 0x1000);
          EncodeCachedVecToActorDirCode(actor);

          if (st->step == 0x10) {
            actor->mode = 4;
            st->step = (GetRandomU32() & 0x1F) + 0x1E;
          }
        }
        break;
      }

      case 6:
        if (actor->unk57 < 0x60) {
          actor->unk57 += 4;
          actor->rotZ += 8;
        } else {
          actor->unk50 = 0;
          actor->drawn = 0;
          actor->mode = 2;
          st->timer = 0x10;
          g_pfnLevelEmitParticles(0x18, 0x50, &actor->posX, (int *)0xC);
          actor->unk1C &= 0x7FFFFFFF;
        }
        break;
      }

      break;
    }

    /* The balloonist (BalloonState): smokes, waits for his gnorc, then flies
     * Spyro out once the world is finished. */
    case 120: {
      BalloonState *st = actor->state;

      if (D_80078BBC[0] >= 2) {
        svF[0] = 0;
        svF[1] = 0x64;
        svF[2] = 0;
        RotateVectorByMatrix(&actor->mtx[0], svF, svF);
        AddVector(svF, svF, &actor->posX);
        g_pfnLevelEmitParticles(2, 0x42, svF, 0);
        svF[0] = 0;
        svF[1] = -0x64;
        svF[2] = 0;
        ApplyActiveGteRotation(svF, svF);
        AddVector(svF, svF, &actor->posX);
        g_pfnLevelEmitParticles(2, 0x42, svF, 0);
      }

      if (D_80078BBC[0] >= 3) {
        FxNode *fx = st->smoke;

        if (fx == 0) {
          fx = AllocWorldSparkle();
          st->smoke = fx;
          if (fx != 0) {
            fx->anchor = &actor->posX;
            st->smoke->size = 0x40;
            st->smoke->unk14 = 0;
            st->smoke->unk18 = 0;
            st->smoke->unk1C = 0;
            st->smoke->r = 0xC0;
            st->smoke->g = 0xC0;
            st->smoke->b = 0x60;
            st->smoke->kind = 9;
            st->smoke->desc = &D_8006E390;
          }
        } else {
          fx->size += 0x20;
          if (st->smoke->size >= 0x401) {
            st->smoke->size = 0x400;
          }
        }
      } else if (st->smoke != 0) {
        ClearWord(st->smoke);
        st->smoke = 0;
      }

      if (D_80078BBC[0] <= 0) {
        if (st->gnorc != 0 && st->gnorc->type == 16) {
          st->gnorc->mode = 0;
        }
        g_pLevelOverlayInitRecord = 0;
        DespawnActorRecord(actor);
        continue;
      }

      if (st->gnorc != 0 &&
          ApproxDist2D(&actor->posX, (int *)((char *)D_80078BBC - 0x164)) >=
              0x2001) {
        st->timer = 0;
      }

      if (actor->subMode != 0x63) {
        if (st->gnorc != 0) {
          switch (actor->subMode) {
          case 0: {
            /* Reeling the gnorc in. */
            EnemyState *ge = st->gnorc->state;
            int high = actor->nextAnim & 1;
            int dist;
            int reach;
            int limit;

            CopyVector(svA, &st->gnorc->posX);

            if (high == 0) {
              svA[0] -=
                  (unsigned short)g_anCosineLut[st->gnorc->rotZ] << 16 >> 20;
              svA[1] -=
                  (unsigned short)g_anSineLut[st->gnorc->rotZ] << 16 >> 20;
            } else {
              svA[0] -=
                  (unsigned short)g_anCosineLut[st->gnorc->rotZ] << 16 >> 22;
              svA[1] -=
                  (unsigned short)g_anSineLut[st->gnorc->rotZ] << 16 >> 22;
            }

            if (high != 0) {
              if (actor->nextFrame >= 7) {
                actor->subMode += 1;
              }

              {
                int tilt = actor->rotY;
                int back = -tilt;

                if ((unsigned int)((tilt - 3) & 0xFF) < 0xFB) {
                  if (back >= 3) {
                    back = 2;
                  }
                  if (back < -2) {
                    back = -2;
                  }
                  actor->rotY = tilt + back;
                }
              }
            }

            SubtractVector(svA, svA, &actor->posX);
            dist = VectorLength(svA, 1);

            if (high == 0 &&
                (TickCountdownByFrameDelta((void *)st, 4) != 0 ||
                 (st->timer < 0x50 &&
                  AbsAngleDelta8(
                      (actor->rotZ + (ge->vel[0] * 0x6E)) & 0xFF,
                      (((unsigned short)g_nCameraEulerYaw << 16 >> 20) + 0x80) &
                          0xFF) < 8))) {
              if (actor->nextAnim != actor->nextAnim + 1) {
                g_nActorAnimDone = 0;
                ANIM_ADVANCE(actor, actor->nextAnim + 1);
              }
            }

            reach = high != 0 ? 0x96 : 0x82;
            limit = reach + 5;
            if (limit < dist) {
              ScaleVector3ByRatio(svA, dist, limit);
            } else {
              ScaleVector3ByRatio(svA, dist, reach - 5);
            }
            AddVector(&actor->posX, &actor->posX, svA);
            TurnActorYawTowardAngle(actor, ArcTan2(svA[0], svA[1], 0), 0xA, 0, 0);
            actor->rotY = ArcTan2(VectorLength(svA, 0), svA[2], 0);
            actor->rotY = StepAngle8Toward(actor->rotY & 0xFF, 0, 0x30);
            break;
          }

          case 1:
            D_80078BBC[0] += 1;
            PlaySoundEffect(g_pLevelSampleBankHeader[0], actor, 0x10, 0);
            actor->subMode += 1;
            DespawnActorRecord(st->gnorc);
            break;

          case 2:
            if (g_nActorAnimDone != 0) {
              if (actor->nextAnim != actor->nextAnim - 1) {
                g_nActorAnimDone = 0;
                ANIM_ADVANCE(actor, actor->nextAnim - 1);
              }
              st->gnorc = 0;
              actor->subMode = 0;
            }
            break;

          case 4: {
            /* Hauling an enemy up to the basket. */
            EnemyState *ge = st->gnorc->state;
            int dist;

            SubtractVector(svJ, &st->gnorc->posX, &actor->posX);
            dist = VectorLength(svJ, 1);

            if (dist < 0x200) {
              ge->timer = 0;
              ge->lean = GetRandomU32() & 0xE;
              ge->roll = GetRandomU32() & 0xE;
              ge->spin = GetRandomU32() & 0xE;
              CopyVector((int *)ge, &st->gnorc->posX);
              st->gnorc->subMode = 3;

              if (st->gnorc->type == 14) {
                g_pfnLevelEmitParticles(1, 0xC, actor, D_8006E494);
              } else if (st->gnorc->type == 15) {
                g_pfnLevelEmitParticles(1, 0xC, actor, D_8006E490);
              } else {
                g_pfnLevelEmitParticles(1, 0xC, actor, g_adwActorBurstColors[st->gnorc->type]);
              }
              st->gnorc = 0;
              do {
              } while (0);

              if (GetSoundVoiceStatusByOwner(actor, g_pLevelSampleBankHeader[0x2F]) == 0) {
                g_nSfxOverrideFlags = 1;
                g_nSfxOverrideVolR = 0x3600;
                g_nSfxOverrideVolL = 0x3600;
                PlaySoundEffect(g_pLevelSampleBankHeader[0x2F], actor, 8, &actor->sfxSlot);
              }
            } else {
              int lim = 0x136;

              if (lim < dist) {
                ScaleVector3ByRatio(svJ, dist, lim);
              } else {
                ScaleVector3ByRatio(svJ, dist, 0x122);
              }
              AddVector(&actor->posX, &actor->posX, svJ);
              TurnActorYawTowardAngle(actor, ArcTan2(svJ[0], svJ[1], 0), 0xA, 0, 0);
              actor->rotY = ArcTan2(VectorLength(svJ, 0), svJ[2], 0);
              actor->rotY = StepAngle8Toward(actor->rotY & 0xFF, 0, 0x30);
            }
            break;
          }
          }
        } else {
          /* Nobody in the basket: drift on the wind. */
          actor->unk50 = 0x10;

          if (st->timer <= 0) {
            svF[0] = -0x258;
            svF[1] = (GetRandomU32() & 0x310) - 0x188;
            svF[2] = g_nCameraCurrentMode == 3 ? (GetRandomU32() & 0x7F) + 0x64
                                     : (GetRandomU32() & 0x1FF) - 0xC8;
            do {
            } while (0);
            st->drift[0] = svF[0];
            st->drift[1] = svF[1];
            st->drift[2] = svF[2];
            st->timer = GetRandomU32() & 0x7B;
          } else {
            int *at;

            st->timer -= g_nFrameStep;
            do {
            } while (0);

            if (g_nCameraCurrentMode == 3) {
              svF[0] = st->drift[0] + (g_nSpyroMotionSpeed >> 2);
              svF[1] = st->drift[1];
              svF[2] = st->drift[2];
              if (svF[2] < 0x64) {
                svF[2] = 0x64;
              }
            } else {
              if (g_nCameraCurrentMode == 0x80000009) {
                svF[0] = st->drift[0] - 0x200;
              } else {
                svF[0] = st->drift[0];
              }
              svF[1] = st->drift[1];
              svF[2] = st->drift[2];
            }

            RotateVectorByMatrix(&SPY.u034, svF, svF);
            AddVector(svF, svF, &SPY.posX);
            at = &actor->posX;
            SubtractVector(svF, svF, at);
            RShiftVector3(svF, 2);
            AddVector(at, at, svF);

            if (CollideSphereWithWorldAndActors(at, 0x100, 0x100) != 0) {
              CopyVector(at, &g_anCollisionResolvedPos);
            }

            if (svF[2] >= 0x21) {
              svF[2] = 0x20;
            }
            if (svF[2] < -0x20) {
              svF[2] = -0x20;
            }

            actor->rotX = 0;
            actor->rotY = svF[2];
            TurnActorYawTowardAngle(actor, g_bSpyroPersistentEulerZ, 4, 0, 0);
          }

          switch (SPY.u164) {
          case 1:
            if (actor->nextAnim != 0 && actor->anim != 0) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, 0);
            }
            break;

          case 2:
            if (actor->nextAnim != 2 && actor->anim != 2) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, 2);
            }
            break;

          case 3:
            if (actor->nextAnim != 4 && actor->anim != 4) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, 4);
            }
            break;
          }
        }

        RefreshActorDerivedState(actor, 4);
      }

      break;
    }

    /* Type 137: guard. Patrols, waits or lurks until Spyro comes within
     * range, then turns on him and charges; after a charge the camera is
     * pulled in on it, and a hit knocks it back. */
    case 137: {
      L18Guard *st = actor->state;

      if ((actor->flags & 0x90000) && actor->mode != 2) {
        do {
        } while (0);
        st->knock = StepAngle8Toward(ArcTan2(actor->posX - spy->posX,
                                                actor->posY - spy->posY, 0),
                                  spy->bodyRotZ, 0x20);
        st->speed = 0xFA;
        st->turn = 0;
        if (actor->mode != 0) {
          *st->flag = 0;
        }
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        ENTER_POSE(actor, 2);
      }
      if (st->init == 0) {
        st->init = 1;
        if (st->lurk != 0 && ApproxDist2D(&actor->posX, &SPY.posX) < 0x2000) {
          if (actor->unk53 != 0xFF) {
            SpawnActorDeathDrop(actor, 1, 0, 0);
            MarkActorCommittedKill(actor);
          }
          DespawnActorRecord(actor);
          continue;
        }
      }
      if (TickCountdownByFrameDelta(&st->timer3, 4) == 0) {
        int d = ApproxDist2D(&actor->posX, &spy->posX) + 0x1000;

        if (st->camDist < d) {
          st->camDist = d;
        }
        SPY.controlFlags = 0x80000200;
        g_anCameraActiveTargetParams.w[0] = -ArcTan2(spy->posX - actor->posX,
                                         SPY.posY - actor->posY, 0) *
                          16;
        g_anCameraActiveTargetParams.w[2] = st->camDist;
      }

      switch (actor->mode) {
      case 0: {
        int *p;

        TickCountdownByFrameDelta(&st->timer2, 4);
        switch (st->mode) {
        case 0:
          TrackEntityYawTowardSpyro(actor, 0xA, 0, 0);
          break;

        case 1:
          ANIM_GO(actor, 1);
          if (st->speed < 0x96) {
            st->speed = 0x96;
          }
          if (ApproxDist2D(&actor->posX,
                            &st->path->nodes[st->path->cur].x) < 0x100) {
            int r;
            PathData *pp;

            do {
              pp = st->path;
              r = RandomIntInRange(0, pp->count - 1);
              pp = st->path;
            } while (r == pp->cur);
            pp->cur = r;
            st->speed = RandomIntInRange(0xB4, 0xE6);
          }
          if (TurnActorYawTowardAngle(
                  actor,
                  ArcTan2(
                      st->path->nodes[st->path->cur].x - actor->posX,
                      st->path->nodes[st->path->cur].y - actor->posY, 0),
                  6, 0x1E, 1) != 0) {
            MoveActorAlongHeading(actor, st->speed, 0, 0, 5);
          }
          break;

        case 2:
          if (st->dist2 == 0) {
            st->dist2 = ApproxDist2D(&actor->posX, &st->path->nodes[0].x);
          }
          break;

        case 3: {
          int spd = 0xA0;
          int d = ApproxDist2D(&actor->posX, &g_pActorListBase[st->target].posX);

          ANIM_GO(actor, 1);
          if (d < 0xFA0) {
            spd = 0x8C;
          } else if (d > 0x1770) {
            spd = 0xB4;
          }
          TurnActorYawTowardAngle(actor,
                        ArcTan2(g_pActorListBase[st->target].posX - actor->posX,
                                      g_pActorListBase[st->target].posY - actor->posY,
                                      0),
                        4, 0, 0);
          MoveActorAlongHeading(actor, spd, 0, 0, 5);
          break;
        }
        }
        if (st->mode == 1 || st->mode == 3) {
          p = &st->path->nodes[0].x;
        } else {
          p = &actor->posX;
        }
        if (st->mode == 2) {
          if (ApproxDist2D(&spy->posX, &st->path->nodes[1].x) < 0x5DC ||
              ApproxDist2D(&spy->posX, &st->path->nodes[2].x) < 0x5DC) {
            st->charging = 0;
            actor->mode = 1;
            continue;
          }
          break;
        }
        if (st->timer2 == 0 && *st->flag == 0 && g_nSpyroAirborneFrames == 0) {
          int dz = (actor->posZ - actor->floorOffset) - SPY.posZ;

          if ((dz > 0 ? dz < 0x320
                      : SPY.posZ - (actor->posZ - actor->floorOffset) < 0x320) &&
              ApproxDist2D(p, &SPY.posX) < st->range << 10) {
            *st->flag = 1;
            st->timer = 0;
            st->charging = 0;
            if (st->mode == 3) {
              st->mode = 1;
              CopyVector(st->vec, p);
            }
            actor->mode = 1;
            continue;
          }
        }
        break;
      }

      case 1: {
        int s137;
        int dist = ApproxDist2D(&actor->posX, &spy->posX);

        if (TickCountdownByFrameDelta(&st->timer, 4) == 0) {
          break;
        }
        ANIM_GO(actor, 1);
        if (st->mode == 2) {
          SteerActorAroundPoint(actor, &st->path->nodes[0].x, st->dist2,
                        ArcTan2(spy->posX - st->path->nodes[0].x,
                                      SPY.posY - st->path->nodes[0].y, 0),
                        8, 0x12C, 0xE, 8, 0xFF, 0xFF, 0, 0, 0);
          if (dist < 0xED8) {
            st->speed = 0xE6;
            st->cam = 0;
            ENTER_POSE(actor, 3);
          }
          break;
        }
        if (st->filter != 0) {
          Actor **pp = g_apSaveMenuAnimActorList;
          Actor *a;

          while ((a = *pp++) != 0) {
            int d;

            if (a->type == 0x193 || a->type == st->filter) {
              d = actor->posX - a->posX;
              if (d > 0 ? d < 0xBB8 : a->posX - actor->posX < 0xBB8) {
                d = actor->posY - a->posY;
                if ((d > 0 ? d < 0xBB8 : a->posY - actor->posY < 0xBB8) &&
                    ApproxDist2D(&actor->posX, &a->posX) < 0x7D0 &&
                    AbsAngleDelta8(actor->rotZ,
                                  ArcTan2(a->posX - actor->posX,
                                                a->posY - actor->posY, 0)) <
                        0x19) {
                  a->flags |= 0x80000;
                }
              }
            }
          }
        }
        if (TickCountdownByFrameDelta(&st->timer2, 4) != 0) {
          st->knock = SPY.bodyRotZ;
          st->timer2 = 0x1E;
        }
        st->turn = (ArcTan2(actor->posX - spy->posX,
                                  actor->posY - spy->posY, 0) -
                    st->knock + 0x100) %
                   0x100;
        if (TurnActorYawTowardAngle(actor,
                          WrapAddAngle8(ArcTan2(spy->posX - actor->posX,
                                                      spy->posY - actor->posY,
                                                      0),
                                        st->turn),
                          5, 0x1E, 1) == 0) {
          if (st->charging == 0) {
            break;
          }
        } else {
          st->charging = 1;
        }
        s137 = 0x15E;
        if (st->speedOverride != 0) {
          s137 = st->speedOverride;
        }
        if (MoveActorAlongHeading(actor, s137, 0, 0x12C, 0x15) != 0) {
        reset137:
          *st->flag = 0;
          st->timer2 = 0x78;
          actor->mode = 0x64;
          continue;
        }
        if (AbsAngleDelta8(spy->bodyRotZ,
                          ArcTan2(actor->posX - spy->posX,
                                        actor->posY - spy->posY, 0)) >= 0x3D ||
            ApproxDist2D(&actor->posX, st->vec) >= 0x4001) {
          goto reset137;
        }
        if (dist < 0xED8 &&
            AbsAngleDelta8(actor->rotZ,
                          ArcTan2(spy->posX - actor->posX,
                                        spy->posY - actor->posY, 0)) < 0x14) {
          st->speed = 0xFA;
          st->cam = 0;
          ENTER_POSE(actor, 3);
        }
        break;
      }

      case 2:
        if (actor->frame < 5) {
          actor->rotZ += st->turn;
        }
        if (st->speed > 0xF) {
          st->speed -= 0xF;
          MoveActorAlongAngle(actor, st->knock, st->speed, 0, 0x12C, 5);
        }
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x28);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case 3:
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (MoveActorAlongHeading(actor, st->speed, 0, 0x12C, 0x15) == 2) {
          *st->flag = 0;
          st->timer2 = 0x78;
          actor->mode = 0x64;
          continue;
        }
        if (actor->frame >= 4 && st->speed > 0x28) {
          st->speed -= 0x1E;
        }
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x5DC) {
          int dz = (actor->posZ - actor->floorOffset) - SPY.posZ;

          if (dz > 0 ? dz < 0x384
                     : SPY.posZ - (actor->posZ - actor->floorOffset) < 0x384) {
            if (st->cam == 0) {
              st->cam = 1;
              st->timer2 = 0xDC;
              SPY.damageFlags |= 0x86;
              g_anSpyroRequestVelocity.x = LookupCosine(actor->rotZ * 16) * 0x87 >> 12;
              g_nSpyroRequestVelocityY = LookupSine(actor->rotZ * 16) * 0x87 >> 12;
              g_nSpyroRequestVelocityZ = 0x32;
            } else if (st->cam == 1) {
              st->cam = 2;
              st->timer3 = 0x1E;
              SPY.controlFlags |= 0x80000200;
              SPY.pCamAnchor = &actor->posX;
              SPY.pCamTarget = &g_anCameraActiveTargetParams;
              g_anCameraActiveTargetParams.w[0] = -ArcTan2(SPY.posX - actor->posX,
                                               SPY.posY - actor->posY, 0) *
                                16;
              g_anCameraActiveTargetParams.w[2] =
                  ApproxDist2D(&actor->posX, &SPY.posX) + 0xC00;
              g_anCameraActiveTargetParams.w[1] = 0x64;
              g_anCameraActiveTargetParams.w[3] = 0;
              g_anCameraActiveTargetParams.w[4] = -0xC8;
              g_anCameraActiveTargetParams.w[5] = 0;
            }
          }
        }
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, 4);
        }
        break;

      case 4:
        if (g_nActorAnimDone != 0) {
          actor->rotZ = WrapAddAngle8(actor->rotZ, 0x80);
          if (st->cam == 0) {
            ENTER_POSE(actor, 1);
          }
          *st->flag = 0;
          ANIM_GO(actor, 1);
          actor->mode = 0x64;
          continue;
        }
        break;

      case 0x64:
        ANIM_GO(actor, 1);
        if (st->mode == 2) {
          SteerActorAroundPoint(actor, &st->path->nodes[0].x, st->dist2,
                        ArcTan2(st->vec[0] - st->path->nodes[0].x,
                                      st->vec[1] - st->path->nodes[0].y, 0),
                        1, 0xC8, 0xE, 8, 0xFF, 0xFF, 0, 0, 0);
          if (ApproxDist2D(&actor->posX, st->vec) < 0x200) {
            ENTER_POSE(actor, 0);
          }
          break;
        }
        if (TurnActorYawTowardAngle(actor,
                          ArcTan2_8bit(st->vec[0] - actor->posX,
                                        st->vec[1] - actor->posY),
                          4, 0x14, 1) != 0) {
          MoveActorAlongHeading(actor, 0xA0, 0, 0, 5);
        }
        if (ApproxDist2D(&actor->posX, st->vec) < 0xB4) {
          ENTER_POSE(actor, 0);
        }
        break;
      }
      RefreshActorDerivedState(actor, 1);
      break;
    }

    /* Ambient flyer (FlyState): circles its anchor and pops near Spyro. */
    case 173: {
      FlyState *st = actor->state;

      if (actor->subMode == 0) {
        actor->rotX = (unsigned short)g_anCosineLut[st->angle] >> 7;
        actor->rotY = (unsigned short)g_anSineLut[st->angle] >> 7;
        st->angle += g_nFrameStep * 2;

        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x200) {
          int height = actor->posZ - actor->floorOffset;
          int drop = height - SPY.posZ;

          if (drop > 0 ? drop < 0x200 : (SPY.posZ - height) < 0x200) {
            int i;

            PlaySoundEffect(g_pLevelSampleBankHeader[0], actor, 0x10, 0);
            RefreshActorDerivedState(actor, 4);

            for (i = 0; i < 6; i++) {
              g_pfnLevelEmitParticles(1, 0xC, actor, (int *)(0x8080 + (i << 24)));
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

        if (st->attach != 0xFF) {
          RefreshActorDerivedState(actor, 4);
          RotateVectorByMatrix(&actor->mtx[0], &g_anRotBillboardOffset4,
                        g_aRotBillboards[st->attach].pos);
          AddVector(g_aRotBillboards[st->attach].pos, g_aRotBillboards[st->attach].pos,
                        &actor->posX);
          if (g_aRotBillboards[st->attach].life < 5) {
            st->attach = 0xFF;
          }
        } else if (st->timer >= 0xF8) {
          int handle = SpawnActorRotBillboard(actor, &g_anRotBillboardOffset4);

          if (handle >= 0 && ApproxDist2D(&actor->posX, &SPY.posX) < 0x4000) {
            st->attach = handle;
          }
          st->timer = (GetRandomU32() & 0x3F) + 0x18;
        }

        st->timer += g_nFrameStep;
      }

      break;
    }

    /* Breakable chest (ChestState): one hit showers gems. */
    case 194: {
      ChestState *st = actor->state;

      if (st->shake != 0) {
        SnapActorToGround(actor);
      }

      if (actor->flags & 0xB0000) {
        int i;

        actor->unk55 = 0x20;
        PlayActorMeshVoice(actor, 0, 0);

        for (i = 0; i < 2; i++) {
          if ((g_nActorPoolCapacity - g_nActorPoolLiveCount) < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0xFF, actor);
          g_pfnLevelSpawnActor(0x100, actor);
        }

        for (i = 0; i < 6; i++) {
          if ((g_nActorPoolCapacity - g_nActorPoolLiveCount) < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0x101, actor);
        }

        g_pfnLevelEmitParticles(5, 2, &actor->posX, 0);
        g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (int *)0x20);
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        DespawnActorRecord(actor);
        continue;
      }

      break;
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

    /* Boss door (DoorState): swings open, rewards the player, then leaves. */
    case 250: {
      DoorState *st = actor->state;
      int *at;

      if (actor->mode == 0) {
        if (st->partner != -1) {
          g_pActorListBase[st->partner].mode = 3;
        }
        actor->mode = 1;
        st->rotX = actor->rotX;
        st->rotY = actor->rotY;
        st->restZ = actor->posZ;
        break;
      }

      if (actor->mode == 1) {
        st->phase += g_nFrameStep;

        if (st->phase >= 0x101) {
          st->phase = 0;
          actor->rotX = st->rotX;
          actor->rotY = st->rotY;
          actor->posZ = st->restZ;
          actor->flags = 0;
          StopSoundVoicesByOwner(actor, 1);
        } else if (st->phase >= 0xC0) {
          int dx;
          int dy;

          if (GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[0]) == 0) {
            PlayActorMeshVoice(actor, 0, 0);
          }

          actor->rotX = g_aShakeRotOffsets[(st->phase - 0xC0) >> 1].a + st->rotX;
          actor->rotY = g_aShakeRotOffsets[(st->phase - 0xC0) >> 1].b + st->rotY;

          dx = (signed char)g_aShakeRotOffsets[(st->phase - 0xC0) >> 1].a;
          dy = (signed char)g_aShakeRotOffsets[(st->phase - 0xC0) >> 1].b;
          if (dx < 0) {
            dx = -dx;
          }
          if (dy < 0) {
            dy = -dy;
          }
          dx += dy;
          actor->posZ = st->restZ + dx * 6;
        } else if (st->phase >= 0xBC) {
          st->rotX = actor->rotX;
          st->rotY = actor->rotY;
          st->restZ = actor->posZ;
        } else if (actor->flags != 0) {
          st->phase = 0xBC;
          StopSoundVoicesByOwner(actor, 1);
        } else if ((st->phase >> 1) == 0x10 && !(GetRandomU32() & 3)) {
          SpawnActorRotBillboard(actor, &g_anRotBillboardOffset0);
        } else if ((st->phase >> 1) == 0x30 && !(GetRandomU32() & 3)) {
          SpawnActorRotBillboard(actor, &g_anRotBillboardOffset1);
        } else if ((st->phase >> 1) == 0x50 && !(GetRandomU32() & 3)) {
          SpawnActorRotBillboard(actor, &g_anRotBillboardOffset2);
        }

        at = &actor->posX;
        if (ApproxDist2D(at, &spy->posX) < 0x800) {
          SubtractVector(A256, at, &spy->posX);
          A256[2] = (A256[2] * 3) >> 2;

          if (VectorLength(A256, 1) < 0x440) {
            int facing;

            facing =
                st->partner != -1 ? g_pActorListBase[st->partner].rotZ : st->unk1C;
            CommitActorKillGems(0, actor);
            SetSpawnAnchorFromActor(actor, facing);

            if (st->reward != -1) {
              g_anLevelDragonsRescued[g_nLevelIntroIndex] += 1;
              g_nWorldDragonsRescued += 1;
              StubRewardHook(st->reward, 0);

              if (st->partner != -1) {
                g_pActorListBase[st->partner].mode = 1;
              }
              DespawnActorRecord(actor);
              continue;
            }

            if (st->unk18 == st->reward) {
              g_anLevelDragonsRescued[g_nLevelIntroIndex] += 1;
              g_nWorldDragonsRescued += 1;

              if (st->partner != -1) {
                g_pActorListBase[st->partner].mode = 1;
              }
              DespawnActorRecord(actor);
              continue;
            }

            actor->mode = 2;
            actor->rotX = st->rotX;
            actor->rotY = st->rotY;
            actor->posZ = st->restZ;
            BeginGemPickupOverlay(actor);
            ZeroVector(&SPY.headLook);
            g_dwSpyroRequestMask = 0x80002147;

            if (g_nSpyroAirborneFrames != 0) {
              g_nSpyroRequestState = 6;
              g_anSpyroRequestVelocity.x = g_anSpyroMotionVec >> 8;
              g_nSpyroRequestVelocityY = D_80078B68 >> 8;
              if (g_nSpyroMotionZ > 0) {
              clear_c68:
                g_nSpyroRequestVelocityZ = 0;
              } else {
                g_nSpyroRequestVelocityZ = g_nSpyroMotionZ >> 6;
              }
            } else {
              int *look = &SPY.u208;

              g_nSpyroRequestState = 3;
              CopyVector(look, &SPY.u10C);
              RShiftVector3(look, 6);
              g_nSpyroRequestVelocityZ = 0;

              if (SPY.u208 != 0 || g_nSpyroRequestVelocityY != 0) {
                ScaleVector3ByRatio(look, VectorLength(look, 0), 0x60);
                goto clear_c68;
              }
            }
          }
        }

      } else {
        g_dwSpyroRequestMask = 0x80002000;
      }

      break;
    }

    /* Dragon fragment, updated out of line. */
    case 251:      TickGemPickupBurstEntity(actor);
      break;

    /* Thrown projectile (ShotState30): flies until it lands. */
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
      ShotState30 *st = actor->state;
      int radius = actor->unk50;

      if (!(radius & 0x80) && radius != 0) {
        if (st->life > 0) {
          int z;

          actor->rotZ += 1;
          st->velZ -= 6;
          if (st->velZ < -0x80) {
            st->velZ = -0x80;
          }
          actor->posX += st->vel[0];
          actor->posY += st->vel[1];
          z = actor->posZ + st->velZ;
          actor->posZ = z;

          if (z >= 0x400) {
            if (CollideSphereWithWorldAndActors(&actor->posX, 0x100, 0x100) != 0) {
              int *nrm = &g_anCollisionProbeVec;
              int along;

              actor->posX = g_anCollisionResolvedPos;
              actor->posY = AS_I32(g_nCollisionResolvedY);
              actor->posZ = AS_I32(g_nCollisionResolvedZ);
              SetVector3Magnitude(nrm, 0x1000);
              along = ((st->vel[0] * nrm[0]) + (st->vel[1] * g_nCollisionProbeVecY) +
                       (st->velZ * g_nCollisionProbeVecZ)) >>
                      11;

              if (along < 0) {
                ScaleVector3ByRatio(nrm, 0x1000, -along);
                st->vel[0] += nrm[0];
                st->vel[1] += AS_I32(g_nCollisionProbeVecY);
                st->velZ += AS_I32(g_nCollisionProbeVecZ);
              }
            }
            st->life -= 1;
          } else {
            DespawnActorRecord(actor);
            continue;
          }
        } else {
          g_pfnLevelEmitParticles(0xA, 0x47, &actor->posX, 0);
          DespawnActorRecord(actor);
          continue;
        }
      }

      break;
    }

    /* Sparkling shard: one puff per frame. */
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

        A256[0] = (GetRandomU32() & 0xFE) - 0x7F;
        A256[1] = (GetRandomU32() & 0xFE) - 0x7F;
        A256[2] = (GetRandomU32() & 0xFE) - 0x40;
        AddVector(A256, A256, &actor->posX);
        g_pfnLevelEmitParticles(1, 0x42, A256, (int *)1);
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
            A272[0] = (GetRandomU32() & 0xFE) - 0x7F;
            A272[1] = (GetRandomU32() & 0xFE) - 0x7F;
            A272[2] = (GetRandomU32() & 0xFE) - 0x40;
            AddVector(A272, A272, &actor->posX);
            g_pfnLevelEmitParticles(1, 0x42, A272, (int *)1);
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
          SpawnActorRotBillboard(actor, D_8006E5C4[GetRandomU32() & 3]);
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
          ZeroVector(A288);
          g_pfnLevelEmitParticles(1, 0, &st->prize->posX, A288);
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
            CopyVector(A304, &st->prize->posX);
            A304[0] += g_anCosineLut[j * 43] >> 6;
            A304[1] += g_anSineLut[j * 43] >> 6;
            A304[2] += 0x28;
            A320[0] = g_anCosineLut[j * 43] >> 7;
            A320[1] = g_anSineLut[j * 43] >> 7;
            A320[2] = 0x10;
            g_pfnLevelEmitParticles(1, 0, A304, A320);
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

    /* Whirlwind pad: a short spin-up before the ride, then a cooldown. */
    case 331:      switch (actor->mode) {
      case 0:
        if (ApproxDist2D(&actor->posX, &SPY.posX) < 0xA01) {
          break;
        }
        actor->mode = 1;
        continue;

      case 1: {
        int height;
        int drop;

        if (ApproxDist2D(&actor->posX, &spy->posX) >= 0x380) {
          break;
        }

        height = actor->posZ - actor->floorOffset;
        drop = height - SPY.posZ;

        if (drop > 0 ? drop < 0x200 : (SPY.posZ - height) < 0x200) {
          SetSpawnAnchorFromActor(actor, actor->rotZ);
          actor->mode = 2;
          actor->anim = 1;
          actor->subMode = 0;
          continue;
        }
        break;
      }

      case 2: {
        int d = g_nFrameStep;
        int tick = actor->subMode + d;

        actor->subMode = tick;
        if ((unsigned char)tick < 0x30) {
          break;
        }
        actor->mode = 0;
        actor->anim = 0;
        continue;
      }
      }
      break;

    /* Fade-out props: removed as soon as their animation finishes. */
    case 400:
    case 405:
    case 477:      if (g_nActorAnimDone == 0) {
        break;
      }
      DespawnActorRecord(actor);
      continue;

    /* Type 390: spinning chest. Each charge spins it faster (with a rising
     * voice); spun fast enough, or flamed, it bursts. */
    case 390: {
      L26Spinner *st = actor->state;
      int pitch = *(unsigned short *)(g_pLevelSampleBankEntries +
                                      g_apActorMeshTable[actor->type]->m_Sounds[1] *
                                          20 +
                                      0xA);
      int cur = D_80075F38[actor->sfxSlot & 0x7F][0];
      int i;

      if ((actor->flags & 0x90000) == 0x10000) {
        actor->updateDist = 0xFF;
        PlaySoundEffect(g_apActorMeshTable[actor->type]->m_Sounds[1], actor, 8,
                      &actor->sfxSlot);
        if (actor->sfxSlot != 0x7F) {
          D_80075F38[actor->sfxSlot & 0x7F][0] = pitch + st->speed * 4;
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
        D_80075F44[actor->sfxSlot & 0x7F][0] = -4;
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
          SubtractVector(A288, &a->posX, &actor->posX);
          d = ABS2(A288[0]) + ABS2(A288[1]);
          if (d < 0x6000 &&
              (unsigned int)(VectorLength(A288, 0) - 0x1000) <= 0x2000 &&
              (unsigned int)(A288[2] + 0x1000) <= 0x2000) {
            d = AbsAngleDelta12(ArcTan2(actor->posX - spy->posX,
                                            actor->posY - spy->posY, 1),
                              ArcTan2(A288[0], A288[1], 1));
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

    /* Type 403: lurker. Roams its path, then pops up and drags Spyro with a
     * tether actor (type 31). */
    case 403: {
      L20State403 *st = actor->state;
      int dist;

      SubtractVector(A304, &spy->posX, &actor->posX);
      dist = ApproxDist2D(&actor->posX, &spy->posX);
      if (dist < 0x32C8) {
        dist = VectorLength(A304, 0);
      }

      if ((actor->flags & 0xB0000) && actor->mode != 2 &&
          actor->mode != 0x1E) {
        do {
        } while (0);
        actor->flags = 0;
        st->u38 = 0x50;
        st->u08 = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                              actor->posY - SPY.posY, 0),
                                g_bSpyroPersistentEulerZ, 0x20, 0x40);
        st->u10 = 0x64;
        if (actor->flags & 0x10000) {
          st->u0C = 0xF0;
        } else {
          st->u0C = 0x140;
        }
        if (st->tether != 0) {
          DespawnActorRecord(st->tether);
          st->tether = 0;
          if (g_pLevelOverlayInitRecord != 0 && st->u28 != 0) {
            g_pLevelOverlayInitRecord->subMode = 0;
          }
        }
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        ENTER_POSE(actor, 2);
      }

      switch (actor->mode) {
      case 0:
        if (dist < 0x2000 && TickCountdownByFrameDelta(&st->u24, 4) != 0 &&
            (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x5DC
                               : -SPY_DZ(actor) < 0x5DC) &&
            AbsAngleDelta8(((unsigned short)g_nCameraEulerYaw << 16 >> 20),
                          ArcTan2(actor->posX - g_anCameraPos[0],
                                        actor->posY - g_anCameraPos[1], 0)) <
                0x28 &&
            dist < 0xC80 &&
            AbsAngleDelta8(actor->rotZ,
                          ArcTan2(spy->posX - actor->posX,
                                        spy->posY - actor->posY, 0)) < 0x14) {
          int k;

          st->tether = g_pfnLevelSpawnActor(0x1F, actor);
          RefreshActorDerivedState(actor, 4);
          GetActorAttachPoint(actor, 0, A336);
          GetActorAttachPoint(actor, 1, A352);
          AddVector(&st->tether->posX, A336, A352);
          RShiftVector3(&st->tether->posX, 1);
          SubtractVector(A336, &st->tether->posX, &spy->posX);
          k = 0x1C000 / VectorLength(A336, 1);
          if (k < 5) {
            k = 5;
          }
          if (k >= 0x80) {
            k = 0x7F;
          }
          st->tether->unk57 = k;
          st->tether->rotZ = actor->rotZ;
          actor->mode = 0x14;
          continue;
        }
        if (st->mode == 0) {
          if (dist > 0x2000) {
            if (TickCountdownByFrameDelta(&st->u18, 4) != 0) {
              FlyPath *p = st->path;
              int c = p->cur;

              do {
                st->path->cur = RandomIntInRange(0, p->count - 1);
                p = st->path;
              } while (c == p->cur);
              actor->mode = 0xA;
              continue;
            }
          } else if (dist >= 0xFA1) {
            FindNearestPathPointToSpyro(st->path, A368);
            if (st->path->cur != A368[0] &&
                dist - ApproxDist2D((int *)&g_anSpyroWorldPos,
                                     PATH_PT((unsigned char *)st->path,
                                             A368[0])) >=
                    0x5DD) {
              st->path->cur = A368[0];
              actor->mode = 0xA;
              continue;
            }
          }
        }
        if (AbsAngleDelta8(actor->rotZ,
                          ArcTan2(spy->posX - actor->posX,
                                        spy->posY - actor->posY, 0)) >= 0x13) {
          actor->mode = 0xF;
          continue;
        }
        break;

      case 2:
        DriveActorTrackedMove(actor, &st->u0C, st->u08, &st->u10, 0xC, sixteen);
        if ((g_nGroundSurfaceType & 0x3F) == 0) {
          int h = ProbeActorGroundHeight(actor);
          int z = actor->posZ - actor->floorOffset;

          if (z - h > 0 ? z - h < 0x64 : h - z < 0x64) {
            Actor *o = g_pfnLevelSpawnActor(0x190, actor);

            PlaySoundEffect(g_pLevelSampleBankHeader[0x1D], actor, 8, &actor->sfxSlot);
            o->unk57 = 0x34;
            st->u24 = 0xA;
            actor->mode = 0x1E;
            continue;
          }
        }
        if (TickCountdownByFrameDelta(&st->u38, 4) != 0 || g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x18);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case 4:
        actor->rotZ += 5;
        if (TickCountdownByFrameDelta(&st->u18, 4) != 0 && g_nActorAnimDone != 0) {
          ENTER_POSE(actor, 0);
        }
        break;

      case 10:
        ANIM_GO(actor, 1);
        if (ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) < 0xDAC) {
          actor->mode = 0xF;
          continue;
        }
        if (TurnActorYawTowardAngle(
                actor,
                ArcTan2(FLY_X(st->path, st->path->cur) - actor->posX,
                              FLY_Y(st->path, st->path->cur) - actor->posY,
                              0),
                3, 2, 1) != 0 &&
            g_nActorAnimDone != 0) {
          st->u1C = ApproxDist2D(&actor->posX,
                                  PATH_PT((unsigned char *)st->path,
                                          st->path->cur)) /
                    0x14;
          actor->mode++;
        }
        break;

      case 11:
        MoveActorAlongHeading(actor, st->u1C, 0, 0, 5);
        if (g_nActorAnimDone != 0) {
          st->u18 = RandomIntInRange(0x28, 0x5A);
          ENTER_POSE(actor, 0);
        }
        break;

      case 15:
        ANIM_GO(actor, 1);
        if (TrackEntityYawTowardSpyro(actor, 8, 2, 1) != 0 && g_nActorAnimDone != 0) {
          ENTER_POSE(actor, 0);
        }
        break;

      case 20: {
        int d;
        int k;

        ANIM_GO(actor, 3);
        RefreshActorDerivedState(actor, 4);
        GetActorAttachPoint(actor, 0, A352);
        GetActorAttachPoint(actor, 1, A376);
        AddVector(&st->tether->posX, A352, A376);
        RShiftVector3(&st->tether->posX, 1);
        SubtractVector(A352, &st->tether->posX, (int *)&g_anSpyroWorldPos);
        d = VectorLength(A352, 1) + 0x1F4;
        if (d < 0x578) {
          d = 0x578;
        }
        if (d >= 0xC81) {
          d = 0xC80;
        }
        k = 0x1C000 / d;
        if (k < 5) {
          k = 5;
        }
        if (k >= 0x80) {
          k = 0x7F;
        }
        st->tether->unk57 = k;
        if (actor->nextFrame < 2) {
          st->tether->unk50 = 0;
        } else {
          st->tether->unk50 = 0x20;
        }
        if (actor->nextFrame >= 4 && dist < 0xC80) {
          SPY.damageFlags |= 7;
        }
        actor->rotZ = ArcTan2(spy->posX - actor->posX,
                                     spy->posY - actor->posY, 0);
        st->tether->rotZ = ArcTan2(spy->posX - actor->posX,
                                          spy->posY - actor->posY, 0);
        st->tether->rotY =
            ArcTan2(ApproxDist2D(&st->tether->posX, &spy->posX),
                          SPY.posZ - st->tether->posZ, 0);
        st->tether->rotY = StepAngle8Toward(st->tether->rotY, 0, 0x20);
        if (g_nActorAnimDone != 0) {
          DespawnActorRecord(st->tether);
          st->tether = 0;
          st->u24 = 0xB4;
          ENTER_POSE(actor, 0);
        }
        break;
      }

      case 30:
        if (TickCountdownByFrameDelta(&st->u24, 4) != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        actor->posZ -= 0x64;
        break;
      }
      RefreshActorDerivedState(actor, 1);
      continue;
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
        ((unsigned char *)rider->state)[0xE] = st->unk14;
      }

      actor->unk55 = 0x20;
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
        A376[0] = ((short)g_anCosineLut[i * 32] >> 8) * 3;
        A376[1] = ((short)g_anSineLut[i * 32] >> 8) * 3;
        A376[2] = 0x18;
        g_pfnLevelEmitParticles(1, 0, &actor->posX, A376);
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

    /* Orbiting satellite (SatState): rides a spinning parent at a fixed
     * radius, or just spins in place behind a non-spinner. */
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
      SatState *st = actor->state;

      if (st->parent->mode == 0 || st->parent->subMode != actor->mode) {
        DespawnActorRecord(actor);
        continue;
      }

      if (st->parent->type == 9) {
        if (ApproxDist2D(&st->parent->posX, &spy->posX) < 0x400) {
          unsigned char was = actor->subMode;
          int prev = g_anCosineLut[was];
          unsigned char now = was + 8;

          actor->subMode = now;
          actor->rotZ =
              actor->rotZ - ((prev * 3) >> 9) + ((g_anCosineLut[now] * 3) >> 9);
        } else {
          int bearing;
          int arm;
          int idx;
          short *swing;

          actor->subMode += 8;
          bearing = ArcTan2(spy->posX - st->parent->posX,
                                  SPY.posY - st->parent->posY, 0);
          A376[0] = (g_anCosineLut[bearing] * 3) >> 4;
          A376[1] = (g_anSineLut[bearing] * 3) >> 4;
          A376[2] = 0;
          A336[0] = (unsigned short)g_anCosineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          A336[1] = (unsigned short)g_anSineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          A336[2] = 0;
          bearing = (bearing + 0x80) & 0xFF;
          actor->rotZ = bearing + ((g_anCosineLut[actor->subMode] * 3) >> 9);
          ScaleVector3Sat(A392, A336, st->radius - 1);
          LShiftVector3(A336, 1);
          arm = st->radius - 1;
          A392[2] += (g_anCosineLut[arm * 2] * 3) >> 3;
          A408[0] = A376[0] * g_anCosineLut[arm * 2];
          A408[1] = A376[1] * g_anCosineLut[arm * 2];
          A408[2] = 0;
          bearing = arm << 1;
          bearing = (bearing - (st->phase * 4)) & 0xFF;
          ScaleVector3Sat(A336, A336, st->phase);
          SubtractVector(A392, A392, A336);
          swing = &g_anCosineLut[bearing];
          actor->posX = A376[0] * *swing;
          actor->posY = A376[1] * *swing;
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, A408);
          RShiftVector3(&actor->posX, 0xA);
          AddVector(&actor->posX, &actor->posX, A376);
          AddVector(&actor->posX, &actor->posX, &st->parent->posX);
          SubtractVector(&actor->posX, &actor->posX, A392);
          actor->posZ = actor->posZ + ((*swing * 3) >> 3) + 0x600;
        }
      } else {
        unsigned char now = actor->subMode + 8;

        actor->subMode = now;
        actor->rotZ = actor->floorOffset + ((g_anCosineLut[now] * 3) >> 9);
      }

      break;
    }

    /* Type 466: roaming ram. Wanders its waypoint ring away from Spyro,
     * charges down its path once he is close, and is knocked back by hits
     * or by its partner. */
    case 466: {
      L18Ram *st = actor->state;

      if ((actor->flags & 0xF0000) && actor->mode != 3) {
        do {
        } while (0);
        if (actor->flags & 0x40000) {
          st->knock = ArcTan2(actor->posX - g_pActorListBase[st->target].posX,
                                    actor->posY - g_pActorListBase[st->target].posY,
                                    0);
          st->speed = 0x8C;
          st->lift = 0x23;
        } else {
          st->knock = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                                  actor->posY - spy->posY, 0),
                                    spy->bodyRotZ, 0x20, 0x40);
          st->speed = 0xC8;
          st->lift = 0x32;
          if (actor->flags & 0x20000) {
            st->speed += 0x46;
            st->lift += 0x32;
          }
          SpawnActorDeathDrop(actor, 3, 0, 0);
          MarkActorCommittedKill(actor);
        }
        if (actor->nextAnim != 3) {
          g_nActorAnimDone = 0;
          actor->frameProgress = sixteen;
          actor->frameRate = sixteen;
          actor->nextAnim = 3;
          actor->nextFrame = 0;
          StubActorHook(actor);
        }
        actor->mode = 3;
        continue;
      }
      actor->flags = 0;
      if (st->chan != -1 && g_anLevelTriggerStateSlots[st->chan] != 0 &&
          (unsigned int)(actor->mode - 3) >= 2 &&
          IsPointInRotatedBox(&actor->posX, &st->p2C) != 0) {
        ENTER_POSE(actor, 4);
      }

      switch (actor->mode) {
      case 0:
        if (TickCountdownByFrameDelta(&st->call, 4) != 0) {
          st->call = (GetRandomU32() & 0x1F) + 0x50;
          PlayActorMeshVoice(actor, (int)GetRandomU32() % 3, 0);
        }
        if (st->mode == 1 && ApproxDist2D(&actor->posX, &SPY.posX) < 0x1800) {
          actor->mode = 10;
          continue;
        }
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x1388) {
          int ang = ArcTan2(spy->posX - actor->posX,
                                  SPY.posY - actor->posY, 0);
          int best = 0;
          int sel;
          int k;
          int d;

          for (k = 0; k < st->path->count; k++) {
            if (k == st->path->cur) {
              continue;
            }
            d = AbsAngleDelta8(ang,
                              ArcTan2(st->path->nodes[k].x - actor->posX,
                                            st->path->nodes[k].y - actor->posY,
                                            0));
            if (best < d) {
              best = d;
              sel = k;
            }
          }
          if (best > 0x40) {
            st->path->cur = sel;
            ENTER_POSE(actor, 1);
          }
        }
        break;

      case 1: {
        int ang =
            ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                          st->path->nodes[st->path->cur].y - actor->posY, 0);

        if (TickCountdownByFrameDelta(&st->call, 4) != 0) {
          st->call = (GetRandomU32() & 0x1F) + 0x50;
          PlayActorMeshVoice(actor, (int)GetRandomU32() % 3, 0);
        }
        if (TurnActorYawTowardAngle(actor, ang, 6, 0x14, 1) != 0) {
          MoveActorAlongAngle(actor, ang, 0x50, 0, 0x12C, 5);
          if (ApproxDist2D(&actor->posX,
                            &st->path->nodes[st->path->cur].x) < 0x100) {
            ENTER_POSE(actor, 0);
          }
        }
        break;
      }

      case 3:
        DriveActorTrackedMove(actor, &st->speed, st->knock, &st->lift, 0xC, 0x10);
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x10);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case 4:
        if (g_anLevelTriggerStateSlots[st->chan] == 0) {
          st->speed = 0;
          st->lift = 0;
          SpawnActorDeathDrop(actor, 1, 0, 0);
          MarkActorCommittedKill(actor);
          ENTER_POSE(actor, 3);
        }
        if (actor->nextFrame >= 0xF) {
          actor->frame = 0;
          actor->nextFrame = 1;
          actor->frameProgress = 0;
        }
        break;

      case 10:
        if (SPY.u09C != 0) {
          break;
        }
        ANIM_GO(actor, 1);
        if (PivotActorAlongPath(actor, st->path, 0x100, 0x78, 0, 8, 0x28, 0xFF, 5) ==
            0x100) {
          g_pActorListBase[st->partner].subMode = 1;
          actor->mode = 11;
          continue;
        }
        break;

      case 11:
        if (TickCountdownByFrameDelta(&st->call, 4) != 0) {
          st->call = (GetRandomU32() & 0x3F) + 0x78;
          PlayActorMeshVoice(actor, (int)GetRandomU32() % 3, 0);
        }
        ANIM_GO(actor, 0);
        break;
      }
      RefreshActorDerivedState(actor, 1);
      break;
    }

    /* Type 476: grabber. Creeps up on Spyro, seizes him, carries him off and
     * throws him; it drops him when hit. */
    case 476: {
      L20State476 *st = actor->state;
      int d0 = ApproxDist2D(&actor->posX, &spy->posX);

      do {
      } while (0);
      if ((actor->flags & 0x90000) && actor->mode != 4 &&
          actor->mode != 0x63) {
        Actor *o = g_pfnLevelSpawnActor(0x1DC, actor);

        CopyVector(&o->posX, &actor->posX);
        o->mode = 0x63;
        if (o->anim != 0xA) {
          ANIM_RESET(o, 0xA);
        }
        SpawnActorDeathDrop(actor, 1, 0, 0);
        MarkActorCommittedKill(actor);
        st->u24 = 0x15E;
        if (actor->mode >= 0xA) {
          g_nSpyroDrawSuppressed = 0;
        }
        st->u20 = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                              actor->posY - SPY.posY, 0),
                                g_bSpyroPersistentEulerZ, 0x20, 0x40);
        st->u28 = 0xF0;
        if (actor->mode != 0) {
          *st->held = 0;
        }
        PlayActorMeshVoice(actor, 1, 0);
        ANIM_SET(actor, 9);
        actor->mode = 4;
        continue;
      }
      actor->flags = 0;
      if (actor->nextAnim == 1 &&
          GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[2]) == 0) {
        PlayActorMeshVoice(actor, 2, (int)&st->u38);
      }

      switch (actor->mode) {
      case 0:
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        if (d0 > 0x2C00) {
          st->u14 = 0;
          ANIM_GO(actor, 0);
        }
        if (TickCountdownByFrameDelta(&st->u18, 4) != 0) {
          int k;
          int r;

          if (st->u14 == 0) {
            if (actor->nextAnim == 0 && actor->nextFrame >= 0xF) {
              actor->frame = 0;
              actor->nextFrame = 1;
              actor->frameProgress = 0;
            }
          } else if (actor->nextAnim == 0 && actor->nextFrame >= 0x1C) {
            g_nActorAnimDone = 0;
            ANIM_ADVANCE(actor, 1);
          }
          if (st->u14 == 0 && d0 < 0x2400) {
            st->u14 = RandomIntInRange(0, 0x3F) + 0x50;
          }
          k = st->u14 - (d0 >> 6);
          r = GetRandomU32() & 0xFF;
          if (TickCountdownByFrameDelta(&st->u34, 4) != 0 && *st->held == 0 && r < k &&
              AbsAngleDelta8(((unsigned short)g_nCameraEulerYaw << 16 >> 20),
                            ArcTan2(actor->posX - g_anCameraPos[0],
                                          actor->posY - g_anCameraPos[1], 0)) <
                  0x28 &&
              SPY_DZ(actor) >= -0x1F3 &&
              (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x578
                                 : -SPY_DZ(actor) < 0x578)) {
            *st->held = 1;
            st->u18 = sixteen;
            StopSoundVoicesByOwner(actor, 1);
            actor->mode = 1;
            continue;
          }
          if (*st->held != 0) {
            st->u34 = 0x3C;
          }
        }
        break;

      case 1: {
        int d1 = ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos);

        if (d1 < 0xFA0) {
          ANIM_GO(actor, 2);
        } else {
          ANIM_GO(actor, 3);
        }
        if (TickCountdownByFrameDelta(&st->u18, 4) != 0) {
          Actor **lp;
          Actor *o;

          if (MoveActorAlongAngle(actor,
                            ArcTan2(spy->posX - actor->posX,
                                          spy->posY - actor->posY, 0),
                            0x15E, 0, 0, 0x15) != 0 ||
              *(int *)((char *)spy + 0x170) != 0) {
            actor->mode = 2;
            continue;
          }
          lp = g_apSaveMenuAnimActorList;
          while ((o = *lp++) != 0) {
            if (o->type == 0x1C &&
                (actor->posX - o->posX > 0 ? actor->posX - o->posX < 0xBB8
                                           : o->posX - actor->posX < 0xBB8) &&
                (actor->posY - o->posY > 0 ? actor->posY - o->posY < 0xBB8
                                           : o->posY - actor->posY < 0xBB8) &&
                ApproxDist2D(&actor->posX, &o->posX) < 0x7D0 &&
                AbsAngleDelta8(actor->rotZ,
                              ArcTan2(o->posX - actor->posX,
                                            o->posY - actor->posY, 0)) < 0x19) {
              o->flags |= 0x80000;
            }
          }
          if (d1 < 0x44C) {
            st->u30 = 0;
            actor->mode = 0xA;
            continue;
          }
        }
        break;
      }

      case 2:
        ANIM_GO(actor, 3);
        if (TurnActorYawTowardAngle(actor,
                          ArcTan2(st->home[0] - actor->posX,
                                        st->home[1] - actor->posY, 0),
                          6, 0x14, 1) != 0) {
          MoveActorAlongAngle(actor,
                        ArcTan2(st->home[0] - actor->posX,
                                      st->home[1] - actor->posY, 0),
                        0xC8, 0, 0, 5);
        }
        if (ApproxDist2D(&actor->posX, st->home) < 0x100) {
          *st->held = 0;
          st->u18 = 0x78;
          ENTER_POSE(actor, 0);
        }
        break;

      case 4:
        actor->rotX += 4;
        actor->rotY += 4;
        if (DriveActorTrackedMove(actor, &st->u24, st->u20, &st->u28, 0xC, 0xE) == 3) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x30);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case 6:
        if (g_nActorAnimDone != 0) {
          actor->mode = 1;
          continue;
        }
        break;

      case 10: {
        int dd = ApproxDist2D(&actor->posX, &spy->posX);
        int lim = 0x50;
        int v;

        if (dd >= 0x12D) {
          lim = 0xFA;
        }
        if (dd < 0x190) {
          g_nSpyroDrawSuppressed = 1;
        }
        if (st->u30 == 0) {
          if (dd < lim) {
            st->u30 = 1;
            actor->posX = spy->posX;
            actor->posY = SPY.posY;
          } else {
            TrackEntityYawTowardSpyro(actor, 6, 0, 0);
            if (MoveActorAlongAngle(actor,
                              ArcTan2(spy->posX - actor->posX,
                                            SPY.posY - actor->posY, 0),
                              lim, 0, 0, 0x15) != 0 &&
                g_nSpyroDrawSuppressed == 0) {
              actor->mode = 2;
              continue;
            }
          }
        }
        g_dwSpyroRequestMask = 0x80002000;
        if (actor->nextFrame >= 0xD) {
          SPY.posX = actor->posX;
          g_nSpyroDrawSuppressed = 1;
          SPY.posY = actor->posY;
          v = actor->rotZ + 0x80;
          g_bSpyroPersistentEulerZ = v;
          g_nSpyroBodyYaw = (v & 0xFF) << 4;
          if (g_nLevelReadyFlag == 0) {
            spy->damageFlags |= 7;
            actor->mode = 0xD;
            continue;
          }
          if (D_8007577C != 0) {
            actor->mode = 0xE;
            continue;
          }
          actor->mode++;
        }
        break;
      }

      case 11:
        ANIM_GO(actor, 4);
        spy->posX = actor->posX;
        spy->posY = actor->posY;
        g_nSpyroDrawSuppressed = 1;
        spy->controlFlags = 0x80002000;
        if (g_nActorAnimDone != 0) {
          actor->mode++;
        }
        break;

      case 12:
        ANIM_GO(actor, 5);
        if (g_nSpyroDrawSuppressed != 0) {
          if (actor->nextFrame >= 0x29) {
            g_nSpyroDrawSuppressed = 0;
            SPY.damageFlags = 0x86;
            SPY.posX =
                actor->posX + ((g_anCosineLut[actor->rotZ] * 0x4B) >> 10);
            SPY.posY =
                actor->posY + ((g_anSineLut[actor->rotZ] * 0x4B) >> 10);
            SubtractVector(A408, &SPY.posX, &actor->posX);
            ScaleVector3ByRatio(A408, VectorLength(A408, 0), 0x64);
            g_nSpyroRequestVelocityZ = 0x37;
            g_anSpyroRequestVelocity.x = A408[0];
            g_nSpyroRequestVelocityY = A408[1];
          } else {
            g_dwSpyroRequestMask = 0x80002000;
          }
        }
        if (g_nActorAnimDone != 0) {
          D_8007577C = 1;
          actor->mode = 2;
          continue;
        }
        break;

      case 13:
        g_dwSpyroRequestMask = 0x80002000;
        ANIM_GO(actor, 8);
        g_dwSpyroTriggerEventFlags |= 7;
        break;

      case 14:
        ANIM_GO(actor, 7);
        if (g_nSpyroDrawSuppressed != 0) {
          if (actor->nextFrame >= 0x10) {
            g_nSpyroDrawSuppressed = 0;
            SPY.damageFlags = 0x86;
            SPY.posX =
                actor->posX + ((g_anCosineLut[actor->rotZ] * 0x4B) >> 10);
            SPY.posY =
                actor->posY + ((g_anSineLut[actor->rotZ] * 0x4B) >> 10);
            SubtractVector(A376, &SPY.posX, &actor->posX);
            ScaleVector3ByRatio(A376, VectorLength(A376, 0), 0x64);
            g_nSpyroRequestVelocityZ = 0x37;
            g_anSpyroRequestVelocity.x = A376[0];
            g_nSpyroRequestVelocityY = A376[1];
          } else {
            g_dwSpyroRequestMask = 0x80002000;
          }
        }
        if (g_nActorAnimDone != 0) {
          actor->mode = 2;
          continue;
        }
        break;

      case 99:
        if (g_nActorAnimDone != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      if (ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) > 0x200) {
        RefreshActorDerivedState(actor, 1);
      }
      continue;
    }

    /* Type 487. */
    case 487:
      if (actor->flags != 0 && actor->mode < 3) {
        actor->mode = 3;
        ANIM_RESET(actor, 3);
        continue;
      }
      switch (actor->mode) {
      case 0:
        break;

      case 1:
        ANIM_SET(actor, 1);
        if (actor->nextFrame >= 0xE) {
          actor->mode = 2;
          ANIM_RESET(actor, 2);
          continue;
        }
        break;

      case 2:
        break;

      case 3:
        if (actor->nextFrame >= 0x13) {
          actor->mode = 4;
          ANIM_RESET(actor, 4);
          continue;
        }
        break;

      case 4:
        break;
      }
      RefreshActorDerivedState(actor, 1);
      continue;
}
  }
}
