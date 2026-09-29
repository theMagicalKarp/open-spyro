/* func_level_16_8007AF28 -- per-frame actor update for level 16 (Blowhard).
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class: a small
 * state machine over the actor's state byte (->unk48), often with a sub-state
 * (->unk49), and usually a class-specific state block behind ->state.
 *
 * Most classes are shared with the other levels' actor updates (level 0's
 * func_level_0_8007D9C8, level 2's func_level_2_8007AE40, level 4's
 * func_level_4_8007AF94, level 30's func_level_30_8007D938); the level-16
 * classes are the rim rider and its bomb and helper (37, 38, 39), the
 * sparking debris (152/153), the path guard (283), the cracking boulder
 * (312), the floor marker (328), the seed pod (329), the ambush plant (333),
 * the boss (495) and the humming bell (497).
 *
 * Load-bearing source forms (matched 2026-09-29):
 *   - No loop-top constant carriers. `one` would take stores of 1 as well as
 *     the compares; without it loop.c hoists 1 into fp for the compares only
 *     and the stores keep their own `li`. 16, 2, 3, 4 and the model and
 *     cosine table addresses are hoisted too, lose allocation and come back
 *     as `li/lui t1` (`sixteen` is a plain 0x10 here).
 *   - Every scratch array is declared at function level, in first-use order;
 *     only case 495's four vectors are block-local (they come last anyway).
 *   - Case 312 declares its debris child pointers once for both debris
 *     loops. Per-loop declarations make them local to one block, local-alloc
 *     then takes s0-s3 there and `actor` loses s3 to s4 function-wide.
 *   - Case 312's shake uses the literal `0x78 - timer` with the ABS2 form of
 *     the height bob (level 0's form); separate dx/dy locals put the index
 *     chain in the wrong register. Case 495's settle test also needs ABS2.
 *   - Case 312 case 1 stores unk49 before unk52/unk50 (so the `li t1,2`
 *     reload leads the block and reorg shares it with the second branch),
 *     and the `(flags & 0xA0000)` test sits after the common tail (`goto`).
 *   - Case 312's jitter is `+= (rand & 0xF) - 8` and the sum is written
 *     back through `ps[3]`; `x - 8 + r` reassociates differently.
 *   - `unk4C` is a word store (`*(int *)&actor->unk4C`), written before the
 *     unk49 store.
 *   - Case 38's spawn loop starts `for (i = 0, ang = rand & 0x3F; ...)`.
 *   - Case 497 splits its channel switch into `case 3: case 4:` and
 *     `case 5: case 7:` with identical bodies (jump.c merges them); the
 *     compare tree needs 5 as its own node. One `r` serves cases 1 and 2.
 *   - The boss counters are `extern int D_800778E8[]` / `D_800778EC[]`
 *     (held base for the increment and the first two stage reads).
 *   - Case 37's rim offset is `tbl[i] << 16 >> 19` (a plain `>> 3` is
 *     narrowed to lhu/sll/sra); case 9 holds `&D_80078A60` in a local.
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

/* The ANIM/ENTER_POSE macros' frame-progress constant. */
#define sixteen 0x10
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
extern void func_8004E3C8(int *pos, int a, int b, int c, Actor *actor, int d);

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

