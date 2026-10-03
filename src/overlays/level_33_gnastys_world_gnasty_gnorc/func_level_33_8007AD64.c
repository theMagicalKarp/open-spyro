/* func_level_33_8007AD64 -- per-frame actor update for level 33 (Gnasty
 * Gnorc). 0x8007AD64, 33,216 bytes.
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class. Most
 * classes are shared with levels 0/7/9/13/16/25/31/34; the level-33 classes
 * are Gnasty Gnorc himself (161), his trail bombs (169) and their shockwave
 * (143), the lane patrollers (159), a counter (168) and a follower (414).
 *
 * Load-bearing source forms:
 *   - Carriers as in level 25 (`oneq`, `one`) but neither is used: every
 *     constant 1 is a literal, so the SImode 1 group is hoisted into fp and
 *     the byte stores stay inline. `spyc` (case 181's headLook) is hoisted
 *     too. Case 21's pose 3 is ENTER_POSE_B (A285) and case 169's posY sine
 *     read uses the g_anSineLut alias, which keeps 3 and D_8006CBF8 out of
 *     loop.c's hoist set, as in the original.
 *   - `base = (char *)D_80078BBC` before the loop: the balloon-stage address
 *     is a spilled equivalence rematerialised at its two uses, while the
 *     stage itself is read through the scalar g_nLevelReadyFlag view.
 *   - Gnasty's camera blocks read the anchor path through a scalar view of
 *     the state block so the loads stay behind the scalar flag stores
 *     (A297/A302); state 8 writes the C4C flag word as D_80078C00[19] so
 *     only the D_8007570C store sinks to the jal.
 *
 * ==== Core records ====
 *
 * Actor record (0x58 bytes), shared by every actor class. ->type picks the
 * behaviour, ->state points at the class's own block, and 0x3C..0x42 hold
 * the animation state driven by the macros further down. */
typedef struct Actor {
  void *state;         /* 0x00 class-specific state block */
  int unk04;           /* 0x04 */
  int unk08;           /* 0x08 collision group */
  int posX;            /* 0x0C position */
  int posY;            /* 0x10 */
  int posZ;            /* 0x14 */
  int flags;           /* 0x18 damage flags */
  int unk1C;           /* 0x1C shadow distance */
  int unk20[5];        /* 0x20 rotation matrix */
  short unk34;         /* 0x34 collision region */
  short type;          /* 0x36 actor class */
  short unk38;         /* 0x38 floor distance */
  unsigned char unk3A; /* 0x3A dropped flag */
  unsigned char unk3B; /* 0x3B */
  unsigned char unk3C; /* 0x3C current animation */
  unsigned char unk3D; /* 0x3D next animation */
  unsigned char unk3E; /* 0x3E current frame */
  unsigned char unk3F; /* 0x3F next frame */
  unsigned char unk40; /* 0x40 frame progress */
  unsigned char unk41; /* 0x41 progress per frame */
  unsigned char unk42; /* 0x42 anim flags: bit 1 done, bit 0 new frame */
  unsigned char unk43; /* 0x43 pod (actor group) */
  unsigned char unk44; /* 0x44 rotation x */
  unsigned char unk45; /* 0x45 rotation y */
  unsigned char unk46; /* 0x46 rotation z */
  unsigned char unk47; /* 0x47 depth offset */
  unsigned char unk48; /* 0x48 state; >= 0x80 means dormant */
  unsigned char unk49; /* 0x49 sub-state */
  unsigned char unk4A; /* 0x4A sector index */
  unsigned char unk4B; /* 0x4B renderer */
  unsigned char unk4C; /* 0x4C */
  unsigned char unk4D;
  unsigned char unk4E;
  unsigned char unk4F;
  unsigned char unk50; /* 0x50 render radius */
  unsigned char unk51; /* 0x51 was drawn last frame */
  unsigned char unk52; /* 0x52 update distance */
  unsigned char unk53;
  unsigned char unk54; /* 0x54 sound channel */
  unsigned char unk55; /* 0x55 sound distance */
  unsigned char unk56; /* 0x56 actor index */
  unsigned char unk57; /* 0x57 scale override */
} Actor;               /* 0x58 */

/* The update list, and the per-actor values the loop publishes before it
 * dispatches. */
extern Actor *D_800700F4[]; /* live actor list, 0-terminated */
extern int D_800756CC;      /* frame delta in ticks */
extern int D_800756C4;      /* frame delta, as seen by the callees */
extern int D_80075794;      /* current actor's animation finished */
extern int D_800757F4;      /* current actor's new-frame flag */

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

extern Model *D_80076378[]; /* per-type model table, indexed by ->type */
extern Actor *D_80075898;   /* Sparx, or null when absent */

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
  int unk14;           /* 0x14 4th argument to func_80017D7C */
  int vel[3];          /* 0x18 current leg velocity, written by func_80017D7C */
  int count;           /* 0x24 frames left on the current leg */
} RoamState;

extern int func_80017D7C(int *from, int *to, int *vel, int a);

/* Progress-per-tick byte of animation `anim` in the actor's model. */
#define TYPE_DESC(a, anim)                                                     \
  (D_80076378[(a)->type]->m_Animations[(anim)]->m_ProgressPerTick)

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
  int idx;   /* 0x04 index into the actor pool at D_80075828 */
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
extern void func_80051FEC(void);
extern void func_800522C0(Actor **list, int mode);
extern void func_80038458(Actor *actor);
extern Actor *func_8003ABC0(Actor *actor, int a, int b, void *c);
extern void func_8003B7C0(Actor *actor);
extern void func_80037E98(Actor *actor);
extern int func_80037EA0(int lo, int hi); /* random in [lo, hi] */
extern int func_80037F10(int a, int b);

/* Step a countdown timer; non-zero when it fires. */
extern int func_80037F90(void *timer, int step);
extern int func_80038074(int a, int b);
extern int func_800381BC(int a, int b); /* signed angle delta */
extern int func_80038EE0(Actor *actor, int heading, int a, int b, int c);
extern int func_80039398(Actor *actor, int a, int b, int c, int d);
extern int func_80039688(Actor *actor, int a, int b, int c, int d, int e);
extern void func_800529E4(Actor *actor, int mode);
extern void func_80052568(Actor *actor);
extern unsigned int func_8006272C(void); /* GetRandomU32 */

/* Particle spawn hook. The last argument is either a packed parameter word
 * or a pointer to a velocity vector, depending on the effect. */
extern void (*D_800758E4)(int a, int effect, int *pos, void *c);

/* Actor spawn hook: (type, parent) -> new actor. */
extern Actor *(*D_800758CC)(int type, Actor *parent);

/* Level actor pool, switch flags, and the sound tables and override
 * globals used when a behaviour plays a sound. */
extern Actor *D_80075828;         /* level actor pool */
extern Actor *D_80075890;         /* end of the level actor pool */
extern int D_80077AE8;            /* bit 0: switch sounds enabled */
extern unsigned char *D_800761D0; /* sound definitions, 20 bytes each */
extern unsigned char *D_800761D4; /* sound id table */
extern int D_800761DC;
extern struct {
  short v;
} D_800761F4;
extern unsigned short D_80073094[]; /* indexed by the switch's bit position */

/* Egg sequence flags, then the vector library: copy, add, subtract, scale,
 * shift, sine and cosine. */
extern int D_8007570C;
extern struct {
  int v;
} D_80077FDC;
extern int D_80077378; /* pad buttons pressed; 0x40 skips the egg */
extern int D_80075964;
extern int D_80076FE8[];
extern int D_80075810;

extern void func_8003C85C(int *pos);
extern int func_800169AC(int dx, int dy);
extern int func_80016C58(int a);           /* LookupSine */
extern int func_80016CB0(int a);           /* LookupCosine */
extern void func_80017700(int *d, int *s); /* CopyVector */
extern void func_80017110(int *d, int *s);
extern void func_80017758(int *d, int *a, int *b); /* AddVector */
extern void func_8001778C(int *d, int *a, int *b); /* SubVector */
extern void func_800176C8(int *d, int n);          /* RShiftVector3 */
extern void func_800176F0(int *v);
extern void func_800177C0(int *d, int *s, int k); /* ScaleVector3Sat */
extern void func_80017BFC(short *d, int *s);
extern void func_80017C24(int *d, int *s);

/* Mark an item collected; the second argument is the actor or its id. */
extern void func_8003B854(int a, void *b);

extern void func_80055A78(int a, Actor *actor, int b, void *c);
extern void func_8003851C(Actor *actor, int a, int b);
extern int func_80038340(Actor *actor); /* ground height under the actor */
extern int func_80038638(Actor *actor, int *target, int a, int heading, int b,
                          int c, int d, int e, int f, int g, int h, int i,
                          int j);
extern int func_80017990(int *pos, int *target); /* planar distance */
extern int func_80016AB4(int dx, int dy, int z); /* Atan2 */
extern int func_80017908(int a, int b);          /* AbsAngleDelta8 */
extern void func_8003C358(Actor *actor, int mode);

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

extern void func_8002B390(int handle, int a, int b);
extern void func_8002B444(int handle, int a, int b);
extern int func_8002B3F4(int handle); /* bits 8..15 are the anim marker */
extern void func_800562A4(Actor *actor, int mode);

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
extern int D_80077368;
extern int D_8007736C;
extern int D_80077370;

/* Single-field views for reading a scalar symbol through a struct type. */
typedef struct {
  int v;
} I32;
#define AS_I32(sym) (((I32 *)&(sym))->v)

typedef struct {
  unsigned char *v;
} PtrU8;
#define AS_PTRU8(sym) (((PtrU8 *)&(sym))->v)

extern int func_8004BE4C(int *pos, int a, int b); /* ground/wall probe */
extern void func_80017330(int *v, int k);         /* normalise to k */
extern void func_800175B8(int *v, int k, int n);

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

extern int D_80075830; /* key collected */

/* Type 187: talking dragon. Dragon count, dialogue selection, and the latch
 * in the save-options block. */
extern int D_80075750;
extern unsigned char D_800758D0[]; /* save-options block; [1] is the latch */
extern int D_800777F4;
extern int D_800777F8;
extern void func_8003DFA4(void);
extern void (*D_800757A0)(Actor *actor);

/* Dynamic actor pool counters; the difference is the free slot count. */
extern int D_800756A4;
extern int D_800756A8;

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
  int unk20;    /* 0x20 index into D_80075828, -1 for none */
  int unk24;    /* 0x24 script id handed to func_8002C914, -1 for none */
  int unk28[7]; /* 0x28 */
  int unk44;    /* 0x44 phase counter, 0 .. 0x100 */
  int unk48;    /* 0x48 saved actor->unk44 */
  int unk4C;    /* 0x4C saved actor->unk45 */
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
  Actor *unk00; /* 0x00 the spawned passenger, valid once ->unk48 is 1 */
  int unk04;    /* 0x04 phase */
  int unk08;    /* 0x08 saved actor->unk44 */
  int unk0C;    /* 0x0C saved actor->unk45 */
  int unk10;    /* 0x10 saved actor->posZ */
  int unk14;    /* 0x14 mode; 1 selects the extra tick and the rearm */
} LiftState;

#define LIFT_STEP(st) (((st)->unk04 - 0x1A0) >> 1)

/* Type 398: exit vortex. Surface-below flags and the level transition
 * globals. */
extern int D_80075718;             /* index of the surface under the probe */
extern unsigned char **D_800785B8; /* surface records, indexed by the above */
extern int D_80075864;
extern int D_800756AC;
extern int D_800756B0;
extern int D_8007579C;
extern int D_800757D8;
extern int D_80075858;
extern int D_800758FC;
extern int D_80075910;

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

extern void func_80033F08(int *out);
extern void func_80034204(int *out);
extern void func_800342F8(void);

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

extern int D_800772D8[]; /* per-D_80075964 completion counters */

/* Type 300: camera trigger volume. Live camera sphere and its template. */
typedef struct Xform6 {
  int w[6];
} Xform6;
extern Xform6 D_80078668; /* live camera transform */
extern Xform6 D_8006C934; /* template it is seeded from */

/* Type 300 state block: activation box, mode flags, timer, linked actor. */
typedef struct TriggerState {
  int unk00; /* 0x00 vertical span; also the offset added to the target Z */
  int unk04; /* 0x04 horizontal radius */
  int flags; /* 0x08 bit0 style, bit1 armed, bit2 held, bit3 owner-gated,
              * bit4 also stamps actor->unk49 */
  int timer; /* 0x0C */
  int idx;   /* 0x10 index into the actor pool at D_80075828 */
} TriggerState;

/* Type 323: respawner. Revives dormant actors of one class out of view. */
typedef struct RespawnState {
  int type;   /* 0x00 the actor type this manager owns */
  int timer;  /* 0x04 */
  int period; /* 0x08 reload for ->timer */
} RespawnState;

/* Actor reset, dialogue, cutscene and checkpoint callees. */
extern void func_800526A8(Actor *actor);

extern void func_8002C914(int script, int a);
extern void func_8002C924(Actor *actor);
extern void func_80059474(Actor *actor, int heading);

extern void func_8003C6E4(Actor *actor); /* the whole of case 251 */

/* Collision contact point, pickup counters and sound volume override. */
extern int D_80076B80;   /* collision contact point */
extern int D_80076B84;   /* ..y */
extern int D_80076B88;   /* ..z */
extern int D_80075808;   /* collision triangle index */
extern int D_8007582C;   /* life count, capped at 99 */
extern int D_800758E8;   /* life orb count */
extern short D_800761E8; /* sound volume override, left/right */
extern short D_800761EA;

/* 256-entry sine and cosine tables. */
extern short D_8006CBF8[];
extern short D_8006CC78[];

/* Callees used by the collectible and fairy behaviours. */
extern void func_80017048(int *dst, int *tbl, int *out);
extern int func_8003AAEC(Actor *actor, int *tbl);
extern void func_800533D0(Actor *actor);
extern void func_80017CB8(int a, int *out);
extern int func_8004D5EC(int *v, int k);
extern int func_800171FC(int *v, int mode);
extern int func_80017A38(int x); /* SquareRoot */
extern int func_80057380(void);
extern void func_8003B9D4(Actor *actor);
extern int func_80033E40(int *a, int *b);
extern int func_80056DC4(Actor *actor, int a);
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

extern int func_80017948(int a, int b); /* signed angle delta, byte-wrapped */
extern void func_8002CCC8(Actor *actor);

/* Type 114: path-following enemy that runs from Spyro and can be knocked
 * back. */
typedef struct WalkerState {
  PathData *path; /* 0x00 */
  int timer;      /* 0x04 */
  int tgtIdx;     /* 0x08 index into D_80075828, -1 when unclaimed */
  int heading;    /* 0x0C */
  int unk10;      /* 0x10 passed by address to func_80039910 */
  int unk14;      /* 0x14 likewise */
} WalkerState;

extern int func_80038DC0(Actor *actor, int a, int b, int c);
extern void func_800385BC(Actor *actor, int a);
extern int func_80038178(int ang, int facing, int a, int b);
extern int func_80039910(Actor *actor, int *a, int heading, int *b, int c,
                         int d);

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

extern void func_800170C0(int *d, int *s);
extern FxNode *func_80058AE8(void);
extern void func_80058B60(FxNode *node);
extern int func_80038098(int a, int b, int c);

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

extern int D_8007572C; /* frame counter; the arm acts on every 4th frame */
extern int func_8004AE38(int *a, int *b);

