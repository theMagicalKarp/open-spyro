/* func_level_0_8007D9C8 -- per-frame actor update for level 0.
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class: a small
 * state machine over the actor's state byte (->unk48), often with a sub-state
 * (->unk49), and usually a class-specific state block behind ->state.
 *
 * Layout of this file:
 *   Core records      Actor, the per-type Model table, waypoint paths
 *   Per-class blocks  one state struct per actor class, each followed by the
 *                     globals and callees its case uses, in roughly case order
 *   Shared objects    the camera (CAM) and Spyro (SPY), each declared as one
 *                     aggregate over its whole address range
 *   Helper macros     absolute value and the three animation idioms
 *   The function      update-list preamble, then the per-class dispatch
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
} Actor; /* 0x58 */

/* The update list, and the per-actor values the loop publishes before it
 * dispatches. */
extern Actor *D_800700F4[]; /* live actor list, NULL-terminated */
extern int D_800756CC;      /* frame delta in ticks */
extern int D_800756C4;      /* frame delta, as seen by the callees */
extern int D_80075794;      /* current actor's animation finished */
extern int D_800757F4;      /* current actor's new-frame flag */

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
extern void func_80017C24(int *d, short *s);

/* Mark an item collected; the second argument is the actor or its id. */
extern void func_8003B854(int a, void *b);

extern void func_80055A78(int a, Actor *actor, int b, void *c);
extern void func_8003851C(Actor *actor, int a, int b);
extern int func_80038340(Actor *actor); /* ground height under the actor */
extern void func_80038638(Actor *actor, int *target, int a, int heading, int b,
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
  PathData *path;      /* 0x00 */
  int timer;           /* 0x04 */
  int tgtIdx;          /* 0x08 index into D_80075828, -1 when unclaimed */
  int heading;         /* 0x0C */
  int unk10;           /* 0x10 passed by address to func_80039910 */
  int unk14;           /* 0x14 likewise */
} WalkerState;