/* ==== The function ==== */
void func_level_16_8007AF28(void) {
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
  int svG[3];
  int svH[3];
  int acc[3][3];
  int svI[3];
  int svJ[3];
  int svK[3];
  int svL[3];
  int svM[3];
  int fx[6];
  int col[3];
  int svN[3];
  int svO[3];
  int svP[3];
  int ps[6];
  int svQ[3];
  int tri[3][3];
  int svR[3];
  int col2[3];
  int svS[3];
  int svT[3];

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

    D_80075794 = actor->unk42 & 2;
    D_800757F4 = actor->unk42 & 1;
    dt = D_800756CC;
    D_800756C4 = dt;

    switch (actor->type) {
    /* Exit drop (DropState). */
    case 9: {
      DropState *st = actor->state;
      int dist = func_80017990(&actor->posX, &CAM.posX);
      int i;

      if (actor->unk48 != 0 ? dist < 0x2800 : dist < 0x2400) {
        if (actor->unk48 == 0) {
          actor->posZ += 0x600;
          func_8003C358(actor, 0);
          actor->posZ -= 0x600;
        }
        actor->unk48 = 1;
      } else {
        actor->unk48 = 0;
      }

      dist = func_80017990(&actor->posX, &SPY.posX);

      for (i = 0; i < D_800756CC; i++) {
        int tick = ++st->tick;

        if (dist < 0x4000) {
          if (!(tick & 3)) {
            D_800758E4(1, 0x15, &actor->posX, (void *)tick);
          }
        } else if ((tick & 0xF) < 8 && !(tick & 3)) {
          D_800758E4(1, 0x4C, &actor->posX, (void *)tick);
        }
      }

      if (dist < 0x400) {
        char *base = (char *)&D_80078A60;
        int top = *(int *)base;
        int z = actor->posZ;
        int drop = top - z;

        if ((unsigned int)(drop - 0x201) < 0x3DFF) {
          int depth = 0x2000;

          if (st->depth != 0) {
            depth = st->depth;
          }

          if (D_80078AD0 == 0x11 || top + 0xC00 < z + depth) {
            if (D_80077FA8 == 2) {
              func_80054578();
            }
            D_80078C4C = 0x80008000;
            D_80078C7C = actor;
            D_80078A84 |= 0x800;
            D_80078C58 = *(int *)((char *)actor + 0x14) + 0x4000;
            func_80017700((int *)(base + 0x174), &actor->posX);

            if (depth < drop) {
              int yaw = D_80076E1E & 0xFFF;

              if (yaw > 0x800) {
                yaw -= 0x1000;
              }
              if (yaw < -0x200) {
                D_8007A6A8[D_80075964] = 1;
                D_80075864 = 0;
                D_8007576C = -1;
                D_800756D0 = 1;
                D_800756B0 = 1;
                D_800756AC = 0;
                D_800757D8 = 1;
                D_8007579C = 1;
                D_800758AC = D_8007596C;
                D_800758B4 = D_8007596C / 10 * 10;
                D_80076E90 = 0x80000012;
                D_80078C4C = 0;
                func_8004AC24(0);
                D_80078CA4 = 0;
                D_80078768 = 0;
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


    /* Vortex pad (VortexState): spins Spyro up and out of the level. */
    case 34: {
      VortexState *st = actor->state;
      unsigned int phase = actor->unk48;

      if (phase != 0 && phase < 5) {
        D_80078C4C = 0x80002000;
        if (D_80078AD0 != 0 && D_80078AD0 != 0xC) {
          actor->unk48 = 5;
        }
      }

      if ((unsigned int)(actor->unk48 - 2) < 3) {
        func_8003C85C(&actor->posX);
        D_800758E4(1, 0xC, actor, (int *)0x602080);
      }

      switch (actor->unk48) {
      case 1:
        D_8007570C = 1;
        func_80017BFC(st->centre, &actor->posX);
        st->angle =
            func_800169AC(actor->posX - SPY.posX, actor->posY - SPY.posY) +
            0x40;
        st->radius = 0;
        st->phase = 0;
        D_80077FDC.v = 1;
        actor->unk44 = 0;
        actor->unk45 = 8;
        actor->unk48 += 1;
        break;

      case 2: {
        st->phase += D_800756CC;

        if (st->phase < 0x80) {
          int step = D_800756CC * 4;

          st->radius += step;
          st->angle += step;
          func_80017C24(&actor->posX, st->centre);
          actor->posX +=
              (func_80016CB0(st->angle * 0x10) * (short)st->radius) >> 12;
          actor->posY +=
              (func_80016C58(st->angle * 0x10) * (short)st->radius) >> 12;
          actor->posZ -= st->phase * 8;
          actor->unk46 += D_800756CC * 4;
        } else {
          int cb;
          int cs;
          int x;
          int y;

          func_80017700(svA, &SPY.posX);
          cb = func_80016CB0(D_80078A66 * 0x10);
          cs = func_80016C58(D_80078A66 * 0x10);
          x = cb * 0xB5;
          y = cs * 0xB5;
          svA[0] += (y + x) * 4 >> 12;
          svA[1] += (int)((func_80016C58(D_80078A66 * 0x10) * 0xB5) -
                          (func_80016CB0(D_80078A66 * 0x10) * 0xB5U)) *
                        4 >>
                    12;
          svA[2] += 0x300;
          func_80017BFC(st->apex, svA);
          func_80017BFC(st->centre, &actor->posX);
          st->phase = 0;
          actor->unk48 += 1;
        }
        if (D_80077378 & 0x40) {
          actor->unk48 = 5;
        }
        break;
      }

      case 3: {
        st->phase += D_800756CC;

        if (st->phase < 0x40) {
          func_80017C24(svB, st->centre);
          func_80017C24(svC, st->apex);
          func_8001778C(svC, svC, svB);
          func_800177C0(svC, svC, st->phase);
          func_800176C8(svC, 6);
          func_80017758(&actor->posX, svB, svC);
          actor->unk46 += D_800756CC * 4;
        } else {
          func_80017700(svD, &SPY.posX);
          svD[0] += func_80016CB0(D_80078A66 * 0x10) >> 3;
          svD[1] += func_80016C58(D_80078A66 * 0x10) >> 3;
          svD[2] += 0x300;
          func_80017BFC(st->centre, svD);
          st->radius = 0x200;
          st->angle = D_80078A66 - 0x40;
          st->phase = 0;
          actor->unk48 += 1;
        }
        if (D_80077378 & 0x40) {
          actor->unk48 = 5;
        }
        break;
      }

      case 4: {
        st->phase += D_800756CC;

        if (st->phase < 0x80) {
          int step = D_800756CC * 4;

          st->radius -= step;
          st->angle -= step;
          func_80017C24(&actor->posX, st->centre);
          actor->posX +=
              (func_80016CB0(st->angle * 0x10) * (short)st->radius) >> 12;
          actor->posY +=
              (func_80016C58(st->angle * 0x10) * (short)st->radius) >> 12;
          actor->posZ -= st->phase * 4;
          actor->unk46 += D_800756CC * 4;
        } else {
          actor->unk48 += 1;
        }
        if (D_80077378 & 0x40) {
          actor->unk48 = 5;
        }
        break;
      }

      case 5: {
        func_8001778C(svE, &actor->posX, &CAM.posX);
        func_80017110(svE, svE);
        D_800758E4(0x10, 0x4D, svE, 0);
        D_8007570C = 0;
        func_800176F0(&D_80078BFC);
        func_8003B854(0, st->idx);
        D_80075810 += 1;
        D_80076FE8[D_80075964] += 1;
        D_80077FDC.v = 0;
        func_80052568(actor);
        continue;
      }
      }

      break;
    }

    /* Type 37: a rider on its parent's rim. Drifts out toward where Spyro
     * was when it last finished an animation, and spawns its type 39 helper
     * when he comes close. */
    case 37: {
      RimState *st = actor->state;

      func_80017700(svD, &actor->posX);
      svD[2] += 0x400;
      func_8004D5EC(svD, 0x10000);
      func_800533D0(actor);

      switch (actor->unk48) {
      case 0:
        if (func_80017990(&actor->posX, &SPY.posX) < 0x200 &&
            actor->unk3E >= 0xF) {
          ENTER_POSE(actor, 4);
        }
        actor->posX =
            st->parent->posX + (D_8006CC78[st->parent->unk46] << 16 >> 19);
        actor->posY =
            st->parent->posY + (D_8006CBF8[st->parent->unk46] << 16 >> 19);
        func_80038DC0(actor, 8, 0, 0);
        if (D_80075794 != 0) {
          st->x = SPY.posX >> 4;
          st->y = SPY.posY >> 4;
          actor->unk48 = 1;
          ANIM_RESET(actor, 1);
        }
        break;

      case 1:
        switch (st->mode) {
        case 1: {
          int d = func_80017990(&actor->posX, &st->parent->posX);

          if (d > st->range) {
            d = st->range;
          }
          if (d >= st->range) {
            ENTER_POSE(actor, 3);
          }
          d += st->reach;
          actor->posX = st->parent->posX +
                        ((d * D_8006CC78[st->parent->unk46]) >> 12);
          actor->posY = st->parent->posY +
                        ((d * D_8006CBF8[st->parent->unk46]) >> 12);
          break;
        }

        case 3:
          if (st->child == 0) {
            st->child = D_800758CC(0x27, actor);
            func_8003851C(actor, 1, 0);
          }
        case 2: {
          int d;

          if (func_80017990(&actor->posX, &st->parent->posX) > st->range) {
            ENTER_POSE(actor, 3);
          }
          svD[0] = st->x << 4;
          svD[1] = st->y << 4;
          d = func_80017990(&actor->posX, svD);
          if (d > 0x3000) {
            d = 0x3000;
          }
          if ((short)st->reach >> 1 >= d) {
            ENTER_POSE(actor, 3);
          }
          actor->posX += (st->reach * D_8006CC78[actor->unk46]) >> 12;
          actor->posY += (st->reach * D_8006CBF8[actor->unk46]) >> 12;
          break;
        }
        }
        if (func_80017990(&actor->posX, &SPY.posX) < 0x200) {
          ENTER_POSE(actor, 4);
        }
        break;

      case 2:
        st->child = 0;
        if (D_80075794 != 0) {
          ((RimParent *)st->parent->state)->busy = 0;
          func_80052568(actor);
        }
        break;

      case 3:
        if (D_80075794 != 0) {
          ENTER_POSE(actor, 4);
        }
        break;

      case 4:
        st->child = D_800758CC(0x27, actor);
        func_8003851C(actor, 1, 0);
        ENTER_POSE(actor, 5);

      case 5:
        if (D_80075794 != 0) {
          ENTER_POSE(actor, 2);
        }
        break;
      }
      break;
    }

    /* Type 38: a thrown bomb. Bursts into four type 38 fragments when it
     * lands on something, and fizzles out on a timer. */
    case 38: {
      int *st = actor->state;

      switch (actor->unk48) {
      case 0:
      case 1:
      case 3:
        if (D_80075794 != 0 && func_80037F90(st, 4) != 0) {
          func_80052568(actor);
        }
        break;

      case 2:
        func_80017700(svA, &actor->posX);
        if (actor->posX < 0x400 || actor->posY < 0x400 ||
            actor->posZ < 0x400) {
          func_80052568(actor);
          continue;
        }
        if (func_8004E2E8(&actor->posX, 0x80, 0x20020) != 0) {
          int i;
          int ang;

          for (i = 0, ang = func_8006272C() & 0x3F; i < 4; i++) {
            if (D_800756A8 - D_800756A4 < 0x15) {
              break;
            }
            D_800758CC(0x26, actor)->unk44 = ang;
            ang += 0x40;
          }
          func_80052568(actor);
          continue;
        }
        if (func_80037F90(st, 4) != 0 ||
            func_8003BCCC(actor, 0x80, 0, 0x100, 0) != 0 ||
            func_8004AE38(svA, &actor->posX) != 0) {
          func_80052568(actor);
          continue;
        }
        if ((*st & 7) == 0) {
          D_800758E4(1, 7, &actor->posX, (void *)0x10);
        }
        break;
      }
      break;
    }

    /* Type 39: the rim rider's helper. Follows its owner and faces the
     * camera until the owner lets it go. */
    case 39: {
      HelperState *st = actor->state;

      if (((RimState *)st->owner->state)->child == 0) {
        func_80052568(actor);
        continue;
      }
      actor->posX = st->owner->posX;
      actor->posY = st->owner->posY;
      if (func_80037F90(&st->beat, 4) != 0) {
        func_8003851C(actor, 0, 0);
        st->beat = 99999;
      }
      if (actor->unk48 == 0) {
        actor->unk46 = func_80016AB4(CAM.posX - actor->posX,
                                     CAM.posY - actor->posY, 0);
        func_80017700(svA, &st->owner->posX);
        if (func_80037F90(st, 4) != 0 || func_8004E2E8(svA, 0x140, 0x26) != 0) {
          ((RimState *)st->owner->state)->child = 0;
          func_80052568(actor);
        }
      }
      break;
    }


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
        svF[0] = func_8006272C() & 3;
        svF[1] = func_8006272C() & 3;
        svF[2] = 0x14;
        D_800758E4(1, 1, &actor->posX, svF);
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

        func_80017700(svG, &actor->posX);
        svG[2] += 0x400;
        func_8004D5EC(svG, 0x10000);
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

            func_80017700(svH, &actor->posX);
            svH[2] += 0x400;

            func_800529E4(actor, 2);

            if (func_8004D5EC(svH, 0x1000) > 0) {
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

        func_800177C0(svI, st->vel, D_800756CC);
        func_800176C8(svI, 1);

        speed = func_800171FC(svI, 1);

        if (220 < speed) {
          func_800175B8(svI, speed, 220);
        }

        func_80017758(svI, &actor->posX, svI);

        if (-220 < st->vel[2]) {
          st->vel[2] -= D_800756CC * 5;
        }

        if (svI[2] < 0) {
          if (actor->type != 14 && actor->type != 15) {
            func_8003B9D4(actor);
          }
          func_80052568(actor);
          continue;
        } else {
          svI[2] += 240;

          if (func_8004BE4C(svI, 240, 240)) {
            if (func_80057380() == 0) {
              if (actor->type != 14 && actor->type != 15) {
                func_8003B9D4(actor);
              }
              func_80052568(actor);
              continue;
            }

            func_80017700(svJ, &D_80077368);
            func_80017700(&actor->posX, &D_80076B80);
            actor->posZ -= 0xF0;

            if (st->count == 0) {
              int groundHeight = func_8004D5EC(svI, 0x400);
              int groundAngle =
                  (signed char)func_800169AC(svJ[2], func_800171FC(svJ, 0));

              if ((svI[2] - 400) < groundHeight && groundAngle < 24) {
                actor->unk49 = 1;
                func_80017700(&actor->posX, svI);
                actor->posZ = groundHeight;

                if (actor->type != 0xF) {

                  st->pitch = -func_800169AC(
                      func_80017A38((svJ[0] * svJ[0]) + (svJ[2] * svJ[2])),
                      svJ[1]);
                  st->yaw = -func_800169AC(svJ[2], svJ[0]);

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

              if (func_80017428(st->vel, svJ, st->vel)) {
                st->count--;

                st->vel[0] = (st->vel[0] >> 3) + (func_8006272C() & 0x3F) - 32;
                st->vel[1] = (st->vel[1] >> 3) + (func_8006272C() & 0x3F) - 32;
                st->vel[2] = (st->vel[2] >> 2) + (func_8006272C() & 0xF);
              }
            }

          } else {
            svI[2] -= 240;
            func_80017700(&actor->posX, svI);
            func_8004D5EC(svI, 0x10000);
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

          func_8001778C(svK, &SPY.posX, st->vel);
          func_800176C8(svK, 5);

          if (func_800171FC(svK, 1) > 480) {
            func_80017700(&actor->posX, &SPY.posX);
            st->timer = 32;
          } else {
            func_800177C0(svK, svK, st->timer);
            func_80017758(&actor->posX, st->vel, svK);
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

          func_8001778C(svG, st->tgt, &actor->posX);

          angleDiff = func_80017948(st->angle, actor->unk46);

          cosWeight1 = func_80016CB0((st->t20 - 1) << 7);
          cosDelta = cosWeight1 - func_80016CB0(st->t20 << 7);
          totalWeight = func_80016CB0((st->t20 - 1) << 7) + 0x1000;

          actor->posX += (svG[0] * cosDelta) / totalWeight;
          actor->posY += ((svG[1] * cosDelta) / totalWeight);
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


    /* The balloonist (BalloonState): smokes, waits for his gnorc, then flies
     * Spyro out once the world is finished. */
    case 120: {
      BalloonState *st = actor->state;

      if (D_80078BBC[0] >= 2) {
        svG[0] = 0;
        svG[1] = 0x64;
        svG[2] = 0;
        func_80017048(&actor->unk20[0], svG, svG);
        func_80017758(svG, svG, &actor->posX);
        D_800758E4(2, 0x42, svG, 0);
        svG[0] = 0;
        svG[1] = -0x64;
        svG[2] = 0;
        func_800170C0(svG, svG);
        func_80017758(svG, svG, &actor->posX);
        D_800758E4(2, 0x42, svG, 0);
      }

      if (D_80078BBC[0] >= 3) {
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

      if (D_80078BBC[0] <= 0) {
        if (st->gnorc != 0 && st->gnorc->type == 16) {
          st->gnorc->unk48 = 0;
        }
        D_80075898 = 0;
        func_80052568(actor);
        continue;
      }

      if (st->gnorc != 0 &&
          func_80017990(&actor->posX, (int *)((char *)D_80078BBC - 0x164)) >=
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

            func_80017700(svL, &st->gnorc->posX);

            if (high == 0) {
              svL[0] -=
                  (unsigned short)D_8006CC78[st->gnorc->unk46] << 16 >> 20;
              svL[1] -=
                  (unsigned short)D_8006CBF8[st->gnorc->unk46] << 16 >> 20;
            } else {
              svL[0] -=
                  (unsigned short)D_8006CC78[st->gnorc->unk46] << 16 >> 22;
              svL[1] -=
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

            func_8001778C(svL, svL, &actor->posX);
            dist = func_800171FC(svL, 1);

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
              func_800175B8(svL, dist, limit);
            } else {
              func_800175B8(svL, dist, reach - 5);
            }
            func_80017758(&actor->posX, &actor->posX, svL);
            func_80038EE0(actor, func_80016AB4(svL[0], svL[1], 0), 0xA, 0, 0);
            actor->unk45 = func_80016AB4(func_800171FC(svL, 0), svL[2], 0);
            actor->unk45 = func_80038098(actor->unk45 & 0xFF, 0, 0x30);
            break;
          }

          case 1:
            D_80078BBC[0] += 1;
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

            func_8001778C(svM, &st->gnorc->posX, &actor->posX);
            dist = func_800171FC(svM, 1);

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
                func_800175B8(svM, dist, lim);
              } else {
                func_800175B8(svM, dist, 0x122);
              }
              func_80017758(&actor->posX, &actor->posX, svM);
              func_80038EE0(actor, func_80016AB4(svM[0], svM[1], 0), 0xA, 0, 0);
              actor->unk45 = func_80016AB4(func_800171FC(svM, 0), svM[2], 0);
              actor->unk45 = func_80038098(actor->unk45 & 0xFF, 0, 0x30);
            }
            break;
          }
          }
        } else {
          /* Nobody in the basket: drift on the wind. */
          actor->unk50 = 0x10;

          if (st->timer <= 0) {
            svG[0] = -0x258;
            svG[1] = (func_8006272C() & 0x310) - 0x188;
            svG[2] = D_80076E28 == 3 ? (func_8006272C() & 0x7F) + 0x64
                                     : (func_8006272C() & 0x1FF) - 0xC8;
            do {
            } while (0);
            st->drift[0] = svG[0];
            st->drift[1] = svG[1];
            st->drift[2] = svG[2];
            st->timer = func_8006272C() & 0x7B;
          } else {
            int *at;

            st->timer -= D_800756CC;
            do {
            } while (0);

            if (D_80076E28 == 3) {
              svG[0] = st->drift[0] + (D_80078B70 >> 2);
              svG[1] = st->drift[1];
              svG[2] = st->drift[2];
              if (svG[2] < 0x64) {
                svG[2] = 0x64;
              }
            } else {
              if (D_80076E28 == 0x80000009) {
                svG[0] = st->drift[0] - 0x200;
              } else {
                svG[0] = st->drift[0];
              }
              svG[1] = st->drift[1];
              svG[2] = st->drift[2];
            }

            func_80017048(&SPY.u034, svG, svG);
            func_80017758(svG, svG, &SPY.posX);
            at = &actor->posX;
            func_8001778C(svG, svG, at);
            func_800176C8(svG, 2);
            func_80017758(at, at, svG);

            if (func_8004BE4C(at, 0x100, 0x100) != 0) {
              func_80017700(at, &D_80076B80);
            }

            if (svG[2] >= 0x21) {
              svG[2] = 0x20;
            }
            if (svG[2] < -0x20) {
              svG[2] = -0x20;
            }

            actor->unk44 = 0;
            actor->unk45 = svG[2];
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


    /* Boss door (DoorState): swings open, rewards the player, then leaves. */
    case 250: {
      DoorState *st = actor->state;
      int *at;

      if (actor->unk48 == 0) {
        if (st->partner != -1) {
          D_80075828[st->partner].unk48 = 3;
        }
        actor->unk48 = 1;
        st->rotX = actor->unk44;
        st->rotY = actor->unk45;
        st->restZ = actor->posZ;
        break;
      }

      if (actor->unk48 == 1) {
        st->phase += D_800756CC;

        if (st->phase >= 0x101) {
          st->phase = 0;
          actor->unk44 = st->rotX;
          actor->unk45 = st->rotY;
          actor->posZ = st->restZ;
          actor->flags = 0;
          func_800562A4(actor, 1);
        } else if (st->phase >= 0xC0) {
          int dx;
          int dy;

          if (func_80056DC4(actor, D_80076378[actor->type]->m_Sounds[0]) == 0) {
            func_8003851C(actor, 0, 0);
          }

          actor->unk44 = D_8006E638[(st->phase - 0xC0) >> 1].a + st->rotX;
          actor->unk45 = D_8006E638[(st->phase - 0xC0) >> 1].b + st->rotY;

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
          st->rotX = actor->unk44;
          st->rotY = actor->unk45;
          st->restZ = actor->posZ;
        } else if (actor->flags != 0) {
          st->phase = 0xBC;
          func_800562A4(actor, 1);
        } else if ((st->phase >> 1) == 0x10 && !(func_8006272C() & 3)) {
          func_8003AAEC(actor, &D_8006E57C);
        } else if ((st->phase >> 1) == 0x30 && !(func_8006272C() & 3)) {
          func_8003AAEC(actor, &D_8006E588);
        } else if ((st->phase >> 1) == 0x50 && !(func_8006272C() & 3)) {
          func_8003AAEC(actor, &D_8006E594);
        }

        at = &actor->posX;
        if (func_80017990(at, &SPY.posX) < 0x800) {
          func_8001778C(svN, at, &SPY.posX);
          svN[2] = (svN[2] * 3) >> 2;

          if (func_800171FC(svN, 1) < 0x440) {
            int facing;

            facing =
                st->partner != -1 ? D_80075828[st->partner].unk46 : st->unk1C;
            func_8003B854(0, actor);
            func_80059474(actor, facing);

            if (st->reward != -1) {
              D_800772D8[D_80075964] += 1;
              D_80075750 += 1;
              func_8002C914(st->reward, 0);

              if (st->partner != -1) {
                D_80075828[st->partner].unk48 = 1;
              }
              func_80052568(actor);
              continue;
            }

            if (st->unk18 == st->reward) {
              D_800772D8[D_80075964] += 1;
              D_80075750 += 1;

              if (st->partner != -1) {
                D_80075828[st->partner].unk48 = 1;
              }
              func_80052568(actor);
              continue;
            }

            actor->unk48 = 2;
            actor->unk44 = st->rotX;
            actor->unk45 = st->rotY;
            actor->posZ = st->restZ;
            func_8002C924(actor);
            func_800176F0(&SPY.headLook);
            D_80078C4C = 0x80002147;

            if (D_80078AF4 != 0) {
              D_80078C70 = 6;
              D_80078C60.x = D_80078B64 >> 8;
              D_80078C64 = D_80078B68 >> 8;
              if (D_80078B6C > 0) {
              clear_c68:
                D_80078C68 = 0;
              } else {
                D_80078C68 = D_80078B6C >> 6;
              }
            } else {
              int *look = &SPY.u208;

              D_80078C70 = 3;
              func_80017700(look, &SPY.u10C);
              func_800176C8(look, 6);
              D_80078C68 = 0;

              if (SPY.u208 != 0 || D_80078C64 != 0) {
                func_800175B8(look, func_800171FC(look, 0), 0x60);
                goto clear_c68;
              }
            }
          }
        }

      } else {
        D_80078C4C = 0x80002000;
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

    /* Type 283: path guard. Walks its waypoint ring, waits on its partner,
     * rears up (and calls for help) when Spyro comes near, and is knocked
     * away when hit. */
    case 283: {
      L16Guard *st = actor->state;

      if (actor->unk48 != 2) {
        D_800758E4(1, 0x41, actor, 0);
        if (actor->flags & 0xB0000) {
          st->speed = 0xDC;
          st->knock = func_80038178(func_80016AB4(actor->posX - SPY.posX,
                                                  actor->posY - SPY.posY, 0),
                                    D_80078A66, 0x20, 0x60);
          if (actor->flags & 0x20000) {
            st->speed += 0x8C;
          }
          func_80056200(actor->unk54, 4);
          func_8003ABC0(actor, 3, 0, 0);
          func_8003B7C0(actor);
          ENTER_POSE(actor, 2);
        }
      }

      switch (actor->unk48) {
      case 0:
        if (st->flags & 2) {
          st->path->cur = st->node;
          if (func_80017990(&actor->posX,
                            &st->path->nodes[st->path->cur].x) <= 0x100) {
            int dz;

            func_80038DC0(actor, 8, 0x10, 1);
            if (func_80017990(&actor->posX, &SPY.posX) < 0x1000) {
              dz = (actor->posZ - actor->unk38) - SPY.posZ;
              if (dz > 0 ? dz < 0x800
                         : SPY.posZ - (actor->posZ - actor->unk38) < 0x800) {
                st->timer = 0;
                ENTER_POSE(actor, 1);
              }
            }
            if (func_80037F90(&st->timer, 4) != 0) {
              st->flags &= 1;
            }
            break;
          }
        } else {
          if (func_80017990(&actor->posX, &SPY.posX) < 0x1000) {
            int dz = (actor->posZ - actor->unk38) - SPY.posZ;

            if (dz > 0 ? dz < 0x800
                       : SPY.posZ - (actor->posZ - actor->unk38) < 0x800) {
              func_80038DC0(actor, 8, 0x10, 1);
              if (st->timer == 0 || --st->timer <= 0) {
                st->flags |= 2;
                ENTER_POSE(actor, 1);
              }
              break;
            }
          }
          if (st->flags == 1) {
            L16Guard *ps = D_80075828[st->partner].state;

            st->path->cur = 0;
            if (st->dist == 0) {
              st->dist = func_80017990(&actor->posX,
                                       &st->path->nodes[st->path->cur].x);
            }
            if (st->dist < ps->dist) {
              st->dist = ps->dist;
            }
            if (ps->heading != 0) {
              st->heading = (ps->heading + 0x80) & 0xFF;
              func_80038638(actor, &st->path->nodes[0].x, st->dist,
                            st->heading, 8, 0x40, 0xE, 0x80, 0xFF, 0xFF, 0x180,
                            0x15C, 0x27);
              st->heading = 0;
              ps->heading = 0;
            } else {
              st->heading =
                  (func_80016AB4(
                       actor->posX - st->path->nodes[st->path->cur].x,
                       actor->posY - st->path->nodes[st->path->cur].y, 0) +
                   st->angle) &
                  0xFF;
              func_80038638(actor, &st->path->nodes[0].x, st->dist,
                            st->heading, 8, 0x40, 0xE, 0x80, 0xFF, 0xFF, 0, 0,
                            4);
            }
            break;
          }
          st->path->cur = 0;
          if (func_80017990(&actor->posX,
                            &st->path->nodes[st->path->cur].x) <= 0x100) {
            break;
          }
        }
        func_80038EE0(
            actor,
            func_80016AB4(st->path->nodes[st->path->cur].x - actor->posX,
                          st->path->nodes[st->path->cur].y - actor->posY, 0),
            8, 0x10, 1);
        func_80039398(actor, 0x60, 0x180, 0x180, 0x27);
        break;

      case 1:
        func_80038DC0(actor, 8, 0x10, 1);
        if (D_80075794 != 0) {
          actor->unk48 = 0;
          ANIM_RESET(actor, 0);
          continue;
        }
        if (st->timer <= 0) {
          st->timer = 1;
          D_800758CC(0x26, actor);
        } else if (st->timer < 2 && actor->unk3F >= 2) {
          st->timer = 2;
          D_800758CC(0x26, actor);
        } else if (st->timer < 0x3C && actor->unk3F >= 6) {
          st->timer = 0x3C;
          D_800758CC(0x26, actor);
        }
        break;

      case 2:
        func_80039910(actor, &st->speed, st->knock, 0, 0xC, 0);
        if (D_80075794 != 0) {
          func_800529E4(actor, 4);
          func_800385BC(actor, 0x18);
          func_80052568(actor);
          continue;
        }
        break;
      }
      func_800529E4(actor, 1);
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
        if (func_80037F90(&st->wait, 4) != 0) {
          st->wait = 0x258 - (func_8006272C() & 0x7F);
          voice = (func_8006272C() & 7) + 7;
        }
        break;

      case 2:
        switch (st->leg) {
        case 0:
          if (func_80037F90(&st->wait, 4) != 0) {
            st->wait = 0x200 - (func_8006272C() & 0x7F);
            voice = (func_8006272C() & 1) + 0xD;
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
            st->left = func_80017D7C(
                &st->path->nodes[st->path->cur].x,
                &st->path->nodes[(st->path->cur + 1) % st->path->count].x,
                st->step, st->speed);
          }

          if (actor->unk54 == 0x7F && st->left == (st->left / 48) * 48) {
            voice = 0xE;
          }
          break;
        }
        }
        break;

      case 3:
        if (func_80037F90(&st->wait, 4) != 0) {
          st->wait = 0x270 - (func_8006272C() & 0x7F);
          voice = (func_8006272C() & 1) + 0xF;
        }
        break;

      case 4:
        if (func_80037F90(&st->wait, 4) != 0) {
          int r;

          st->wait = 0x258 - (func_8006272C() & 0x7F);
          r = func_8006272C();
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
        func_80055A78(D_800761D4[voice], actor, 8, &actor->unk54);
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
      func_8001778C(svN, &SPY.posX, &actor->posX);
      distance = func_800171FC(svN, 0);
      if (distance < st->unk04 && svN[2] >= -0x3FF && svN[2] < st->unk00 &&
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

        svO[0] = (func_8006272C() & 0xFE) - 0x7F;
        svO[1] = (func_8006272C() & 0xFE) - 0x7F;
        svO[2] = (func_8006272C() & 0xFE) - 0x40;
        func_80017758(svO, svO, &actor->posX);
        D_800758E4(1, 0x42, svO, (int *)1);
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
            svP[0] = (func_8006272C() & 0xFE) - 0x7F;
            svP[1] = (func_8006272C() & 0xFE) - 0x7F;
            svP[2] = (func_8006272C() & 0xFE) - 0x40;
            func_80017758(svP, svP, &actor->posX);
            D_800758E4(1, 0x42, svP, (int *)1);
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
          svQ[0] = (func_8006272C() & 7) + 0x10;
          svQ[1] = (func_8006272C() & 7) + 6;
          D_800758E4(1, 0x10, ps, svQ);
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

    /* Type 328: floor-snapped marker. Finds the floor triangle under itself
     * once, then sits at that triangle's centroid. */
    case 328: {
      int *st = actor->state;

      if (*st < 0) {
        int z;
        int d;

        actor->posZ += 0x200;
        z = func_8004D5EC(&actor->posX, 0x400);
        d = actor->posZ - z;
        if (d < 0) {
          d = -d;
        }
        if (d < 0x400) {
          actor->posZ = z;
          *st = D_80075808;
        } else {
          actor->posZ -= 0x200;
        }
        if (*st < 0) {
          break;
        }
      }
      func_80017CB8(*st, tri[0]);
      func_80017700(&actor->posX, tri[0]);
      func_80017758(&actor->posX, &actor->posX, tri[1]);
      func_80017758(&actor->posX, &actor->posX, tri[2]);
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
          func_800176F0(svQ);
          D_800758E4(1, 0, &st->prize->posX, svQ);
        }
        if ((func_80017990(&st->prize->posX, &SPY.posX) < 0x1E0 &&
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
            func_80017700(svR, &st->prize->posX);
            svR[0] += D_8006CC78[j * 43] >> 6;
            svR[1] += D_8006CBF8[j * 43] >> 6;
            svR[2] += 0x28;
            col2[0] = D_8006CC78[j * 43] >> 7;
            col2[1] = D_8006CBF8[j * 43] >> 7;
            col2[2] = 0x10;
            D_800758E4(1, 0, svR, col2);
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

    /* Type 333: ambush plant. Rears up and bites when Spyro walks past. */
    case 333:
      switch (actor->unk48) {
      case 0:
        if (func_80017990(&actor->posX, &SPY.posX) > 0xA00) {
          actor->unk48 = 1;
        }
        break;

      case 1:
        if (func_80017990(&actor->posX, &SPY.posX) < 0x380) {
          int dz = (actor->posZ - actor->unk38) - SPY.posZ;

          if (dz > 0 ? dz < 0x200
                     : SPY.posZ - (actor->posZ - actor->unk38) < 0x200) {
            func_80059474(actor, actor->unk46);
            actor->unk48 = 2;
            actor->unk3C = 1;
            actor->unk49 = 0;
          }
        }
        break;

      case 2:
        actor->unk49 += D_800756CC;
        if (actor->unk49 >= 0x30) {
          actor->unk48 = 0;
          actor->unk3C = 0;
        }
        break;
      }
      break;


    /* Water bubbles and splash. */
    case 405:
    case 477:
      /* Removed as soon as the animation finishes. */
      if (D_80075794 == 0) {
        break;
      }
      func_80052568(actor);
      continue;


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
        if (func_80017990(&st->parent->posX, &SPY.posX) < 0x400) {
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
          bearing = func_80016AB4(SPY.posX - st->parent->posX,
                                  SPY.posY - st->parent->posY, 0);
          svQ[0] = (D_8006CC78[bearing] * 3) >> 4;
          svQ[1] = (D_8006CBF8[bearing] * 3) >> 4;
          svQ[2] = 0;
          svR[0] = (unsigned short)D_8006CC78[(bearing + 0x40) & 0xFF] << 16 >> 21;
          svR[1] = (unsigned short)D_8006CBF8[(bearing + 0x40) & 0xFF] << 16 >> 21;
          svR[2] = 0;
          bearing = (bearing + 0x80) & 0xFF;
          actor->unk46 = bearing + ((D_8006CC78[actor->unk49] * 3) >> 9);
          func_800177C0(svS, svR, st->radius - 1);
          func_800176A0(svR, 1);
          arm = st->radius - 1;
          svS[2] += (D_8006CC78[arm * 2] * 3) >> 3;
          svT[0] = svQ[0] * D_8006CC78[arm * 2];
          svT[1] = svQ[1] * D_8006CC78[arm * 2];
          svT[2] = 0;
          bearing = arm << 1;
          bearing = (bearing - (st->phase * 4)) & 0xFF;
          func_800177C0(svR, svR, st->phase);
          func_8001778C(svS, svS, svR);
          swing = &D_8006CC78[bearing];
          actor->posX = svQ[0] * *swing;
          actor->posY = svQ[1] * *swing;
          actor->posZ = 0;
          func_8001778C(&actor->posX, &actor->posX, svT);
          func_800176C8(&actor->posX, 0xA);
          func_80017758(&actor->posX, &actor->posX, svQ);
          func_80017758(&actor->posX, &actor->posX, &st->parent->posX);
          func_8001778C(&actor->posX, &actor->posX, svS);
          actor->posZ = actor->posZ + ((*swing * 3) >> 3) + 0x600;
        }
      } else {
        unsigned char now = actor->unk49 + 8;

        actor->unk49 = now;
        actor->unk46 = actor->unk38 + ((D_8006CC78[now] * 3) >> 9);
      }

      break;
    }
    

    /* Type 495: the level boss. Patrols three paths in turn, dropping a
     * type 37 rider on Spyro when he is in front of it; each hit sends it
     * to the next path, and the third ends the fight. */
    case 495: {
      L16Boss *st = actor->state;
      int dist = func_80017990(&actor->posX, &SPY.posX);
      int dz = (actor->posZ - actor->unk38) - SPY.posZ;

      if ((dz > 0 ? dz < 0x578
                  : SPY.posZ - (actor->posZ - actor->unk38) < 0x578) &&
          dist < 0x2800 && func_80038250(&actor->posX) != 0 &&
          D_80078AD0 != 0xB && D_80078AD0 != 0x14 && actor->unk48 != 0x50 &&
          actor->unk48 != 0x5A) {
        D_80078C4C = 0x80010000;
        D_80078C7C = actor;
      }
      if (func_80038C4C(&SPY.posX, &st->unk74) != 0) {
        actor->unk55 = 0x10;
      }
      if (actor->flags & 0x90000) {
        int v[3];

        actor->flags = 0;
        func_80017700(v, &actor->posX);
        v[2] += 0x400;
        if (func_80033E40(v, &SPY.posX) != 0) {
          switch (actor->unk48) {
          case 5:
          case 0x47:
          case 0x48:
          case 0x51:
          case 0x52:
          case 0x53:
          case 0x5B:
          case 0x5C:
          case 0x5D:
            D_800778E8[0] += 1;
            if (st->minion != 0) {
              func_800562A4(st->minion, 1);
              func_80052568(st->minion);
              st->minion = 0;
            }
            if (actor->unk48 == 0x5B || actor->unk48 == 0x5C ||
                actor->unk48 == 0x5D) {
              func_80056200(actor->unk54, 4);
              ENTER_POSE(actor, 7);
            }
            if (st->prev >= 0) {
              Actor *a = &D_80075828[st->prev];
              int *as = a->state;

              func_80017700(&a->posX, &actor->posX);
              func_8003ABC0(a, 1, 0, 0);
              func_8003B7C0(a);
              st->prev = *as;
              func_80052568(a);
            }
            ENTER_POSE(actor, 2);
          }
        }
      }
      func_800529E4(actor, 4);
      func_80017700(svQ, &actor->posX);
      svQ[2] += 0x400;
      func_8004D5EC(svQ, 0x10000);
      func_800533D0(actor);
      if (st->floorZ > 0) {
        int z = actor->posZ;
        int d = st->floorZ - z;

        if (ABS2(d) > 0x20 && actor->unk48 != 0) {
          actor->posZ = z + (d >> 3);
          goto settled;
        }
      }
      st->floorZ = -1;
    settled:
      if (st->started == 0) {
        if (D_800778EC[0] == 0) {
          if (func_80037F90(&st->delay, 4) != 0) {
            D_8007570C = 0;
            st->started = 1;
          } else {
            D_8007570C = 1;
            D_80078C4C = 0x80002000;
          }
        } else {
          Actor *a = &D_80075828[st->prev];
          int *as = a->state;

          st->started = 1;
          if (D_800778EC[0] > 0) {
            actor->unk48 = 0x51;
            st->unk60 = 0;
            st->phase = 1;
            st->path = st->paths[3];
            if (D_80075828[st->prev].unk53 != 0xFF &&
                D_80075828[st->prev].unk48 < 0x80) {
              func_8003ABC0(a, 1, 0, 0);
              func_8003B7C0(a);
              func_80052568(a);
            }
            st->prev = *as;
            func_8002B390(0, 0xFC, 0);
            st->chan = -1;
          }
          if (D_800778EC[0] >= 2) {
            actor->unk48 = 0x5B;
            st->path = st->paths[4];
            a = &D_80075828[st->prev];
            as = a->state;
            if (a->unk53 != 0xFF && a->unk48 < 0x80) {
              func_8003ABC0(a, 1, 0, 0);
              func_8003B7C0(a);
              func_80052568(a);
            }
            st->prev = *as;
          }
          func_80017700(&actor->posX, &st->path->nodes[0].x);
          st->floorZ = actor->posZ + 0x578;
          if (D_800778EC[0] == 3) {
            func_80052568(actor);
            continue;
          }
        }
      }

      switch (actor->unk48) {
      case 0:
        switch (st->phase) {
        case 0:
          actor->unk48 = 0x46;
          st->p24 = 0;
          break;
        case 1:
          actor->unk48 = 0x50;
          st->p24 = 0;
          break;
        case 2:
          actor->unk48 = 0x5A;
          st->p24 = 0;
          break;
        }
        break;

      case 2:
        if (D_80075794 != 0) {
          if (st->chan >= 0) {
            if (func_8002B3F4(st->chan) & 2) {
              func_8002B390(st->chan, 0xFC, 0);
              actor->unk48 = 0x63;
              actor->flags = 0;
              st->chan = -1;
            }
          } else {
            actor->flags = 0;
            actor->unk48 = 0x63;
          }
        }
        break;

      case 7:
        if (D_80075794 != 0) {
          func_80038458(actor);
          func_8003ABC0(actor, 1, 0, 0);
          func_8003B7C0(actor);
          func_80052568(actor);
          continue;
        }
        break;

      case 0x46:
        actor->unk48 = 0x47;
        st->floorZ = actor->posZ + 0x578;
        break;

      case 0x47:
        func_80038DC0(actor, 8, 0x10, 1);
        if (dist < 0x2800 && st->minion == 0 && st->floorZ == -1) {
          actor->unk48 = 0x48;
          st->floorZ = actor->posZ - 0x578;
          ANIM_GO(actor, 5);
        }
        break;

      case 0x48:
        func_80038DC0(actor, 8, 0x10, 1);
        if (actor->unk3C == 5 && actor->unk3E >= 6 && st->minion == 0) {
          int v[3];

          func_80017700(v, &actor->posX);
          v[2] += 0x400;
          if (func_80033E40(v, &SPY.posX) != 0) {
            st->minion = D_800758CC(0x25, actor);
            func_8003851C(st->minion, 0, 0);
          }
        }
        if (D_80075794 != 0) {
          if (actor->unk3C == 5 && st->minion != 0) {
            ANIM_SET(actor, 6);
          }
          if (D_80075794 != 0 && actor->unk3C == 6) {
            actor->unk48 = 0x47;
            st->floorZ = actor->posZ + 0x578;
            ANIM_SET(actor, 0);
          }
        }
        break;

      case 0x50:
        if (func_8003BFC0(actor, st->path, st->p18, &st->p24, 0x20, 4) == 2) {
          actor->unk48 = 0x51;
          actor->unk55 = 0;
          st->floorZ = actor->posZ + 0x578;
          st->path = st->paths[3];
        }
        actor->unk45 = 0;
        break;

      case 0x51:
        if (dist < 0x2800 && st->floorZ == -1) {
          if (st->unk60 == 0) {
            if (func_80039E94(actor, st->path, 0x80, 0xB4, 0, 8, 0x28, 0xFF,
                              1) != 0) {
              st->unk60 = 1;
            }
          } else if (st->minion == 0 && dist > 0x1000) {
            actor->unk48 = 0x52;
            st->floorZ = actor->posZ - 0x578;
            ANIM_GO(actor, 5);
          }
          break;
        }
        func_80038DC0(actor, 8, 0x10, 1);
        break;

      case 0x52:
        func_80038DC0(actor, 8, 0x10, 1);
        if (func_80017908(D_80078A66,
                          func_80016AB4(actor->posX - SPY.posX,
                                        actor->posY - SPY.posY, 0)) < 0x30 &&
            actor->unk3C == 5 && actor->unk3E >= 6 && st->minion == 0) {
          int v[3];

          func_80017700(v, &actor->posX);
          v[2] += 0x400;
          if (func_80033E40(v, &SPY.posX) != 0) {
            st->minion = D_800758CC(0x25, actor);
            func_8003851C(st->minion, 0, 0);
          }
        }
        if (D_80075794 != 0) {
          if (actor->unk3C == 5 && st->minion != 0) {
            ANIM_SET(actor, 6);
          }
          if (D_80075794 != 0 && actor->unk3C == 6) {
            actor->unk48 = 0x53;
            st->wait = 0x6E;
            st->unk60 = 0;
            ANIM_SET(actor, 0);
          }
        }
        break;

      case 0x53:
        if (func_80037F90(&st->wait, 4) != 0) {
          st->floorZ = actor->posZ + 0x578;
          actor->unk48 = 0x51;
        }
        break;

      case 0x5A:
        if (func_8003BFC0(actor, st->path, st->p18, &st->p24, 0x20, 4) == 2) {
          actor->unk48 = 0x5B;
          st->floorZ = actor->posZ + 0x578;
          st->path = st->paths[4];
        }
        actor->unk45 = 0;
        break;

      case 0x5B:
        if (dist < 0x2800 && st->floorZ == -1) {
          if (st->unk60 == 0) {
            if (func_80039E94(actor, st->path, 0x80, 0xB4, 0, 8, 0x28, 0xFF,
                              1) != 0) {
              st->unk60 = 1;
            }
          } else if (st->minion == 0 && dist > 0x1000) {
            actor->unk48 = 0x5C;
            st->floorZ = actor->posZ - 0x578;
            ANIM_GO(actor, 5);
          }
          break;
        }
        func_80038DC0(actor, 8, 0x10, 1);
        break;

      case 0x5C:
        func_80038DC0(actor, 8, 0x10, 1);
        if (func_80017908(D_80078A66,
                          func_80016AB4(actor->posX - SPY.posX,
                                        actor->posY - SPY.posY, 0)) < 0x30 &&
            actor->unk3C == 5 && actor->unk3E >= 6 && st->minion == 0) {
          int v[3];

          func_80017700(v, &actor->posX);
          v[2] += 0x400;
          if (func_80033E40(v, &SPY.posX) != 0) {
            st->minion = D_800758CC(0x25, actor);
            func_8003851C(st->minion, 0, 0);
          }
        }
        if (D_80075794 != 0) {
          if (actor->unk3C == 5 && st->minion != 0) {
            ANIM_SET(actor, 6);
          }
          if (D_80075794 != 0) {
            if (actor->unk3C == 5) {
              ANIM_SET(actor, 6);
            }
            if (D_80075794 != 0 && actor->unk3C == 6) {
              actor->unk48 = 0x5D;
              st->unk60 = 0;
              st->wait = 0x6E;
              ANIM_SET(actor, 0);
            }
          }
        }
        break;

      case 0x5D:
        if (func_80037F90(&st->wait, 4) != 0) {
          st->floorZ = actor->posZ + 0x578;
          actor->unk48 = 0x5B;
        }
        break;

      case 0x63:
        st->phase++;
        switch (st->phase) {
        case 0:
          st->path = st->paths[0];
          break;
        case 1:
          st->path = st->paths[1];
          break;
        case 2:
          st->path = st->paths[2];
          break;
        }
        st->path->cur = 0;
        st->p28 = 0;
        st->p2C = 0;
        ENTER_POSE(actor, 0);
      }
      break;
    }

    /* Type 497: humming bell. Rings a looping voice while it is alive,
     * rings out when struck, and is removed once the voice has died away. */
    case 497: {
      L16Bell *st = actor->state;
      int r;

      if ((actor->flags & 0x90000) && actor->unk48 != 1) {
        func_80056200(actor->unk54, 4);
        func_8003ABC0(actor, 4, 0, 0);
        func_8003B7C0(actor);
        func_8002B3F4(st->chan);
        ENTER_POSE(actor, 1);
      }

      switch (actor->unk48) {
      case 0:
        actor->unk48 = 2;
        ANIM_RESET(actor, 2);
        continue;

      case 1:
        if (D_80075794 != 0 && actor->unk50 != 0) {
          actor->unk50 = 0;
          actor->unk41 = 0;
          actor->unk08 = 0;
          func_800385BC(actor, 0x20);
        }
        switch (st->chan) {
        case 3:
        case 4:
          r = func_8002B3F4(st->chan);
          if (r & 2) {
            if (((r >> 8) & 0xFF) != 0) {
              func_8002B390(st->chan, 0xFC, 0);
            } else if (actor->unk50 == 0) {
              func_80052568(actor);
              continue;
            }
          }
          break;

        case 5:
        case 7:
          r = func_8002B3F4(st->chan);
          if (r & 2) {
            if (((r >> 8) & 0xFF) != 0) {
              func_8002B390(st->chan, 0xFC, 0);
            } else if (actor->unk50 == 0) {
              func_80052568(actor);
              continue;
            }
          }
          break;
        }
        break;

      case 2:
        if (D_8007572C & 1) {
          D_800758E4(1, 8, actor, &D_80075828[st->target]);
        }
        r = func_8002B3F4(st->chan);
        if ((r & 2) && func_80037F90(&st->timer, 4) != 0) {
          st->timer = st->period;
          if (st->chan >= 5 && ((r >> 8) & 0xFF) >= 0x65) {
            st->timer = 0;
          }
          func_8002B390(st->chan, 0xFC, 0);
        }
        break;
      }
      func_800529E4(actor, 1);
      break;
    }
}
  }
}
