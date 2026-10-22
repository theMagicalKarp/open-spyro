/* Level25_UpdateActors -- per-frame actor update for level 25 (Dark
 * Passage).
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class: a small
 * state machine over the actor's state byte (->mode), often with a sub-state
 * (->subMode), and usually a class-specific state block behind ->state.
 *
 * Most classes are shared with the other levels' actor updates (levels 9,
 * 16, 26 and 31); the level-25 classes are the grabber and its lookout (50,
 * 51), the sentry pair (53, 54), the falling rock (56), the lantern and its
 * flame (122, 124), the flame-lit switch (129), the flying ember (155) and
 * the cannon (402).
 *
 * Load-bearing source forms (matched 2026-09-30):
 *   - Loop-top carriers `oneq` (short) then `one`, each used once (case 9);
 *     every other 1 is a literal, so int sites and compares use the hoisted
 *     fp while byte stores stay inline `li`. 16 is a literal, not a carrier.
 *   - Scratch arrays at function level in frame order (the shared slots
 *     s152..s576 are reused across arms).
 *   - Case 402 reads its two gain columns as `g_anSineLut[t * 2 - 6]` and
 *     `[t * 2 - 5]`; two separate symbols compile identically but lose the
 *     reload spill slot that makes the frame 0x2D0.
 *   - Case 50 holds its scratch vector in a pointer local (s0) and reaches
 *     its grab/partner arms by goto, after the drop block.
 *   - The timer and speed selects in cases 51, 53 and 54 are if/else
 *     around the store; the range tests in cases 51 and 53 are written
 *     positively (ABS2 form).
 *   - Case 110's first state is written like case 331's (`< 0xA01 -> break;
 *     unk48 = 1; continue`) so the two call tails cross-jump.
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
extern int D_80077AE8;            /* bit 0: switch sounds enabled */
extern unsigned char *g_pLevelSampleBankEntries; /* sound definitions, 20 bytes each */
extern unsigned char *g_pLevelSampleBankHeader; /* sound id table */
extern int g_nSfxOverrideFlags;
extern struct {
  short v;
} g_nSfxOverridePitch;
extern unsigned short D_80073094[]; /* indexed by the switch's bit position */

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

/* Sparkle nodes spawned around items. */
typedef struct AnimNode {
  int pos[3];         /* 0x00 */
  unsigned char flag; /* 0x0C */
  unsigned char pad[11];
} AnimNode;

extern AnimNode D_80077108[];

/* Sparkle anchor offsets per model family. */
extern int D_8006E5A0;
extern int D_8006E5B8;
extern int D_8006E5AC;

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
extern unsigned char g_abSaveOptionsBlock[]; /* save-options block; [1] is the latch */
extern int D_800777F4;
extern int D_800777F8;
extern void ResetSpyroLinearMotion(void);
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

extern AngPair D_8006E638[];

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
extern unsigned char **g_pTriggerRegionTable; /* surface records, indexed by the above */
extern int g_nCdStreamState;
extern int g_nLevelTransitionPhase;
extern int g_nLevelTransitionTallyActive;
extern int g_nGamePaused;
extern int g_nGamestate;
extern int g_nScriptedMobViewYawSpin;
extern int g_nScriptedMobViewPitchTilt;
extern int g_nLevelIntroParallaxCountdown;

/* 0x18-byte spherical camera coordinates; D_8006CA24 holds the presets. */
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

extern Xform D_8006CA24[];

extern void ComputeCameraToTargetAngle(int *out);
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
  int unk18;        /* 0x18 index into D_8006CA24; negative selects none */
} PortalState;

/* Crystal dragon sparkle anchors, one per sparkle phase. */
extern int D_8006E57C;
extern int D_8006E588;
extern int D_8006E594;

extern int g_anLevelDragonsRescued[]; /* per-g_nLevelIntroIndex completion counters */

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
  int unk20;           /* 0x20 */
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
extern void *D_8006E330[];

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

/* ==== level_30 globals ====
 *
 * The hub's own state: which stage of the balloon sequence the world is in,
 * the scripted-camera block at 0x80078C4C, and the three ambience channels. */
extern int g_nWorldTreasureCollected;           /* gem total shown on the balloon */
extern unsigned char D_800758D1; /* per-world "seen it" flags */
extern unsigned char D_800758D2;
extern unsigned char D_800758D3;
extern unsigned char D_800758D4;
extern unsigned char D_800758D5;
extern int D_80077350;

extern short g_nCameraEulerYaw; /* camera yaw, as the balloonist reads it */
extern int g_nCameraCurrentMode;   /* camera mode */
extern int g_nCameraSpringYaw;   /* camera spherical target */
extern int g_nCameraYaw;   /* camera spherical current */
extern int g_nCameraNextMode;
extern Xform *g_pCameraTargetParams; /* active camera preset, or null */

extern volatile unsigned char g_bSpyroOtBinBias;
extern int g_anSpyroBodyMtx[]; /* wind basis matrix */
extern int g_nSpyroState;   /* Spyro's state */
extern int g_nSpyroStateFlags;
extern int g_nSpyroStateTimer;
extern int g_nSpyroAirborneFrames;
extern int g_anSpyroMotionVec; /* look-at offset */
extern int D_80078B68;
extern int g_nSpyroMotionZ;
extern int g_nSpyroMotionSpeed;
extern int g_nSpyroBodyYaw; /* camera yaw accumulator */
extern int g_nSpyroWorldPosY;
extern volatile unsigned char
    g_bSpyroPersistentEulerZ;          /* SPY.bodyRotZ, absolute form (A240) */
extern int g_nLevelReadyFlag[]; /* balloon sequence stage (== SPY.u164) */
extern int g_nSpyroSwingEulerPitchTarget;

extern int g_dwSpyroRequestMask;         /* scripted camera control word */
extern signed char D_80078C5E; /* destination level */
extern struct {
  int x;
  int y;
} g_anSpyroRequestVelocity; /* scripted camera look-at */
#define g_nSpyroRequestVelocityY g_anSpyroRequestVelocity.y
extern int g_nSpyroRequestVelocityZ;
extern int g_nSpyroRequestState; /* scripted camera mode */
extern PortalDesc *volatile g_pGemPickupCamPathData;
extern int g_nGemPickupCamPathMaxStep;
extern int g_nGemPickupCamPathReverseFlag;

extern unsigned char g_bAmbienceChannelMask; /* ambience channel enables */
extern unsigned char D_80078E99;
extern unsigned char D_80078E9A;
extern unsigned char D_8007A6C7;
extern unsigned char D_8007A6C8;

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