extern void func_80038DC0(Actor *actor, int a, int b, int c);
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
  (a)->unk40 = 0x10;                                                           \
  (a)->unk41 = 0x10;                                                           \
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
    (a)->unk40 = 0x10;                                                         \
    (a)->unk41 = 0x10;                                                         \
    (a)->unk3C = (a)->unk3D;                                                   \
    (a)->unk3D = (n);                                                          \
    (a)->unk3E = (a)->unk3F;                                                   \
    (a)->unk3F = 0;                                                            \
    func_80037E98(a);                                                          \
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
void func_level_0_8007D9C8(void) {
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
  func_80051FEC();
  func_800522C0(D_800700F4, 0);

  if (D_800756CC >= 3) {
    func_800522C0(D_800700F4, D_800756CC == 3 ? 0x80000001 : 0x80000000);
  }

  actorList = D_800700F4;

  /* Visit every actor on the list; dormant ones (state >= 0x80) are skipped. */
  while (actor = *actorList++) {
    if (actor->unk48 >= 0x80) {
      continue;
    }

    /* Publish this actor's animation flags and the frame delta for the
     * callees. */
    D_80075794 = actor->unk42 & 2;
    flag1 = actor->unk42 & 1;
    D_800756C4 = D_800756CC;
    D_800757F4 = flag1;

    /* One case per actor class. `break` and `continue` both end this actor's
     * update; `continue` also skips any shared tail inside the case. */
    switch (actor->type) {
    /* Portal name text: shown while the camera faces the portal. */
    case 1: {
      int shouldSpawnText, cameraAngleDiff;
      int distance = func_80017990(&actor->posX, &CAM.posX);

      if (distance < (actor->unk48 ? 12 : 11) * 1024) {
        shouldSpawnText = actor->unk48 == 0;

        actor->unk48 = 1;

        cameraAngleDiff = func_80017908(
            actor->unk46, func_80016AB4(CAM.posX - actor->posX,
                                        CAM.posY - actor->posY, 0));

        if (cameraAngleDiff <= 56) {
          if (actor->unk49 == 1) {
            shouldSpawnText = 1;
          }
          actor->unk49 = 0;
        } else if (cameraAngleDiff > 71) {
          if (actor->unk49 == 0) {
            shouldSpawnText = 1;
          }
          actor->unk49 = 1;
        } else {
          actor->unk48 = 0;
        }

        if (actor->unk48 && shouldSpawnText) {
          func_8003C358(actor, 1);
        }
      } else {
        actor->unk48 = 0;
      }

      break;
    }

    /* Path patroller (PathState). */
    case 10: {
      PathState *st = (PathState *)actor->state;

      if ((actor->flags & 0xB0000) != 0 && actor->unk48 != 3) {
        actor->flags = 0;
        st->heading = func_80016AB4(actor->posX - SPY.posX,
                                    actor->posY - SPY.posY, 0);

        if ((actor->flags & 0x10000) != 0) {
          st->unk20 = 0xC8;
        } else {
          st->unk20 = 0x190;
        }

        func_8003ABC0(actor, 3, 0, 0);
        func_8003B7C0(actor);
        actor->unk48 = 3;
        ANIM_RESET(actor, 3);
        continue;
      }

      switch (actor->unk48) {
      case 0: {
        func_80038458(actor);

        if (func_80017990(&actor->posX, &SPY.posX) < 0x1400 &&
            ABS2(actor->posZ - SPY.posZ) < 0x400) {
          if (actor->unk43 != 0xFF) {
            Actor **scan = D_800700F4;
            Actor *other;

            while ((other = *scan++) != 0) {
              PathState *ost = (PathState *)other->state;

              if (other->unk43 == actor->unk43 && other->unk48 == 0) {
                if (ost->path->cur == st->path->cur) {
                  other->unk48 = 1;
                  ost->unk30 = func_80037EA0(6, 0x28);
                }
              }
            }
          }

          actor->unk48 = 1;
          continue;
        }

        if (actor->unk3D != 0) {
          D_80075794 = 0;
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
            func_80016AB4(SPY.posX - actor->posX, SPY.posY - actor->posY, 0);
        nextNode = (st->path->cur + 1) % st->path->count;
        previousNode = (st->path->cur - 1 + st->path->count) % st->path->count;

        nextAngle = func_80016AB4(st->path->nodes[nextNode].x - actor->posX,
                                  st->path->nodes[nextNode].y - actor->posY, 0),
        previousAngle =
            func_80016AB4(st->path->nodes[previousNode].x - actor->posX,
                          st->path->nodes[previousNode].y - actor->posY, 0);

        if (func_80017908(spyroAngle, nextAngle) >
            func_80017908(spyroAngle, previousAngle)) {
          st->path->cur = nextNode;
        } else {
          st->path->cur = previousNode;
        }

        angle = func_80016AB4(st->path->nodes[st->path->cur].x - actor->posX,
                              st->path->nodes[st->path->cur].y - actor->posY, 0);
        angle = (angle + func_80037EA0(-0x20, 0x20)) & 0xFF;
        angle2 =
            func_80016AB4(actor->posX - SPY.posX, actor->posY - SPY.posY, 0);
        angleScale = 108;
        angle2 += (func_800381BC(angle2, angle) * angleScale) >> 7;
        angle2 &= 0xFF;
        st->heading = angle2;
        st->unk24 = func_80037EA0(0x6E, 0xA0);
        st->unk20 = 0;
        distance = func_80017990(&actor->posX,
                                 (int *)&st->path->nodes[st->path->cur]);
        st->unk28 = (distance / st->unk24) >> 1;

        if (st->unk28 >= 0x5B) {
          st->unk28 = 0x5A;
        }

        actor->unk48 = 2;
        continue;
      }
      case 2: {
        int distance = func_80017990(&actor->posX,
                                     (int *)&st->path->nodes[st->path->cur]);

        if (func_80037F90(&st->unk30, 4) == 0) {
          break;
        }

        if (st->unk20 < st->unk24) {
          st->unk20 += 0xA;
        }

        if (actor->unk3D != 1) {
          D_80075794 = 0;
          ANIM_ADVANCE(actor, 1);
        }

        if (func_80037F90(&st->unk28, 4) != 0) {
          int angleDelta;
          int turn;

          angleDelta = func_800381BC(
              func_80016AB4(st->path->nodes[st->path->cur].x - actor->posX,
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

        if (func_80038EE0(actor, st->heading, 6, 0x14, 1) != 0) {
          if (func_80039398(actor, st->unk20, 0x12C, 0x12C, 0x15) != 0) {
            st->unk2C++;
          } else {
            st->unk2C = 0;
          }
          if (st->unk2C >= 4 && D_80075794 != 0) {
            if (distance < 0x1400) {
              actor->unk48 = 0;
              continue;
            }
            st->heading = func_80038074(st->heading, func_80037F10(0x3C, 0x64));
          }
        }
        if (distance < 0xC00 && D_80075794 != 0) {
          actor->unk48 = 0;
          continue;
        }

        break;
      }

      case 3: {
        if (st->unk20 >= 0x10) {
          st->unk20 -= 0xF;
          func_80039688(actor, st->heading, st->unk20, 0, 0x2BC, 5);
        }

        if (D_80075794 != 0) {
          func_80052568(actor);
          continue;
        }

        break;
      }
      }

      func_800529E4(actor, 1);
      break;
    }

    /* Particle emitter on a random timer. */
    case 11: {
      int *st;

      st = (int *)actor->state;
      if (func_80037F90(st, 4) == 0) {
        break;
      }
      D_800758E4(1, 0x16, &actor->posX, 0);
      *st = func_8006272C() & 0xE;
      break;
    }

    /* Butterfly (ChaseState). */
    case 16: {
      ChaseState *st = (ChaseState *)actor->state;

      if (SPY.u164 < 3 && func_80017990(&actor->posX, &SPY.posX) < 2400) {
        int actorZ = actor->posZ - actor->unk38;

        if (ABS2(actorZ - SPY.posZ) < 1800) {
          if (D_80075898 == 0) {
            if (SPY.u164 >= 0) {
              D_80075898 = D_800758CC(120, actor);

              func_8003851C(D_80075898, 0, 0);

              SPY.u164 = 1;

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

    /* Pressure switch (SwitchState). */
    case 18: {
      SwitchState *st = (SwitchState *)actor->state;
      int active = D_80077AE8 & 1;

      switch (actor->unk49) {
      case 0: {
        if (func_80017990(&actor->posX, &SPY.posX) < 0x500 &&
            ABS2((actor->posZ - actor->unk38) - SPY.posZ) < 0x280) {
          if ((D_80075828[st->idx].unk49 >> st->shift) & 1) {
            D_80075828[st->idx].unk49 = 0;
          } else {
            D_80075828[st->idx].unk49 |= 1 << st->shift;
          }
          actor->unk49 = 1;
        }
        break;
      }

      case 1: {
        if (func_80017990(&actor->posX, &SPY.posX) >= 0x601 ||
            ABS2((actor->posZ - actor->unk38) - SPY.posZ) >= 0x2C1) {
          actor->unk49 = 0;
        }
        break;
      }
      }

      if (D_80075828[st->idx].unk48 == 10) {
        if ((actor->unk48 & 1) == 0) {
          actor->unk48 = 1;
          ANIM_RESET(actor, 1);
        }
        break;
      }

      if (D_80075828[st->idx].unk48 == 11) {
        if ((actor->unk48 & 1) != 0) {
          actor->unk48 = 0;
          ANIM_RESET(actor, 0);
        }
        break;
      }

      if (((D_80075828[st->idx].unk49 >> st->shift) & 1) != 0 && active != 0 &&
          D_80075828[st->idx].unk48 < 0x63) {
        if ((actor->unk48 & 1) == 0) {
          D_800761DC = 2;
          D_800761F4.v =
              *(unsigned short *)(D_800761D0 + D_800761D4[0x37] * 20 + 0xA) +
              D_80073094[st->shift];
          func_80055A78(D_80076378[actor->type]->m_Sounds[0], actor, 8,
                        &actor->unk54);
        }
        actor->unk48 = 1;
        ANIM_RESET(actor, 1);
      } else if ((actor->unk48 & 1) != 0) {
        actor->unk48 = 0;
        ANIM_RESET(actor, 0);
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

    /* Sequenced prop gated on the exit vortices (AnimState). */
    case 49: {
      AnimState *st = actor->state;
      int marker;

      if (st->started == 0) {
        if (D_80078E7C != 0) {
          func_8002B390(st->handle, 0xFC, 0);
          func_8002B444(st->handle, 0x3B, 0);
        }
        st->started = 1;
      }

      marker = (func_8002B3F4(st->handle) >> 8) & 0xFF;

      switch (marker) {
      case 0:
        if (D_8007A6A9 || D_8007A6AA || D_8007A6AB || D_8007A6AC) {
          /* Wake only once the linked actor is dormant and Spyro is near. */
          if (D_80075828[st->idx].unk48 >= 0x80 &&
              func_80017990(&actor->posX, &SPY.posX) < 0x4000) {
            func_8002B390(st->handle, 0xFC, 0);
            actor->unk55 = 0x30;
            func_80055A78(D_800761D4[0x41], actor, 8, &actor->unk54);
          }
        }
        break;

      case 0x3C:
        func_800562A4(actor, 1);
        if (func_80017990(&actor->posX, &SPY.posX) >= 0x4801) {
          func_8002B390(st->handle, 0xFC, 0);
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

      if (st->life > 0 && actor->unk51) {
        if (func_8004BE4C(&actor->posX, 0x100, 0x100)) {
          int *nrm = &D_80077368;
          int dot;

          func_80017330(nrm, 0x1000);
          dot = st->vel[0] * nrm[0] + st->vel[1] * D_8007736C +
                    st->vel[2] * D_80077370 >>
                11;
          if (dot < 0) {
            func_800175B8(nrm, 0x1000, (dot >> 2) - (dot));
            st->vel[0] += nrm[0];
            st->vel[1] += AS_I32(D_8007736C);
            st->vel[2] += AS_I32(D_80077370);
          }
        }

        actor->posX += st->vel[0];
        actor->posY += st->vel[1];
        st->vel[2] -= 6;
        if (st->vel[2] < -128)
          st->vel[2] = -128;
        actor->posZ += st->vel[2];

        actor->unk44 += st->spin0;
        actor->unk45 += st->spin1;
        actor->unk46 += st->spin2;
        if ((st->life & 3) == 0) {

          svD[0] = func_8006272C() & 3;
          svD[1] = func_8006272C() & 3;
          svD[2] = 0x14;
          D_800758E4(1, 1, &actor->posX, svD);
        }
        st->life--;
      } else {
        D_800758E4(8, 0x46, &actor->posX, (void *)0x10);
        func_80052568(actor);
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

    /* Collectibles: life statue, life orb, gems (PickupState). */
    case 14:
    case 15:
    case 83:
    case 84:
    case 85:
    case 86:
    case 87: {
      PickupState *st = (PickupState *)actor->state;

      int dist = func_80017990(&actor->posX, &SPY.posX);

      if (st->timer < 0xFA) {
        st->timer += D_800756CC;
      }

      if (actor->type != 15) {

        if (st->anim != 255) {
          func_800529E4(actor, 4);

          if (actor->type == 14) {
            func_80017048(actor->unk20, &D_8006E5B8, D_80077108[st->anim].pos);
          } else {
            func_80017048(actor->unk20, &D_8006E5A0, D_80077108[st->anim].pos);
          }

          func_80017758(D_80077108[st->anim].pos, D_80077108[st->anim].pos,
                        &actor->posX);

          if (D_80077108[st->anim].flag < 5) {
            st->anim = 255;
          }
        } else {
          if (st->timer > 0xf7) {
            int handle = func_8003AAEC(actor, &D_8006E5A0);
            if (handle >= 0 && dist < 0x4000) {
              st->anim = handle;
            }

            st->timer = (func_8006272C() & 0x1F) + 33;
          }
        }
      }

      if (!actor->unk51 && dist > 0x2000 && actor->unk49 == 1 && st->sub == 0) {
        break;
      }

      switch (actor->unk49) {
      case 0: {
        int *nrm = &D_80077368;

        func_80017700(svA, &actor->posX);
        svA[2] += 0x400;
        func_8004D5EC(svA, 0x10000);
        st->pitch = -func_800169AC(
            func_80017A38((nrm[0] * nrm[0]) + (D_80077370 * D_80077370)),
            D_8007736C);
        st->yaw = -func_800169AC(D_80077370, nrm[0]);

        if (st->pitch != 0 || st->yaw != 0) {
          actor->unk46 = 0;
        }

        actor->unk49 = 1;
        break;
      }
      case 1: {
        if (st->sub != 0) {

          if (st->sub == 1) {
            func_80038458(actor);
            func_800533D0(actor);
            if (actor->type >= 83) {
              int *nrm = &D_80077368;

              st->pitch = -func_800169AC(
                  func_80017A38((nrm[0] * nrm[0]) + (D_80077370 * D_80077370)),
                  D_8007736C);
              st->yaw = -func_800169AC(D_80077370, nrm[0]);
            }

          } else if (st->sub == 2) {

            func_80017CB8(st->unk0C, acc[0]);

            func_80017758(acc[0], acc[0], acc[1]);
            func_80017758(acc[0], acc[0], acc[2]);

            actor->posX = acc[0][0] / 3;
            actor->posY = acc[0][1] / 3;
            actor->posZ = acc[0][2] / 3;

            func_800529E4(actor, 2);
            func_80038458(actor);
            func_800533D0(actor);

          } else if (st->sub == 3) {
            int colIndex;

            func_80017700(svF, &actor->posX);
            svF[2] += 0x400;

            func_800529E4(actor, 2);

            if (func_8004D5EC(svF, 0x1000) > 0) {
              colIndex = D_80075808;
              st->sub = 2;
              st->unk0C = colIndex;
            }
          }
        }

        if (dist < 1434) {
          int zDist = (SPY.posZ - 356) - actor->posZ;

          if (ABS2(zDist) < 512) {

            if (D_80075898 != 0) {

              if (SPY.u09C == 0) {

                int res = func_80033E40(&D_80075898->posX, &actor->posX);

                if (res) {
                  ChaserState *cs = (ChaserState *)D_80075898->state;

                  if (cs->owner == 0) {
                    if (!func_80056DC4(D_80075898,
                                       D_80076378[D_80075898->type]->m_Sounds[0])) {
                      D_800761DC = 1;
                      D_800761EA = 0x2000;
                      D_800761E8 = 0x2000;
                      func_8003851C(D_80075898, 0, 0);
                    }

                    D_80075898->unk49 = 4;
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

        func_800177C0(svG, st->vel, D_800756CC);
        func_800176C8(svG, 1);

        speed = func_800171FC(svG, 1);

        if (220 < speed) {
          func_800175B8(svG, speed, 220);
        }

        func_80017758(svG, &actor->posX, svG);

        if (-220 < st->vel[2]) {
          st->vel[2] -= D_800756CC * 5;
        }

        if (svG[2] < 0) {
          if (actor->type != 14 && actor->type != 15) {
            func_8003B9D4(actor);
          }
          func_80052568(actor);
          continue;
        } else {
          svG[2] += 240;

          if (func_8004BE4C(svG, 240, 240)) {
            if (func_80057380() == 0) {
              if (actor->type != 14 && actor->type != 15) {
                func_8003B9D4(actor);
              }
              func_80052568(actor);
              continue;
            }

            func_80017700(svH, &D_80077368);
            func_80017700(&actor->posX, &D_80076B80);
            actor->posZ -= 0xF0;

            if (st->count == 0) {
              int groundHeight = func_8004D5EC(svG, 0x400);
              int groundAngle =
                  (signed char)func_800169AC(svH[2], func_800171FC(svH, 0));

              if ((svG[2] - 400) < groundHeight && groundAngle < 24) {
                actor->unk49 = 1;
                func_80017700(&actor->posX, svG);
                actor->posZ = groundHeight;

                if (actor->type != 0xF) {

                  st->pitch = -func_800169AC(
                      func_80017A38((svH[0] * svH[0]) + (svH[2] * svH[2])),
                      svH[1]);
                  st->yaw = -func_800169AC(svH[2], svH[0]);

                  if (st->pitch != 0 || st->yaw != 0) {
                    actor->unk46 = 0;
                  }
                }

                func_800533D0(actor);
              } else {
                st->count++;
              }
            }

            if (st->count != 0) {
              D_800761DC = 1;
              D_800761E8 = 0x3CCC >> (3 - st->count);
              D_800761EA = 0x3CCC >> (3 - st->count);

              func_80055A78(D_800761D4[0x2F], actor, 8, &actor->unk54);

              if (func_80017428(st->vel, svH, st->vel)) {
                st->count--;

                st->vel[0] = (st->vel[0] >> 3) + (func_8006272C() & 0x3F) - 32;
                st->vel[1] = (st->vel[1] >> 3) + (func_8006272C() & 0x3F) - 32;
                st->vel[2] = (st->vel[2] >> 2) + (func_8006272C() & 0xF);
              }
            }

          } else {
            svG[2] -= 240;
            func_80017700(&actor->posX, svG);
            func_8004D5EC(svG, 0x10000);
            func_800533D0(actor);
          }
        }

        if (dist < 1434) {
          int zDist = (((int *)&SPY.posX)[2] - 356) - actor->posZ;

          if (ABS2(zDist) < 512) {

            if (D_80075898 != 0) {

              if (SPY.u09C == 0 && st->count < 3) {

                ChaserState *cs = (ChaserState *)D_80075898->state;

                if (cs->owner == 0) {
                  if (!func_80056DC4(D_80075898,
                                     D_80076378[D_80075898->type]->m_Sounds[0])) {
                    D_800761DC = 1;
                    D_800761EA = 0x2000;
                    D_800761E8 = 0x2000;
                    func_8003851C(D_80075898, 0, 0);
                  }

                  D_80075898->unk49 = 4;
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
          func_80017700(&actor->posX, &SPY.posX);
        } else {

          func_8001778C(svI, &SPY.posX, st->vel);
          func_800176C8(svI, 5);

          if (func_800171FC(svI, 1) > 480) {
            func_80017700(&actor->posX, &SPY.posX);
            st->timer = 32;
          } else {
            func_800177C0(svI, svI, st->timer);
            func_80017758(&actor->posX, st->vel, svI);
            actor->posZ += D_8006CBF8[st->timer * 4] / 12;
          }
        }

        actor->unk44 = actor->unk44 - 7 + st->pitch;
        actor->unk45 = actor->unk45 - 7 + st->yaw;
        actor->unk46 = actor->unk46 - 7 + st->roll;
        break;
      }

      case 4: {
        continue;
      }
      }
      if (actor->unk49 < 3) {
        if (actor->type == 14) {
          actor->unk44 = (D_8006CC78[st->roll] >> 9) + st->pitch;
          actor->unk45 = (D_8006CBF8[st->roll] >> 9) + st->yaw;
          st->roll += D_800756CC << 1;
        } else if (actor->type == 15) {
          actor->unk46 += 8;
          actor->unk45 -= 6;
        } else {
          actor->unk44 = (D_8006CC78[st->roll] >> 7) + st->pitch;
          actor->unk45 = (D_8006CBF8[st->roll] >> 7) + st->yaw;
          st->roll += D_800756CC << 1;
        }
      }
      if ((st->timer > 32 && dist < 512 && (-384 < (SPY.posZ - actor->posZ)) &&
           ((SPY.posZ - actor->posZ) < 512)) ||
          (actor->unk49 == 3 && st->timer > 64)) {

        if (D_80075898 != 0) {
          ChaserState *cs = (ChaserState *)D_80075898->state;

          if (cs->owner == actor) {
            cs->owner = 0;
          }
        }

        if (actor->type == 14) {
          func_80055A78(D_800761D4[0], actor, 16, 0);
          func_800529E4(actor, 4);
          D_800758E4(16, 70, &actor->posX, (void *)8);
          D_800758E4(16, 70, &actor->posX, (void *)0x10);

          if (D_8007582C < 99) {
            D_8007582C++;
          }

          func_8003B854(0, actor);
          func_80052568(actor);
          continue;
        } else if (actor->type == 15) {
          func_80055A78(D_800761D4[0], actor, 16, 0);
          D_800758E8++;

          func_80052568(actor);
        } else {
          func_8003B9D4(actor);
          func_80052568(actor);
          continue;
        }
      }

      break;
    }

    /* Dragon-pad fairy (PoleState). */
    case 110: {
      PoleState *st = (PoleState *)actor->state;

      Actor *tgtA = &D_80075828[st->idxA];
      Actor *tgtB = &D_80075828[st->idxB];

      switch (actor->unk48) {
      case 0: {
        actor->unk08 = 0;
        if (func_80017990(&tgtB->posX, &SPY.posX) > 2560) {
          actor->unk48 = 1;
        }
        break;
      }

      case 1: {
        if (tgtA->unk48 >= 0x80u) {
          actor->unk48 = 2;
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

          if (func_80017990(&actor->posX, &SPY.posX) < 0x2000) {
            if (ABS2(actor->posZ - actor->unk38 - SPY.posZ) < 2048) {
              if (st->flag24 == 0 || tgtB->unk48 == 1) {
                st->flag24 = 0;
                actor->posZ = st->baseZ + 0x80;
                actor->unk46 = func_80016AB4(SPY.posX - actor->posX,
                                             SPY.posY - actor->posY, 0) -
                               128;
                actor->unk50 = 0x20;
                actor->unk48 = 3;
                actor->unk57 = 0x60;

                D_800758E4(0x18, 0x50, &actor->posX, (void *)0xC);

                func_800529E4(actor, 1);
                func_8004D5EC(&actor->posX, 0x1000);
                func_800533D0(actor);
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
          actor->unk46 = actor->unk46 + 8;
        } else {
          actor->unk48 = 4;
          st->timer = 0;
          st->t20 = 30;
        }
        break;
      }

      case 4: {
        st->timer++;
        actor->posZ = st->baseZ + (func_80016CB0(st->timer << 7) >> 6);

        if ((tgtB->unk48 == 0 || (tgtB->unk48 == 2 && tgtB->unk49 >= 0x10)) &&
            func_80017990(&tgtB->posX, &SPY.posX) < 1024 &&
            ABS2(tgtB->posZ - tgtB->unk38 - SPY.posZ) < 512) {

          if ((SPY.state == 0 || SPY.state == 0xD) && SPY.u080 > 0) {
            actor->unk48 = 6;
            st->flag24 = 1;
            func_8002CCC8(actor);
            break;
          }
        }

        if (func_80017990(&actor->posX, &SPY.posX) > 0x2400 ||
            (ABS2(actor->posZ - actor->unk38 - SPY.posZ) > 0xC00)) {
          actor->unk48 = 6;
        } else {
          st->t20--;
          if (st->t20 == 0) {
            int baseDistance = (func_8006272C() & 0x3F) + 0x140;
            char finalAngle = (func_80016AB4(SPY.posX - tgtB->posX,
                                             SPY.posY - tgtB->posY, 0) +
                               (func_8006272C() & 0xF) - 8);

            func_80017700(st->tgt, &tgtB->posX);

            actor->unk48 = 5;

            st->tgt[0] -= (func_80016CB0(finalAngle * 0x10) * baseDistance) >> 12;
            st->tgt[1] -= (func_80016C58(finalAngle * 0x10) * baseDistance) >> 12;

            st->angle = func_80016AB4(SPY.posX - st->tgt[0],
                                      SPY.posY - st->tgt[1], 0);
            st->t20 = 0;
          }
        }
        break;
      }
      case 5: {
        st->timer++;
        actor->posZ = st->baseZ + (func_80016CB0(st->timer << 7) >> 6);

        if ((tgtB->unk48 == 0 || (tgtB->unk48 == 2 && tgtB->unk49 >= 0x10)) &&
            func_80017990(&tgtB->posX, &SPY.posX) < 0x400 &&
            ABS2(tgtB->posZ - tgtB->unk38 - SPY.posZ) < 512) {

          if ((SPY.state == 0 || SPY.state == 0xD) && SPY.u080 > 0) {
            actor->unk48 = 6;
            st->flag24 = 1;
            func_8002CCC8(actor);
            break;
          }
        }

        if (func_80017990(&actor->posX, &SPY.posX) > 0x2800 ||
            ABS2(actor->posZ - actor->unk38 - SPY.posZ) >= 0xC01) {
          actor->unk48 = 6;
        } else {
          int angleDiff;
          int cosWeight1;
          int cosDelta;
          int totalWeight;

          st->t20++;

          func_8001778C(svF, st->tgt, &actor->posX);

          angleDiff = func_80017948(st->angle, actor->unk46);

          cosWeight1 = func_80016CB0((st->t20 - 1) << 7);
          cosDelta = cosWeight1 - func_80016CB0(st->t20 << 7);
          totalWeight = func_80016CB0((st->t20 - 1) << 7) + 0x1000;

          actor->posX += (svF[0] * cosDelta) / totalWeight;
          actor->posY += ((svF[1] * cosDelta) / totalWeight);
          actor->unk46 += (angleDiff * cosDelta) / totalWeight;

          func_8004D5EC(&actor->posX, 0x1000);
          func_800533D0(actor);

          if (st->t20 == 0x10) {
            actor->unk48 = 4;
            st->t20 = (func_8006272C() & 0x1F) + 0x1E;
          }
        }

        break;
      }

      case 6: {
        if (actor->unk57 < 96) {
          actor->unk57 += 4;
          actor->unk46 = actor->unk46 + 8;
        } else {
          actor->unk50 = 0;
          actor->unk51 = 0;
          actor->unk48 = 2;
          st->timer = 16;

          D_800758E4(0x18, 0x50, &actor->posX, (void *)0xC);

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
          (unsigned int)(actor->unk48 - 7) >= 3) {
        func_8003ABC0(actor, 3, 0, 0);
        func_8003B7C0(actor);

        if ((actor->flags & 0x20000) != 0) {
          st->unk10 = 0x190;
          st->unk14 = 0x46;
          st->heading = func_80038178(
              func_80016AB4(actor->posX - SPY.posX, actor->posY - SPY.posY, 0),
              SPY.bodyRotZ, 0x28, 0x80);
          ENTER_POSE(actor, 8);
        }

        func_80055A78(D_800761D4[0x27], actor, 8, &actor->unk54);
        st->unk14 = 0xB4;
        st->unk10 = 0x118;
        st->heading = func_80038178(
            func_80016AB4(actor->posX - SPY.posX, actor->posY - SPY.posY, 0),
            SPY.bodyRotZ, 0x20, 0x40);
        ENTER_POSE(actor, 7);
      }

      if (st->tgtIdx >= 0 && D_80075828[st->tgtIdx].unk48 >= 0x80) {
        st->tgtIdx = -1;
      }

      switch (actor->unk48) {
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
            func_80017990(&actor->posX, &SPY.posX) < 0x2800 &&
            ABS2((actor->posZ - actor->unk38) - SPY.posZ) < 0x800) {
          ENTER_POSE(actor, 4);
        }

        if (st->tgtIdx >= 0 && func_80037F90(&st->timer, 4) != 0) {
          if (func_80017990(&actor->posX, &D_80075828[st->tgtIdx].posX) <
              0x1000) {
            ENTER_POSE(actor, 2);
          }

          ENTER_POSE(actor, 1);
        }

        func_80038458(actor);
        break;
      }
      case 1: {
        int xyDistance;
        int deltaY;
        int deltaZ;

        xyDistance = D_80075828[st->tgtIdx].posX - actor->posX;
        deltaY = D_80075828[st->tgtIdx].posY - actor->posY;

        xyDistance = ABS(xyDistance);
        xyDistance += ABS(deltaY);
        deltaZ = D_80075828[st->tgtIdx].posZ - actor->posZ;
        deltaZ = ABS(deltaZ);
        distance = xyDistance + deltaZ;

        if (distance < 0x1800 &&
            func_80017990(&actor->posX, &D_80075828[st->tgtIdx].posX) <
                0x1000) {
          ENTER_POSE(actor, 2);
        }

        if (D_80075794 != 0) {
          st->timer = (func_8006272C() & 0x3F) + 0x20;
          ENTER_POSE(actor, 0);
        }

        func_80038458(actor);
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
            func_80017990(&actor->posX, &SPY.posX) < 0x2800 &&
            ABS2((actor->posZ - actor->unk38) - SPY.posZ) < 0x800) {
          ENTER_POSE(actor, 4);
        }

        func_80038EE0(actor,
                      func_80016AB4(D_80075828[st->tgtIdx].posX - actor->posX,
                                    D_80075828[st->tgtIdx].posY - actor->posY,
                                    0),
                      8, 0x10, 1);

        if (D_80075794 != 0) {
          st->timer = (func_8006272C() & 0x3F) + 0x20;
          ENTER_POSE(actor, 0);
        }

        func_80038458(actor);
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
            func_80017990(&actor->posX, &SPY.posX) < 0x2800 &&
            ABS2((actor->posZ - actor->unk38) - SPY.posZ) < 0x800) {
          if (st->path->cur < st->path->count - 1) {
            st->path->cur++;
          }
          ENTER_POSE(actor, 4);
        }

        if (func_80017990(&actor->posX,
                          (int *)&st->path->nodes[st->path->cur]) < 0x200) {
          if (st->path->cur == 0) {
            ENTER_POSE(actor, 0);
          }
          st->path->cur--;
        }

        angle = func_80016AB4(st->path->nodes[st->path->cur].x - actor->posX,
                              st->path->nodes[st->path->cur].y - actor->posY, 0);
        angleDelta = (angle - actor->unk46) & 0xFF;
        if (angleDelta > 0x80) {
          angleDelta -= 0x100;
        }

        if (ABS2(angleDelta) < 0x20) {
          func_80039398(actor, 0x40, 0x200, 0x200, 0x27);
        }

        if (angleDelta < -8) {
          angleDelta = -8;
        }
        if (angleDelta > 8) {
          angleDelta = 8;
        }

        actor->unk46 += angleDelta;
        break;
      }

      case 4: {
        if (D_80075794 != 0) {
          if (SPY.state == 0xB || SPY.state == 0x14) {
            if (actor->unk3C != 6) {
              D_80075794 = 0;
              ANIM_RESET(actor, 6);
            }
            actor->unk48 = 0x14;
            continue;
          }

          ENTER_POSE(actor, 5);
        }

        func_80038DC0(actor, 8, 0x10, 1);
        break;
      }

      case 5: {
        int angle;
        int angleDelta;

        if (func_80017990(&actor->posX,
                          (int *)&st->path->nodes[st->path->cur]) < 0x200) {
          if (st->path->cur < st->path->count - 1) {
            st->path->cur++;
          } else {
            st->timer = 0x78;
            ENTER_POSE(actor, 6);
          }
        }

        angle = func_80016AB4(st->path->nodes[st->path->cur].x - actor->posX,
                              st->path->nodes[st->path->cur].y - actor->posY, 0);
        angleDelta = (angle - actor->unk46) & 0xFF;
        if (angleDelta > 0x80) {
          angleDelta -= 0x100;
        }

        if (ABS2(angleDelta) < 0x20) {
          func_80039398(actor, 0x82, 0x200, 0x200, 0x27);
        }

        if (angleDelta < -8) {
          angleDelta = -8;
        }
        if (angleDelta > 8) {
          angleDelta = 8;
        }

        actor->unk46 += angleDelta;
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
            func_80017990(&actor->posX, &SPY.posX) < 0x2800 &&
            ABS2((actor->posZ - actor->unk38) - SPY.posZ) < 0x800) {
          func_80038DC0(actor, 8, 0x10, 1);
          st->timer = 0x78;
          break;
        }

        if (func_80037F90(&st->timer, 4) != 0) {
          st->path->cur = st->path->count - 1;
          ENTER_POSE(actor, 3);
        }

        break;
      }
      case 7: {
        if (D_80075794 != 0) {
          ENTER_POSE(actor, 9);
        }

        actor->unk45 += 4;
        func_80038EE0(actor, (st->heading + 0x80) & 0xFF, 0x10, 0x20, 1);
        func_80039910(actor, &st->unk10, st->heading, &st->unk14, 0xC, 0xC);
        break;
      }

      case 8: {
        if (D_80075794 != 0) {
          func_800529E4(actor, 4);
          func_800385BC(actor, 0x1C);
          func_80052568(actor);
          continue;
        }

        func_80039910(actor, &st->unk10, st->heading, &st->unk14, 0xC, 0xC);
        func_80038EE0(actor, (st->heading + 0x80) & 0xFF, 0x10, 0x20, 1);
        break;
      }

      case 9: {
        if (func_80039910(actor, &st->unk10, st->heading, &st->unk14, 0xC,
                          0x10) == 3) {
          func_800529E4(actor, 4);
          func_800385BC(actor, 0x10);
          func_80052568(actor);
          continue;
        }

        func_80038EE0(actor, (st->heading + 0x80) & 0xFF, 0x10, 0x20, 1);
        break;
      }

      case 0x14: {
        func_80038DC0(actor, 4, 0, 0);

        if (SPY.state != 0xB && SPY.state != 0x14) {
          ENTER_POSE(actor, 5);
        }

        break;
      }

      default: {
        break;
      }
      }

      func_800529E4(actor, 1);
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
        func_80017048(actor->unk20, svF, svF);
        func_80017758(svF, svF, &actor->posX);
        D_800758E4(2, 0x42, svF, 0);

        svF[0] = 0;
        svF[1] = -0x64;
        svF[2] = 0;
        func_800170C0(svF, svF);
        func_80017758(svF, svF, &actor->posX);
        D_800758E4(2, 0x42, svF, 0);
      }

      if (SPY.u164 >= 3) {
        if (st->fx == 0) {
          FxNode *g = func_80058AE8();
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
          func_80058B60(st->fx);
          st->fx = 0;
        }
      }

      if (SPY.u164 <= 0) {
        Actor *target = st->rider;
        if (target != 0 && target->type == 16) {
          target->unk48 = 0;
        }
        D_80075898 = 0;
        func_80052568(actor);
        break;
      }

      if (st->rider != 0 && func_80017990(&actor->posX, &SPY.posX) > 8192) {
        st->timer = 0;
      }

      if (actor->unk49 == 99)
        break;

      if (st->rider != 0) {
        switch (actor->unk49) {
        case 0: {
          Actor *target = st->rider;
          ChaseState *bp = target->state;
          int isPickupAnimPhase = actor->unk3D & 1;
          int dist;
          int targetSpeed;

          func_80017700(svA, &target->posX);
          if (isPickupAnimPhase == 0) {
            svA[0] -= D_8006CC78[st->rider->unk46] >> 4;
            svA[1] -= D_8006CBF8[st->rider->unk46] >> 4;
          } else {
            svA[0] -= D_8006CC78[st->rider->unk46] >> 6;
            svA[1] -= D_8006CBF8[st->rider->unk46] >> 6;
          }

          if (isPickupAnimPhase) {
            int delta;
            if (actor->unk3F >= 7) {
              actor->unk49++;
            }

            if (2 < actor->unk45 && actor->unk45 < 254) {
              delta = -actor->unk45;
              if (delta > 2) {
                delta = 2;
              }
              if (delta < -2) {
                delta = -2;
              }
              actor->unk45 = actor->unk45 + delta;
            }
          }

          func_8001778C(svA, svA, &actor->posX);
          dist = func_800171FC(svA, 1);

          if (isPickupAnimPhase == 0) {
            if (func_80037F90(st, 4) != 0 ||
                (st->timer < 80 &&
                 func_80017908((actor->unk46 + (bp->dir * 110)) & 0xFF,
                               ((CAM.rotZ >> 4) + 0x80) & 0xFF) < 8)) {
              if (actor->unk3D != actor->unk3D + 1) {
                D_80075794 = 0;
                ANIM_ADVANCE(actor, actor->unk3D + 1);
              }
            }
          }
          targetSpeed = isPickupAnimPhase ? 150 : 130;
          if (targetSpeed + 5 < dist) {
            func_800175B8(svA, dist, targetSpeed + 5);
          } else {
            func_800175B8(svA, dist, targetSpeed - 5);
          }
          func_80017758(&actor->posX, &actor->posX, svA);
          func_80038EE0(actor, func_80016AB4(svA[0], svA[1], 0), 0xA, 0, 0);
          pitchAngle = func_80016AB4(func_800171FC(svA, 0), svA[2], 0);
          actor->unk45 = pitchAngle;
          actor->unk45 = func_80038098(pitchAngle, 0, 0x30);
          break;
        }
        case 1: {
          SPY.u164++;
          func_80055A78(D_800761D4[0], actor, 0x10, 0);
          actor->unk49++;
          func_80052568(st->rider);
          break;
        }
        case 2: {
          if (D_80075794 != 0) {
            if (actor->unk3D != actor->unk3D - 1) {
              D_80075794 = 0;
              ANIM_ADVANCE(actor, actor->unk3D - 1);
            }
            st->rider = 0;
            actor->unk49 = 0;
          }
          break;
        }
        case 4: {
          PickupState *cp = st->rider->state;
          int dist;

          func_8001778C(svJ, &st->rider->posX, &actor->posX);
          dist = func_800171FC(svJ, 1);

          if (dist < 512) {
            cp->timer = 0;
            cp->pitch = func_8006272C() & 0xE;
            cp->yaw = func_8006272C() & 0xE;
            cp->roll = func_8006272C() & 0xE;

            func_80017700(cp->vel, &st->rider->posX);
            st->rider->unk49 = 3;

            if (st->rider->type == 14)
              D_800758E4(1, 0xC, (int *)actor, D_8006E494);
            else if (st->rider->type == 15)
              D_800758E4(1, 0xC, (int *)actor, D_8006E490);
            else
              D_800758E4(1, 0xC, (int *)actor, D_8006E330[st->rider->type]);

            st->rider = 0;
            if (!func_80056DC4(actor, AS_PTRU8(D_800761D4)[0x2F])) {
              D_800761DC = 1;
              D_800761EA = 0x3600;
              D_800761E8 = 0x3600;
              func_80055A78(D_800761D4[0x2F], actor, 8, &actor->unk54);
            }
          } else {
            int dcheck = 310;

            if (dcheck < dist)
              func_800175B8(svJ, dist, 310);
            else
              func_800175B8(svJ, dist, 290);

            func_80017758(&actor->posX, &actor->posX, svJ);
            func_80038EE0(actor, func_80016AB4(svJ[0], svJ[1], 0), 0xA, 0, 0);
            pitchAngle = func_80016AB4(func_800171FC(svJ, 0), svJ[2], 0);
            actor->unk45 = pitchAngle;
            actor->unk45 = func_80038098(pitchAngle, 0, 0x30);
          }
          break;
        }
        }
      } else {

        actor->unk50 = 0x10;
        if (st->timer <= 0) {

          svF[0] = -0x258;
          svF[1] = (func_8006272C() & 0x310) - 0x188;
          if (CAM.state == 3) {
            svF[2] = (func_8006272C() & 0x7F) + 0x64;
          } else {
            svF[2] = (func_8006272C() & 0x1FF) - 0xC8;
          }
          st->vx = svF[0];
          st->vy = svF[1];
          st->vz = svF[2];
          st->timer = func_8006272C() & 0x7B;
        } else {
          st->timer -= D_800756CC;
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
          func_80017048(&SPY.u034, svF, svF);
          func_80017758(svF, svF, &SPY.posX);
          func_8001778C(svF, svF, &actor->posX);
          func_800176C8(svF, 2);
          func_80017758(&actor->posX, &actor->posX, svF);

          if (func_8004BE4C(&actor->posX, 0x100, 0x100)) {
            func_80017700(&actor->posX, &D_80076B80);
          }

          if (svF[2] > 32)
            svF[2] = 32;
          if (svF[2] < -32)
            svF[2] = -32;
          actor->unk44 = 0;
          actor->unk45 = svF[2];
          func_80038EE0(actor, SPY.bodyRotZ, 4, 0, 0);
        }

        switch (SPY.u164) {
        case 1:
          if (actor->unk3D != 0) {
            if (actor->unk3C != 0) {
              D_80075794 = 0;
              ANIM_RESET(actor, 0);
            }
          }
          break;
        case 2:
          if (actor->unk3D != 2) {
            if (actor->unk3C != 2) {
              D_80075794 = 0;
              ANIM_RESET(actor, 2);
            }
          }

          break;
        case 3:
          if (actor->unk3D != 4) {
            if (actor->unk3C != 4) {
              D_80075794 = 0;
              ANIM_RESET(actor, 4);
            }
          }
          break;
        }
      }
      func_800529E4(actor, 4);
      break;
    }

    /* Key (SpinState). */
    case 173: {
      SpinState *st;
      int dz;
      int ang;
      int i;

      st = (SpinState *)actor->state;
      if (actor->unk49 != 0) {
        continue;
      }

      actor->unk44 = (unsigned short)D_8006CC78[st->angle] >> 7;
      actor->unk45 = (unsigned short)D_8006CBF8[st->angle] >> 7;
      st->angle = st->angle + D_800756CC * 2;

      if (func_80017990(&actor->posX, &SPY.posX) < 0x200) {
        dz = (actor->posZ - actor->unk38) - SPY.posZ;
        if (dz > 0 ? dz < 0x200
                   : SPY.posZ - (actor->posZ - actor->unk38) < 0x200) {
          func_80055A78(D_800761D4[0], actor, 0x10, 0);
          func_800529E4(actor, 4);

          for (i = 0; i < 6; i++) {
            D_800758E4(1, 0xC, (int *)actor, (void *)(0x8080 + 0x1000000 * i));
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

      if (st->anim != 0xFF) {
        func_800529E4(actor, 4);
        func_80017048(actor->unk20, &D_8006E5AC, D_80077108[st->anim].pos);
        func_80017758(D_80077108[st->anim].pos, D_80077108[st->anim].pos,
                      &actor->posX);
        if (D_80077108[st->anim].flag < 5) {
          st->anim = 0xFF;
        }
      } else if (st->timer >= 0xF8) {
        ang = func_8003AAEC(actor, &D_8006E5AC);
        if (ang >= 0 && func_80017990(&actor->posX, &SPY.posX) < 0x4000) {
          st->anim = ang;
        }
        st->timer = (func_8006272C() & 0x3F) + 0x18;
      }

      st->timer = st->timer + D_800756CC;
      continue;
    }

    /* Talking dragon: when Spyro faces it up close, start its dialogue. */
    case 187: {
      if (actor->unk48 == 0) {
        if (func_80017990(&actor->posX, &SPY.posX) >= 0x1000) {
          actor->unk48 = 1;
        }
      } else if ((SPY.state == 0 || SPY.state == 1 || SPY.state == 0x15 ||
                  SPY.state == 2 || SPY.state == 0xB) &&
                 func_80017990(&actor->posX, &SPY.posX) < 0x780 &&
                 func_80017908(SPY.bodyRotZ,
                               func_80016AB4(actor->posX - SPY.posX,
                                             actor->posY - SPY.posY, 0)) <
                     0x20 &&
                 func_80017908(actor->unk46,
                               func_80016AB4(SPY.posX - actor->posX,
                                             SPY.posY - actor->posY, 0)) <
                     0x28) {
        actor->unk48 = 0;
        SPY.controlFlags = 0x80002000;
        func_8003DFA4();
        func_800176F0(&SPY.headLook);
        D_800757A0(actor);

        if (D_80075750 < 10) {
          if (D_800758D0[1] == 0) {
            D_800777F4 = 0;
            D_800758D0[1] = 1;
          } else {
            D_800777F4 = 2;
          }
        } else if (D_800758D0[1] < 2) {
          if (D_800758D0[1] == 0) {
            D_800777F4 = 3;
          } else {
            D_800777F4 = 4;
          }
          D_800758D0[1] = 2;
        } else {
          D_800777F4 = 0x1E;
          D_800777F8 = (int)func_8006272C() % 3;
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

    /* Crystal dragon (RideState). */
    case 250: {
      RideState *st = (RideState *)actor->state;

      if (actor->unk48 == 0) {
        int padIdx = st->unk20;
        if (padIdx != -1) {
          D_80075828[padIdx].unk48 = 3;
        }
        actor->unk48 = 1;
        st->unk48 = actor->unk44;
        st->unk4C = actor->unk45;
        st->unk50 = actor->posZ;
      } else if (actor->unk48 == 1) {
        st->unk44 += D_800756CC;
        if (st->unk44 > 256) {
          st->unk44 = 0;
          actor->unk44 = st->unk48;
          actor->unk45 = st->unk4C;
          actor->posZ = st->unk50;
          actor->flags = 0;
          func_800562A4(actor, 1);
        } else if (st->unk44 >= 192) {

          if (!func_80056DC4(actor, D_80076378[actor->type]->m_Sounds[0])) {
            func_8003851C(actor, 0, 0);
          }
          actor->unk44 =
              (signed char)D_8006E638[(st->unk44 - 192) >> 1].a + st->unk48;
          actor->unk45 =
              (signed char)D_8006E638[(st->unk44 - 192) >> 1].b + st->unk4C;
          actor->posZ =
              st->unk50 +
              (ABS2((signed char)D_8006E638[(st->unk44 - 192) >> 1].a) +
               ABS2((signed char)D_8006E638[(st->unk44 - 192) >> 1].b)) *
                  6;
        } else if (st->unk44 >= 188) {
          st->unk48 = actor->unk44;
          st->unk4C = actor->unk45;
          st->unk50 = actor->posZ;
        } else if (actor->flags != 0) {
          st->unk44 = 188;
          func_800562A4(actor, 1);
        } else {
          if ((st->unk44 >> 1) == 16 && (func_8006272C() & 3) == 0) {
            func_8003AAEC(actor, &D_8006E57C);
          } else if ((st->unk44 >> 1) == 48 && (func_8006272C() & 3) == 0) {
            func_8003AAEC(actor, &D_8006E588);
          } else if ((st->unk44 >> 1) == 80 && (func_8006272C() & 3) == 0) {
            func_8003AAEC(actor, &D_8006E594);
          }
        }

        if (func_80017990(&actor->posX, &SPY.posX) < 2048) {

          func_8001778C(svK, &actor->posX, &SPY.posX);
          svK[2] = (svK[2] * 3) >> 2;
          if (func_800171FC(svK, 1) < 1088) {
            int rotZ;
            int cutsceneId;
            if (st->unk20 != -1) {
              rotZ = D_80075828[st->unk20].unk46;
            } else {
              rotZ = st->unk1C;
            }
            func_8003B854(0, actor);
            func_80059474(actor, rotZ);

            if (st->unk24 != -1) {
              D_800772D8[D_80075964]++;
              D_80075750++;
              func_8002C914(st->unk24, 0);
              if (st->unk20 != -1) {
                D_80075828[st->unk20].unk48 = 1;
              }
              func_80052568(actor);
              continue;
            } else if (cutsceneId = st->unk18, cutsceneId == st->unk24) {
              D_800772D8[D_80075964]++;
              D_80075750++;
              if (st->unk20 != cutsceneId) {
                D_80075828[st->unk20].unk48 = 1;
              }
              func_80052568(actor);
              continue;
            } else {
              actor->unk48 = 2;
              actor->unk44 = st->unk48;
              actor->unk45 = st->unk4C;
              actor->posZ = st->unk50;
              func_8002C924(actor);
              func_800176F0(&SPY.headLook);
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
                func_80017700(&SPY.u208, &SPY.u10C);
                func_800176C8(&SPY.u208, 6);
                SPY.u210 = 0;
                if (SPY.u208 != 0 || SPY.u20C != 0) {
                  func_800175B8(&SPY.u208, func_800171FC(&SPY.u208, 0), 0x60);
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
      func_8003C6E4(actor);
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

    /* Ambient sound source (RoamState). */
    case 286: {
      RoamState *st = actor->state;
      int snd;

      actor->unk55 = st->unk04;
      snd = -1;

      switch (st->mode) {
      case 0:
        snd = 6;
        break;

      case 1:
        if (func_80037F90(&st->timer, 4) != 0) {
          st->timer = 0x258 - (func_8006272C() & 0x7F);
          snd = (func_8006272C() & 7) + 7;
        }
        break;

      case 2:
        switch (st->sub) {
        case 0:
          if (func_80037F90(&st->timer, 4) != 0) {
            st->timer = 0x200 - (func_8006272C() & 0x7F);
            snd = (func_8006272C() & 1) + 0xD;
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
                  func_80017D7C((int *)(st->path + offA),
                                (int *)(st->path + offB), st->vel, st->unk14);
            }
          }

          if (actor->unk54 == 0x7F && st->count % 48 == 0) {
            snd = 0xE;
          }
          break;
        }
        break;

      case 3:
        if (func_80037F90(&st->timer, 4) != 0) {
          st->timer = 0x270 - (func_8006272C() & 0x7F);
          snd = (func_8006272C() & 1) + 0xF;
        }
        break;

      case 4:
        if (func_80037F90(&st->timer, 4) != 0) {
          st->timer = 0x258 - (func_8006272C() & 0x7F);
          snd = (int)func_8006272C() % 4 + 0x15;
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
      func_80055A78(D_800761D4[snd], actor, 8, &actor->unk54);
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
        if (D_80075828[st->idx].type == 0xAD) {
          if (D_80075828[st->idx].unk49 == 0)
            break;
          if (func_80017990(&actor->posX, &SPY.posX) >= 0x1C01)
            break;
        } else if (D_80075828[st->idx].type == 0xFA ||
                   D_80075828[st->idx].type == 0x53) {
          if (D_80075828[st->idx].unk48 < 0x80)
            break;
        } else {
          break;
        }
      }
      flags = st->flags;
      if (flags & 2) {
        if (st->timer != 0) {
          if (func_80037F90(&st->timer, 4) != 0)
            st->flags = (st->flags & ~2) | 4;
          break;
        }
        if (func_80017990(&actor->posX, &SPY.posX) < 0x1000 &&
            ABS2((actor->posZ - actor->unk38) - SPY.posZ) < 0x400) {
          st->timer = 0xB4;
        }
        break;
      }
      if (flags & 0x10)
        actor->unk49 = 1;
      func_80055A78(D_800761D4[54], actor, 8, &actor->unk54);
      if ((func_8006272C() & 1) == 0)
        D_800758E4(1, 6, (int *)actor, 0);
      func_8001778C(svK, &SPY.posX, &actor->posX);
      distance = func_800171FC(svK, 0);
      if (distance < st->unk04 && svK[2] >= -0x3FF && svK[2] < st->unk00 &&
          (SPY.state == 0x11 || SPY.posZ + 0xC00 < actor->posZ + st->unk00)) {
        spyro = &SPY;
        spyro->controlFlags = 0x80000000;
        SPY.pScriptedActor = actor;
        SPY.damageFlags |= 0x800;
        SPY.u200 = actor->posZ + st->unk00;
        func_80017700(&SPY.contact, &actor->posX);
        if (st->flags & 1) {
          spyro->controlFlags |= 0x8000;
        } else {
          D_80078668 = D_8006C934;
          SPY.pCamAnchor = &SPY.posX;
          SPY.pCamTarget = &D_80078668;
          spyro->controlFlags |= 0x200;
          D_80078668.w[0] = ((0x80 - actor->unk46) << 4) & 0xFFF;
        }
        st->timer = 0x78;
        break;
      }
      if ((st->flags & ~1) == 4 && st->timer != 0 &&
          func_80037F90(&st->timer, 4) != 0) {
        st->flags = (st->flags & ~4) | 2;
      }
      break;
    }

    /* Respawner (RespawnState). */
    case 323: {
      RespawnState *st = actor->state;
      Actor *p;

      func_80017700(&actor->posX, &SPY.posX);
      if (func_80037F90(&st->timer, 4) == 0) {
        break;
      }
      st->timer = st->period;

      for (p = D_80075828; p < D_80075890; p++) {
        if (p->type != st->type) {
          continue;
        }
        if (p->unk48 < 0x80) {
          continue;
        }
        if (func_80017990(&SPY.posX, p->state) < 0x6001) {
          continue;
        }

        func_80017700(&p->posX, p->state);
        func_800526A8(p);
        p->unk3A &= 0x7F;
        p->flags = 0;
        p->unk48 = 0;
        p->unk40 = 0;
        p->unk41 = TYPE_DESC(p, 0);
        p->unk3C = 0;
        p->unk3D = 0;
        p->unk3E = 0;
        p->unk3F = 1;
      }
      break;
    }

    /* Dragon pad: saves a checkpoint when Spyro lands on it. */
    case 331: {
      switch (actor->unk48) {
      case 0: {
        if (func_80017990(&actor->posX, &SPY.posX) > 2560) {
          actor->unk48 = 1;
        }
        break;
      }

      case 1: {
        if (func_80017990(&actor->posX, &SPY.posX) < 896 &&
            ABS2(actor->posZ - actor->unk38 - SPY.posZ) < 512) {

          func_80059474(actor, actor->unk46);
          actor->unk48 = 2;
          actor->unk3C = 1;
          actor->unk49 = 0;
        }
        break;
      }

      case 2: {
        actor->unk49 += D_800756CC;

        if (actor->unk49 >= 0x30) {
          actor->unk48 = 0;
          actor->unk3C = 0;
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
      if (D_80075794 != 0) {
        /* Base speed comes from the animation that is playing. */
        int poseBase = TYPE_DESC(actor, actor->unk3D);

        actor->unk41 = poseBase + func_80037EA0(-4, 4);
      }

      if ((actor->flags & 0x10000) != 0 && actor->unk3D == 0) {
        if (actor->unk3C != 1) {
          D_80075794 = 0;
          ANIM_RESET(actor, 1);
        }
        break;
      }

      if (actor->unk3D != 1) {
        break;
      }

      func_800529E4(actor, 1);

      if (D_80075794 != 0 && actor->unk3C != 2) {
        D_80075794 = 0;
        ANIM_RESET(actor, 2);
      }
      break;

    /* Flock leader (FlockState). */
    case 339: {
      FlockState *st = actor->state;

      if ((actor->flags & 0xB0000) != 0 && actor->unk48 < 4) {
        while (st->chainA >= 0 && D_80075828[st->chainA].unk48 >= 0x80) {
          st->chainA = *(int *)D_80075828[st->chainA].state;
        }

        if (st->chainA >= 0) {
          Actor *p = &D_80075828[st->chainA];
          int *link = p->state;

          func_80017700(&p->posX, &actor->posX);
          func_8003ABC0(p, 1, 0, 0);
          func_8003B7C0(p);
          st->chainA = *link;
          func_80052568(p);
          actor->flags = 0;
          ENTER_POSE(actor, 4);
        } else {
          int n = 0;

          while (st->chainB >= 0) {
            Actor *p = &D_80075828[st->chainB];
            int *link = p->state;

            if (p->unk48 < 0x80) {
              func_80017700(&p->posX, &actor->posX);
              st->slots[n][0] = actor->posX + (func_80016CB0(n * 0x555) >> 2);
              st->slots[n][1] = actor->posY + (func_80016C58(n * 0x555) >> 2);
              st->slots[n][2] = actor->posZ;
              func_8003ABC0(p, 2, 0, st->slots[n]);
              n++;
              func_8003B7C0(p);
              func_80052568(p);
            }

            st->chainB = *link;
          }

          st->slots[n][0] = actor->posX + (func_80016CB0(n * 0x555) >> 2);
          st->slots[n][1] = actor->posY + (func_80016C58(n * 0x555) >> 2);
          st->slots[n][2] = actor->posZ;
          func_8003ABC0(actor, 2, 0, st->slots[n]);
          func_8003B7C0(actor);
          st->unk04 = 0x118;
          st->unk00 = func_80038178(
              func_80016AB4(actor->posX - SPY.posX, actor->posY - SPY.posY, 0),
              SPY.bodyRotZ, 0x20, 0x40);
          actor->unk48 = 5;
          ANIM_RESET(actor, 5);
          continue;
        }
      }

      switch (actor->unk48) {
      case 0: {
        int distance = func_80017990(&actor->posX, &SPY.posX);

        if (distance < 0x1400 &&
            ABS2((actor->posZ - actor->unk38) - SPY.posZ) < 0x400) {
          ENTER_POSE(actor, 1);
        }

        if (distance < 0x2800) {
          func_80038DC0(actor, 8, 0x10, 1);
        }
        break;
      }

      case 1: {
        func_80038DC0(actor, 8, 0x10, 1);

        if (D_80075794 != 0) {
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

        if (actor->unk48 == 4) {
          actor->flags = 0;
        }

        if (func_80017990(&actor->posX,
                          (int *)&st->path->nodes[st->path->cur]) < 0x400) {
          if (func_80017990(&actor->posX, &SPY.posX) > 0x1400 ||
              ABS2((actor->posZ - actor->unk38) - SPY.posZ) > 0x800) {
            ENTER_POSE(actor, 0);
          } else {
            spyroAngle =
                func_80016AB4(SPY.posX - actor->posX, SPY.posY - actor->posY, 0);
            nextNode = (st->path->cur + 1) % st->path->count;
            previousNode =
                (st->path->cur - 1 + st->path->count) % st->path->count;

            nextAngle =
                func_80016AB4(st->path->nodes[nextNode].x - actor->posX,
                              st->path->nodes[nextNode].y - actor->posY, 0),
            previousAngle =
                func_80016AB4(st->path->nodes[previousNode].x - actor->posX,
                              st->path->nodes[previousNode].y - actor->posY, 0);

            if (func_80017908(spyroAngle, nextAngle) >
                func_80017908(spyroAngle, previousAngle)) {
              st->path->cur = nextNode;
            } else {
              st->path->cur = previousNode;
            }
          }
        }

        angle = func_80016AB4(st->path->nodes[st->path->cur].x - actor->posX,
                              st->path->nodes[st->path->cur].y - actor->posY, 0);
        previousAngle = (angle - actor->unk46) & 0xFF;
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
        actor->unk46 += previousAngle;

        if (ABS2(nextAngle) < 0x20) {
          if (actor->unk48 == 4) {
            func_80039398(actor, 0xD2, 0x200, 0x200, 0x27);
          } else {
            func_80039398(actor, 0x60, 0x200, 0x200, 0x27);
          }
        }

        if (D_80075794 != 0) {
          if (actor->unk48 == 2) {
            actor->unk48 = 3;
            ANIM_RESET(actor, 3);
          } else {
            actor->unk48 = 2;
            if (actor->unk3D != 2) {
              ANIM_ADVANCE(actor, 2);
            }
          }
          continue;
        }
        break;
      }
      case 5: {
        func_800529E4(actor, 1);

        if (D_80075794 != 0) {
          func_800529E4(actor, 4);
          func_800385BC(actor, 0x20);
          func_80052568(actor);
          continue;
        }

        func_80039910(actor, &st->unk04, st->unk00, 0, 0xC, 0);
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
          func_8002B390(st->anim, 0xFC, 0);
          func_8002B444(st->anim, 0x77, 0);
          actor->unk48 = 0x63;
        }
        st->primed = 1;
      }

      switch (actor->unk48) {
      case 0:
        if (actor->unk49 == 0x1F) {
          actor->unk48 = 5;
        }
        break;

      case 5:
        func_8002B390(st->anim, 0xFC, 0);
        actor->unk48 = 0xA;
        st->timer = 0x1E;
        actor->unk55 = 0x30;
        func_80055A78(D_800761D4[0x41], actor, 8, &actor->unk54);
        break;

      case 0xA:
        if ((func_8002B3F4(st->anim) & 2) != 0) {
          actor->unk48 = 0x63;
          break;
        }
        if (func_80037F90(&st->timer, 4) == 0) {
          break;
        }
        actor->unk48 = 0xB;
        st->timer = 0x1E;
        break;

      case 0xB:
        if ((func_8002B3F4(st->anim) & 2) != 0) {
          actor->unk48 = 0x63;
          break;
        }
        if (func_80037F90(&st->timer, 4) == 0) {
          break;
        }
        actor->unk48 = 0xA;
        st->timer = 0x1E;
        break;

      case 0x63:
        func_800562A4(actor, 1);
        break;
      }
      break;
    }

    /* Water bubbles and splash. */
    case 405:
    case 477:
      /* Removed as soon as the animation finishes. */
      if (D_80075794 == 0) {
        break;
      }
      func_80052568(actor);
      continue;

    /* Collision-triangle rider (PatchRideState). */
    case 391: {
      PatchRideState *st = actor->state;
      int tri[3][3];

      if (st->handle < 0) {
        /* Unattached: probe just ahead and attach to that triangle. */
        tri[0][0] = (short)D_8006CC78[actor->unk46] >> 1;
        tri[0][1] = (short)D_8006CBF8[actor->unk46] >> 1;
        tri[0][2] = 0;
        func_80017758(tri[0], tri[0], &actor->posX);

        if (func_8004AE38(tri[0], &actor->posX) == 0) {
          break;
        }

        st->handle = D_80075808;
        func_80017CB8(st->handle, tri[0]);
        func_80017758(tri[0], tri[0], tri[1]);
        func_80017758(tri[0], tri[0], tri[2]);
        tri[0][0] /= 3;
        tri[0][1] /= 3;
        tri[0][2] /= 3;
        func_8001778C(st->offset, &actor->posX, tri[0]);
        break;
      }

      func_80017CB8(st->handle, tri[0]);
      func_80017758(tri[0], tri[0], tri[1]);
      func_80017758(tri[0], tri[0], tri[2]);
      tri[0][0] /= 3;
      tri[0][1] /= 3;
      tri[0][2] /= 3;
      func_80017758(&actor->posX, tri[0], st->offset);

      if ((D_8007572C & 3) != 0) {
        break;
      }
      tri[0][0] = (short)D_8006CC78[actor->unk46] >> 5;
      tri[0][1] = (short)D_8006CBF8[actor->unk46] >> 5;
      tri[0][2] = 0x20;
      D_800758E4(1, 0xE, &actor->posX, tri[0]);
      break;
    }

    /* Exit vortex (PortalState). */
    case 398: {
      PortalState *st = actor->state;
      PortalDesc *p;
      int mag;
      short progressed;
      switch (actor->unk48) {
      case 1: {
        int vec[3];

        SPY.controlFlags =
            (0x80000000 | 0x2000 | 0x4000 | 0x100 | 0x40 | 0x4 | 0x2 | 0x1);
        SPY.u248 = actor->unk49;
        SPY.pPortal = st->desc;
        SPY.u244 = 0x60;
        SPY.u218 = 5;
        SPY.u027 = 0x7F;

        p = st->desc;
        func_80017758(vec, p->posA, p->posB);
        func_800176C8(vec, 1);

        func_8001778C(&SPY.u208, vec, &SPY.posX);
        func_80017758(&SPY.u208, &SPY.u208, st->offset);

        mag = func_800171FC(&SPY.u208, 1);
        if (mag > 128) {
          func_800175B8(&SPY.u208, mag, 128);
        } else {
          if (actor->unk49 != 0) {
            progressed = (st->desc->index < (st->desc->count - 1));
          } else {
            progressed = st->desc->index;
          }

          if (progressed) {
            actor->unk48 = 2;
          }
        }

        if (func_8004BE4C(&CAM.posX, 0x300, 0x300) != 0 &&
            (D_80075718 & 0x3F) < 0x3F && D_800785B8[D_80075718][0] == 6) {
          actor->unk48 = 0;
          CAM.unkC0 = (0x80000000 | 0x10 | 0x2);
          SPY.u218 = 0xF;
          D_80075864 = 0;
          D_800756B0 = 1;
          D_800756AC = 0;
          D_800757D8 = 1;
          D_8007579C = 1;
          SPY.u206 = st->unk14;
          func_80033F08(&CAM.posX);

          CAM.sim.v[0] += SPY.u11C;

          SPY.u11C += D_80075858;
          SPY.bodyRotZ = SPY.u11C >> 4;
          CAM.sphere = CAM.sim;
          CAM.sphere.v[0] -= (SPY.bodyRotZ) << 4;
          func_80034204(&CAM.posX);
          func_80017758(&CAM.posX, &CAM.posX, &SPY.posX);
          func_800342F8();
          D_80075910 = D_800758FC;
          if (st->unk18 >= 0) {
            CAM.preset = &D_8006CA24[st->unk18];
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

        if (func_8004BE4C(&CAM.posX, 0x300, 0x300) != 0 &&
            (D_80075718 & 0x3F) < 0x3F && D_800785B8[D_80075718][0] == 6) {
          actor->unk48 = 0;
          CAM.unkC0 = (0x80000000 | 0x10 | 0x2);
          D_80075864 = 0;
          D_800756B0 = 1;
          D_800756AC = 0;
          D_800757D8 = 1;
          D_8007579C = 1;
          SPY.u206 = st->unk14;
          func_80033F08(&CAM.posX);

          CAM.sim.v[0] += SPY.u11C;

          SPY.u11C += D_80075858;
          SPY.bodyRotZ = SPY.u11C >> 4;
          CAM.sphere = CAM.sim;
          CAM.sphere.v[0] -= (SPY.bodyRotZ) << 4;
          func_80034204(&CAM.posX);
          func_80017758(&CAM.posX, &CAM.posX, &SPY.posX);
          func_800342F8();
          D_80075910 = D_800758FC;
          if (st->unk18 >= 0) {
            CAM.preset = &D_8006CA24[st->unk18];
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

      if (actor->unk48 == 0) {
        *base = actor->posZ;
        actor->unk48 = 1;
        break;
      }

      actor->posZ = *base + (D_8006CBF8[actor->unk49] >> 4);
      actor->unk49 += D_800756CC;
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
        func_80038458(actor);
        func_800533D0(actor);
      }

      if (actor->unk48 == 0) {
        st->unk04 += D_800756CC;

        if (st->unk04 >= 0x1E1) {
          st->unk04 = 0;
          actor->unk44 = st->unk08;
          actor->unk45 = st->unk0C;
          actor->posZ = st->unk10;
          actor->unk48 = 1;
          actor->unk40 = 8;
          actor->unk41 = 8;
          actor->unk3D = 1;

          st->unk00 = D_800758CC(0x1A6, actor);
          st->unk00->unk44 = actor->unk44;
          st->unk00->unk45 = actor->unk45;
          st->unk00->unk46 = actor->unk46;
          st->unk00->unk41 = 0x10;
          st->unk00->unk52 = 0;
          st->unk00->unk50 = 0;
          st->unk00->unk51 = 0;
          st->unk00->unk47 = 5;

          func_8003851C(actor, 0, 0);
        } else if (st->unk04 >= 0x1A0) {
          actor->unk44 = D_8006E638[LIFT_STEP(st)].a + st->unk08;
          actor->unk45 = D_8006E638[LIFT_STEP(st)].b + st->unk0C;
          a = (signed char)D_8006E638[LIFT_STEP(st)].a;
          b = (signed char)D_8006E638[LIFT_STEP(st)].b;
          if (a < 0) {
            a = -a;
          }
          if (b < 0) {
            b = -b;
          }
          actor->posZ = st->unk10 + (a + b) * 6;

          if (actor->unk54 == 0x7F) {
            func_8003851C(actor, 1, 0);
          }
        } else if (st->unk04 >= 0x19C) {
          /* Just before the shake: remember the rest pose. */
          st->unk08 = actor->unk44;
          st->unk0C = actor->unk45;
          st->unk10 = actor->posZ;
          if (st->unk14 == 1) {
            st->unk04 = 0;
          }
        }
      } else {
        if (actor->unk3C == 1) {
          st->unk04 += D_800756CC;
          st->unk00->unk50 = 0x10;
          st->unk00->unk52 = 0xFF;
        }

        if (st->unk04 >= 0xE6) {
          func_80052568(st->unk00);
          st->unk04 = func_8006272C() & 0xFF;
          actor->unk40 = 8;
          actor->unk41 = 8;
          actor->unk48 = 0;
          actor->unk3D = 0;
          if (actor->unk54 == 0x7F) {
            func_8003851C(actor, 3, 0);
          }
        }
      }

      if ((actor->flags & 0xB0000) == 0) {
        break;
      }

      rider = func_8003ABC0(actor, 5, 0, 0);
      if (st->unk14 == 1) {
        ((unsigned char *)rider->state)[0xE] = 1;
      }

      actor->unk55 = 0x20;
      func_8003851C(actor, 4, 0);

      /* Fragment bursts; each stops early once the dynamic pool is full. */
      for (i = 0; i < 2; i++) {
        if (D_800756A8 - D_800756A4 < 0x15) {
          break;
        }
        D_800758CC(0x1A7, actor);
        D_800758CC(0x1A8, actor);
      }

      for (i = 0; i < 6; i++) {
        if (D_800756A8 - D_800756A4 < 0x15) {
          break;
        }
        D_800758CC(0x1A9, actor);
      }

      for (i = 0; i < 8; i++) {
        int vec[3];

        vec[0] = ((short)D_8006CC78[i * 32] >> 8) * 3;
        vec[1] = ((short)D_8006CBF8[i * 32] >> 8) * 3;
        vec[2] = 0x18;
        D_800758E4(1, 0, &actor->posX, vec);
      }

      D_800758E4(0x10, 0x46, &actor->posX, (void *)0x18);

      if (actor->unk48 == 1) {
        func_80052568(st->unk00);
      }

      func_80055A78(D_80076378[actor->type]->m_Sounds[2], actor, 8,
                    (void *)(long)actor->unk54);
      func_80052568(actor);
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

      if (st->owner->unk48 == 0 || st->owner->unk49 != actor->unk48) {
        func_80052568(actor);
      } else if (st->owner->type == 9) {
        if (func_80017990(&st->owner->posX, &SPY.posX) < 0x400) {
          int oldval;
          oldval = D_8006CC78[actor->unk49];
          actor->unk49 += 8;
          actor->unk46 = actor->unk46 - (oldval * 3 >> 9) +
                         (D_8006CC78[actor->unk49] * 3 >> 9);
        } else {
          int anglespyro;
          int vecA[3];
          int vecB[3];
          int vecC[3];
          int vecD[3];

          actor->unk49 += 8;
          anglespyro = func_80016AB4(SPY.posX - st->owner->posX,
                                     SPY.posY - st->owner->posY, 0);
          vecA[0] = D_8006CC78[anglespyro] * 3 >> 4;
          vecA[1] = D_8006CBF8[anglespyro] * 3 >> 4;
          vecA[2] = 0;
          vecB[0] = D_8006CC78[anglespyro + 64 & 0xFF] >> 5;
          vecB[1] = D_8006CBF8[(anglespyro + 64) & 0xFF] >> 5;
          vecB[2] = 0;
          anglespyro = (anglespyro + 0x80) & 0xFF;
          actor->unk46 = anglespyro + (D_8006CC78[actor->unk49] * 3 >> 9);
          func_800177C0(vecC, vecB, st->unk04 - 1);
          func_800176A0(vecB, 1);
          anglespyro = 2;
          anglespyro = (st->unk04 - 1) * anglespyro;
          vecC[2] += D_8006CC78[anglespyro] * 3 >> 3;
          vecD[0] = vecA[0] * D_8006CC78[anglespyro];
          vecD[1] = vecA[1] * D_8006CC78[anglespyro];
          vecD[2] = 0;
          anglespyro = ((anglespyro - st->unk06 * 4)) & 0xFF;
          func_800177C0(vecB, vecB, st->unk06);
          func_8001778C(vecC, vecC, vecB);
          actor->posX = vecA[0] * D_8006CC78[anglespyro];
          actor->posY = vecA[1] * D_8006CC78[anglespyro];
          actor->posZ = 0;
          func_8001778C(&actor->posX, &actor->posX, vecD);
          func_800176C8(&actor->posX, 10);
          func_80017758(&actor->posX, &actor->posX, vecA);
          func_80017758(&actor->posX, &actor->posX, &st->owner->posX);
          func_8001778C(&actor->posX, &actor->posX, vecC);
          actor->posZ = actor->posZ + (D_8006CC78[anglespyro] * 3 >> 3) + 0x600;
        }
      } else {
        actor->unk49 += 8;
        actor->unk46 = actor->unk38 + (D_8006CC78[actor->unk49] * 3 >> 9);
      }
    }
    break;

    default: /* 77..82, 88..109, 258..259 and everything unlisted */
      break;
    }
  }
}