/* Type 350: sequenced prop driven by the 0x8002Bxxx player, unlocked once. */
typedef struct PropAnimState {
  int anim;   /* 0x00 handle for func_8002B390 / B3F4 / B444 */
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
extern void func_800176A0(int *d, int n);

/* Types 257/425: falling chest fragments. Integrate until life, visibility
 * or the floor runs out. */
typedef struct TumbleState {
  short vel[3];  /* 0x00 */
  short spin[3]; /* 0x06 per-frame delta for ->unk44/45/46 */
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
#define ANIM_RESET(a, anim)                                                    \
  (a)->unk40 = 0;                                                              \
  (a)->unk41 = TYPE_DESC(a, anim);                                             \
  (a)->unk3C = (anim);                                                         \
  (a)->unk3D = (anim);                                                         \
  (a)->unk3E = 0;                                                              \
  (a)->unk3F = 1;

/* ANIM_ADVANCE promotes the queued animation and queues `next` after it. */
#define ANIM_ADVANCE(a, next)                                                  \
  (a)->unk40 = sixteen;                                                        \
  (a)->unk41 = sixteen;                                                        \
  (a)->unk3C = (a)->unk3D;                                                     \
  (a)->unk3D = (next);                                                         \
  (a)->unk3E = (a)->unk3F;                                                     \
  (a)->unk3F = 0;                                                              \
  func_80037E98(a);

/* ENTER_POSE sets the state byte to `n`, advances to animation `n` unless it
 * is already queued, and moves on to the next actor. It ends in a bare
 * `continue`, so use it only inside braces. */
#define ENTER_POSE(a, n)                                                       \
  (a)->unk48 = (n);                                                            \
  if ((a)->unk3D != (n)) {                                                     \
    (a)->unk40 = sixteen;                                                      \
    (a)->unk41 = sixteen;                                                      \
    (a)->unk3C = (a)->unk3D;                                                   \
    (a)->unk3D = (n);                                                          \
    (a)->unk3E = (a)->unk3F;                                                   \
    (a)->unk3F = 0;                                                            \
    func_80037E98(a);                                                          \
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
  int parent;   /* 0x00 parent actor index into D_80075828 */
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
 * -back state machine driven by ->unk49. */
typedef struct EnemyState {
  int vel[3];           /* 0x00 velocity, also read as a vector */
  short node;           /* 0x0C current path node */
  unsigned char mode;   /* 0x0E sub-mode within ->unk49 */
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
extern int D_80075860;           /* gem total shown on the balloon */
extern unsigned char D_800758D1; /* per-world "seen it" flags */
extern unsigned char D_800758D2;
extern unsigned char D_800758D3;
extern unsigned char D_800758D4;
extern unsigned char D_800758D5;
extern int D_80077350;

extern short D_80076E20; /* camera yaw, as the balloonist reads it */
extern int D_80076E28;   /* camera mode */
extern int D_80076E48;   /* camera spherical target */
extern int D_80076E60;   /* camera spherical current */
extern int D_80076E90;
extern Xform *D_80076EA8; /* active camera preset, or null */

extern volatile unsigned char D_80078A7F;
extern int D_80078A8C[]; /* wind basis matrix */
extern int D_80078AD0;   /* Spyro's state */
extern int D_80078AD4;
extern int D_80078AD8;
extern int D_80078AF4;
extern int D_80078B64; /* look-at offset */
extern int D_80078B68;
extern int D_80078B6C;
extern int D_80078B70;
extern int D_80078B74; /* camera yaw accumulator */
extern int D_80078A5C;
extern volatile unsigned char
    D_80078A66;          /* SPY.bodyRotZ, absolute form (A240) */
extern int D_80078BBC[]; /* balloon sequence stage (== SPY.u164) */
extern int D_80078BFC;

extern int D_80078C4C;         /* scripted camera control word */
extern signed char D_80078C5E; /* destination level */
extern struct {
  int x;
  int y;
} D_80078C60; /* scripted camera look-at */
#define D_80078C64 D_80078C60.y
extern int D_80078C68;
extern int D_80078C70; /* scripted camera mode */
extern PortalDesc *volatile D_80078C98;
extern int D_80078C9C;
extern int D_80078CA0;

extern unsigned char D_80078E98; /* ambience channel enables */
extern unsigned char D_80078E99;
extern unsigned char D_80078E9A;
extern unsigned char D_8007A6C7;
extern unsigned char D_8007A6C8;

extern void func_8002B390(int ch, int vol, int a);
extern void func_8002B444(int ch, int n, int a);
extern int func_8002B3F4(int ch);
extern int func_80017D7C(int *from, int *to, int *step, int speed);
extern void func_80039AA8(Actor *actor, void *st);
extern int func_800381BC(int a, int b);

/* ==== Globals shared with level 4 ==== */
extern short D_80076E1E; /* camera yaw */
extern unsigned char D_80077FA8;
extern int D_80078A84; /* SPY.damageFlags, absolute form */
extern int D_80078C58;
extern Actor *D_80078C7C;
extern unsigned char D_8007A6A8[]; /* per-level "left through the exit" flags */
extern int D_8007576C;
extern int D_800756D0;
extern int D_800758AC;
extern int D_800758B4; /* current level id */
extern int D_8007596C;
extern int D_80078CA4;
extern int D_80078768;
extern int D_80078A60; /* SPY.posZ, absolute form */
extern void func_80054578(void);
extern void func_8004AC24(int a0);

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
  int partner;     /* 0x20 index into D_80075828 */
  int alarm;       /* 0x24 */
  int drop;        /* 0x28 index into D_80075828, -1 for none */
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

extern int func_8003B0DC(int stage);
extern void func_8003B728(Actor *actor, int a);
extern int func_80039E94(Actor *actor, void *path, int a, int b, int c, int d,
                          int e, int f, int g);

/* An animation change that also clears the finished flag. */
#define ANIM_GO(a, n)                                                          \
  if ((a)->unk3D != (n)) {                                                     \
    D_80075794 = 0;                                                            \
    ANIM_ADVANCE(a, n);                                                        \
  }
#define ANIM_SET(a, n)                                                         \
  if ((a)->unk3C != (n)) {                                                     \
    D_80075794 = 0;                                                            \
    ANIM_RESET(a, n);                                                          \
  }


/* ENTER_POSE with the already-there test first. */
#define ENTER_POSE4(a, n)                                                      \
  (a)->unk48 = (n);                                                            \
  if ((a)->unk3D == (n)) {                                                     \
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
extern int func_8004E2E8(int *pos, int a, int b);
extern int func_8003BCCC(Actor *actor, int a, int b, int c, int d);

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
  int partner;    /* 0x18 index into D_80075828 */
  int heading;    /* 0x1C */
  int speed;      /* 0x20 knock-back speed */
  int knock;      /* 0x24 knock-back heading */
} L16Guard;
extern void func_80056200(int ch, int a);

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
extern int func_8004E3C8(int *pos, int a, int b, int c, Actor *actor, int d);

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
  int target; /* 0x30 index into D_80075828 */
} L16Bell;
extern int D_8007572C;

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
  int prev;           /* 0x64 index into D_80075828 of the previous stage's gate */
  int wait;           /* 0x68 */
  char pad6C[0x8];
  int unk74;          /* 0x74 */
} L16Boss;
extern int D_800778E8[]; /* hits taken */
extern int D_800778EC[]; /* stage reached */
extern int func_80038250(int *pos);
extern int func_80038C4C(int *a, int *b);
extern int func_8003BFC0(Actor *actor, PathData *path, int *a, int *b, int c,
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
  int partner; /* 0x10 index into D_80075828, -1 for none */
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
  int partner;    /* 0x28 index into D_80075828 */
} PostState;
extern void func_8003AA84(Actor *actor);
extern void func_8003B1E8(Actor *actor, int a);
/* An animation change at half speed, keyed on the current animation. */
#define ANIM_BLEND(a, n)                                                       \
  if ((a)->unk3C != (n)) {                                                     \
    (a)->unk40 = 8;                                                            \
    (a)->unk41 = 8;                                                            \
    (a)->unk3C = (a)->unk3D;                                                   \
    (a)->unk3D = (n);                                                          \
    (a)->unk3E = (a)->unk3F;                                                   \
    (a)->unk3F = 0;                                                            \
    func_80037E98(a);                                                          \
  }
/* Case 166 -- path patroller. */
typedef struct PatrolState {
  PostPath *path; /* 0x00 */
  int mode;       /* 0x04 */
  int flag;       /* 0x08 */
  int near;       /* 0x0C */
  int timer;      /* 0x10 */
} PatrolState;
extern int func_80038A40(Actor *actor, PostPath *path, int *idx);
extern int D_80075904;
/* Case 174 -- the cage that carries Spyro. */
typedef struct CageState {
  int idx;    /* 0x00 index of its flyer in D_80075828 */
  int shake;  /* 0x04 shake timer, then ride timer */
  int rotX;   /* 0x08 rest pose */
  int rotY;   /* 0x0C */
  int restZ;  /* 0x10 */
  int handle; /* 0x14 for func_8003A9EC */
} CageState;
extern Actor *D_80075758;
extern int D_80078C00[];
extern int D_80078C04;
extern void func_800177F8(int *d, int *s, int k);
extern int func_8003A9EC(Actor *actor, int handle);
/* ENTER_POSE with an empty loop at the head of the change: cse starts a new
 * block there, so the `unk3D = n` store takes its own copy of the constant
 * (merged again after loop.c). Keeps the constant's lifetime short enough
 * that loop.c does not hoist it (see case 115). */
#define ENTER_POSE_B(a, n)                                                     \
  (a)->unk48 = (n);                                                            \
  if ((a)->unk3D != (n)) {                                                     \
    do {                                                                       \
    } while (0);                                                               \
    (a)->unk40 = sixteen;                                                      \
    (a)->unk41 = sixteen;                                                      \
    (a)->unk3C = (a)->unk3D;                                                   \
    (a)->unk3D = (n);                                                          \
    (a)->unk3E = (a)->unk3F;                                                   \
    (a)->unk3F = 0;                                                            \
    func_80037E98(a);                                                          \
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
extern int func_80039228(Actor *actor, int vx, int vy, int vz, int a, int b,
                         int c);

/* Case 213 -- circling bird. */
typedef struct L9Bird {
  int home[3]; /* 0x00 perch */
  int timer;   /* 0x0C */
  int radius;  /* 0x10 */
  int angle;   /* 0x14 */
  int height;  /* 0x18 */
  int call;    /* 0x1C */
} L9Bird;
extern int func_8003BAD0(Actor *actor, int *target, int a, int b, int c,
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
extern void func_8003AA84(Actor *actor);

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
extern void func_80052D64(Actor *actor, int a, int *pos);



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
  int partner; /* 0x00 index into D_80075828 */
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
  int partner;        /* 0x00 index into D_80075828 */
  unsigned int phase; /* 0x04 */
  int unk08;
  int unk0C;
  int baseZ;          /* 0x10 */
  int big;            /* 0x14 */
} L26Bob;

/* Case 298 -- bouncer riding its partner. */
typedef struct L26Rider {
  int partner; /* 0x00 index into D_80075828 */
  int speed;   /* 0x04 */
  int lift;    /* 0x08 */
  int heading; /* 0x0C */
} L26Rider;

/* Case 88 -- escort. */
typedef struct L26Escort {
  int leader;     /* 0x00 index into D_80075828 */
  int partner;    /* 0x04 index into D_80075828, -1 = none */
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
  int handle; /* 0x10 for func_8003A9EC */
} L26Spinner;
extern int D_80075F38[][7]; /* voice table */
extern int D_80075F44[][7]; /* its word 3 */

/* Case 392 -- homing spark. */
typedef struct L26Spark {
  int vel[3]; /* 0x00 */
  int timer;  /* 0x0C */
} L26Spark;
extern int func_80017928(int a, int b);

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
extern void func_8003C994(Actor *actor, void *path, int a, int b, int c, int d,
                          int e);

typedef struct L26Rot {
  unsigned char r[3];
} L26Rot;


#define sixteen 0x10

/* ==== Classes shared with level 31 ==== */

/* Set the state byte and queue animation `n` without promoting the current
 * one, then move on to the next actor. */
#define ANIM_QUEUE(a, n)                                                       \
  (a)->unk48 = (n);                                                            \
  if ((a)->unk3D != (n)) {                                                     \
    (a)->unk40 = sixteen;                                                      \
    (a)->unk41 = sixteen;                                                      \
    (a)->unk3D = (n);                                                          \
    (a)->unk3F = 0;                                                            \
    func_80037E98(a);                                                          \
  }                                                                            \
  continue



/* Case 401 -- breakable rock. */
typedef struct L31Rock {
  int shake;  /* 0x00 shake timer, 0 = still */
  int rotX;   /* 0x04 rest rotation */
  int rotY;   /* 0x08 */
  int restZ;  /* 0x0C */
  int handle; /* 0x10 for func_8003A9EC */
} L31Rock;

/* ==== Level 25 classes ==== */

extern int D_80075814; /* g_nSpyroDrawSuppressed */

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
  int link;      /* 0x24 index of its partner in D_80075828, -1 = none */
  int fleeDir;   /* 0x28 */
  int fleeSpeed; /* 0x2C */
  int fleeStep;  /* 0x30 */
} L25Grabber;

/* Clear the partner's "held" bit. */
#define L25_UNLINK(st)                                                         \
  if ((st)->link >= 0) {                                                       \
    D_80075828[(st)->link].unk49 &= 0xFB;                                      \
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
  int link;  /* 0x0C index of its partner in D_80075828, -1 = none */
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

/* from level_34 */
extern int D_80078A58; /* Spyro, as a plain symbol (the loop's spy carrier) */
/* from level_34 */
typedef struct {
  int x;
  int y;
  int z;
} CamLook;
/* from level_34 */
#define CAM_LOOK (*(CamLook *)&D_80078C60)
/* from level_34 */
extern void *D_80078C78; /* SPY.pCamTarget, absolute form */
/* from level_34 */
extern int *D_80078C74;  /* SPY.pCamAnchor, absolute form */
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
/* from level_34 */
extern int func_80038BB0(PathData *path);
/* from level_34 */
extern int func_80038400(Actor *actor, int a);
/* from level_34 */
typedef struct {
  void *v;
} PtrV;
/* from level_34 */
#define AS_PTRV(sym) (((PtrV *)&(sym))->v)
/* from level_34 */
typedef struct WispState {
  Actor *target; /* 0x00 actor to reach, or null to chase Spyro */
  int timer;     /* 0x04 lifetime */
} WispState;
/* from level_34 */
typedef struct TetherState {
  int timer;   /* 0x00 */
  int speed;   /* 0x04 knockback speed */
  int heading; /* 0x08 knockback heading */
  int link;    /* 0x0C walker to follow (index into D_80075828), or -1 */
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
  int link;    /* 0x0C linked guard (index into D_80075828), or -1 */
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
extern int func_8003A420(Actor *actor, PathData *path, int a, int b, int c,
                         int d, int e, int f);
/* from level_34 */
extern int func_8003A16C(Actor *actor, PathData *path, int a, int b, int c,
                         int d, int e, int *f);
/* from level_34 */
extern void func_80016D2C(void *a, int *b, int c);
/* from level_34 */
#define FACE4(a, h, s, t)                                                      \
  ((int (*)(Actor *, int, int, int))func_80038EE0)(a, h, s, t)
/* from level_34 */
#define RESET_IDLE(a)                                                          \
  (a)->unk48 = 0;                                                              \
  ANIM_RESET(a, 0);                                                            \
  continue
/* from level_34 */
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
/* from level_34 */
typedef struct CatapultState {
  int angle;     /* 0x00 0..0x120 */
  int vel;       /* 0x04 */
  int link;      /* 0x08 actor to fling (index into D_80075828), or -1 */
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
typedef struct {
  char pad00[0x38];
  int posX; /* 0x38 */
  int posY; /* 0x3C */
} GuardTarget;
/* from level_34 */
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
  int watch;          /* 0x28 index into D_80075828 */
  int guard;          /* 0x2C index into D_80075828 */
  int done;           /* 0x30 */
  GuardTarget *target; /* 0x34 */
  int timer38;        /* 0x38 */
  unsigned char *path; /* 0x3C */
  int boxA[6];        /* 0x40 */
  int boxB[6];        /* 0x58 */
} GuardState370;
/* from level_34 */
extern void func_8003B47C(Actor *actor, int a, int b);
/* from level_34 */
extern int func_8003838C(Actor *actor);
/* from level_34 */
typedef struct LookoutState {
  int heading;   /* 0x00 knock-back heading */
  int speed;     /* 0x04 knock-back speed */
  int timer;     /* 0x08 swipe cooldown */
  int count;     /* 0x0C idle loops left */
  int home;      /* 0x10 rest heading */
  int init;      /* 0x14 */
  int link;      /* 0x18 guard to rouse (index into D_80075828), or -1 */
  int camTimer;  /* 0x1C */
  int camTimer2; /* 0x20 */
  int camDist;   /* 0x24 */
  int range;     /* 0x28 in 1024s */
  int wait;      /* 0x2C */
} LookoutState;
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
/* from level_34 */
extern int D_80075914;
/* from level_34 */
extern int D_80077898[];
/* from level_34 */
#define ANIM_WORD(a) (*(int *)&(a)->unk3C)
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
  func_80017908(spy->bodyRotZ, func_80016AB4((a)->posX - spy->posX,            \
                                             (a)->posY - spy->posY, 0))
/* from level_34 */
#define SPY_DZ(a) ((a)->posZ - (a)->unk38 - SPY.posZ)
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
  if ((a)->unk3D != (n)) {                                                     \
    D_80075794 = 0;                                                            \
    (a)->unk40 = 8;                                                            \
    (a)->unk41 = 8;                                                            \
    (a)->unk3C = (a)->unk3D;                                                   \
    (a)->unk3D = (n);                                                          \
    (a)->unk3E = (a)->unk3F;                                                   \
    (a)->unk3F = 0;                                                            \
    func_80037E98(a);                                                          \
  }
/* from level_34 */
#define ANIM_RETIME(a)                                                         \
  if (ABS2((a)->unk3F - (a)->unk3E) < 3) {                                     \
    (a)->unk41 = TYPE_DESC(a, (a)->unk3C);                                     \
  }
/* from level_34 */
#define ANIM_JUMP(a, n, f)                                                     \
  D_80075794 = 0;                                                              \
  (a)->unk40 = sixteen;                                                        \
  (a)->unk41 = sixteen;                                                        \
  (a)->unk3C = (a)->unk3D;                                                     \
  (a)->unk3D = (n);                                                            \
  (a)->unk3E = (a)->unk3F;                                                     \
  (a)->unk3F = (f);                                                            \
  func_80037E98(a)
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
extern int func_80038AFC(FlyPath *path, int *nearest);
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
extern int D_80075678;   /* level music cue */
/* from level_34 */
extern int D_8007566C[]; /* per-flag "seen" latches */
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
  int seen;             /* 0x2C index into D_8007566C, 0 = none */
} LootVortexState;
/* from level_34 */
extern int D_80075918; /* HUD countdown */
/* from level_34 */
extern int *D_80075680;
/* from level_32 */
typedef struct ShempState {
  int ang;             /* 0x00 heading << 4 */
  int unk04[3];        /* 0x04 */
  int timer;           /* 0x10 */
  int unk14;           /* 0x14 */
  int unk18;           /* 0x18 passed by address to func_80038FC8 */
  int phase;           /* 0x1C 0..2 */
  unsigned char *path; /* 0x20 byte 0 = node count */
  unsigned char *cam;  /* 0x24 camera anchors, one per phase */
  int node[3];         /* 0x28 path node per phase */
  int flag;            /* 0x34 */
  int count;           /* 0x38 */
  int chain;           /* 0x3C first of a linked list in D_80075828 */
  int init;            /* 0x40 */
  int timer2;          /* 0x44 */
} ShempState;
/* from level_32 */
extern int func_80038FC8(Actor *actor, int *a, int *b, int c, int d, int e);
/* from level_32 */
extern int D_80075850;
/* from level_32 */
extern int D_8007866C, D_80078670, D_80078674, D_80078678, D_8007867C;
/* from level_32 */
typedef struct L7State184 {
  int link;    /* 0x00 handed to the spawned bolt */
  int flag;    /* 0x04 */
  int u08;     /* 0x08 */
  int heading; /* 0x0C */
  int speed;   /* 0x10 */
  int timer;   /* 0x14 */
} L7State184;
/* from level_32 */
typedef struct L7State183 {
  int link;    /* 0x00 */
  int u04;     /* 0x04 */
  int u08;     /* 0x08 */
  int heading; /* 0x0C */
  int u10;     /* 0x10 */
  int u14;     /* 0x14 */
} L7State183;
/* from level_32 */
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
/* from level_32 */
typedef struct L7State197 {
  int vel[3];         /* 0x00 */
  short u0C;          /* 0x0C */
  short u0E;          /* 0x0E */
} L7State197;
/* from level_32 */
typedef struct L31Hatch {
  int pad00;
  int kind;     /* 0x04 0 = opens to charge or flame, else charge only */
  Actor *child; /* 0x08 released occupant */
  int handle;   /* 0x0C for func_8003A9EC */
} L31Hatch;
/* from level_32 */
typedef struct L31Mount {
  int rider; /* 0x00 index into D_80075828, -1 = none */
  int init;  /* 0x04 */
  int knock; /* 0x08 knock-back heading */
  int speed; /* 0x0C */
  int lift;  /* 0x10 */
} L31Mount;
/* from level_32 */
typedef struct L31Carrier {
  int hatch;   /* 0x00 index of its hatch in D_80075828 */
  Actor *held; /* 0x04 carried occupant */
  int timer;   /* 0x08 */
  int knock;   /* 0x0C knock-back heading */
  int speed;   /* 0x10 */
  int lift;    /* 0x14 */
} L31Carrier;
/* from level_32 */
typedef struct L31Barrel {
  short timer;  /* 0x00 */
  short blast;  /* 0x02 blast pushed Spyro */
  Actor *rider; /* 0x04 */
  int heading;  /* 0x08 run heading */
  int sink;     /* 0x0C sink speed */
  int link;     /* 0x10 index into D_80075828, -1 = none */
  short handle; /* 0x14 for func_8003A9EC */
} L31Barrel;
/* from level_32 */
typedef struct L31Debris {
  int vx;    /* 0x00 */
  int vy;    /* 0x04 */
  int vz;    /* 0x08 */
  int floor; /* 0x0C */
  unsigned char spinX; /* 0x10 */
  unsigned char spinY; /* 0x11 */
  unsigned char spinZ; /* 0x12 */
} L31Debris;
/* from level_32 */
#define BARREL_BURST(a, st)                                                    \
  {                                                                            \
    Actor *sp_ = D_800758CC(0x190, a);                                         \
    Actor *w_;                                                                 \
                                                                               \
    func_800562A4(a, 1);                                                       \
    func_80055A78(D_80076378[(a)->type]->m_Sounds[3], sp_, 8, &sp_->unk54);    \
    w_ = (st)->rider;                                                          \
    if (w_ != 0 && w_->type == 0x9A && w_->unk48 == 2) {                       \
      w_->unk49 = 1;                                                           \
    }                                                                          \
    func_80052568(a);                                                          \
    continue;                                                                  \
  }
/* from level_32 */
typedef struct L31Thrower {
  int held;  /* 0x00 index of the carried actor in D_80075828, -1 = none */
  int duck;  /* 0x04 Spyro charged: duck instead of throwing */
  int timer; /* 0x08 */
  int knock; /* 0x0C knock-back heading */
  int speed; /* 0x10 */
  int lift;  /* 0x14 */
} L31Thrower;
/* from level_32 */
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
  int link;       /* 0x40 actor that frees the patrol (index into D_80075828) */
  int camDone;    /* 0x44 */
} WardState;
/* from level_32 */
typedef struct Follower {
  PathData *path;    /* 0x00 */
  short hold;        /* 0x04 */
  short vz;          /* 0x06 */
  short unk08;       /* 0x08 */
  signed char spinX; /* 0x0A */
  signed char spinY; /* 0x0B */
  int pathDist;      /* 0x0C */
  int link;          /* 0x10 leader (index into D_80075828) */
  int target[3];     /* 0x14 */
  int timer;         /* 0x20 */
} Follower;
/* from level_32 */
typedef struct GrazerState {
  char pad00[0x20];
  int speed; /* 0x20 knock-back speed */
  int knock; /* 0x24 knock-back heading */
  int wait;  /* 0x28 graze timer */
  int voice; /* 0x2C bleat timer */
} GrazerState;
/* from level_32 */
extern int D_80076DF8[]; /* camera position */
/* from level_32 */
typedef struct L32State145 {
  int u00;     /* 0x00 flags; bit 0 armed */
  int u04;     /* 0x04 knock-back speed */
  int u08;     /* 0x08 knock-back heading */
  int u0C;     /* 0x0C */
  int u10;     /* 0x10 shot timer */
  int u14;     /* 0x14 */
  int u18;     /* 0x18 shot timer reload */
  void *u1C;   /* 0x1C path */
  int u20;     /* 0x20 */
  int u24;     /* 0x24 */
  int u28[6];  /* 0x28 trigger box */
  int u40[6];  /* 0x40 trigger box */
  int u58[6];  /* 0x58 trigger box */
  int u70;     /* 0x70 initialised */
  int u74;     /* 0x74 sink timer */
} L32State145;
/* from level_32 */
typedef struct L32State147 {
  int u00;     /* 0x00 flags; bit 0 armed */
  void *u04;   /* 0x04 path */
  int u08;     /* 0x08 knock-back speed */
  int u0C;     /* 0x0C knock-back heading */
  int u10;     /* 0x10 */
  int u14;     /* 0x14 */
  int u18;     /* 0x18 yaw toward Spyro */
  int u1C;     /* 0x1C yaw limit (0xFF none) */
  int u20;     /* 0x20 yaw limit (0xFF none) */
  int u24;     /* 0x24 */
  int u28;     /* 0x28 sweep rate */
  int u2C;     /* 0x2C sweep yaw << 4 */
  int u30;     /* 0x30 timer */
  int u34[6];  /* 0x34 trigger box */
  int u4C[6];  /* 0x4C trigger box */
  int u64[6];  /* 0x64 trigger box */
  int u7C;     /* 0x7C initialised */
} L32State147;
/* from level_32 */
extern int func_800179F0(int a, int b, int c, int d);
/* from level_32 */
typedef struct L32State148 {
  int u00;              /* 0x00 flags; bit 0 armed */
  int u04;              /* 0x04 knock-back speed */
  int u08;              /* 0x08 knock-back heading */
  int u0C;              /* 0x0C */
  int u10[6];           /* 0x10 trigger box */
  unsigned char *u28;   /* 0x28 target path: count, index, points */
  void *u2C;            /* 0x2C path */
  int u30;              /* 0x30 timer */
  int u34;              /* 0x34 throw timer */
  Actor *u38;           /* 0x38 held lob (type 176), -1 once thrown */
  int u3C;              /* 0x3C */
  int u40;              /* 0x40 aim-at-path range */
  int u44[6];           /* 0x44 trigger box */
  int u5C[6];           /* 0x5C trigger box */
  int u74;              /* 0x74 initialised */
} L32State148;
/* from level_32 */
typedef struct L32Lob {
  int u00;            /* 0x00 */
  short h4;           /* 0x04 */
  short h6;           /* 0x06 */
  short h8;           /* 0x08 */
  short hA;           /* 0x0A */
  unsigned char bC;   /* 0x0C */
  unsigned char bD;   /* 0x0D */
  unsigned char bE;   /* 0x0E */
  unsigned char bF;   /* 0x0F */
} L32Lob;
/* from level_32 */
extern int func_8003891C(int *from, int *to, int a, int b, void *c);
/* from level_32 */
typedef struct L32Shot {
  int vel[3]; /* 0x00 */
  short u0C;  /* 0x0C life timer */
  short u0E;  /* 0x0E long-reach timer */
} L32Shot;
/* from level_32 */
typedef struct L32Timer {
  int u00; /* 0x00 */
  int u04; /* 0x04 re-arm timer */
} L32Timer;
/* from level_32 */
typedef struct L32State236 {
  int u00[8]; /* 0x00 */
  int u20;    /* 0x20 knock-back speed */
  int u24;    /* 0x24 knock-back heading */
  int u28;    /* 0x28 */
  int u2C;    /* 0x2C idle timer */
} L32State236;
/* from level_32 */
extern int g_anCollisionProbeVec[];

/* ==== Level 33 classes ==== */

/* Type 143 state. */
typedef struct L33State143 {
  int u00; /* 0x00 radius */
  int u04; /* 0x04 timer */
} L33State143;

/* Type 169's trail point. */
typedef struct L33Trail {
  int pos[3]; /* 0x00 */
  int u0C[3]; /* 0x0C */
  int age;    /* 0x18 */
} L33Trail;   /* 0x1C */

/* Type 169 state. */
typedef struct L33State169 {
  L33Trail *trail; /* 0x00 */
  short h04;       /* 0x04 spawn timer */
  short h06;       /* 0x06 life timer */
  short h08;       /* 0x08 landing delay */
  short h0A;       /* 0x0A trail length */
  int u0C;         /* 0x0C speed */
  int u10;         /* 0x10 climb */
} L33State169;

extern int D_80075684;      /* trail slots used this frame */
extern int D_800772C8[];    /* trail lengths */
extern L33Trail *D_80078658[]; /* trails */

#define FLY_X(p, i) (*(int *)((char *)(p) + ((i) << 4) + 0x8))
#define FLY_Y(p, i) (*(int *)((char *)(p) + ((i) << 4) + 0xC))
#define FLY_FLAGS(p, i) (*(int *)((char *)(p) + ((i) << 4) + 0x14))

/* Type 159 state. */
typedef struct L33State159 {
  FlyPath *path; /* 0x00 */
  int u04;       /* 0x04 lane; indexes D_80075838 */
  int u08;       /* 0x08 slow-down timer */
  int u0C;       /* 0x0C */
  int u10;       /* 0x10 speed */
  int u14;       /* 0x14 knock-back heading */
  int u18;       /* 0x18 knock-back speed */
  int u1C;       /* 0x1C vertical speed */
  int u20;       /* 0x20 end node going back */
  int u24;       /* 0x24 end node going forward */
  int u28[14];   /* 0x28 trigger box */
  int u60;       /* 0x60 partner actor index, -1 none */
} L33State159;

extern int D_80075838[]; /* laps per lane */

/* Type 161 state (Gnasty Gnorc). */
typedef struct GnastyState {
  FlyPath *path;            /* 0x00 current escape path */
  FlyPath *path2;           /* 0x04 the other escape path */
  FlyPath *path3;           /* 0x08 camera reference path */
  int u0C;                  /* 0x0C */
  int u10;                  /* 0x10 */
  int u14;                  /* 0x14 knock-back heading */
  int u18;                  /* 0x18 speed */
  int u1C;                  /* 0x1C vertical speed */
  int u20;                  /* 0x20 queued jump speed */
  int u24[12];              /* 0x24 */
  int u54;                  /* 0x54 */
  int u58;                  /* 0x58 timer */
  int phase;                /* 0x5C fight phase */
  int timer;                /* 0x60 throw timer */
  int u64;                  /* 0x64 */
  int throws;               /* 0x68 bombs thrown */
  int u6C;                  /* 0x6C */
  int box70[6];             /* 0x70 trigger box */
  int box88[6];             /* 0x88 trigger box */
  int boxA0[6];             /* 0xA0 trigger box */
  int uB8;                  /* 0xB8 */
  int key2;                 /* 0xBC actor index, -1 none */
  int key;                  /* 0xC0 actor index */
  int seen;                 /* 0xC4 */
  unsigned char *home;      /* 0xC8 */
  int camTimer;             /* 0xCC */
  int uD0;                  /* 0xD0 */
  int drop[3];              /* 0xD4 key actors dropped while hit */
  L33Trail trails[3][150];  /* 0xE0 one per live bomb */
} GnastyState;

/* Gravity toward the floor under the actor. */
#define GNASTY_FALL(a, st)                                                     \
  {                                                                            \
    int h_ = func_80038400(a, 0xBB8);                                          \
    int z_;                                                                    \
                                                                               \
    (st)->u1C -= 0x19;                                                         \
    if ((st)->u1C < -0x190) {                                                  \
      (st)->u1C = -0x190;                                                      \
    }                                                                          \
    z_ = (a)->posZ + (st)->u1C;                                                \
    if (z_ < h_) {                                                             \
      (a)->posZ = h_;                                                          \
    } else {                                                                   \
      (a)->posZ = z_;                                                          \
    }                                                                          \
  }

extern int D_80077CC8[];         /* level flags */
extern unsigned char D_80078A64; /* SPY.bodyRotX, absolute form */
extern unsigned char D_80078A65; /* SPY.bodyRotY, absolute form */
extern int D_800757AC;
#define PATH_END(p) ((int *)((char *)(p) + (((p)->count << 4) - 8)))
extern short g_anSineLut[];
extern int g_nLevelReadyFlag; /* == D_80078BBC, scalar view */

/* ==== The function ==== */
#define spy (&SPY)
void func_level_33_8007AD64(void) {
  Actor **actorList;
  Actor *actor;
  short oneq;
  int one;
  SpyroObj *spyc;
  char *base;
  int dt;

  /* Scratch vectors, in frame order. */
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
  int fx[6];
  int col[3];
  int o128[3];
  int o138[2];
  int o140[3];
  int o150[3];
  int o160[3];
  int o170[2];
  int w181[3];
  int s392[3];
  int v310[3];
  int ps[6];
  int s448[3];
  int s480[3];
  int col2[3];
  int s496[3];
  int s512[3];

  /* Build this frame's update list. A long frame (delta >= 3) gets an extra
   * pass. */
  func_80051FEC();
  func_800522C0(D_800700F4, 0);

  if (D_800756CC >= 3) {
    func_800522C0(D_800700F4, D_800756CC == 3 ? 0x80000001 : 0x80000000);
  }

  actorList = D_800700F4;
  base = (char *)D_80078BBC;

  /* Visit every actor on the list; dormant ones (state >= 0x80) are skipped. */
  while (actor = *actorList++) {
    spyc = (SpyroObj *)&D_80078A58;
    oneq = 1;
    one = 1;

    if (actor->unk48 >= 0x80) {
      continue;
    }

    D_80075794 = actor->unk42 & 2;
    D_800757F4 = actor->unk42 & 1;
    dt = D_800756CC;
    D_800756C4 = dt;

    switch (actor->type) {




    /* Tethered follower (FollowState): rides a parent actor until the parent
     * goes dormant, then takes the parent's transform over and re-points the
     * other followers of that parent at itself. */
    case 13: {
      FollowState *st = actor->state;
      Actor *parent = &D_80075828[st->parent];

      if (st->detached == 0) {
        if (st->trackPos != 0) {
          func_8001778C(st->pos, st->pos, &parent->posX);
          actor->unk52 = 0xFF;
        } else {
          actor->unk52 = 0x20;
        }

        if (st->active != 0 &&
            (parent->unk48 >= 0x80 || (parent->unk53 & 0x80))) {
          Actor *scan;
          int self;

          parent->unk53 = actor->unk53;
          func_80052568(actor);

          if (parent->unk48 < 0x80) {
            func_80052568(parent);
          }

          /* Take the parent's whole record over (gcc's block_move_loop). */
          *actor = *parent;
          func_800526A8(actor);

          actor->unk51 = 1;
          self = actor - D_80075828;
          parent = D_80075828;

          for (; parent < D_80075890; parent++) {
            if (parent != actor && parent->unk48 < 0x80 && parent->type == 13) {
              FollowState *other = parent->state;

              if (other->parent == st->parent) {
                other->parent = self;
              }
            }
          }

          actor->unk48 = 0;
          ANIM_RESET(actor, 0);
        } else {
          st->detached = 1;
          goto follow_carry;
        }
      } else {
      follow_carry:
        if (st->trackPos != 0) {
          func_80017700(&actor->posX, &parent->posX);
        }

        if (st->release != 0) {
          /* Released: drift away and pop. */
          if (func_80037F90(&st->release, 4) != 0) {
            int *vel = st->pos;

            if (st->trackPos != 0) {
              func_80017758(vel, vel, &actor->posX);
            } else {
              func_80017700(&actor->posX, &parent->posX);
            }

            st->pos[2] += 0x400;

            if (st->unk24 != 0) {
              func_8003ABC0(actor, 4, 0, 0);
            } else if (func_8004D5EC(st->pos, 0x1400) != 0) {
              st->pos[2] -= 0x400;
              func_8003ABC0(actor, 2, 0, st->pos);
            } else {
              func_8003ABC0(actor, 1, 0, 0);
            }
            func_8003B7C0(actor);
            *(volatile int *)&st->count <<= 7;
            D_800761DC = 2;
            D_800761F4.v =
                *(unsigned short *)(D_800761D0 + D_800761D4[0x37] * 20 + 0xA) +
                st->count;
            func_80055A78(D_800761D4[0x37], actor, 8, &actor->unk54);
            func_80052568(actor);
            continue;
          }
        } else if (actor->unk49 != 0 || parent->unk48 >= 0x80) {
          st->release = 9;
          st->count = 1;

          for (parent = D_80075828; parent < actor; parent++) {
            if (parent->unk48 < 0x80 && parent->type == 13 &&
                *(int *)parent->state == st->parent) {
              st->release += 9;
              st->count += 1;
            }
          }

          if (st->count == 1) {
            D_800761DC = 2;
            D_800761F4.v =
                *(unsigned short *)(D_800761D0 + D_800761D4[0x37] * 20 + 0xA);
            func_80055A78(D_800761D4[0x37], actor, 8, &actor->unk54);
          }
        }
      }

      break;
    }





    /* Butterfly (ChaseState). */
    case 16: {
      ChaseState *st = (ChaseState *)actor->state;

      if (g_nLevelReadyFlag < 3 && func_80017990(&actor->posX, (int *)(base - 0x164)) < 2400) {
        int actorZ = actor->posZ - actor->unk38;

        if (ABS2(actorZ - SPY.posZ) < 1800) {
          if (D_80075898 == 0) {
            if (g_nLevelReadyFlag >= 0) {
              D_80075898 = D_800758CC(120, actor);

              func_8003851C(D_80075898, 0, 0);

              g_nLevelReadyFlag = 1;

              func_80052568(actor);
              break;
            }
          } else if (D_80075898->unk49 != 99) {
            ChaserState *cs = (ChaserState *)D_80075898->state;

            if (cs->owner == 0) {
              if (func_800381BC(SPY.bodyRotZ,
                                func_80016AB4(actor->posX - SPY.posX,
                                              actor->posY - SPY.posY, 0)) < 0) {
                st->dir = 1;
              } else {
                st->dir = -1;
              }

              D_80075898->unk49 = 0;
              cs->owner = actor;
              cs->timer = func_80037EA0(150, 210);

              actor->unk48 = 1;
            }
          }
        }
      }

      if (actor->unk50 & 0x80)
        break;

      switch (actor->unk48) {
      case 0: {
        if (func_80037F90(&st->t14, 2) == 0 || actor->unk51) {
          int refHeight, rotated, moved;

          if (func_80037F90(&st->t12, 1) != 0) {
            int r = func_80037F10(30, 90);
            if (func_8006272C() & 1) {
              r = -r;
            }

            st->heading = func_80038074(st->heading, r);
            st->t12 = func_80037EA0(60, 140);
          }

          if (func_80037F90(&st->t13, 1) != 0) {
            st->vz = func_80037F10(10, 25);
            st->t13 = func_80037EA0(80, 140);
          }

          if (st->vz >= 128) {
            actor->posZ = actor->posZ + (st->vz - 256);
          } else {
            actor->posZ = actor->posZ + st->vz;
          }

          refHeight = actor->posZ - func_80038340(actor);

          if (refHeight < 300) {
            st->vz = func_80037EA0(10, 25);
            st->t13 = func_80037EA0(80, 140);
          }

          if (1024 < refHeight && refHeight < 2048) {
            st->vz = -func_80037EA0(10, 25);
            st->t13 = func_80037EA0(80, 140);
          }

          rotated = func_80038EE0(actor, st->heading, 4, 0xA, 1);
          moved = func_80039398(actor, 30, 0, 50, 1);

          if (rotated && moved) {
            st->heading = func_80038074(st->heading, func_80037EA0(98, 158));
            st->t12 = func_80037EA0(30, 80);
          }

          if (func_80017990(&actor->posX, &st->tgtX) > 2000) {
            int angle = func_80016AB4(st->tgtX - actor->posX,
                                      st->tgtY - actor->posY, 0);

            st->heading = angle;
            st->t12 = 40;
          }
        } else {
          func_80052568(actor);
          continue;
        }
        break;
      }
      case 1: {
        func_80038638(actor, &SPY.posX, 0x320,
                      func_80038074(func_80016AB4(actor->posX - SPY.posX,
                                                  actor->posY - SPY.posY, 0),
                                    st->dir * 8),
                      8, 0x82, 0xE, 0x80, 0xFF, 0xFF, 0, 0, 0);

        if (D_80075898 == 0 || D_80075898->unk49 == 99) {
          actor->unk48 = 0;
        } else {
          int refHeight;

          if (func_80037F90(&st->t13, 1)) {
            st->vz = func_80037F10(5, 20);
            st->t13 = func_80037EA0(40, 80);
          }

          if (st->vz >= 128) {
            actor->posZ = (actor->posZ) + (st->vz - 256);
          } else {
            actor->posZ = actor->posZ + st->vz;
          }

          refHeight = actor->posZ - func_80038340(actor);

          if (refHeight < 200) {
            st->vz = func_80037EA0(5, 20);
            st->t13 = func_80037EA0(40, 80);
          }

          if (1000 < refHeight && refHeight < 2048) {
            st->vz = -func_80037EA0(5, 20);
            st->t13 = func_80037EA0(40, 80);
          }
        }
        break;
      }
      }

      break;
    }


    /* Dragon egg (ShotState). */
    case 34: {
      ShotState *st;

      st = (ShotState *)actor->state;

      if (actor->unk48 != 0 && actor->unk48 < 5) {
        SPY.controlFlags = 0x80002000;
        if (SPY.state != 0 && SPY.state != 0xC) {
          actor->unk48 = 5;
        }
      }
      if ((unsigned int)(actor->unk48 - 2) < 3) {
        func_8003C85C(&actor->posX);
        D_800758E4(1, 0xC, (int *)actor, (void *)0x602080);
      }

      switch (actor->unk48) {
      case 1:
        D_8007570C = 1;
        func_80017BFC(st->from, &actor->posX);
        st->heading =
            func_800169AC(actor->posX - SPY.posX, actor->posY - SPY.posY) +
            0x40;
        st->speed = 0;
        st->phase = 0;
        D_80077FDC.v = 1;
        actor->unk44 = 0;
        actor->unk45 = 8;
        actor->unk48 = actor->unk48 + 1;
        break;

      case 2:
        st->phase = st->phase + D_800756CC;
        if (st->phase < 0x80) {
          int step = D_800756CC << 2;
          st->speed = st->speed + step;
          st->heading = st->heading + step;
          func_80017C24(&actor->posX, st->from);
          actor->posX += (func_80016CB0(st->heading << 4) * st->speed) >> 12;
          actor->posY += (func_80016C58(st->heading << 4) * st->speed) >> 12;
          {
            int ph = st->phase;
            actor->unk46 = actor->unk46 + (D_800756CC << 2);
            actor->posZ = actor->posZ - (ph << 3);
          }
        } else {
          func_80017700(svA, &SPY.posX);
          {
            int c = func_80016CB0(SPY.bodyRotZ << 4);
            int s = func_80016C58(SPY.bodyRotZ << 4);
            int cr = c * 724;
            int sr = s * 724;
            svA[0] = svA[0] + ((sr + cr) >> 12);
          }
          {
            int s = func_80016C58(SPY.bodyRotZ << 4);
            int c = func_80016CB0(SPY.bodyRotZ << 4);
            int sr = s * 724;
            int cr = c * 724;
            svA[1] = svA[1] + ((sr - cr) >> 12);
          }
          svA[2] = svA[2] + 0x300;
          func_80017BFC(st->to, svA);
          func_80017BFC(st->from, &actor->posX);
          st->phase = 0;
          actor->unk48 = actor->unk48 + 1;
        }
        if ((D_80077378 & 0x40) != 0) {
          actor->unk48 = 5;
        }
        break;

      case 3:
        st->phase = st->phase + D_800756CC;
        if (st->phase < 0x40) {
          func_80017C24(svB, st->from);
          func_80017C24(svC, st->to);
          func_8001778C(svC, svC, svB);
          func_800177C0(svC, svC, st->phase);
          func_800176C8(svC, 6);
          func_80017758(&actor->posX, svB, svC);
          actor->unk46 = actor->unk46 + (D_800756CC << 2);
        } else {
          func_80017700(svD, &SPY.posX);
          svD[0] = svD[0] + (func_80016CB0(SPY.bodyRotZ << 4) >> 3);
          svD[1] = svD[1] + (func_80016C58(SPY.bodyRotZ << 4) >> 3);
          svD[2] = svD[2] + 0x300;
          func_80017BFC(st->from, svD);
          st->heading = SPY.bodyRotZ - 0x40;
          st->speed = 0x200;
          st->phase = 0;
          actor->unk48 = actor->unk48 + 1;
        }
        if ((D_80077378 & 0x40) != 0) {
          actor->unk48 = 5;
        }
        break;

      case 4:
        st->phase = st->phase + D_800756CC;
        if (st->phase < 0x80) {
          int step = D_800756CC << 2;
          st->speed = st->speed - step;
          st->heading = st->heading - step;
          func_80017C24(&actor->posX, st->from);
          actor->posX += (func_80016CB0(st->heading << 4) * st->speed) >> 12;
          actor->posY += (func_80016C58(st->heading << 4) * st->speed) >> 12;
          {
            int ph = st->phase;
            actor->unk46 = actor->unk46 + (D_800756CC << 2);
            actor->posZ = actor->posZ - (ph << 2);
          }
        } else {
          actor->unk48 = actor->unk48 + 1;
        }
        if ((D_80077378 & 0x40) != 0) {
          actor->unk48 = 5;
        }
        break;

      case 5:
        func_8001778C(svE, &actor->posX, &CAM.posX);
        func_80017110(svE, svE);
        D_800758E4(0x10, 0x4D, svE, 0);
        D_8007570C = 0;
        func_800176F0(&SPY.headLook);
        func_8003B854(0, st->unk00);
        D_80075810 = D_80075810 + 1;
        D_80076FE8[D_80075964] += 1;
        D_80077FDC.v = 0;
        func_80052568(actor);
        break;

      default:
        break;
      }
      break;
    }

    /* Type 168: counts its sub-state down, restarting its animation each
     * time, then shares 363's exit. */
    case 168:
      if (actor->unk49 != 0) {
        actor->unk49--;
        actor->unk3E = 0;
        actor->unk3F = 1;
        actor->unk40 = 0;
      }



    /* Types 363/364: a one-shot effect that is removed when its animation
     * ends. */
    case 363:
    case 364:
    case 507:
    case 508:      if (D_80075794 != 0) {
        func_80052568(actor);
        continue;
      }
      break;





    /* Explosion shard with a ground bounce (ShardState). */
    case 67:
    case 68:
    case 255:
    case 256: {
      ShardState *st = actor->state;

      if (st->life > 0 && actor->unk51 != 0) {

      if (func_8004BE4C(&actor->posX, 0x100, 0x100) != 0) {
        int *nrm = &D_80077368;
        int along;
        register int push asm("$7");

        func_80017330(nrm, 0x1000);
        along = (st->vel[0] * nrm[0]) + (st->vel[1] * D_8007736C) +
                (st->vel[2] * D_80077370);
        push = along >> 11;

        if (push < 0) {
          func_800175B8(nrm, 0x1000, (along >> 13) - push);
          st->vel[0] += nrm[0];
          st->vel[1] += AS_I32(D_8007736C);
          st->vel[2] += AS_I32(D_80077370);
        }
      }

      actor->posX += st->vel[0];
      actor->posY += st->vel[1];
      st->vel[2] -= 6;
      if (st->vel[2] < -0x80) {
        st->vel[2] = -0x80;
      }
      actor->posZ += st->vel[2];
      actor->unk44 += st->spin[0];
      actor->unk45 += st->spin[1];
      actor->unk46 += st->spin[2];

      if (!(st->life & 3)) {
        svD[0] = func_8006272C() & 3;
        svD[1] = func_8006272C() & 3;
        svD[2] = 0x14;
        D_800758E4(1, 1, &actor->posX, svD);
      }

      st->life -= 1;
      break;
      }
      D_800758E4(8, 0x46, &actor->posX, (int *)0x10);
      func_80052568(actor);
      continue;
    }





    /* Falling chest fragments (TumbleState). */
    case 69:
    case 151:
    case 257: {
      TumbleState *st = actor->state;

      /* Retire when out of life, no longer drawn, or sunk to the floor. */
      if (st->life == 0) {
        func_80052568(actor);
        continue;
      }
      if (actor->unk51 == 0) {
        func_80052568(actor);
        continue;
      }
      if (actor->posZ <= st->floorZ) {
        func_80052568(actor);
        continue;
      }

      actor->posX += st->vel[0];
      actor->posY += st->vel[1];

      st->vel[2] -= 6;
      if (st->vel[2] < -0x80) {
        st->vel[2] = -0x80;
      }
      actor->posZ += st->vel[2];

      actor->unk44 += st->spin[0];
      actor->unk45 += st->spin[1];
      actor->unk46 += st->spin[2];

      st->life -= 1;
      break;
    }


    /* The generic enemy: patrols, notices Spyro, charges, gets knocked back.
     * ->unk49 is the outer state and st->mode the sub-state. */
    case 14:
    case 15:
    case 83:
    case 84:
    case 85:
    case 86:
    case 87: {
      EnemyState *st = actor->state;
      int toSpyro;

      toSpyro = func_80017990(&actor->posX, &SPY.posX);

      if (st->timer < 0xFA) {
        st->timer += D_800756CC;
      }

      if (actor->type != 15) {
        if (st->attach != 0xFF) {
          func_800529E4(actor, 4);
          if (actor->type == 14) {
            func_80017048(&actor->unk20[0], &D_8006E5B8,
                          D_80077108[st->attach].pos);
          } else {
            func_80017048(&actor->unk20[0], &D_8006E5A0,
                          D_80077108[st->attach].pos);
          }
          func_80017758(D_80077108[st->attach].pos,
                        D_80077108[st->attach].pos, &actor->posX);

          if (D_80077108[st->attach].flag < 5) {
            st->attach = 0xFF;
          }
        } else if (st->timer >= 0xF8) {
          int handle = func_8003AAEC(actor, &D_8006E5A0);

          if (handle >= 0 && toSpyro < 0x4000) {
            st->attach = handle;
          }
          st->timer = (func_8006272C() & 0x1F) + 0x21;
        }
      }

      if (actor->unk51 != 0 || toSpyro <= 0x2000 || actor->unk49 != 1 ||
          st->mode != 0) {
        switch (actor->unk49) {
        case 0:
          /* Idle: sample the floor normal and face along it. */
          func_80017700(svA, &actor->posX);
          svA[2] += 0x400;
          func_8004D5EC(svA, 0x10000);
          {
            int *nrm = &D_80077368;

            st->lean = -func_800169AC(
                func_80017A38((nrm[0] * nrm[0]) + (D_80077370 * D_80077370)),
                D_8007736C);
            st->roll = -func_800169AC(D_80077370, nrm[0]);
          }

          if (st->lean != 0 || st->roll != 0) {
            actor->unk46 = 0;
          }
          actor->unk49 = 1;
          goto enemy_pose;

        case 1: {

          if (st->mode == 0) {
          } else if (st->mode == 1) {
            func_80038458(actor);
            func_800533D0(actor);
            if (actor->type >= 83) {
              {
                int *nrm = &D_80077368;

                st->lean = -func_800169AC(
                    func_80017A38((nrm[0] * nrm[0]) + (D_80077370 * D_80077370)),
                    D_8007736C);
                st->roll = -func_800169AC(D_80077370, nrm[0]);
              }
            }
          } else if (st->mode == 2) {
            func_80017CB8(st->node, acc[0]);
            func_80017758(acc[0], acc[0], acc[1]);
            func_80017758(acc[0], acc[0], acc[2]);
            actor->posX = acc[0][0] / 3;
            actor->posY = acc[0][1] / 3;
            actor->posZ = acc[0][2] / 3;
            func_800529E4(actor, 2);
            func_80038458(actor);
            func_800533D0(actor);
          } else if (st->mode == 3) {
            func_80017700(svF, &actor->posX);
            svF[2] += 0x400;
            func_800529E4(actor, 2);
            if (func_8004D5EC(svF, 0x1000) > 0) {
              st->node = D_80075808;
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

            if (dz < 0x200 && D_80075898 != 0 && D_80078AF4 == 0 &&
                func_80033E40(&D_80075898->posX, &actor->posX) != 0) {
              {
                BalloonState *bal = D_80075898->state;

                if (bal->gnorc == 0) {
                  if (func_80056DC4(D_80075898,
                                    D_80076378[D_80075898->type]->m_Sounds[0]) ==
                      0) {
                    D_800761DC = 1;
                    D_800761EA = 0x2000;
                    D_800761E8 = 0x2000;
                    func_8003851C(D_80075898, 0, 0);
                  }
                  D_80075898->unk49 = 4;
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

          func_800177C0(svG, (int *)st, D_800756CC);
          func_800176C8(svG, 1);
          dist = func_800171FC(svG, 1);

          if (dist >= 0xDD) {
            func_800175B8(svG, dist, 0xDC);
          }

          at = &actor->posX;
          func_80017758(svG, at, svG);

          if (st->vel[2] >= -0xDB) {
            st->vel[2] -= D_800756CC * 5;
          }

          if (svG[2] < 0) {
            if (actor->type != 14 && actor->type != 15) {
              func_8003B9D4(actor);
            }
            func_80052568(actor);
            continue;
          }

          svG[2] += 0xF0;

          if (func_8004BE4C(svG, 0xF0, 0xF0) != 0) {
            if (func_80057380() == 0) {
              if (actor->type != 14 && actor->type != 15) {
                func_8003B9D4(actor);
              }
              func_80052568(actor);
              continue;
            }

            func_80017700(svH, &D_80077368);
            func_80017700(at, &D_80076B80);
            actor->posZ -= 0xF0;

            if (st->bounce == 0) {
              int floor = func_8004D5EC(svG, 0x400);
              int slope = (signed char)func_800169AC(svH[2], func_800171FC(svH, 0));

              if ((svG[2] - 0x190) < floor && slope < 0x18) {
                actor->unk49 = 1;
                func_80017700(at, svG);
                actor->posZ = floor;
                if (actor->type != 15) {
                  st->lean = -func_800169AC(
                      func_80017A38((svH[0] * svH[0]) + (svH[2] * svH[2])),
                      svH[1]);
                  st->roll = -func_800169AC(svH[2], svH[0]);
                  if (st->lean != 0 || st->roll != 0) {
                    actor->unk46 = 0;
                  }
                }
                func_800533D0(actor);
              } else {
                st->bounce += 1;
              }


              if (st->bounce == 0) {
                goto enemy_settle;
              }
            }

            D_800761DC = 1;
            D_800761E8 = 0x3CCC >> (3 - st->bounce);
            D_800761EA = 0x3CCC >> (3 - st->bounce);
            func_80055A78(D_800761D4[0x2F], actor, 8, &actor->unk54);

            if (func_80017428((int *)st, svH, (int *)st) != 0) {
              st->bounce -= 1;
              st->vel[0] = (st->vel[0] >> 3) + (func_8006272C() & 0x3F) - 0x20;
              st->vel[1] = (st->vel[1] >> 3) + (func_8006272C() & 0x3F) - 0x20;
              st->vel[2] = (st->vel[2] >> 2) + (func_8006272C() & 0xF);
            }
          } else {
            svG[2] -= 0xF0;
            func_80017700(at, svG);
            func_8004D5EC(svG, 0x10000);
            func_800533D0(actor);
          }

        enemy_settle:
          if (toSpyro < 0x59A) {
            int top = actor->posZ + 0x164;
            register int py asm("$2") = SPY.posZ;
            int dz = py - top;

            if (dz < 0) {
              dz = -dz;
            }

            if (dz < 0x200 && D_80075898 != 0 && D_80078AF4 == 0 &&
                st->bounce < 3) {
              {
                BalloonState *bal = D_80075898->state;

                if (bal->gnorc == 0) {
                  if (func_80056DC4(D_80075898,
                                    D_80076378[D_80075898->type]->m_Sounds[0]) ==
                      0) {
                    D_800761DC = 1;
                    D_800761EA = 0x2000;
                    D_800761E8 = 0x2000;
                    func_8003851C(D_80075898, 0, 0);
                  }
                  D_80075898->unk49 = 4;
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
            func_80017700(&actor->posX, &SPY.posX);
          } else {
            func_8001778C(svI, &spy->posX, (int *)st);
            func_800176C8(svI, 5);

            if (func_800171FC(svI, 1) >= 0x1E1) {
              func_80017700(&actor->posX, &spy->posX);
              st->timer = 0x20;
            } else {
              func_800177C0(svI, svI, st->timer);
              func_80017758(&actor->posX, (int *)st, svI);
              actor->posZ += (short)(D_8006CBF8[st->timer * 4] / 12);
            }
          }

          actor->unk44 += (unsigned char)(st->lean - 7);
          do {
          } while (0);
          actor->unk45 += (unsigned char)(st->roll - 7);
          do {
          } while (0);
          actor->unk46 += (unsigned char)(st->spin - 7);
          goto enemy_pose;

        case 4:
          continue;

        default:
        enemy_pose:
          if (actor->unk49 < 3) {
            if (actor->type == 14) {
              int v;

              v = ((unsigned short)D_8006CC78[st->spin] << 16 >> 25) + (unsigned char)st->lean;
              actor->unk44 = v;
              v = ((unsigned short)D_8006CBF8[st->spin] << 16 >> 25) + (unsigned char)st->roll;
              actor->unk45 = v;
              st->spin += D_800756CC * 2;
            } else if (actor->type == 15) {
              actor->unk46 += 8;
              actor->unk45 -= 6;
            } else {
              int v;

              v = ((unsigned short)D_8006CC78[st->spin] << 16 >> 23) + (unsigned char)st->lean;
              actor->unk44 = v;
              v = ((unsigned short)D_8006CBF8[st->spin] << 16 >> 23) + (unsigned char)st->roll;
              actor->unk45 = v;
              st->spin += D_800756CC * 2;
            }
          }

          if ((st->timer >= 0x21 && toSpyro < 0x200 &&
               (unsigned int)((SPY.posZ - actor->posZ) + 0x17F) < 0x37F) ||
              (actor->unk49 == 3 && st->timer >= 0x41)) {
            if (D_80075898 != 0) {
              BalloonState *bal = D_80075898->state;

              if (bal->gnorc == actor) {
                bal->gnorc = 0;
              }
            }

            if (actor->type == 14) {
              func_80055A78(D_800761D4[0], actor, 0x10, 0);
              func_800529E4(actor, 4);
              D_800758E4(0x10, 0x46, &actor->posX, (int *)8);
              D_800758E4(0x10, 0x46, &actor->posX, (int *)0x10);
              if (D_8007582C < 0x63) {
                D_8007582C += 1;
              }
              func_8003B854(0, actor);
              func_80052568(actor);
              continue;
            }

            if (actor->type == 15) {
              func_80055A78(D_800761D4[0], actor, 0x10, 0);
              D_800758E8 += 1;
              func_80052568(actor);
            } else {

              func_8003B9D4(actor);
              func_80052568(actor);
              continue;
            }
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

      if (g_nLevelReadyFlag >= 2) {
        svF[0] = 0;
        svF[1] = 0x64;
        svF[2] = 0;
        func_80017048(&actor->unk20[0], svF, svF);
        func_80017758(svF, svF, &actor->posX);
        D_800758E4(2, 0x42, svF, 0);
        svF[0] = 0;
        svF[1] = -0x64;
        svF[2] = 0;
        func_800170C0(svF, svF);
        func_80017758(svF, svF, &actor->posX);
        D_800758E4(2, 0x42, svF, 0);
      }

      if (g_nLevelReadyFlag >= 3) {
        FxNode *fx = st->smoke;

        if (fx == 0) {
          fx = func_80058AE8();
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
        func_80058B60(st->smoke);
        st->smoke = 0;
      }

      if (g_nLevelReadyFlag <= 0) {
        if (st->gnorc != 0 && st->gnorc->type == 16) {
          st->gnorc->unk48 = 0;
        }
        D_80075898 = 0;
        func_80052568(actor);
        continue;
      }

      if (st->gnorc != 0 &&
          func_80017990(&actor->posX, (int *)(base - 0x164)) >=
              0x2001) {
        st->timer = 0;
      }

      if (actor->unk49 != 0x63) {
        if (st->gnorc != 0) {
          switch (actor->unk49) {
          case 0: {
            /* Reeling the gnorc in. */
            EnemyState *ge = st->gnorc->state;
            int high = actor->unk3D & 1;
            int dist;
            int reach;
            int limit;

            func_80017700(svA, &st->gnorc->posX);

            if (high == 0) {
              svA[0] -=
                  (unsigned short)D_8006CC78[st->gnorc->unk46] << 16 >> 20;
              svA[1] -=
                  (unsigned short)D_8006CBF8[st->gnorc->unk46] << 16 >> 20;
            } else {
              svA[0] -=
                  (unsigned short)D_8006CC78[st->gnorc->unk46] << 16 >> 22;
              svA[1] -=
                  (unsigned short)D_8006CBF8[st->gnorc->unk46] << 16 >> 22;
            }

            if (high != 0) {
              if (actor->unk3F >= 7) {
                actor->unk49 += 1;
              }

              {
                int tilt = actor->unk45;
                int back = -tilt;

                if ((unsigned int)((tilt - 3) & 0xFF) < 0xFB) {
                  if (back >= 3) {
                    back = 2;
                  }
                  if (back < -2) {
                    back = -2;
                  }
                  actor->unk45 = tilt + back;
                }
              }
            }

            func_8001778C(svA, svA, &actor->posX);
            dist = func_800171FC(svA, 1);

            if (high == 0 &&
                (func_80037F90((void *)st, 4) != 0 ||
                 (st->timer < 0x50 &&
                  func_80017908(
                      (actor->unk46 + (ge->vel[0] * 0x6E)) & 0xFF,
                      (((unsigned short)D_80076E20 << 16 >> 20) + 0x80) &
                          0xFF) < 8))) {
              if (actor->unk3D != actor->unk3D + 1) {
                D_80075794 = 0;
                ANIM_ADVANCE(actor, actor->unk3D + 1);
              }
            }

            reach = high != 0 ? 0x96 : 0x82;
            limit = reach + 5;
            if (limit < dist) {
              func_800175B8(svA, dist, limit);
            } else {
              func_800175B8(svA, dist, reach - 5);
            }
            func_80017758(&actor->posX, &actor->posX, svA);
            func_80038EE0(actor, func_80016AB4(svA[0], svA[1], 0), 0xA, 0, 0);
            actor->unk45 = func_80016AB4(func_800171FC(svA, 0), svA[2], 0);
            actor->unk45 = func_80038098(actor->unk45 & 0xFF, 0, 0x30);
            break;
          }

          case 1:
            g_nLevelReadyFlag += 1;
            func_80055A78(D_800761D4[0], actor, 0x10, 0);
            actor->unk49 += 1;
            func_80052568(st->gnorc);
            break;

          case 2:
            if (D_80075794 != 0) {
              if (actor->unk3D != actor->unk3D - 1) {
                D_80075794 = 0;
                ANIM_ADVANCE(actor, actor->unk3D - 1);
              }
              st->gnorc = 0;
              actor->unk49 = 0;
            }
            break;

          case 4: {
            /* Hauling an enemy up to the basket. */
            EnemyState *ge = st->gnorc->state;
            int dist;

            func_8001778C(svJ, &st->gnorc->posX, &actor->posX);
            dist = func_800171FC(svJ, 1);

            if (dist < 0x200) {
              ge->timer = 0;
              ge->lean = func_8006272C() & 0xE;
              ge->roll = func_8006272C() & 0xE;
              ge->spin = func_8006272C() & 0xE;
              func_80017700((int *)ge, &st->gnorc->posX);
              st->gnorc->unk49 = 3;

              if (st->gnorc->type == 14) {
                D_800758E4(1, 0xC, actor, D_8006E494);
              } else if (st->gnorc->type == 15) {
                D_800758E4(1, 0xC, actor, D_8006E490);
              } else {
                D_800758E4(1, 0xC, actor, D_8006E330[st->gnorc->type]);
              }
              st->gnorc = 0;
              do {
              } while (0);

              if (func_80056DC4(actor, D_800761D4[0x2F]) == 0) {
                D_800761DC = 1;
                D_800761EA = 0x3600;
                D_800761E8 = 0x3600;
                func_80055A78(D_800761D4[0x2F], actor, 8, &actor->unk54);
              }
            } else {
              int lim = 0x136;

              if (lim < dist) {
                func_800175B8(svJ, dist, lim);
              } else {
                func_800175B8(svJ, dist, 0x122);
              }
              func_80017758(&actor->posX, &actor->posX, svJ);
              func_80038EE0(actor, func_80016AB4(svJ[0], svJ[1], 0), 0xA, 0, 0);
              actor->unk45 = func_80016AB4(func_800171FC(svJ, 0), svJ[2], 0);
              actor->unk45 = func_80038098(actor->unk45 & 0xFF, 0, 0x30);
            }
            break;
          }
          }
        } else {
          /* Nobody in the basket: drift on the wind. */
          actor->unk50 = 0x10;

          if (st->timer <= 0) {
            svF[0] = -0x258;
            svF[1] = (func_8006272C() & 0x310) - 0x188;
            svF[2] = D_80076E28 == 3 ? (func_8006272C() & 0x7F) + 0x64
                                     : (func_8006272C() & 0x1FF) - 0xC8;
            do {
            } while (0);
            st->drift[0] = svF[0];
            st->drift[1] = svF[1];
            st->drift[2] = svF[2];
            st->timer = func_8006272C() & 0x7B;
          } else {
            int *at;

            st->timer -= D_800756CC;
            do {
            } while (0);

            if (D_80076E28 == 3) {
              svF[0] = st->drift[0] + (D_80078B70 >> 2);
              svF[1] = st->drift[1];
              svF[2] = st->drift[2];
              if (svF[2] < 0x64) {
                svF[2] = 0x64;
              }
            } else {
              if (D_80076E28 == 0x80000009) {
                svF[0] = st->drift[0] - 0x200;
              } else {
                svF[0] = st->drift[0];
              }
              svF[1] = st->drift[1];
              svF[2] = st->drift[2];
            }

            func_80017048(&SPY.u034, svF, svF);
            func_80017758(svF, svF, &SPY.posX);
            at = &actor->posX;
            func_8001778C(svF, svF, at);
            func_800176C8(svF, 2);
            func_80017758(at, at, svF);

            if (func_8004BE4C(at, 0x100, 0x100) != 0) {
              func_80017700(at, &D_80076B80);
            }

            if (svF[2] >= 0x21) {
              svF[2] = 0x20;
            }
            if (svF[2] < -0x20) {
              svF[2] = -0x20;
            }

            actor->unk44 = 0;
            actor->unk45 = svF[2];
            func_80038EE0(actor, D_80078A66, 4, 0, 0);
          }

          switch (SPY.u164) {
          case 1:
            if (actor->unk3D != 0 && actor->unk3C != 0) {
              D_80075794 = 0;
              ANIM_RESET(actor, 0);
            }
            break;

          case 2:
            if (actor->unk3D != 2 && actor->unk3C != 2) {
              D_80075794 = 0;
              ANIM_RESET(actor, 2);
            }
            break;

          case 3:
            if (actor->unk3D != 4 && actor->unk3C != 4) {
              D_80075794 = 0;
              ANIM_RESET(actor, 4);
            }
            break;
          }
        }

        func_800529E4(actor, 4);
      }

      break;
    }

    /* Type 143. */
    case 143: {
      L33State143 *st = actor->state;

      st->u00 += dt * 14;
      actor->unk57 = 0xBB80 / st->u00;
      if (actor->unk57 < 5) {
        actor->unk57 = 5;
      }
      if (actor->unk57 >= 0x80) {
        actor->unk57 = 0x7F;
      }
      if (st->u04 != 0) {
        func_8004E2E8(&actor->posX, st->u00, 7);
      }
      if (actor->unk3D == 1 && D_80075794 != 0) {
        func_80052568(actor);
        continue;
      }
      if (func_80037F90(&st->u04, 4) != 0) {
        ANIM_SET(actor, 1);
      }
      continue;
    }




    /* Types 152/153: sparking debris. Falls under gravity, spinning, and
     * pops when it runs out of life or reaches the floor. */
    case 152:
    case 153: {
      DebrisFallState *st = actor->state;

      if (actor->unk51 == 0) {
        func_80052568(actor);
        continue;
      }
      if (--actor->unk49 == 0 || actor->posZ < st->floor) {
        D_800758E4(8, 0x46, &actor->posX, (void *)0x10);
        func_80052568(actor);
        continue;
      }
      func_80017758(&actor->posX, &actor->posX, st->vel);
      st->vel[2] -= 0xC;
      actor->unk44 += st->spin[0];
      actor->unk45 += st->spin[1];
      actor->unk46 += st->spin[2];
      if (!(actor->unk49 & 1)) {
        func_80017700(fx, &actor->posX);
        fx[3] = func_8006272C() & 3;
        fx[4] = func_8006272C() & 3;
        fx[5] = 0x14;
        col[0] = 0x80;
        col[1] = 0x70;
        col[2] = 0x40;
        D_800758E4(1, 0x11, fx, col);
      }
      break;
    }

    /* Type 159: a path patroller that turns back toward Spyro. */
    case 159: {
      L33State159 *st = actor->state;
      int dist;

      func_8001778C(o128, &SPY.posX, &actor->posX);
      dist = func_80017990(&actor->posX, &SPY.posX);
      if (dist < 0x32C8) {
        dist = func_800171FC(o128, 0);
      }

      if (actor->flags != 0 && actor->unk48 != 2) {
        st->u18 = 0xFA;
        if (actor->flags & 0x20000) {
          st->u18 = 0x145;
        }
        st->u14 = func_80038178(func_80016AB4(actor->posX - SPY.posX,
                                              actor->posY - SPY.posY, 0),
                                D_80078A66, 0x20, 0x40);
        st->u1C = 0x28;
        ENTER_POSE(actor, 2);
      }
      actor->flags = 0;

      if (st->u04 == 1 && actor->unk50 == 0 &&
          ((func_8002B3F4(4) >> 8) & 0xFF) > 0) {
        actor->unk50 = 0x28;
        D_80075828[st->u60].unk50 = 0x28;
      }

      if (actor->unk3D == 0 && actor->unk50 != 0) {
        if (actor->unk3F == 1 && !(func_8006272C() & 3) &&
            func_80056DC4(actor, D_80076378[actor->type]->m_Sounds[6]) == 0) {
          func_8003851C(actor, 6, 0);
        }
        if (actor->unk3F == 0xA && !(func_8006272C() & 3) &&
            func_80056DC4(actor, D_80076378[actor->type]->m_Sounds[5]) == 0) {
          func_8003851C(actor, 5, 0);
        }
      }

      switch (actor->unk48) {
      case 0:
        func_80038DC0(actor, 5, 0, 0);
        if (dist < 0x1964 &&
            (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x578
                               : -SPY_DZ(actor) < 0x578)) {
          st->u10 = 0x12C;
          if (st->u04 == 0) {
            st->path->dir = 1;
            st->u08 = 0x6E;
          } else {
            st->path->dir = 1;
            st->u08 = 0x96;
          }
          st->u08 -= D_80075838[st->u04] * 6;
          if (func_80038C4C(&SPY.posX, st->u28) != 0) {
            st->path->dir *= -1;
          }
          if (st->path->dir == -1) {
            st->path->cur = st->u20;
          } else {
            st->path->cur = st->u24;
          }
          ENTER_POSE(actor, 1);
        }
        break;

      case 1: {
        FlyPath *p;
        int spd;
        int r;
        int h;
        int z;

        if (actor->posZ < 0x800) {
          func_80017700(&actor->posX,
                        PATH_PT((unsigned char *)st->path, st->path->cur));
          func_80038458(actor);
        }
        if (func_80037F90(&st->u08, 4) != 0) {
          int lo = 0xDC - D_80075838[st->u04] * 6;

          st->u10 -= 0xA;
          if (st->u10 < lo) {
            st->u10 = lo;
          }
        }
        spd = st->u10;
        if (FLY_FLAGS(st->path, st->path->cur) & 0xFFC) {
          spd = FLY_FLAGS(st->path, st->path->cur) & 0xFFC;
        }
        r = func_80039E94(actor, st->path, 0x1C2, spd, 0, 0xA, 0x28, 0xFF, 1);
        h = func_80038400(actor, 0xBB8);
        st->u1C -= 0x1E;
        if (st->u1C < -0x190) {
          st->u1C = -0x190;
        }
        z = actor->posZ + st->u1C;
        if (z < h) {
          actor->posZ = h;
        } else {
          actor->posZ = z;
        }
        if ((r & 0x100) && (FLY_FLAGS(st->path, st->path->cur) & 3) == 1) {
          st->u1C = 0x12C;
        }
        if ((st->path->dir == -1 && r == (st->u20 | 0x100)) ||
            (st->path->dir == 1 && r == (st->u24 | 0x100))) {
          D_80075838[st->u04]++;
          if (D_80075838[st->u04] >= 6) {
            D_80075838[st->u04] = 5;
          }
          ENTER_POSE(actor, 0);
        }
        func_80038BB0((PathData *)st->path);
        func_80038AFC(st->path, o138);
        if (dist < 0x3000) {
          int a;
          int n;
          int i;

          a = func_80017908(
              func_80016AB4(FLY_X(st->path, st->path->cur) - actor->posX,
                            FLY_Y(st->path, st->path->cur) - actor->posY, 0),
              actor->unk46);
          n = 0;
          p = st->path;
          i = p->cur;
          while (i != o138[0]) {
            i -= p->dir;
            i = (i + p->count) % p->count;
            n++;
          }
          if ((unsigned)(n - 1) < 7 && a < 0x10 &&
              func_80017908(actor->unk46,
                            func_80016AB4(SPY.posX - actor->posX,
                                          SPY.posY - actor->posY, 0)) < 0x3A &&
              n < st->path->count >> 1) {
            st->path->dir = -st->path->dir;
            st->path->cur = (st->path->cur - st->path->dir + st->path->count) %
                            st->path->count;
          }
        }
        break;
      }

      case 2:
        func_80039910(actor, &st->u18, st->u14, &st->u1C, 0xC, sixteen);
        if (D_80075794 != 0) {
          func_800529E4(actor, 4);
          func_800385BC(actor, 0x20);
          func_80052568(actor);
          continue;
        }
        break;
      }

      if (actor->unk48 != 2 && st->u60 != -1) {
        func_800529E4(actor, 4);
        func_80052D64(actor, 0, &D_80075828[st->u60].posX);
        func_80038340(&D_80075828[st->u60]);
        func_800533D0(&D_80075828[st->u60]);
      }
      func_800529E4(actor, 1);
      continue;
    }

    /* Type 161: Gnasty Gnorc. Runs his escape paths, throws trail bombs
     * (169) at Spyro and drops the vault keys as he is hit. */
    case 161: {
      GnastyState *st = actor->state;
      int dist;

      dist = func_80017990(&actor->posX, &SPY.posX);
      func_8003AA84(actor);

      if (actor->flags & 0x10000) {
        if (st->phase == 2 &&
            func_80017990(&actor->posX,
                          PATH_END(st->path)) < 0xBB8 &&
            actor->unk48 != 7) {
          FlyPath *t;

          func_8002B390(1, 0xFC, 0);
          D_80075828[st->key].unk49 = 1;
          st->camTimer = 0xA0;
          st->u1C = 0;
          st->u20 = 0;
          st->u6C = 0;
          st->phase++;
          t = st->path;
          st->path = st->path2;
          st->path2 = t;
          ENTER_POSE(actor, 7);
        } else if (st->phase == 3 &&
                   func_80017990(&actor->posX,
                                 PATH_END(st->path)) < 0xBB8 &&
                   actor->unk48 != 8) {
          st->phase = 4;
          st->u18 = 0xD2;
          st->u14 = func_80016AB4(actor->posX - SPY.posX,
                                  actor->posY - SPY.posY, 0);
          if (actor->unk53 != 0xFF) {
            D_80077CC8[0] &= ~1;
          }
          func_8003ABC0(actor, 4, 0, 0);
          ENTER_POSE(actor, 8);
        }
      }

      if (st->phase < 2) {
        func_80017700(o140, &actor->posX);
        o140[2] += 0x3E8;
        if (D_80078AD0 == 0xB || D_80078AD0 == 0x14) {
          st->camTimer = 0x3C;
        }
        if (func_80037F90(&st->camTimer, 4) != 0 && dist < 0x4000 &&
            (func_80038250(o140) != 0 || dist < 0x3000)) {
          D_80078C4C = 0x80010000;
          D_80078C7C = actor;
        }
      }

      if (st->key2 != -1 && D_80075828[st->key2].unk49 != 0 &&
          func_80017908(D_80078A66,
                        func_80016AB4(actor->posX - SPY.posX,
                                      actor->posY - SPY.posY, 0)) < 0x28 &&
          (func_80017990(&actor->posX, &SPY.posX) < 0x1B58 ||
           st->throws >= 4)) {
        st->key2 = -1;
        actor->unk48 = 0xA;
        continue;
      }

      if (st->seen == 0 && D_80078AF4 == 0 &&
          (func_80038C4C(&SPY.posX, st->box70) != 0 ||
           func_80038C4C(&SPY.posX, st->boxA0) != 0)) {
        st->seen = 1;
      }

      if ((unsigned)(actor->unk48 - 6) < 2) {
        int ca = (func_80016AB4(PATH_X((unsigned char *)st->path2,
                                       st->path2->count - 1) -
                                    PATH_X((unsigned char *)st->path3, 0),
                                PATH_Y((unsigned char *)st->path2,
                                       st->path2->count - 1) -
                                    PATH_Y((unsigned char *)st->path3, 0),
                                0) +
                  0x80) &
                 0xFF;

        if (func_80037F90(&st->camTimer, 4) == 0) {
          int *anc;

          D_80078C4C |= 0x80002200;
          anc = PATH_END(*(FlyPath **)((char *)st + 4));
          D_80078C78 = &D_80078668;
          *(int *)&D_80078668 = -ca << 4;
          D_80078670 = 0x1770;
          D_8007866C = 0x19;
          D_80078674 = 0;
          D_80078678 = -0x32;
          D_8007867C = 0;
          D_8007570C = 1;
          D_80078C74 = anc;
        } else {
          D_8007570C = 0;
        }
      }

      if ((unsigned)(actor->unk48 - 8) < 2) {
        int ca = (SPY.bodyRotZ + 0x40) & 0xFF;
        int *anc;

        func_80017758(PATH_PT((unsigned char *)st->path, 0), &actor->posX,
                      &SPY.posX);
        func_800176C8(PATH_PT((unsigned char *)st->path, 0), 1);
        D_80078C4C = 0x80002200;
        anc = (int *)((char *)*(FlyPath **)st + 8);
        D_80078C78 = &D_80078668;
        *(int *)&D_80078668 = -ca << 4;
        D_80078670 = 0xFA0;
        D_8007866C = 0x19;
        D_80078674 = 0;
        D_80078678 = -0x32;
        D_8007867C = 0;
        D_80078C74 = anc;
      }

      actor->flags = 0;
      switch (actor->unk48) {
      case 0:
        func_80038458(actor);
        func_80038DC0(actor, 4, 0, 0);
        if (func_80037F90(&st->timer, 4) != 0 && st->seen != 0 &&
            func_80017908(((unsigned short)D_80076E20 << 16 >> 20),
                          func_80016AB4(actor->posX - D_80076DF8[0],
                                        actor->posY - D_80076DF8[1], 0)) <
                0x1E &&
            func_80017990(&SPY.posX, PATH_PT(st->home, 0)) < 0x3000 &&
            (unsigned)(dist - 0x1801) < 0x4FFF) {
          st->throws = 0;
          actor->unk48 = 2;
          continue;
        }
        break;

      case 2:
        if (st->phase == 2) {
          if (SPY_DZ(actor) < 0 && dist < 0x1388) {
            actor->unk48 = 5;
            continue;
          }
        } else if (st->phase == 3) {
          if (st->throws < 4) {
            if (dist >= 0xE10) {
              goto anim2;
            }
            st->timer = 0;
          } else {
            st->timer = 0x96;
          }
          actor->unk48 = 0xC;
          continue;
        } else if (st->throws >= 4 ||
                   func_80017908(((unsigned short)D_80076E20 << 16 >> 20),
                                 func_80016AB4(actor->posX - D_80076DF8[0],
                                               actor->posY - D_80076DF8[1],
                                               0)) > 0x28 ||
                   (unsigned)(dist - 0x1770) > 0x55F0) {
          st->timer = 0x96;
          ENTER_POSE(actor, 0);
        }
      anim2:
        if (func_80038DC0(actor, 5, 0x14, 1) == 0) {
          continue;
        }
        if (func_80037F90(&st->timer, 4) != 0) {
          ANIM_GO(actor, 2);
        } else if (D_80075794 != 0) {
          ANIM_GO(actor, 0);
        }
        if (st->timer == 0 && actor->unk3F >= 9) {
          Actor *o = D_800758CC(0xA9, actor);
          L33State169 *q = o->state;
          int n;
          int spd = 0x190;

          func_800529E4(actor, 4);
          func_80052D64(actor, 0, &o->posX);
          o->unk46 =
              func_80016AB4(SPY.posX - o->posX, SPY.posY - o->posY, 0);
          func_80017700(o160, &SPY.posX);
          o160[2] = func_8004D5EC(o160, 0x1000);
          func_8001778C(o150, o160, &o->posX);
          n = func_800171FC(o150, 0) / spd;
          q->u0C = spd;
          if (n <= 0) {
            n = 1;
          }
          q->u10 = (o160[2] - o->posZ) / n;
          if (q->u10 < -0x1F4) {
            q->u10 = -0x1F4;
          }
          q->trail = st->trails[st->throws % 3];
          q->h0A = 0;
          q->h06 = 0xA0;
          q->h08 = n - 1;
          st->throws++;
          func_8003851C(actor, 0, (int)&st->uD0);
          if (st->phase == 2 && st->throws >= 2) {
            actor->unk48 = 5;
            continue;
          }
          st->timer = 0x50;
        }
        break;

      case 3:
        func_80038DC0(actor, 6, 0, 0);
        if (D_80075794 != 0) {
          actor->unk48 = 5;
          continue;
        }
        break;

      case 4:
        GNASTY_FALL(actor, st);
        func_80038DC0(actor, 4, 0, 0);
        if ((dist < 0x2000 && SPY_DZ(actor) >= -0x6A3) || dist < 0x16A8) {
          st->u54 = 0;
          actor->unk48 = 5;
          continue;
        }
        if (func_80017990(&SPY.posX, PATH_PT((unsigned char *)st->path, 12)) <
            0x2000) {
          func_80017700(&actor->posX,
                        PATH_END(st->path));
          func_80038458(actor);
          st->path->cur = 0;
          actor->unk48 = 0x15;
          continue;
        }
        break;

      case 5: {
        int spd;
        int far;
        unsigned int i;
        int r;

        ANIM_GO(actor, 1);
        if (st->u20 != 0) {
          st->u1C = st->u20;
          st->u20 = 0;
        }
        i = st->path->cur;
        spd = 0x13B;
        if (FLY_FLAGS(st->path, i) & 0xFFC) {
          spd = FLY_FLAGS(st->path, i) & 0xFFC;
        }
        if (i >= 0xE) {
          far = 0x1964;
          if (st->u18 >= 0x10F) {
            far = 0x1F40;
          }
          spd = 0xEB;
          if (i >= 0x1D) {
            spd = 0xD7;
          }
          if (dist < far &&
              (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x4B0
                                 : -SPY_DZ(actor) < 0x4B0)) {
            spd = 0x122;
          }
        }
        st->u18 = spd;
        r = func_80039E94(actor, st->path, 0x200, spd, 0, 7, 0x28, 0xFF, 0);
        GNASTY_FALL(actor, st);
        if (r == 0x101 && func_80017990(&actor->posX, &SPY.posX) > 0x2000) {
          ENTER_POSE(actor, 4);
        }
        if (r == 0x100) {
          func_80038AFC(st->path, o170);
          if (dist > 0x4800 && o170[0] > 0x10 &&
              o170[0] < st->path->count - 1) {
            st->throws = 0;
            actor->unk48 = 2;
            continue;
          }
          st->u58 = 0xC8;
          actor->unk48 = 0x15;
          continue;
        }
        break;
      }

      case 6: {
        int spd;
        int r;
        int h;
        int z;

        if (D_80075794 != 0) {
          ANIM_GO(actor, 1);
        }
        if (func_80037F90(&st->u58, 4) == 0) {
          func_80038EE0(
              actor,
              func_80016AB4(
                  PATH_X((unsigned char *)st->path, st->path->cur) -
                      actor->posX,
                  PATH_Y((unsigned char *)st->path, st->path->cur) -
                      actor->posY,
                  0),
              6, 0, 0);
          h = func_80038400(actor, 0xBB8);
          st->u1C -= 0x19;
          if (st->u1C < -0x190) {
            st->u1C = -0x190;
          }
          z = actor->posZ + st->u1C;
          if (z < h) {
            actor->posZ = h;
            ANIM_GO(actor, 1);
          } else {
            actor->posZ = z;
          }
          break;
        }
        if (st->u20 != 0) {
          st->u1C = st->u20;
          st->u20 = 0;
        }
        spd = 0x104;
        if (FLY_FLAGS(st->path, st->path->cur) & 0xFFC) {
          spd = FLY_FLAGS(st->path, st->path->cur) & 0xFFC;
        }
        r = func_80039E94(actor, st->path, 0x12C, spd, 0, 0xA, 0x28, 0xFF, 0);
        GNASTY_FALL(actor, st);
        if (r == 0x100) {
          actor->unk48 = 0xC;
          continue;
        }
        if (st->u6C == 0 &&
            func_80017990(&actor->posX, PATH_PT((unsigned char *)st->path, 6)) <
                0x258) {
          st->u6C = 1;
          actor->unk48 = 0xB;
          continue;
        }
        if (r & 0x100) {
          switch (FLY_FLAGS(st->path, st->path->cur) & 3) {
          case 1:
            st->u20 = 0xDC;
            break;
          case 2:
            st->u20 = 0x10E;
            break;
          case 3:
            st->u20 = 0x140;
            break;
          }
        }
        break;
      }

      case 7:
        if (D_80075794 != 0) {
          st->u58 = 0;
          ANIM_GO(actor, 1);
          actor->unk48 = 6;
          continue;
        }
        break;

      case 8:
        D_80078C00[19] = D_80078C4C | 0x80002000;
        D_8007570C = 1;
        func_80039910(actor, &st->u18, st->u14, 0, 0xC, 0);
        if (st->drop[0] != -1 && actor->unk3F >= 6) {
          D_80075828[st->drop[0]].unk49 = 1;
          st->drop[0] = -1;
        }
        if (st->drop[1] != -1 && actor->unk3F >= 0xB) {
          D_80075828[st->drop[1]].unk49 = 1;
          st->drop[1] = -1;
        }
        if (st->drop[2] != -1 && actor->unk3F >= 0x10) {
          D_80075828[st->drop[2]].unk49 = 1;
          st->drop[2] = -1;
        }
        if (D_80075814 == 0 && actor->unk3F >= 0xE) {
          Actor *o = D_800758CC(0x19E, actor);

          *(Actor **)o->state = actor;
          func_80017700(&o->posX, &SPY.posX);
          o->unk44 = D_80078A64;
          o->unk45 = D_80078A65;
          o->unk46 = D_80078A66;
          D_80075814 = 1;
        }
        if (D_80075794 != 0) {
          actor->unk50 = 0;
          actor->unk51 = 0;
          func_800529E4(actor, 4);
          func_800385BC(actor, 0x40);
          st->u58 = 0x3C;
          actor->unk48 = 9;
          continue;
        }
        break;

      case 9:
        D_80078C4C |= 0x80002000;
        func_80037F90(&st->u58, 4);
        if (st->u58 < 0x10) {
          D_80075918 = sixteen - st->u58;
          if (D_80075918 >= 0x10) {
            D_80075918 = 0xF;
          }
        }
        if (st->u58 == 0) {
          D_8007A6A8[D_80075964] = 1;
          D_800757AC = 0;
          D_8007566C[0] = 2;
          D_800757D8 = 0xE;
          D_8007579C = 1;
          D_8007570C = 0;
          D_80075814 = 0;
          *D_80075680 = 0;
        }
        break;

      case 10:
        ANIM_GO(actor, 1);
        if (func_80038EE0(actor,
                          func_80016AB4(
                              PATH_X((unsigned char *)st->path, 1) -
                                  actor->posX,
                              PATH_Y((unsigned char *)st->path, 1) -
                                  actor->posY,
                              0),
                          7, 0x14, 1) != 0) {
          func_80039398(actor, 0xF0, 0, 0, 5);
        }
        if (func_80017990(&actor->posX, PATH_PT((unsigned char *)st->path, 1)) <
            0x320) {
          st->phase = 2;
          st->path->cur = 2;
          ENTER_POSE(actor, 4);
        }
        break;

      case 11:
        GNASTY_FALL(actor, st);
        if (func_80038DC0(actor, 5, 0, 0) != 0) {
          ANIM_GO(actor, 4);
        }
        if (func_80038C4C(&SPY.posX, st->box88) != 0) {
          func_8002B390(0, 0xFC, 0);
          ANIM_GO(actor, 1);
          actor->unk48 = 6;
          continue;
        }
        break;

      case 12: {
        int h = func_80038400(actor, 0xBB8);
        int z;

        ANIM_GO(actor, 0);
        st->u1C -= 0x19;
        if (st->u1C < -0x190) {
          st->u1C = -0x190;
        }
        z = actor->posZ + st->u1C;
        if (z < h) {
          actor->posZ = h;
        } else {
          actor->posZ = z;
        }
        func_80038DC0(actor, 4, 0, 0);
        if (st->timer == 0 && (unsigned)(dist - 0xE10) < 0x16A8 &&
            D_80078AF4 == 0) {
          st->throws = 0;
          actor->unk48 = 2;
          continue;
        }
        if (func_80037F90(&st->timer, 4) != 0 && dist < 0xE10) {
          ANIM_GO(actor, 3);
          actor->unk48 = 0xD;
          continue;
        }
        break;
      }

      case 13:
        func_80038DC0(actor, 6, 0, 0);
        if (D_80075794 != 0) {
          st->timer = 0x78;
          ANIM_GO(actor, 0);
          actor->unk48 = 0xC;
          continue;
        }
        break;

      case 20:
        if (func_80037F90(&st->u58, 4) != 0) {
          actor->unk48 = 5;
          continue;
        }
        if (func_80038DC0(actor, 6, 0, 0) != 0) {
          ANIM_GO(actor, 0);
        }
        break;

      case 21:
        if (func_80038DC0(actor, 6, 0, 0) != 0) {
          ANIM_GO(actor, 0);
        }
        if (func_80037F90(&st->u58, 4) != 0 && SPY_DZ(actor) < 0 &&
            func_80017990(&actor->posX, &SPY.posX) < 0x251C) {
          actor->unk48 = 5;
          continue;
        }
        if (dist < 0xE10) {
          ENTER_POSE_B(actor, 3);
        }
        break;
      }
      func_800529E4(actor, 1);
      continue;
    }

    /* Type 169: a rolling wave that lays a trail and spawns shockwaves (143)
     * where it lands. */
    case 169: {
      L33State169 *st = actor->state;
      int s = actor->unk48;

      if (s < 3) {
        int n;
        int i;

        if (s < 0) {
          goto out169;
        }

        func_80017700(o150, &actor->posX);
        if (actor->unk48 == 0) {
          st->h04 += D_800756CC;
          if (st->h04 >= 0xA) {
            Actor *o = D_800758CC(0xA8, actor);

            func_80017700(&o->posX, &actor->posX);
            o->unk49 = 0;
            o->unk45 = func_80016AB4(st->u0C, st->u10, 0);
            o->unk46 = actor->unk46;
            o->unk50 = 0x50;
            st->h04 = 0;
          }
          actor->posX += (st->u0C * D_8006CC78[actor->unk46]) >> 12;
          actor->posY += (st->u0C * g_anSineLut[actor->unk46]) >> 12;
          actor->posZ += st->u10;
          if (func_80037F90(&st->h06, 2) != 0) {
            actor->unk48 = 1;
          }
          if (actor->posX < 0 || actor->posY < 0 || actor->posZ < 0) {
            actor->unk50 = 0;
            actor->unk51 = 0;
            actor->unk48 = 1;
            continue;
          }
          if (--st->h08 < 0) {
            st->h08 = 0;
          }
          if (st->h08 == 0 && func_8004AE38(o150, &actor->posX) != 0) {
            Actor *o = D_800758CC(0x8F, actor);
            L33State143 *p = o->state;

            p->u00 = 0x190;
            p->u04 = 0x12;
            o->unk57 = 0xBB80 / p->u00;
            if (o->unk57 < 5) {
              o->unk57 = 5;
            }
            if (o->unk57 >= 0x80) {
              o->unk57 = 0x7F;
            }
            actor->unk48 = 1;
            func_8003851C(actor, 0, 0);
          }
          if (actor->unk48 == 1) {
            actor->unk50 = 0;
            actor->unk51 = 0;
          }
        }
        n = 0;
        if (actor->unk48 == 0 && D_80075684 < 3 && st->h0A < 0x78) {
          int k = st->h0A;

          func_80017700(st->trail[k].pos, &actor->posX);
          st->trail[k].age = 0;
          st->h0A++;
        }
        for (i = 0; i < st->h0A; i++) {
          st->trail[i].age += D_800756CC;
          if (st->trail[i].age < 0x51) {
            n++;
          }
        }
        if (n == 0 && actor->unk48 != 0) {
          func_80052568(actor);
          continue;
        }
        if (D_80075684 < 3) {
          int k = D_80075684;

          D_800772C8[k] = st->h0A;
          D_80075684 = k + 1;
          D_80078658[k] = st->trail;
        }
        if (actor->unk48 == 1) {
          actor->unk48 = 2;
        }
      }
    out169:
      func_800529E4(actor, 1);
      continue;
    }





    /* Ambient flyer (FlyState): circles its anchor and pops near Spyro. */
    case 173: {
      FlyState *st = actor->state;

      if (actor->unk49 == 0) {
        actor->unk44 = (unsigned short)D_8006CC78[st->angle] >> 7;
        actor->unk45 = (unsigned short)D_8006CBF8[st->angle] >> 7;
        st->angle += D_800756CC * 2;

        if (func_80017990(&actor->posX, &SPY.posX) < 0x200) {
          int height = actor->posZ - actor->unk38;
          int drop = height - SPY.posZ;

          if (drop > 0 ? drop < 0x200 : (SPY.posZ - height) < 0x200) {
            int i;

            func_80055A78(D_800761D4[0], actor, 0x10, 0);
            func_800529E4(actor, 4);

            for (i = 0; i < 6; i++) {
              D_800758E4(1, 0xC, actor, (int *)(0x8080 + (i << 24)));
            }

            D_80075830 = 1;
            actor->unk57 = 0x40;
            actor->unk50 = 0;
            actor->unk52 = 0;
            actor->unk51 = 0;
            actor->unk1C = 0;
            actor->unk4A = 0xFF;
            actor->unk49 = 2;
          }
        }

        if (st->attach != 0xFF) {
          func_800529E4(actor, 4);
          func_80017048(&actor->unk20[0], &D_8006E5AC,
                        D_80077108[st->attach].pos);
          func_80017758(D_80077108[st->attach].pos, D_80077108[st->attach].pos,
                        &actor->posX);
          if (D_80077108[st->attach].flag < 5) {
            st->attach = 0xFF;
          }
        } else if (st->timer >= 0xF8) {
          int handle = func_8003AAEC(actor, &D_8006E5AC);

          if (handle >= 0 && func_80017990(&actor->posX, &SPY.posX) < 0x4000) {
            st->attach = handle;
          }
          st->timer = (func_8006272C() & 0x3F) + 0x18;
        }

        st->timer += D_800756CC;
      }

      break;
    }

    /* Type 181: loot vortex (LootVortexState). */
    case 181: {
      LootVortexState *st = actor->state;
      unsigned int ks = D_80075828[st->key].unk48;
      int kt = D_80075828[st->key].type;
      int open = 0;

      if (ks >= 0x80 || (kt == 0x9F && ks == 2)) {
        open = 1;
      }
      if (actor->unk49 == 0) {
        if (open) {
          actor->unk44 = (unsigned short)D_8006CC78[st->phase] >> 7;
          actor->unk45 = (unsigned short)D_8006CBF8[st->phase] >> 7;
          st->phase += D_800756CC * 2;
        }
        if ((func_80017990(&actor->posX, &SPY.posX) < 0x200 &&
             ABS2(SPY_DZ(actor)) < 0x200 && open) ||
            (D_80075828[st->key].unk48 >= 0x80 &&
             D_80075828[st->key].type == 0xB1)) {
          int i;
          int c;

          func_80055A78(D_800761D4[0], actor, 0x10, 0);
          func_800529E4(actor, 4);
          i = 0;
          c = 0x8080;
        spark:
          D_800758E4(1, 0xC, (int *)actor, (void *)c);
          i++;
          c += 0x1000000;
          if (i < 6) {
            goto spark;
          }
          D_80075830 = 1;
          actor->unk50 = 0;
          actor->unk52 = 0xFF;
          actor->unk51 = 0;
          actor->unk1C = 0;
          actor->unk57 = 0x40;
          actor->unk4A = 0xFF;
          actor->unk49 = 1;
          st->mode = 1;
        }
        if (st->spark != 0xFF && actor->unk50 != 0) {
          func_800529E4(actor, 4);
          func_80017048(actor->unk20, &D_8006E5AC, D_80077108[st->spark].pos);
          func_80017758(D_80077108[st->spark].pos, D_80077108[st->spark].pos,
                        &actor->posX);
          if (D_80077108[st->spark].flag < 5) {
            st->spark = 0xFF;
          }
        } else if (st->timer >= 0xF8 && actor->unk50 != 0) {
          int s = func_8003AAEC(actor, &D_8006E5AC);

          if (s >= 0 && func_80017990(&actor->posX, &SPY.posX) < 0x4000) {
            st->spark = s;
          }
          st->timer = (func_8006272C() & 0x3F) + 0x18;
        }
        st->timer += D_800756CC;
      }

      switch (st->mode) {
      case 0: {
        if (D_80075828[st->key].type != 0xB1) {
          break;
        }
        if (D_80075828[st->key].unk48 < 0x7F) {
          actor->unk50 = 0;
          actor->unk52 = 0xFF;
          actor->unk51 = 0;
          actor->unk1C = 0;
          actor->unk57 = 0x40;
          actor->unk4A = 0xFF;
          break;
        }
        actor->unk49 = 1;
        st->mode = 1;
        D_80075830 = 1;
        break;
      }

      case 1:
        if (((unsigned)SPY.state < 2 || SPY.state == 0x15 || SPY.state == 2) &&
            func_80038C4C(&SPY.posX, st->pad) &&
            func_80017908(D_80078A66,
                          func_80016AB4(D_80075828[st->target].posX - SPY.posX,
                                        D_80075828[st->target].posY - SPY.posY,
                                        0)) < 0x30) {
          D_80078C4C = 0x80002000;
          func_8003DFA4();
          func_800176F0(&spyc->headLook);
          actor->unk49 = 2;
          actor->unk44 = 0;
          actor->unk45 = 0;
          actor->unk46 = D_80078A66 + 0x80;
          actor->unk51 = 1;
          actor->unk50 = 0x18;
          st->mode = 2;
          st->timer = 0;
          if (st->seen != 0) {
            if (D_8007566C[st->seen] != 0) {
              int d;
              int r;

              st->timer = 0x80;
              d = func_80017908(D_80075828[st->target].unk46,
                                (unsigned char)(SPY.bodyRotZ + 0x40));
              actor->unk45 = 0xC0;
              r = D_80078A66 + 0x40;
              actor->unk46 = r + d * (st->timer - 0x80) / 64;
            } else {
              D_8007566C[st->seen] = 1;
            }
          }
          func_80017700(&actor->posX, &SPY.posX);
        }
        break;

      case 2:
        if (D_80077378 & 0x40) {
          st->timer = 0xD2;
        }
        D_80078C4C = 0x80002000;
        actor->unk50 = 0x18;
        st->timer += D_800756CC;
        D_8007570C = 1;
        if (st->timer < 0xC0) {
          int *v = o150;

          v[0] = 0x200;
          v[1] = 0;
          v[2] = 0x140;
          func_80017048(actor->unk20, v, v);
          func_80017758(v, v, &D_80075828[st->target].posX);
          func_8001778C(v, v, &SPY.posX);
          v[2] -= 0x100;
          func_800176C8(v, 5);
          func_800177C0(v, v, st->timer);
          func_800177F8(v, v, 6);
          func_80017758(v, v, &SPY.posX);
          v[2] += 0x100;
          v[2] += (unsigned short)D_8006CBF8[(st->timer << 7) / 192] << 16 >> 19;
          func_80017700(&actor->posX, v);
          D_80078C00[0] =
              (unsigned short)D_8006CBF8[(st->timer << 7) / 192] << 16 >> 19;
          if (st->timer < 0x40) {
            int r;

            actor->unk45 = -st->timer;
            r = D_80078A66 + 0x80;
            actor->unk46 = r + st->timer * 4;
          } else if (st->timer < 0x80) {
            int r;

            actor->unk45 = 0xC0;
            r = D_80078A66 + 0x80;
            actor->unk46 = r + (st->timer - 0x40) * 3;
          } else {
            int d = func_80017908(D_80075828[st->target].unk46,
                                  (unsigned char)(D_80078A66 + 0x40));
            int r = D_80078A66 + 0x40;

            actor->unk46 = r + d * (st->timer - 0x80) / 64;
          }
        } else if (st->timer < 0xD0) {
          int *w = w181;

          actor->unk46 = D_80075828[st->target].unk46;
          w[0] = 0x200 - (st->timer - 0xC0) * 8;
          w[2] = 0x140;
          w[1] = 0;
          func_80017048(actor->unk20, w, w);
          func_80017758(&actor->posX, w, &D_80075828[st->target].posX);
        }
        if (st->timer >= 0xD0 || !((unsigned)SPY.state < 2 || SPY.state == 0x15 ||
                                    SPY.state == 2 || SPY.state == 3)) {
          actor->unk50 = 0;
          actor->unk51 = 0;
          D_8007570C = 0;
          D_800758E4(0x20, 0x46, &actor->posX, (void *)0x18);
          func_8003B7C0(actor);
          st->mode = 3;
        }
        func_800529E4(actor, 4);
        if ((short)(st->timer % 4) < 2) {
          D_800758E4(1, 0xC, (int *)actor, (void *)0x8080);
        } else {
          D_800758E4(1, 0xC, (int *)actor, (void *)0x5008080);
        }
        break;

      case 3:
        func_80055A78(D_800761D4[0x3C], 0, 0x10, 0);
        func_8002B390(st->snd, 0xFC, 0);
        D_80078C00[1] = 0;
        D_80078C00[0] = 0;
        D_80078C00[-1] = 0;
        D_80075830 = 0;
        switch (st->snd) {
        case 0:
          D_80075678 = 0x563B;
          break;
        case 1:
          D_80075678 = 0x4E3C;
          break;
        case 2:
          D_80075678 = 0x663A;
          break;
        case 3:
          D_80075678 = 0x5E3A;
          break;
        }
        func_80052568(actor);
        continue;
      }
      break;
    }




    /* Wooden chest (BreakState). */
    case 194: {
      BreakState *st;
      int i;

      st = (BreakState *)actor->state;
      if (st->unk04 != 0) {
        func_80038458(actor);
      }
      if ((actor->flags & 0xB0000) == 0) {
        continue;
      }

      actor->unk55 = 0x20;
      func_8003851C(actor, 0, 0);

      /* Two of each of the first pair, then six of the third. */
      for (i = 0; i < 2; i++) {
        if (D_800756A8 - D_800756A4 < 0x15) {
          break;
        }
        D_800758CC(0xFF, actor);
        D_800758CC(0x100, actor);
      }
      for (i = 0; i < 6; i++) {
        if (D_800756A8 - D_800756A4 < 0x15) {
          break;
        }
        D_800758CC(0x101, actor);
      }

      D_800758E4(5, 2, &actor->posX, 0);
      D_800758E4(0x10, 0x46, &actor->posX, (void *)0x20);
      func_8003ABC0(actor, 3, 0, 0);
      func_8003B7C0(actor);
      func_80052568(actor);
      continue;
    }



    /* Wobbling chest (WobbleState): plays a recoil pose, then breaks. */
    case 195: {
      WobbleState *st = actor->state;

      if (st->unk10 != 0) {
        func_80038458(actor);
      }

      if (st->phase != 0) {
        int phase = st->phase + D_800756CC;

        st->phase = phase;

        if (phase < 0x40) {
          int dx;
          int dy;

          actor->unk44 = D_8006E638[st->phase >> 1].a + st->rotX;
          actor->unk45 = D_8006E638[st->phase >> 1].b + st->rotY;

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
          actor->unk44 = st->rotX;
          actor->unk45 = st->rotY;
          actor->posZ = st->restZ;
        }
      }

      if (actor->flags & 0xA0000) {
        int i;

        actor->unk55 = 0x20;
        func_8003851C(actor, 0, 0);

        for (i = 0; i < 2; i++) {
          if ((D_800756A8 - D_800756A4) < 0x15) {
            break;
          }
          D_800758CC(0x136, actor);
          D_800758CC(0x137, actor);
        }

        for (i = 0; i < 6; i++) {
          if ((D_800756A8 - D_800756A4) < 0x15) {
            break;
          }
          D_800758CC(0x135, actor);
        }

        D_800758E4(0x10, 0x46, &actor->posX, (int *)0x18);
        func_8003ABC0(actor, 3, 0, 0);
        func_8003B7C0(actor);
        func_80052568(actor);
      } else {
        st->handle = func_8003A9EC(actor, st->handle);

        if ((actor->flags & 0x10000) && st->phase == 0) {
          st->phase = 1;
          st->rotX = actor->unk44;
          st->rotY = actor->unk45;
          st->restZ = actor->posZ;
        }
      }

      actor->flags = 0;
      break;
    }



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

          actor->unk46 += 1;

          st->vel[2] -= 6;
          if (st->vel[2] < -0x80) {
            st->vel[2] = -0x80;
          }

          actor->posX += st->vel[0];
          actor->posY += st->vel[1];
          actor->posZ += st->vel[2];

          if (actor->posZ > 1023) {
            if (func_8004BE4C(&actor->posX, 0x100, 0x100)) {
              int *nrm = &D_80077368;
              int dot;

              actor->posX = D_80076B80;
              actor->posY = AS_I32(D_80076B84);
              actor->posZ = AS_I32(D_80076B88);

              func_80017330(nrm, 0x1000);

              dot = (st->vel[0] * nrm[0] + st->vel[1] * D_8007736C +
                     st->vel[2] * D_80077370) >>
                    11;

              if (dot < 0) {
                func_800175B8(nrm, 0x1000, -dot);
                st->vel[0] += nrm[0];
                st->vel[1] += AS_I32(D_8007736C);
                st->vel[2] += AS_I32(D_80077370);
              }
            }
            st->life--;
          } else {
            func_80052568(actor);
          }
        } else {
          D_800758E4(10, 0x47, &actor->posX, 0);
          func_80052568(actor);
        }
      }
      break;
    }





    /* Sparkling shard: 1 puff per frame. */
    case 309: {
      ShardState *st = actor->state;

      if (st->life != 0 && actor->unk51 != 0) {
        actor->posX += st->vel[0];
        actor->posY += st->vel[1];
        st->vel[2] -= 6;
        if (st->vel[2] < -0x80) {
          st->vel[2] = -0x80;
        }
        actor->posZ += st->vel[2];
        actor->unk44 += st->spin[0];
        actor->unk45 += st->spin[1];
        actor->unk46 += st->spin[2];

        s392[0] = (func_8006272C() & 0xFE) - 0x7F;
        s392[1] = (func_8006272C() & 0xFE) - 0x7F;
        s392[2] = (func_8006272C() & 0xFE) - 0x40;
        func_80017758(s392, s392, &actor->posX);
        D_800758E4(1, 0x42, s392, (int *)1);
        st->life -= 1;
      } else {
        func_80052568(actor);
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
        if (actor->unk51 == 0) {
          func_80052568(actor);
          continue;
        }

        if (func_8004BE4C(&actor->posX, 0x100, 0x100) != 0) {
          int *nrm = &D_80077368;
          int along;

          func_80017330(nrm, 0x1000);
          along = (st->vel[0] * nrm[0]) + (st->vel[1] * D_8007736C) +
                  (st->vel[2] * D_80077370);
          i = along >> 11;

          if (i < 0) {
            func_800175B8(nrm, 0x1000, (along >> 13) - i);
            st->vel[0] += nrm[0];
            st->vel[1] += AS_I32(D_8007736C);
            st->vel[2] += AS_I32(D_80077370);
          }
        }

        actor->posX += st->vel[0];
        actor->posY += st->vel[1];
        st->vel[2] -= 6;
        if (st->vel[2] < -0x80) {
          st->vel[2] = -0x80;
        }
        actor->posZ += st->vel[2];
        actor->unk44 += st->spin[0];
        i = 0;
        {
          actor->unk45 += st->spin[1];
          actor->unk46 += st->spin[2];

          do {
            i += 1;
            v310[0] = (func_8006272C() & 0xFE) - 0x7F;
            v310[1] = (func_8006272C() & 0xFE) - 0x7F;
            v310[2] = (func_8006272C() & 0xFE) - 0x40;
            func_80017758(v310, v310, &actor->posX);
            D_800758E4(1, 0x42, v310, (int *)1);
          } while (i < 3);
        }

        st->life -= 1;
        break;
      }

      if (actor->unk51 != 0) {
        D_800758E4(8, 0x46, &actor->posX, (int *)0x10);
      }
      func_80052568(actor);
      continue;
    }



    /* Type 312: cracking boulder. Shakes and smokes when hit by a charge or
     * flame, then bursts into three rings of debris and knocks the camera
     * back. */
    case 312: {
      L16Boulder *st = actor->state;

      switch (actor->unk49) {
      case 0:
        *(int *)&actor->unk4C = D_8006E52C[actor->unk53];
        actor->unk49 = 1;
        break;

      case 1:
        if (actor->flags & 0x40000) {
          actor->unk49 = 2;
          actor->unk52 = 0xFF;
          actor->unk50 = 0x7F;
          if (st->timer < 0x5A) {
            st->timer = 0x5A;
          }
        } else if (actor->flags & 0x10000) {
          actor->unk49 = 2;
          actor->unk52 = 0xFF;
          actor->unk50 = 0x7F;
          st->timer = 0;
        } else {
          goto hit;
        }
        st->rotX = actor->unk44;
        st->rotY = actor->unk45;
        st->restZ = actor->posZ;
        break;

      hit:
        if (actor->flags & 0xA0000) {
          actor->unk49 = 3;
          actor->unk50 = 0;
          actor->unk51 = 0;
          st->timer = 0;
        }
        break;

      case 2: {
        int t = st->timer + D_800756CC;
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
          func_80017048(actor->unk20, ps, ps);
          func_80017758(ps, ps, &actor->posX);
          ps[3] = D_8006E614[i >> 1].vel[0];
          ps[4] = D_8006E614[i >> 1].vel[1];
          ps[5] = D_8006E614[i >> 1].vel[2];
          func_80017048(actor->unk20, ps + 3, ps + 3);
          ps[3] += (func_8006272C() & 0xF) - 8;
          ps[4] += (func_8006272C() & 0xF) - 8;
          D_800758E4(1, 0x4A, ps, ps + 3);
        }
        n = st->timer / 40 + 1;
        for (i = 0; i < n; i++) {
          ps[0] = (func_8006272C() & 0x1FF) - 0xFF;
          ps[1] = (func_8006272C() & 0x1FF) - 0xFF;
          ps[2] = (func_8006272C() & 0x7F) + 0xC0;
          ps[3] = ps[0] >> 4;
          ps[4] = ps[4] >> 4;
          ps[5] = (func_8006272C() & 0xF) + 8;
          func_80017048(actor->unk20, ps, ps);
          func_80017758(ps, ps, &actor->posX);
          func_80017048(actor->unk20, ps + 3, ps + 3);
          s448[0] = (func_8006272C() & 7) + 0x10;
          s448[1] = (func_8006272C() & 7) + 6;
          D_800758E4(1, 0x10, ps, s448);
          if (actor->unk54 == 0x7F) {
            func_8003851C(actor, 0, 0);
          }
        }
        if (st->timer >= 0x39) {
          actor->unk44 = D_8006E638[(0x78 - st->timer) >> 1].a + st->rotX;
          actor->unk45 = D_8006E638[(0x78 - st->timer) >> 1].b + st->rotY;
          actor->posZ =
              st->restZ +
              (ABS2((signed char)D_8006E638[(0x78 - st->timer) >> 1].a) +
               ABS2((signed char)D_8006E638[(0x78 - st->timer) >> 1].b)) *
                  6;
        }
        } else {
          actor->unk49 = 3;
          actor->unk50 = 0;
          actor->unk51 = 0;
          st->timer = 0;
        }
        break;
      }

      case 3:
        if (st->timer == 0) {
          int i;
          Actor *c;
          DebrisFallState *cs;

          func_8003851C(actor, 1, 0);
          func_8003ABC0(actor, 6, 0, 0);
          for (i = 0; i < 8; i++) {
            short *p;

            if (D_800756A8 - D_800756A4 < 0x15) {
              break;
            }
            p = D_800758CC(0x97, actor)->state;
            p[0] >>= 1;
            p[1] >>= 1;
          }
          for (i = 0; i < 16; i++) {
            if (D_800756A8 - D_800756A4 < 0x15) {
              break;
            }
            c = D_800758CC(0x99, actor);
            cs = c->state;
            c->unk49 = (func_8006272C() & 1) + 0x12;
            func_80017700(&c->posX, &actor->posX);
            c->posX += (D_8006CC78[i * 16] >> 4) + (func_8006272C() & 0x1F) - 0xF;
            c->posY += (D_8006CBF8[i * 16] >> 4) + (func_8006272C() & 0x1F) - 0xF;
            c->posZ += (func_8006272C() & 0xFF) + 0x80;
            cs->vel[0] = D_8006CC78[i * 16] / 32 + (func_8006272C() & 0x1F) - 0xF;
            cs->vel[1] = D_8006CBF8[i * 16] / 32 + (func_8006272C() & 0x1F) - 0xF;
            cs->vel[2] = (func_8006272C() & 0x3F) + 0x30;
            c->unk44 = func_8006272C();
            c->unk45 = func_8006272C();
            c->unk46 = func_8006272C();
            cs->spin[0] = func_8006272C() & 0xF;
            cs->spin[1] = func_8006272C() & 0xF;
            cs->spin[2] = func_8006272C() & 0xF;
            cs->floor = actor->posZ;
          }
          for (i = 0; i < 12; i++) {
            if (D_800756A8 - D_800756A4 < 0x15) {
              break;
            }
            c = D_800758CC(0x98, actor);
            cs = c->state;
            c->unk49 = (func_8006272C() & 3) + 0x19;
            func_80017700(&c->posX, &actor->posX);
            c->posX += (D_8006CC78[(i << 8) / 12] >> 4) + (func_8006272C() & 0x3F) - 0x1F;
            c->posY += (D_8006CBF8[(i << 8) / 12] >> 4) + (func_8006272C() & 0x3F) - 0x1F;
            c->posZ += (func_8006272C() & 0xFF) + 0x80;
            cs->vel[0] = (short)(D_8006CC78[(i << 8) / 12] / 56) + (func_8006272C() & 0x1F) - 0xF;
            cs->vel[1] = (short)(D_8006CBF8[(i << 8) / 12] / 56) + (func_8006272C() & 0x1F) - 0xF;
            cs->vel[2] = (func_8006272C() & 0x7F) + 0x60;
            c->unk44 = func_8006272C();
            c->unk45 = func_8006272C();
            c->unk46 = func_8006272C();
            cs->spin[0] = func_8006272C() & 0xF;
            cs->spin[1] = func_8006272C() & 0xF;
            cs->spin[2] = func_8006272C() & 0xF;
            cs->floor = actor->posZ;
          }
        }
        if (st->pushed == 0 &&
            func_8004E2E8(&actor->posX, (st->timer + 1) << 8, 0x86) != 0) {
          st->pushed = 1;
          D_80078C60.x = SPY.posX - actor->posX;
          D_80078C68 = 0;
          D_80078C64 = SPY.posY - actor->posY;
          if (D_80078C60.x != 0 || D_80078C64 != 0) {
            int len = func_800171FC(&D_80078C60.x, 1);

            func_800175B8(&D_80078C60.x, len,
                          (0x1000 - func_80017990(&actor->posX, &SPY.posX)) /
                              48);
          }
          D_80078C68 = 0x46;
        }
        func_8004E3C8(&actor->posX, (st->timer + 1) << 8, 0, 0x50000, actor, 0);
        if (++st->timer >= 8) {
          func_8003B7C0(actor);
          func_80052568(actor);
        }
        break;
      }
      actor->flags = 0;
      break;
    }






    /* Type 329: seed pod. Wobbles when struck and holds a floating prize
     * above itself; Spyro takes the prize by touching it (or by being in a
     * grab state), otherwise the pod settles back after a while. */
    case 329: {
      L16Pod *st = actor->state;

      actor->unk55 = 0x20;
      switch (actor->unk48) {
      case 0:
        if (actor->unk49 == 0) {
          *(int *)&actor->unk4C = D_8006E52C[actor->unk53];
          actor->unk49 = 1;
          actor->unk52 = 0x10;
        }
        if (actor->flags & 0xB0000) {
          st->timer = 0;
          st->rotX = actor->unk44;
          st->rotY = actor->unk45;
          st->restZ = actor->posZ;
          actor->unk52 = 0x40;
          st->prize = func_8003ABC0(actor, 5, 0, 0);
          st->prize->unk49 = 4;
          st->prize->unk50 = 0;
          st->prize->unk51 = 0;
          st->prize->unk44 = 0;
          st->prize->unk45 = 0;
          D_800758E4(8, 0x40, &actor->posX, (void *)(st->prize->type - 0x53));
          func_8003851C(actor, 0, 0);
          actor->unk48 = 1;
          ANIM_RESET(actor, 1);
          continue;
        }
        if (st->timer >= 0xF8) {
          func_8003AAEC(actor, D_8006E5C4[func_8006272C() & 3]);
          st->timer = (func_8006272C() & 0x3F) + 0x18;
        }
        st->timer += D_800756CC;
        break;

      case 1: {
        int i;

        st->prize->unk50 = 0x18;
        func_80017700(&st->prize->posX, &actor->posX);
        st->prize->posZ += func_80016C58((st->timer << 11) / 90) / 3 + 0x1A4;
        st->prize->unk46 += D_800756CC * 2;
        if (st->timer < 0x40) {
          int dx;
          int dy;

          actor->unk44 = D_8006E638[st->timer >> 1].a + st->rotX;
          actor->unk45 = D_8006E638[st->timer >> 1].b + st->rotY;
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
            (st->timer & 3) <= ((st->timer + D_800756CC) & 3)) {
          func_800176F0(s448);
          D_800758E4(1, 0, &st->prize->posX, s448);
        }
        if ((func_80017990(&st->prize->posX, &spy->posX) < 0x1E0 &&
             (unsigned int)(SPY.posZ - st->prize->posZ + 0xDF) < 0x2BF) ||
            D_80078AD0 == 0x2C || D_80078AD0 == 0x18 ||
            (D_80078AD0 == 0x14 && (D_80078AD4 & 0x40))) {
          func_8003B9D4(st->prize);
          func_80052568(st->prize);
          func_8003851C(actor, 2, 0);
          for (i = 0; i < 2; i++) {
            if (D_800756A8 - D_800756A4 < 0x15) {
              break;
            }
            D_800758CC(0x43, actor);
            D_800758CC(0x44, actor);
          }
          for (i = 0; i < 6; i++) {
            if (D_800756A8 - D_800756A4 < 0x15) {
              break;
            }
            D_800758CC(0x45, actor);
          }
          D_800758E4(5, 2, &actor->posX, 0);
          D_800758E4(0x10, 0x46, &actor->posX, (void *)0x20);
          func_80052568(actor);
          continue;
        }
        if (actor->flags & 0x80000) {
          st->prize->unk49 = 2;
          func_8003851C(actor, 2, 0);
          for (i = 0; i < 2; i++) {
            if (D_800756A8 - D_800756A4 < 0x15) {
              break;
            }
            D_800758CC(0x43, actor);
            D_800758CC(0x44, actor);
          }
          for (i = 0; i < 6; i++) {
            if (D_800756A8 - D_800756A4 < 0x15) {
              break;
            }
            D_800758CC(0x45, actor);
          }
          D_800758E4(5, 2, &actor->posX, 0);
          D_800758E4(0x10, 0x46, &actor->posX, (void *)0x20);
          func_80052568(actor);
          continue;
        }
        st->timer += D_800756CC;
        if (st->timer >= 0x56) {
          int j;

          for (j = 0; j < 6; j++) {
            func_80017700(s480, &st->prize->posX);
            s480[0] += D_8006CC78[j * 43] >> 6;
            s480[1] += D_8006CBF8[j * 43] >> 6;
            s480[2] += 0x28;
            col2[0] = D_8006CC78[j * 43] >> 7;
            col2[1] = D_8006CBF8[j * 43] >> 7;
            col2[2] = 0x10;
            D_800758E4(1, 0, s480, col2);
          }
          func_80052568(st->prize);
          actor->unk3A &= 0x7F;
          actor->unk52 = 0x10;
          actor->unk44 = st->rotX;
          actor->unk45 = st->rotY;
          actor->posZ = st->restZ;
          st->timer = 0;
          func_8003851C(actor, 1, 0);
          actor->unk48 = 0;
          ANIM_RESET(actor, 0);
          continue;
        }
        break;
      }
      }
      actor->flags = 0;
      break;
    }

    /* Type 414: turns toward the actor it follows. */
    case 414: {
      Actor **st = actor->state;

      func_80038EE0(actor,
                    func_80016AB4((*st)->posX - actor->posX,
                                  (*st)->posY - actor->posY, 0),
                    4, 0, 0);
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

      if (st->parent->unk48 == 0 || st->parent->unk49 != actor->unk48) {
        func_80052568(actor);
        continue;
      }

      if (st->parent->type == 9) {
        if (func_80017990(&st->parent->posX, &spy->posX) < 0x400) {
          unsigned char was = actor->unk49;
          int prev = D_8006CC78[was];
          unsigned char now = was + 8;

          actor->unk49 = now;
          actor->unk46 =
              actor->unk46 - ((prev * 3) >> 9) + ((D_8006CC78[now] * 3) >> 9);
        } else {
          int bearing;
          int arm;
          int idx;
          short *swing;

          actor->unk49 += 8;
          bearing = func_80016AB4(spy->posX - st->parent->posX,
                                  SPY.posY - st->parent->posY, 0);
          s448[0] = (D_8006CC78[bearing] * 3) >> 4;
          s448[1] = (D_8006CBF8[bearing] * 3) >> 4;
          s448[2] = 0;
          s480[0] = (unsigned short)D_8006CC78[(bearing + 0x40) & 0xFF] << 16 >> 21;
          s480[1] = (unsigned short)D_8006CBF8[(bearing + 0x40) & 0xFF] << 16 >> 21;
          s480[2] = 0;
          bearing = (bearing + 0x80) & 0xFF;
          actor->unk46 = bearing + ((D_8006CC78[actor->unk49] * 3) >> 9);
          func_800177C0(s496, s480, st->radius - 1);
          func_800176A0(s480, 1);
          arm = st->radius - 1;
          s496[2] += (D_8006CC78[arm * 2] * 3) >> 3;
          s512[0] = s448[0] * D_8006CC78[arm * 2];
          s512[1] = s448[1] * D_8006CC78[arm * 2];
          s512[2] = 0;
          bearing = arm << 1;
          bearing = (bearing - (st->phase * 4)) & 0xFF;
          func_800177C0(s480, s480, st->phase);
          func_8001778C(s496, s496, s480);
          swing = &D_8006CC78[bearing];
          actor->posX = s448[0] * *swing;
          actor->posY = s448[1] * *swing;
          actor->posZ = 0;
          func_8001778C(&actor->posX, &actor->posX, s512);
          func_800176C8(&actor->posX, 0xA);
          func_80017758(&actor->posX, &actor->posX, s448);
          func_80017758(&actor->posX, &actor->posX, &st->parent->posX);
          func_8001778C(&actor->posX, &actor->posX, s496);
          actor->posZ = actor->posZ + ((*swing * 3) >> 3) + 0x600;
        }
      } else {
        unsigned char now = actor->unk49 + 8;

        actor->unk49 = now;
        actor->unk46 = actor->unk38 + ((D_8006CC78[now] * 3) >> 9);
      }

      break;
    }
}
  }
}