extern int PickupGroupHasLiveActor(int stage);
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
extern int RaiseSpyroTriggerInRadius(int *pos, int a, int b);

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
extern int D_800778E8[]; /* hits taken */
extern int D_800778EC[]; /* stage reached */
extern int CheckSpyroLineOfSightTo(int *pos);
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
extern int FindNearestPathPointToActor(Actor *actor, PostPath *path, int *idx);
extern int g_nHitRumbleTimer;
/* Case 174 -- the cage that carries Spyro. */
typedef struct CageState {
  int idx;    /* 0x00 index of its flyer in g_pActorListBase */
  int shake;  /* 0x04 shake timer, then ride timer */
  int rotX;   /* 0x08 rest pose */
  int rotY;   /* 0x0C */
  int restZ;  /* 0x10 */
  int handle; /* 0x14 for SelectActorLodBand */
} CageState;
extern Actor *D_80075758;
extern int g_nCameraScriptStickRequestY[];
extern int g_nCameraScriptStickRequestX;
extern void DivideVector3(int *d, int *s, int k);
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
extern int SteerActorTowardPoint(Actor *actor, int *target, int a, int b, int c,
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

/* ==== Level 26 classes ==== */

/* Case 52 -- bomb-lobbing lizard. */
typedef struct L26Lobber {
  int timer;   /* 0x00 */
  int heading; /* 0x04 knock-back heading */
  int speed;   /* 0x08 knock-back speed */
  int reload;  /* 0x0C */
} L26Lobber;

/* Case 61 -- sinking block. */
typedef struct L26Sink {
  int partner; /* 0x00 index into g_pActorListBase */
  int depth;   /* 0x04 */
} L26Sink;

/* Case 160 -- wandering critter. */
typedef struct L26Critter {
  char pad00[0x38];
  int knock; /* 0x38 knock-back heading */
  int speed; /* 0x3C knock-back speed */
} L26Critter;

/* Case 296 -- bobbing platform. */
typedef struct L26Bob {
  int partner;        /* 0x00 index into g_pActorListBase */
  unsigned int phase; /* 0x04 */
  int unk08;
  int unk0C;
  int baseZ;          /* 0x10 */
  int big;            /* 0x14 */
} L26Bob;

/* Case 298 -- bouncer riding its partner. */
typedef struct L26Rider {
  int partner; /* 0x00 index into g_pActorListBase */
  int speed;   /* 0x04 */
  int lift;    /* 0x08 */
  int heading; /* 0x0C */
} L26Rider;

/* Case 88 -- escort. */
typedef struct L26Escort {
  int leader;     /* 0x00 index into g_pActorListBase */
  int partner;    /* 0x04 index into g_pActorListBase, -1 = none */
  PathData *path; /* 0x08 */
  int p0C[3];     /* 0x0C */
  int timer;      /* 0x18 */
  int slot;       /* 0x1C place in the ring */
  int speed;      /* 0x20 */
} L26Escort;

/* Case 390 -- spinning chest. */
typedef struct L26Spinner {
  Actor *lid; /* 0x00 */
  int angle;  /* 0x04 */
  int speed;  /* 0x08 */
  int timer;  /* 0x0C */
  int handle; /* 0x10 for SelectActorLodBand */
} L26Spinner;
extern int D_80075F38[][7]; /* voice table */
extern int D_80075F44[][7]; /* its word 3 */

/* Case 392 -- homing spark. */
typedef struct L26Spark {
  int vel[3]; /* 0x00 */
  int timer;  /* 0x0C */
} L26Spark;

/* Case 62 -- path guard. */
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

typedef struct L26Rot {
  unsigned char r[3];
} L26Rot;

#define sixteen 0x10

/* ==== Classes shared with level 31 ==== */

/* Set the state byte and queue animation `n` without promoting the current
 * one, then move on to the next actor. */
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

/* Case 401 -- breakable rock. */
typedef struct L31Rock {
  int shake;  /* 0x00 shake timer, 0 = still */
  int rotX;   /* 0x04 rest rotation */
  int rotY;   /* 0x08 */
  int restZ;  /* 0x0C */
  int handle; /* 0x10 for SelectActorLodBand */
} L31Rock;

/* ==== Level 25 classes ==== */

extern int g_nSpyroDrawSuppressed;

/* Case 50 -- grabber. Only one grabber at a time may hold Spyro; the shared
 * slot names the holder. */
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

/* Clear the partner's "held" bit. */
#define L25_UNLINK(st)                                                         \
  if ((st)->link >= 0) {                                                       \
    g_pActorListBase[(st)->link].subMode &= 0xFB;                                      \
  }

/* Give up the shared slot. */
#define L25_DROP(st)                                                           \
  *(st)->slot = 0;                                                             \
  L25_UNLINK(st)

/* Case 53 -- sentry. */
typedef struct L25Sentry {
  int speed; /* 0x00 retreat speed (case 54) */
  int step;  /* 0x04 */
  int dir;   /* 0x08 retreat heading */
  int link;  /* 0x0C index of its partner in g_pActorListBase, -1 = none */
  int timer; /* 0x10 */
} L25Sentry;

/* Case 122 -- lantern. */
typedef struct L25Lantern {
  int held;    /* 0x00 */
  int timer;   /* 0x04 */
  FxNode *fx;  /* 0x08 glow node */
  int pos[3];  /* 0x0C glow anchor */
  int sound;   /* 0x18 sound handle, -1 = none */
} L25Lantern;

/* Case 402 -- cannon ball: fired from a barrel, then flown along a path
 * whose header is { count, cur } and whose points follow at +8 (PATH_X). */
typedef struct L25Shot {
  int link;            /* 0x00 index of the actor it wakes on arrival */
  int vel[3];          /* 0x04 */
  int timer;           /* 0x10 */
  unsigned char *path; /* 0x14 */
  int init;            /* 0x18 1 = aim the barrel along the path */
} L25Shot;
#define PATH_PT(p, i) ((int *)((p) + (((i) << 4) + 0x8)))
extern short D_8006CBEC[];
extern short D_8006CBEE[];

/* ==== The function ==== */
#define spy (&SPY)
void Level25_UpdateActors(void) {
  Actor **actorList;
  Actor *actor;
  short oneq;
  int one;
  char *base;
  int dt;

  /* Scratch vectors, in frame order. */
  int svA[3];
  int svB[3];
  int svC[3];
  int svD[3];
  int svE[3];
  int svZ[3];
  int s152[3];
  int s168[3];
  int s184[3];
  int acc[3][3];
  int svG[3];
  int svH[3];
  int svI[3];
  int s288[3];
  int s304[3];
  int svX[3];
  int svY[3];
  int fx[6];
  int col[3];
  int svW[3];
  int svV[3];
  int v250[3];
  int svM[3];
  int svT[3];
  int ps[6];
  int s496[3];
  int s512[3];
  int s528[3];
  int s544[3];
  int s560[3];
  int s576[3];
  int v496[3];
  int v528[3];
  int v544[3];

  /* Build this frame's update list. A long frame (delta >= 3) gets an extra
   * pass. */
  BuildActiveActorList();
  TickGemCutsceneActorAnims(g_apSaveMenuAnimActorList, 0);

  if (g_nFrameStep >= 3) {
    TickGemCutsceneActorAnims(g_apSaveMenuAnimActorList, g_nFrameStep == 3 ? 0x80000001 : 0x80000000);
  }

  actorList = g_apSaveMenuAnimActorList;
  base = (char *)&g_nSpyroWorldPosZ;

  /* Visit every actor on the list; dormant ones (state >= 0x80) are skipped. */
  while (actor = *actorList++) {
    oneq = 1;
    one = 1;

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

    /* Type 50: grabber. Seizes Spyro when he wanders close in front of it,
     * holds him under a scripted camera, then walks back to its post. */
    case 50: {
      L25Grabber *st = actor->state;

      if ((actor->flags & 0x90000) && actor->mode != 5 && actor->mode != 1) {
        if (*st->slot == actor) {
          L25_DROP(st);
        }
        SpawnActorDeathDrop(actor, 1, 0, 0);
        MarkActorCommittedKill(actor);
        ENTER_POSE(actor, 5);
      }
      actor->flags = 0;
      if (st->link >= 0) {
        if (actor->mode != 3 && actor->mode != 5 &&
            g_pActorListBase[st->link].subMode == 3) {
          if (*st->slot == actor) {
            L25_DROP(st);
          }
          ENTER_POSE(actor, 3);
        }
      } else {
        actor->type = 0x33;
        actor->mode = 0;
        ANIM_RESET(actor, 0);
        continue;
      }

      switch (actor->mode) {
      case 0:
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        if (*st->slot == actor && TickCountdownByFrameDelta(&st->hold, 4)) {
          L25_DROP(st);
        }
        if (TickCountdownByFrameDelta(&st->timer, 4) == 0) {
          break;
        }
        if (ApproxDist2D(&actor->posX, &SPY.posX) >= 0x1800) {
          break;
        }
        if (ABS(actor->posZ - actor->floorOffset - g_nSpyroWorldPosZ + 0x164) >= 0x200) {
          break;
        }
        if (g_nSpyroAirborneFrames != 0 || g_nSpyroState == 0xB || g_nSpyroState == 0x14) {
          break;
        }
        if (*st->slot != 0) {
          break;
        }
        if (AbsAngleDelta8(actor->rotZ,
                          ArcTan2(SPY.posX - actor->posX,
                                        SPY.posY - actor->posY, 0)) >= 0x20) {
          break;
        }
        actor->subMode = 0;
        *st->slot = actor;
        st->hold = 0x3C;
        st->timer = 0x3C;
        ENTER_POSE(actor, 2);

      case 1:
        if (g_nActorAnimDone != 0) {
          actor->subMode = 1;
          L25_UNLINK(st);
          st->timer = 0xF0;
          ENTER_POSE(actor, 2);
        }
        if (g_nSpyroDrawSuppressed != 0 && actor->nextFrame >= 0x20) {
          SubtractVector((int *)&g_anSpyroRequestVelocity, st->grip, &actor->posX);
          RShiftVector3((int *)&g_anSpyroRequestVelocity, 4);
          g_dwSpyroTriggerEventFlags = 0x80;
          g_nSpyroDrawSuppressed = 0;
          actor->subMode = 1;
          break;
        }
        if (actor->subMode != 0) {
          break;
        }
        if (actor->nextFrame >= 7) {
          if (g_nSpyroDrawSuppressed == 0) {
            st->grip[0] = 0x25F;
            st->grip[1] = 0;
            st->grip[2] = 0x367;
            RefreshActorDerivedState(actor, 4);
            RotateVectorByMatrix(actor->mtx, st->grip, st->grip);
            AddVector(st->grip, &actor->posX, st->grip);
          }
          {
            int *v = svD;

            SubtractVector(v, st->grip, &SPY.posX);
            ScaleVector3Sat(v, v, ((actor->nextFrame - 7) << 12) / 25);
            RShiftVector3(v, 0xC);
            AddVector((int *)((char *)&SPY + 0x1F8), &SPY.posX, v);
          }
          g_nSpyroRequestState = 0;
          g_nSpyroDrawSuppressed = 1;
          g_dwSpyroRequestMask = 0x80006107;
        } else {
          g_dwSpyroRequestMask = 0x80002100;
          g_nSpyroRequestState = 0x23;
        }
        break;

      case 2:
        if (actor->subMode != 0) {
          int heading;
          int dist;

          if (*st->slot == actor && TickCountdownByFrameDelta(&st->hold, 4)) {
            L25_DROP(st);
          }
          heading = ArcTan2(st->homeX - actor->posX,
                                  st->homeY - actor->posY, 0);
          dist = ApproxDist2D(&actor->posX, &st->homeX);
          if (TickCountdownByFrameDelta(&st->timer, 4) || dist < 0x20) {
            st->timer = 0x3C;
            ENTER_POSE(actor, 0);
          }
          if (dist > 0x80) {
            dist = 0x80;
          }
          if (TurnActorYawTowardAngle(actor, heading, 4, 0x18, 1) == 0) {
            break;
          }
          MoveActorAlongHeading(actor, dist, 0x500, 0x500, 0x37);
          break;
        }
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        MoveActorAlongAngle(actor,
                      ArcTan2(SPY.posX - actor->posX,
                                    SPY.posY - actor->posY, 0),
                      0x200, 0x500, 0x500, 0x37);
        if (TickCountdownByFrameDelta(&st->timer, 4) == 0) {
          if (ApproxDist2D(&actor->posX, &SPY.posX) >= 0x5DC) {
            break;
          }
          if (ABS(actor->posZ - actor->floorOffset - g_nSpyroWorldPosZ + 0x164) >= 0x200) {
            break;
          }
          actor->subMode = 0;
          if (st->link < 0) {
            goto pose1;
          }
          if (g_pActorListBase[st->link].subMode & 2) {
            goto grab;
          }
        }
        L25_DROP(st);
        actor->subMode = 1;
        st->timer = 0xF0;
        break;
      grab:
        g_pActorListBase[st->link].subMode |= 4;
        ENTER_POSE(actor, 1);
      pose1:
        ENTER_POSE(actor, 1);

      case 3:
        if (g_nActorAnimDone == 0) {
          break;
        }
        actor->type = 0x33;
        actor->subMode = 1;
        st->timer = 0xF0;
        actor->mode = 1;
        ANIM_RESET(actor, 1);
        continue;

      case 4: {
        int reach = (actor->nextFrame << 7) + 0x200;

        if (reach > 0x500) {
          reach = 0x500;
        }
        DispatchActorContactAtSphere(&actor->posX, reach, 0, 0, actor, 1);
        if (g_nActorAnimDone == 0) {
          break;
        }
        st->timer = 0;
        ANIM_QUEUE(actor, 0);
      }

      case 5:
        if (g_nActorAnimDone == 0) {
          break;
        }
        RefreshActorDerivedState(actor, 4);
        BurstActorWithSound(actor, 0x34);
        DespawnActorRecord(actor);
        continue;
      }
      RefreshActorDerivedState(actor, 1);
      break;
    }

    /* Type 51: lookout. Shares the grabber's slot; bolts when struck and
     * turns into type 50 once its partner is roused. */
    case 51: {
      L25Grabber *st = actor->state;

      if (actor->flags & 0xB0000) {
        if (actor->mode != 3) {
          if (*st->slot == actor) {
            *st->slot = 0;
          }
          if (actor->flags & 0x10000) {
            st->fleeSpeed = 0xC8;
          } else {
            st->fleeSpeed = 0x190;
          }
          st->fleeStep = 0;
          st->fleeDir = StepAndLerpAngle8(ArcTan2(actor->posX - SPY.posX,
                                                    actor->posY - SPY.posY, 0),
                                      g_bSpyroPersistentEulerZ, 0x20, 0x40);
          SpawnActorDeathDrop(actor, 3, 0, 0);
          MarkActorCommittedKill(actor);
          actor->mode = 3;
          actor->flags = 0;
          if (actor->nextAnim != 3) {
            ANIM_ADVANCE(actor, 3);
          }
          continue;
        }
      } else if (actor->mode != 3 && st->link >= 0 &&
                 g_pActorListBase[st->link].subMode == 2) {
        if (*st->slot == actor) {
          *st->slot = 0;
        }
        actor->type = 0x32;
        actor->mode = 4;
        ANIM_RESET(actor, 4);
        continue;
      }

      switch (actor->mode) {
      case 0:
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        if (TickCountdownByFrameDelta(&st->timer, 4) &&
            ApproxDist2D(&actor->posX, &SPY.posX) < 0xC00 &&
            ABS2(actor->posZ - actor->floorOffset - g_nSpyroWorldPosZ) < 0x400 &&
            g_nSpyroState != 0xB && g_nSpyroState != 0x14 && *st->slot == 0 &&
            AbsAngleDelta8(actor->rotZ,
                          ArcTan2(SPY.posX - actor->posX,
                                        SPY.posY - actor->posY, 0)) < 0x20) {
          actor->subMode = 0;
          *st->slot = actor;
          st->timer = 0x3C;
          ENTER_POSE(actor, 1);
        }
        break;

      case 1:
        if (actor->subMode != 0) {
          int heading;
          int dist;

          heading = ArcTan2(st->homeX - actor->posX,
                                  st->homeY - actor->posY, 0);
          dist = ApproxDist2D(&actor->posX, &st->homeX);
          if (TickCountdownByFrameDelta(&st->timer, 4) || dist < 0x20) {
            st->timer = 0x3C;
            ENTER_POSE(actor, 0);
          }
          if (dist > 0x80) {
            dist = 0x80;
          }
          if (TurnActorYawTowardAngle(actor, heading, 4, 0x18, 1) == 0) {
            break;
          }
          MoveActorAlongHeading(actor, dist, 0x200, 0x200, 0x37);
          break;
        }
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        MoveActorAlongAngle(actor,
                      ArcTan2(SPY.posX - actor->posX,
                                    SPY.posY - actor->posY, 0),
                      0xF0, 0x200, 0x200, 0x37);
        if (TickCountdownByFrameDelta(&st->timer, 4)) {
          *st->slot = 0;
          actor->subMode = 1;
          st->timer = 0xF0;
          break;
        }
        if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x300 &&
            ABS2(actor->posZ - actor->floorOffset - g_nSpyroWorldPosZ) < 0x320) {
          actor->mode = 2;
          actor->subMode = 0;
          if (actor->nextAnim != 2) {
            ANIM_ADVANCE(actor, 2);
          }
          continue;
        }
        break;

      case 2:
        if (g_nActorAnimDone == 0) {
          break;
        }
        actor->subMode = 1;
        *st->slot = 0;
        st->timer = 0xF0;
        ENTER_POSE(actor, 1);

      case 3:
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x18);
          DespawnActorRecord(actor);
          continue;
        }
        DriveActorTrackedMove(actor, &st->fleeSpeed, st->fleeDir, &st->fleeStep, 0xC,
                      0x10);
        break;
      }
      RefreshActorDerivedState(actor, 5);
      break;
    }

    /* Type 52: bomb-lobbing lizard. Hides until Spyro is in range, then
     * lobs type 55 bombs at him; knocked away when hit. */
    case 52: {
      L26Lobber *st = actor->state;

      if ((actor->flags & 0x90000) && actor->mode != 2) {
        st->speed = 0xF0;
        st->heading = SPY.bodyRotZ;
        SpawnActorDeathDrop(actor, 1, 0, 0);
        MarkActorCommittedKill(actor);
        ENTER_POSE(actor, 2);
      }

      switch (actor->mode) {
      case 0:
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0 && actor->drawn != 0 &&
            ApproxDist2D(&actor->posX, &spy->posX) < 0x2800) {
          int dz = (actor->posZ - actor->floorOffset) - SPY.posZ;

          if (dz > 0 ? dz < 0xA00
                     : SPY.posZ - (actor->posZ - actor->floorOffset) < 0xA00) {
            ENTER_POSE(actor, 1);
          }
        }
        break;

      case 1:
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, 0);
        }
        if (actor->nextFrame >= 6 && st->timer == 0) {
          RefreshActorDerivedState(actor, 4);
          g_pfnLevelEmitParticles(0x10, 0x1C, g_pfnLevelSpawnActor(0x37, actor), (void *)0x10);
          st->timer = st->reload;
        }
        break;

      case 2:
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x28);
          DespawnActorRecord(actor);
          continue;
        }
        if (st->speed > 0) {
          MoveActorAlongAngle(actor, st->heading, st->speed, 0x1F4, 0x2BC, 1);
          st->speed -= 0x10;
        }
        break;
      }
      break;
    }

    /* Type 53: sentry. Watches for Spyro in front of it and raises the
     * alarm; turns into type 54 once its partner is roused. */
    case 53: {
      L25Sentry *st = actor->state;

      func_8003AA84(actor);
      actor->flags = 0;
      if (st->link >= 0) {
        if (actor->mode != 2 && g_pActorListBase[st->link].subMode == 3) {
          ENTER_POSE(actor, 2);
        }
      } else {
        actor->type = 0x36;
        actor->mode = 0;
        ANIM_RESET(actor, 0);
        continue;
      }

      switch (actor->mode) {
      case 0:
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        if (st->timer != 0) {
          if (actor->subMode < 2 && g_nSpyroState == 0xE) {
            st->timer += 0x3C;
            actor->subMode = 2;
          }
          if (TickCountdownByFrameDelta(&st->timer, 4) == 0) {
            break;
          }
          if (actor->drawn == 0) {
            break;
          }
          st->timer = 1;
          ENTER_POSE(actor, 1);
        }
        if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x1400 &&
            ABS2(actor->posZ - actor->floorOffset - g_nSpyroWorldPosZ) < 0x800 &&
            AbsAngleDelta8(actor->rotZ,
                          ArcTan2(SPY.posX - actor->posX,
                                        SPY.posY - actor->posY, 0)) < 0x18) {
          if (actor->subMode) {
            st->timer = 0x2C;
          } else {
            st->timer = 0x1A;
          }
          actor->subMode = 1;
          break;
        }
        actor->subMode = 0;
        break;

      case 1:
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, 0);
        }
        if (actor->nextFrame < 5) {
          break;
        }
        if (st->timer == 0) {
          break;
        }
        {
          Actor *shot;

          RefreshActorDerivedState(actor, 4);
          shot = g_pfnLevelSpawnActor(0x9B, actor);
          if (ApproxDist2D(&actor->posX, &SPY.posX) < 0xC00) {
            CopyVector(&shot->posX, &SPY.posX);
          }
        }
        st->timer = 0;
        break;

      case 2:
        if (g_nActorAnimDone == 0) {
          break;
        }
        actor->type = 0x36;
        actor->mode = 0;
        ANIM_RESET(actor, 0);
        continue;

      case 3:
        if (g_nActorAnimDone == 0) {
          break;
        }
        ANIM_QUEUE(actor, 0);
      }
      RefreshActorDerivedState(actor, 1);
      break;
    }

    /* Type 54: roused sentry. Backs away when struck; otherwise watches and
     * fires like type 53. */
    case 54: {
      L25Sentry *st = actor->state;

      func_8003AA84(actor);
      if ((actor->flags & 0xA0000) && actor->mode < 2) {
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        st->speed = 0x190;
        st->step = 0x8C;
        st->dir = StepAndLerpAngle8(ArcTan2(actor->posX - SPY.posX,
                                              actor->posY - SPY.posY, 0),
                                g_bSpyroPersistentEulerZ, 0x28, 0x80);
        ENTER_POSE(actor, 2);
      }
      actor->flags = 0;
      if (actor->mode < 2 && st->link >= 0 &&
          g_pActorListBase[st->link].subMode == 2) {
        actor->type = 0x35;
        actor->mode = 3;
        ANIM_RESET(actor, 3);
        continue;
      }

      switch (actor->mode) {
      case 0:
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        if (st->timer != 0) {
          if (actor->subMode < 2 && g_nSpyroState == 0xE) {
            st->timer += 0x3C;
            actor->subMode = 2;
          }
          if (TickCountdownByFrameDelta(&st->timer, 4) == 0) {
            break;
          }
          if (actor->drawn == 0) {
            break;
          }
          st->timer = 1;
          ENTER_POSE(actor, 1);
        }
        if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x1400 &&
            ABS2(actor->posZ - actor->floorOffset - g_nSpyroWorldPosZ) < 0x800 &&
            AbsAngleDelta8(actor->rotZ,
                          ArcTan2(SPY.posX - actor->posX,
                                        SPY.posY - actor->posY, 0)) < 0x18) {
          if (actor->subMode) {
            st->timer = 0x2C;
          } else {
            st->timer = 0x1A;
          }
          actor->subMode = 1;
          break;
        }
        actor->subMode = 0;
        break;

      case 1:
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, 0);
        }
        if (actor->nextFrame < 2) {
          break;
        }
        if (st->timer == 0) {
          break;
        }
        RefreshActorDerivedState(actor, 4);
        g_pfnLevelSpawnActor(0x9B, actor);
        st->timer = 0;
        break;

      case 2:
        if (g_nActorAnimDone != 0) {
          ANIM_QUEUE(actor, 3);
        }
        actor->rotY += 4;
        TurnActorYawTowardAngle(actor, (st->dir + 0x80) & 0xFF, 0x10, 0x20, 1);
        DriveActorTrackedMove(actor, &st->speed, st->dir, &st->step, 0xC, 0xC);
        break;

      case 3:
        if (DriveActorTrackedMove(actor, &st->speed, st->dir, &st->step, 0xC, 0x10) ==
            3) {
          ENTER_POSE(actor, 4);
        }
        TurnActorYawTowardAngle(actor, (st->dir + 0x80) & 0xFF, 0x10, 0x20, 1);
        break;

      case 4:
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x18);
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      RefreshActorDerivedState(actor, 1);
      break;
    }

    /* Type 55: lobbed bomb. Bursts on landing, on a timer or against
     * Spyro, then plays its burst and is removed. */
    case 55: {
      int *st = actor->state;

      switch (actor->mode) {
      case 0:
        if (actor->posX < 0x400 || actor->posY < 0x400 ||
            actor->posZ < 0x400) {
          actor->mode = 1;
          break;
        }
        CopyVector(svA, &actor->posX);
        if (*st < 0x64) {
          DispatchActorContactAtSphere(&actor->posX, 0x64, 0, 0x20000, actor, 0);
        }
        if (RaiseSpyroTriggerInRadius(&actor->posX, 0x64, 7) != 0 ||
            TickCountdownByFrameDelta(st, 4) != 0 ||
            TryMoveActorForward(actor, 0xE0, 0, 0x100, 0) != 0 ||
            CastRayWorldAndActors(svA, &actor->posX) != 0) {
          actor->mode = 1;
        }
        g_pfnLevelEmitParticles(1, 0x1C, actor, (void *)8);
        break;

      case 1:
        g_pfnLevelEmitParticles(0x10, 0x1C, actor, (void *)0x10);
        DespawnActorRecord(actor);
        continue;
      }
      break;
    }

    /* Type 56: falling rock. Drops until it lands, hits Spyro or times
     * out, then shatters. */
    case 56: {
      int *st = actor->state;

      switch (actor->mode) {
      case 0:
        CopyVector(svZ, &actor->posX);
        if (*st < 0xC8) {
          DispatchActorContactAtSphere(&actor->posX, 0x64, 0, 0x20000, actor, 0);
        }
        if (RaiseSpyroTriggerInRadius(&actor->posX, 0x64, 7) || TickCountdownByFrameDelta(st, 4) ||
            TryMoveActorForward(actor, 0x80, 0, 0x100, 0) ||
            CastRayWorldAndActors(svZ, &actor->posX)) {
          actor->mode = 1;
          ANIM_RESET(actor, 1);
        }
        continue;

      case 1:
        if (g_nActorAnimDone != 0) {
          DespawnActorRecord(actor);
        }
        continue;
      }
      continue;
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

    /* Explosion shard with a ground bounce (ShardState). */
    case 67:
    case 68:
    case 255:
    case 256:
    case 423:
    case 424: {
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
        s152[0] = GetRandomU32() & 3;
        s152[1] = GetRandomU32() & 3;
        s152[2] = 0x14;
        g_pfnLevelEmitParticles(1, 1, &actor->posX, s152);
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
            RotateVectorByMatrix(&actor->mtx[0], &D_8006E5B8,
                          D_80077108[st->attach].pos);
          } else {
            RotateVectorByMatrix(&actor->mtx[0], &D_8006E5A0,
                          D_80077108[st->attach].pos);
          }
          AddVector(D_80077108[st->attach].pos, D_80077108[st->attach].pos,
                        &actor->posX);

          if (D_80077108[st->attach].flag < 5) {
            st->attach = 0xFF;
          }
        } else if (st->timer >= 0xF8) {
          int handle = SpawnActorRotBillboard(actor, &D_8006E5A0);

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
          CopyVector(s168, &actor->posX);
          s168[2] += 0x400;
          FindGroundHeightBelow(s168, 0x10000);
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
            CopyVector(s184, &actor->posX);
            s184[2] += 0x400;
            RefreshActorDerivedState(actor, 2);
            if (FindGroundHeightBelow(s184, 0x1000) > 0) {
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

    /* Dragon-pad fairy (PoleState). */
    case 110: {
      PoleState *st = (PoleState *)actor->state;

      Actor *tgtA = &g_pActorListBase[st->idxA];
      Actor *tgtB = &g_pActorListBase[st->idxB];

      switch (actor->mode) {
      case 0: {
        actor->unk08 = 0;
        if (ApproxDist2D(&tgtB->posX, &SPY.posX) < 0xA01) {
          break;
        }
        actor->mode = 1;
        continue;
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

          SubtractVector(s168, st->tgt, &actor->posX);

          angleDiff = SignedAngleDelta8(st->angle, actor->rotZ);

          cosWeight1 = LookupCosine((st->t20 - 1) << 7);
          cosDelta = cosWeight1 - LookupCosine(st->t20 << 7);
          totalWeight = LookupCosine((st->t20 - 1) << 7) + 0x1000;

          actor->posX += (s168[0] * cosDelta) / totalWeight;
          actor->posY += ((s168[1] * cosDelta) / totalWeight);
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

    /* The balloonist (BalloonState): smokes, waits for his gnorc, then flies
     * Spyro out once the world is finished. */
    case 120: {
      BalloonState *st = actor->state;

      if (g_nLevelReadyFlag[0] >= 2) {
        s168[0] = 0;
        s168[1] = 0x64;
        s168[2] = 0;
        RotateVectorByMatrix(&actor->mtx[0], s168, s168);
        AddVector(s168, s168, &actor->posX);
        g_pfnLevelEmitParticles(2, 0x42, s168, 0);
        s168[0] = 0;
        s168[1] = -0x64;
        s168[2] = 0;
        ApplyActiveGteRotation(s168, s168);
        AddVector(s168, s168, &actor->posX);
        g_pfnLevelEmitParticles(2, 0x42, s168, 0);
      }

      if (g_nLevelReadyFlag[0] >= 3) {
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

      if (g_nLevelReadyFlag[0] <= 0) {
        if (st->gnorc != 0 && st->gnorc->type == 16) {
          st->gnorc->mode = 0;
        }
        g_pLevelOverlayInitRecord = 0;
        DespawnActorRecord(actor);
        continue;
      }

      if (st->gnorc != 0 &&
          ApproxDist2D(&actor->posX, (int *)((char *)g_nLevelReadyFlag - 0x164)) >=
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

            CopyVector(s288, &st->gnorc->posX);

            if (high == 0) {
              s288[0] -=
                  (unsigned short)g_anCosineLut[st->gnorc->rotZ] << 16 >> 20;
              s288[1] -=
                  (unsigned short)g_anSineLut[st->gnorc->rotZ] << 16 >> 20;
            } else {
              s288[0] -=
                  (unsigned short)g_anCosineLut[st->gnorc->rotZ] << 16 >> 22;
              s288[1] -=
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

            SubtractVector(s288, s288, &actor->posX);
            dist = VectorLength(s288, 1);

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
              ScaleVector3ByRatio(s288, dist, limit);
            } else {
              ScaleVector3ByRatio(s288, dist, reach - 5);
            }
            AddVector(&actor->posX, &actor->posX, s288);
            TurnActorYawTowardAngle(actor, ArcTan2(s288[0], s288[1], 0), 0xA, 0, 0);
            actor->rotY = ArcTan2(VectorLength(s288, 0), s288[2], 0);
            actor->rotY = StepAngle8Toward(actor->rotY & 0xFF, 0, 0x30);
            break;
          }

          case 1:
            g_nLevelReadyFlag[0] += 1;
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

            SubtractVector(s304, &st->gnorc->posX, &actor->posX);
            dist = VectorLength(s304, 1);

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
                g_pfnLevelEmitParticles(1, 0xC, actor, D_8006E330[st->gnorc->type]);
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
                ScaleVector3ByRatio(s304, dist, lim);
              } else {
                ScaleVector3ByRatio(s304, dist, 0x122);
              }
              AddVector(&actor->posX, &actor->posX, s304);
              TurnActorYawTowardAngle(actor, ArcTan2(s304[0], s304[1], 0), 0xA, 0, 0);
              actor->rotY = ArcTan2(VectorLength(s304, 0), s304[2], 0);
              actor->rotY = StepAngle8Toward(actor->rotY & 0xFF, 0, 0x30);
            }
            break;
          }
          }
        } else {
          /* Nobody in the basket: drift on the wind. */
          actor->unk50 = 0x10;

          if (st->timer <= 0) {
            s168[0] = -0x258;
            s168[1] = (GetRandomU32() & 0x310) - 0x188;
            s168[2] = g_nCameraCurrentMode == 3 ? (GetRandomU32() & 0x7F) + 0x64
                                     : (GetRandomU32() & 0x1FF) - 0xC8;
            do {
            } while (0);
            st->drift[0] = s168[0];
            st->drift[1] = s168[1];
            st->drift[2] = s168[2];
            st->timer = GetRandomU32() & 0x7B;
          } else {
            int *at;

            st->timer -= g_nFrameStep;
            do {
            } while (0);

            if (g_nCameraCurrentMode == 3) {
              s168[0] = st->drift[0] + (g_nSpyroMotionSpeed >> 2);
              s168[1] = st->drift[1];
              s168[2] = st->drift[2];
              if (s168[2] < 0x64) {
                s168[2] = 0x64;
              }
            } else {
              if (g_nCameraCurrentMode == 0x80000009) {
                s168[0] = st->drift[0] - 0x200;
              } else {
                s168[0] = st->drift[0];
              }
              s168[1] = st->drift[1];
              s168[2] = st->drift[2];
            }

            RotateVectorByMatrix(&SPY.u034, s168, s168);
            AddVector(s168, s168, &SPY.posX);
            at = &actor->posX;
            SubtractVector(s168, s168, at);
            RShiftVector3(s168, 2);
            AddVector(at, at, s168);

            if (CollideSphereWithWorldAndActors(at, 0x100, 0x100) != 0) {
              CopyVector(at, &g_anCollisionResolvedPos);
            }

            if (s168[2] >= 0x21) {
              s168[2] = 0x20;
            }
            if (s168[2] < -0x20) {
              s168[2] = -0x20;
            }

            actor->rotX = 0;
            actor->rotY = s168[2];
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

    /* Type 122: lantern. Lit and snuffed by Spyro; the glow node grows and
     * fades with it. */
    case 122: {
      L25Lantern *st = actor->state;

      if (actor->flags & 0xB0000) {
        actor->flags = 0;
        actor->subMode |= 3;
        if (actor->mode < 3) {
          g_pfnLevelSpawnActor(0x7C, actor);
          actor->mode = 3;
          ANIM_RESET(actor, 3);
          continue;
        }
      }
      actor->flags = 0;
      if (actor->subMode & 1) {
        if (st->fx == 0) {
          FxNode *g = AllocWorldSparkle();

          st->fx = g;
          if (g != 0) {
            g->anchor = st->pos;
            st->fx->size = 0x40;
            st->fx->unk14 = 0;
            st->fx->unk18 = 0;
            st->fx->unk1C = 0;
            st->fx->r = 0xC0;
            st->fx->g = 0xC0;
            st->fx->b = 0x60;
            st->fx->unk20 = -8;
          }
        } else {
          st->fx->size += 0x20;
          if (st->fx->size > 0x800) {
            st->fx->size = 0x800;
          }
          if (st->sound >= 0 && st->fx->size >= 0x300 &&
              ((GetWorldChunkAnimState(st->sound) >> 8) & 0xFF) < 0xB) {
            ModifyWorldChunkAnimFlags(st->sound, 0xFC, 0);
          }
        }
      } else if (st->fx != 0) {
        st->fx->size -= 0x40;
        if (st->fx->size <= 0x400 && st->sound >= 0 &&
            ((GetWorldChunkAnimState(st->sound) >> 8) & 0xFF) > 0) {
          ModifyWorldChunkAnimFlags(st->sound, 0xFC, 0);
        }
        if (st->fx->size <= 0) {
          ClearWord(st->fx);
          st->fx = 0;
        }
      }
      if (st->fx != 0) {
        RefreshActorDerivedState(actor, 4);
        GetActorAttachPoint(actor, 0, svX);
        GetActorAttachPoint(actor, 1, svY);
        AddVector(st->pos, svX, svY);
        RShiftVector3(st->pos, 1);
      }

      switch (actor->mode) {
      case 0:
        if (st->held == 0) {
          if (TickCountdownByFrameDelta(&st->timer, 4) == 0) {
            break;
          }
          if (actor->subMode & 4) {
            break;
          }
          st->timer = 0x78;
          actor->subMode &= 0xFD;
          if (actor->subMode & 1) {
            if (st->sound >= 0) {
              ModifyWorldChunkAnimFlags(st->sound, 0xFC, 0);
            }
            ENTER_POSE(actor, 1);
          }
          ENTER_POSE(actor, 2);
        }
        actor->subMode |= 2;
        break;

      case 1:
        actor->subMode &= 0xFE;
        if (g_nActorAnimDone == 0) {
          break;
        }
        actor->subMode |= 2;
        ENTER_POSE(actor, 0);

      case 2:
        actor->subMode |= 1;
        if (g_nActorAnimDone == 0) {
          break;
        }
        if (st->sound >= 0) {
          ModifyWorldChunkAnimFlags(st->sound, 0xFC, 0);
        }
        actor->subMode |= 2;
        ENTER_POSE(actor, 0);

      case 3:
        if (g_nActorAnimDone == 0) {
          break;
        }
        st->timer = 0x1C2;
        ANIM_QUEUE(actor, 4);

      case 4:
        if (TickCountdownByFrameDelta(&st->timer, 4) == 0) {
          break;
        }
        {
          Actor *flame = g_pfnLevelSpawnActor(0x7C, actor);

          actor->mode = 5;
          flame->mode = 1;
          ANIM_RESET(flame, 1);
        }
        continue;

      case 5:
        break;
      }
      RefreshActorDerivedState(actor, 1);
      break;
    }

    /* Type 124: lantern flame. Removes itself when its animation ends,
     * restarting the lantern's idle animation the second time. */
    case 124: {
      Actor **st = actor->state;

      if (actor->mode == 0) {
        if (g_nActorAnimDone != 0) {
          DespawnActorRecord(actor);
        }
        continue;
      }
      if (actor->mode == 1 && g_nActorAnimDone != 0) {
        DespawnActorRecord(actor);
        (*st)->mode = 0;
        ANIM_RESET(*st, 0);
      }
      continue;
    }

    /* Type 129: flame-lit switch. */
    case 129:
      switch (actor->mode) {
      case 0:
        if (!(actor->flags & 0x20000)) {
          continue;
        }
        ENTER_POSE(actor, 1);

      case 1:
        if (g_nActorAnimDone == 0) {
          continue;
        }
        actor->flags = 0;
        ENTER_POSE(actor, 0);
      }
      continue;

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

    /* Type 155: flying ember. Rides the floor, trails sparks, and bursts
     * when it lands, hits Spyro, times out or leaves the level. */
    case 155: {
      int *st = actor->state;
      int i;

      switch (actor->mode) {
      case 0:
        if (actor->posX < 0x400 || actor->posY < 0x400 ||
            actor->posZ < 0x400) {
          actor->mode = 1;
          continue;
        }
        {
          int floorZ = FindGroundHeightBelow(&actor->posX, 0x2000);

          if (floorZ != 0) {
            int tilt = actor->posZ - floorZ - 0x200;

            tilt = -tilt >> 2;

            if (tilt > 0x20) {
              tilt = 0x20;
            }
            if (tilt < -0x20) {
              tilt = -0x20;
            }
            actor->rotY = tilt;
          }
        }
        CopyVector(svW, &actor->posX);
        if (*st < 0xC8) {
          DispatchActorContactAtSphere(&actor->posX, 0x64, 0, 0x20000, actor, 0);
        }
        if (RaiseSpyroTriggerInRadius(&actor->posX, 0x64, 7) || TickCountdownByFrameDelta(st, 4) ||
            TryMoveActorForward(actor, 0x80, 0, 0x100, 0) ||
            CastRayWorldAndActors(svW, &actor->posX)) {
          actor->mode = 1;
        }
        ZeroVector(svW);
        g_pfnLevelEmitParticles(1, 1, &actor->posX, svW);
        continue;

      case 1:
        for (i = 0; i < 16; i++) {
          svW[0] = RandomIntInRange(-0x5A, 0x5A);
          svW[1] = RandomIntInRange(-0x5A, 0x5A);
          svW[2] = RandomIntInRange(-0x19, 0x3C);
          CopyVector(svV, svW);
          LShiftVector3(svV, 2);
          AddVector(svV, svV, &actor->posX);
          g_pfnLevelEmitParticles(1, 0xD, svV, svW);
        }
        DespawnActorRecord(actor);
        continue;
      }
      continue;
    }

    /* Type 160: wandering critter; knocked away when hit. */
    case 160: {
      L26Critter *st = actor->state;

      if ((actor->flags & 0xB0000) && actor->mode != 1) {
        actor->flags = 0;
        st->knock = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                                actor->posY - spy->posY, 0),
                                  spy->bodyRotZ, 0x20, 0x40);
        {
          if (actor->flags & 0x10000) {
            st->speed = 0x8C;
          } else {
            st->speed = 0xFA;
          }
        }
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        actor->mode = 1;
        ANIM_RESET(actor, 1);
        continue;
      }

      switch (actor->mode) {
      case 0:
        UpdateActorWander(actor, st);
        break;

      case 1:
        if (st->speed >= 0x10) {
          st->speed -= 0xF;
          MoveActorAlongAngle(actor, st->knock, st->speed, 0, 0x2BC, 5);
        }
        if (g_nActorAnimDone != 0) {
          DespawnActorRecord(actor);
          continue;
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

        if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x200) {
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
          RotateVectorByMatrix(&actor->mtx[0], &D_8006E5AC,
                        D_80077108[st->attach].pos);
          AddVector(D_80077108[st->attach].pos, D_80077108[st->attach].pos,
                        &actor->posX);
          if (D_80077108[st->attach].flag < 5) {
            st->attach = 0xFF;
          }
        } else if (st->timer >= 0xF8) {
          int handle = SpawnActorRotBillboard(actor, &D_8006E5AC);

          if (handle >= 0 && ApproxDist2D(&actor->posX, &SPY.posX) < 0x4000) {
            st->attach = handle;
          }
          st->timer = (GetRandomU32() & 0x3F) + 0x18;
        }

        st->timer += g_nFrameStep;
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

          actor->rotX = D_8006E638[st->phase >> 1].a + st->rotX;
          actor->rotY = D_8006E638[st->phase >> 1].b + st->rotY;

          dx = (signed char)D_8006E638[st->phase >> 1].a;
          dy = (signed char)D_8006E638[st->phase >> 1].b;
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

          actor->rotX = D_8006E638[(st->phase - 0xC0) >> 1].a + st->rotX;
          actor->rotY = D_8006E638[(st->phase - 0xC0) >> 1].b + st->rotY;

          dx = (signed char)D_8006E638[(st->phase - 0xC0) >> 1].a;
          dy = (signed char)D_8006E638[(st->phase - 0xC0) >> 1].b;
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
          SpawnActorRotBillboard(actor, &D_8006E57C);
        } else if ((st->phase >> 1) == 0x30 && !(GetRandomU32() & 3)) {
          SpawnActorRotBillboard(actor, &D_8006E588);
        } else if ((st->phase >> 1) == 0x50 && !(GetRandomU32() & 3)) {
          SpawnActorRotBillboard(actor, &D_8006E594);
        }

        at = &actor->posX;
        if (ApproxDist2D(at, &spy->posX) < 0x800) {
          SubtractVector(v250, at, &spy->posX);
          v250[2] = (v250[2] * 3) >> 2;

          if (VectorLength(v250, 1) < 0x440) {
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
      SubtractVector(v250, &SPY.posX, &actor->posX);
      distance = VectorLength(v250, 0);
      if (distance < st->unk04 && v250[2] >= -0x3FF && v250[2] < st->unk00 &&
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
          actor->rotX = D_8006E638[(0x78 - st->timer) >> 1].a + st->rotX;
          actor->rotY = D_8006E638[(0x78 - st->timer) >> 1].b + st->rotY;
          actor->posZ =
              st->restZ +
              (ABS2((signed char)D_8006E638[(0x78 - st->timer) >> 1].a) +
               ABS2((signed char)D_8006E638[(0x78 - st->timer) >> 1].b)) *
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

    /* Respawn warden (WardenState): puts its group back once Spyro has left. */
    case 323: {
      WardenState *st = actor->state;

      CopyVector(&actor->posX, &SPY.posX);

      if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
        Actor *scan;

        st->timer = st->period;

        for (scan = g_pActorListBase; scan < g_pActorPoolDynBase; scan++) {
          if (scan->type == st->group && scan->mode >= 0x80 &&
              ApproxDist2D(&SPY.posX, scan->state) >= 0x6001) {
            CopyVector(&scan->posX, scan->state);
            InsertActorIntoSpatialGrid(scan);
            scan->flags = 0;
            scan->mode = 0;
            scan->unk3A &= 0x7F;
            ANIM_RESET(scan, 0);
          }
        }
      }

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

          actor->rotX = D_8006E638[st->timer >> 1].a + st->rotX;
          actor->rotY = D_8006E638[st->timer >> 1].b + st->rotY;
          dx = (signed char)D_8006E638[st->timer >> 1].a;
          dy = (signed char)D_8006E638[st->timer >> 1].b;
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

    /* Whirlwind pad: a short spin-up before the ride, then a cooldown. */
    case 331:
      switch (actor->mode) {
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

    /* Type 401: breakable rock. Shakes when struck; a charge shatters it
     * into rubble and dust and removes it. */
    case 401: {
      L31Rock *st = actor->state;

      if (st->shake != 0) {
        st->shake += g_nFrameStep;
        if (st->shake < 0x40) {
          int dx;
          int dy;

          actor->rotX = D_8006E638[st->shake >> 1].a + st->rotX;
          actor->rotY = D_8006E638[st->shake >> 1].b + st->rotY;
          dx = (signed char)D_8006E638[st->shake >> 1].a;
          dy = (signed char)D_8006E638[st->shake >> 1].b;
          if (dx < 0) {
            dx = -dx;
          }
          if (dy < 0) {
            dy = -dy;
          }
          dx += dy;
          actor->posZ = st->restZ + dx * 8;
        } else {
          st->shake = 0;
          actor->rotX = st->rotX;
          actor->rotY = st->rotY;
          actor->posZ = st->restZ;
        }
      }
      if (actor->flags & 0x80000) {
        int i;

        actor->unk55 = 0x20;
        PlayActorMeshVoice(actor, 0, 0);
        for (i = 0; i < 4;) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0x136, actor);
          i++;
          g_pfnLevelSpawnActor(0x137, actor);
        }
        for (i = 0; i < 0xA;) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0x135, actor);
          i++;
        }
        g_pfnLevelEmitParticles(0x20, 0x46, &actor->posX, 0x18);
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        DespawnActorRecord(actor);
      } else {
        st->handle = SelectActorLodBand(actor, st->handle);
        if ((actor->flags & 0x30000) && st->shake == 0) {
          st->shake = 1;
          st->rotX = actor->rotX;
          st->rotY = actor->rotY;
          st->restZ = actor->posZ;
        }
      }
      actor->flags = 0;
      break;
    }

    /* Type 402: cannon. Smokes while it builds up, fires, then flies its
     * ball along a path to the target, waking the linked actor. */
    case 402: {
      L25Shot *st = actor->state;

      if (st->init == 1) {
        int pitch;

        SubtractVector(s496, PATH_PT(st->path, 1), PATH_PT(st->path, 0));
        pitch = ArcTan2(VectorLength(s496, 0), s496[2], 0);
        actor->rotY = pitch - 0x40;
        if (((unsigned int)(pitch + 0x40) & 0xFF) < 0x60) {
          actor->rotY = 0xE0;
        }
        actor->rotZ = ArcTan2(s496[0], s496[1], 0);
        RefreshActorDerivedState(actor, 4);
        st->init = 2;
      }

      switch (actor->subMode) {
      case 0:
        if (st->link >= 0 && g_pActorListBase[st->link].mode >= 0x80) {
          DespawnActorRecord(actor);
          continue;
        }
        if (!(actor->flags & 0x10000)) {
          continue;
        }
        actor->subMode = 1;
        actor->updateDist = 0xFF;
        actor->unk50 = 0x7F;
        continue;

      case 1:
        st->timer += g_nFrameStep;
        if (st->timer < 0x5A) {
          s512[0] = 0;
          s512[1] = 0;
          s512[2] = 0x14C;
          RotateVectorByMatrix(actor->mtx, s512, s512);
          AddVector(s512, s512, &actor->posX);
          s544[0] = 0;
          s544[1] = 6;
          s544[2] = -0xC;
          RotateVectorByMatrix(actor->mtx, s544, s544);
          s544[0] += (GetRandomU32() & 0xF) - 8;
          s544[1] += (GetRandomU32() & 0xF) - 8;
          g_pfnLevelEmitParticles(1, 0x4A, s512, s544);
          if (actor->sfxSlot != 0x7F) {
            continue;
          }
          PlayActorMeshVoice(actor, 0, 0);
          continue;
        }
        if (st->timer < 0xB4 &&
            (st->timer & 3) <= ((st->timer + g_nFrameStep) & 3)) {
          s512[0] = 0;
          s512[1] = 0;
          s512[2] = 0x14C;
          RotateVectorByMatrix(actor->mtx, s512, s512);
          AddVector(s512, s512, &actor->posX);
          s544[0] = 0;
          s544[1] = ((st->timer - 0x5A) >> 3) + 9;
          s544[2] = -0x12 - ((st->timer - 0x5A) >> 2);
          RotateVectorByMatrix(actor->mtx, s544, s544);
          g_pfnLevelEmitParticles(1, 0, s512, s544);
          if (actor->rotX < 0x80) {
            actor->rotX = -((st->timer - 0x5A) >> 5);
          } else {
            actor->rotX = (st->timer - 0x5A) >> 5;
          }
          continue;
        }
        if (st->timer <= 0xB4) {
          continue;
        }
        actor->subMode = 2;
        actor->unk50 = 0x40;
        actor->updateDist = 0x40;
        st->path[1] = 1;
        st->timer = 0;
        ZeroVector(st->vel);
        PlayActorMeshVoice(actor, 1, 0);
        continue;

      case 2:
        if (st->timer == 4) {
          SubtractVector(s560, PATH_PT(st->path, st->path[1]), &actor->posX);
          CopyVector(st->vel, s560);
          st->path[1]++;
          if (st->path[1] == st->path[0]) {
            Actor *target = &g_pActorListBase[st->link];

            if (st->link > 0) {
              target->updateDist = 0xFF;
              target->flags |= 0x80000;
            }
            DespawnActorRecord(actor);
          }
          st->timer = 0;
        } else {
          if (st->timer == 3) {
            SubtractVector(s560, PATH_PT(st->path, st->path[1]), &actor->posX);
            RShiftVector3(s560, 1);
          } else {
            SubtractVector(s560, PATH_PT(st->path, st->path[1]),
                          (int *)(st->path + ((st->path[1] << 4) - 8)));
            ScaleVector3Sat(s560, s560, g_anSineLut[st->timer * 2 - 6]);
            ScaleVector3Sat(s576, st->vel, g_anSineLut[st->timer * 2 - 5]);
            AddVector(s560, s560, s576);
            RShiftVector3(s560, 0xA);
          }
          st->timer++;
        }
        VectorLength(s560, 1);
        actor->rotZ = ArcTan2_8bit(s560[0], s560[1]);
        actor->rotY = -ArcTan2_8bit(s560[2], VectorLength(s560, 0));
        AddVector(&actor->posX, &actor->posX, s560);
        ZeroVector(s560);
        g_pfnLevelEmitParticles(1, 0, &actor->posX, s560);
        continue;
      }
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
          actor->rotX = D_8006E638[LIFT_STEP(st)].a + st->unk08;
          actor->rotY = D_8006E638[LIFT_STEP(st)].b + st->unk0C;
          a = (signed char)D_8006E638[LIFT_STEP(st)].a;
          b = (signed char)D_8006E638[LIFT_STEP(st)].b;
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
        ((unsigned char *)rider->state)[0xE] = one;
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
        s576[0] = ((short)g_anCosineLut[i * 32] >> 8) * 3;
        s576[1] = ((short)g_anSineLut[i * 32] >> 8) * 3;
        s576[2] = 0x18;
        g_pfnLevelEmitParticles(1, 0, &actor->posX, s576);
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
        if (ApproxDist2D(&st->parent->posX, &SPY.posX) < 0x400) {
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
          bearing = ArcTan2(SPY.posX - st->parent->posX,
                                  SPY.posY - st->parent->posY, 0);
          s576[0] = (g_anCosineLut[bearing] * 3) >> 4;
          s576[1] = (g_anSineLut[bearing] * 3) >> 4;
          s576[2] = 0;
          v496[0] = (unsigned short)g_anCosineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          v496[1] = (unsigned short)g_anSineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          v496[2] = 0;
          bearing = (bearing + 0x80) & 0xFF;
          actor->rotZ = bearing + ((g_anCosineLut[actor->subMode] * 3) >> 9);
          ScaleVector3Sat(v528, v496, st->radius - 1);
          LShiftVector3(v496, 1);
          arm = st->radius - 1;
          v528[2] += (g_anCosineLut[arm * 2] * 3) >> 3;
          v544[0] = s576[0] * g_anCosineLut[arm * 2];
          v544[1] = s576[1] * g_anCosineLut[arm * 2];
          v544[2] = 0;
          bearing = arm << 1;
          bearing = (bearing - (st->phase * 4)) & 0xFF;
          ScaleVector3Sat(v496, v496, st->phase);
          SubtractVector(v528, v528, v496);
          swing = &g_anCosineLut[bearing];
          actor->posX = s576[0] * *swing;
          actor->posY = s576[1] * *swing;
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, v544);
          RShiftVector3(&actor->posX, 0xA);
          AddVector(&actor->posX, &actor->posX, s576);
          AddVector(&actor->posX, &actor->posX, &st->parent->posX);
          SubtractVector(&actor->posX, &actor->posX, v528);
          actor->posZ = actor->posZ + ((*swing * 3) >> 3) + 0x600;
        }
      } else {
        unsigned char now = actor->subMode + 8;

        actor->subMode = now;
        actor->rotZ = actor->floorOffset + ((g_anCosineLut[now] * 3) >> 9);
      }

      break;
    }
}
  }
}
