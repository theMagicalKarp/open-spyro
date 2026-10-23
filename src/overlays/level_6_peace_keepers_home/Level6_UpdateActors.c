/* Level6_UpdateActors -- per-frame actor update for level 6 (Peace
 * Keepers home). 0x8007E3A0, 48,824 bytes.
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class. The
 * level-6 classes are the breakable prop (146), the hint giver (188, level
 * 24's 191 with other dialogue ids), the critter (193), the Peace Keeper
 * soldiers (214/216, a kind-driven state machine), the cannon (225) and its
 * fireballs (237), the reward burst (417) and falling sparks (454-456).
 *
 * Load-bearing source forms (case 225, fireball launch):
 *   - The two 0xF0 stores go through the multi-set `h`, which puts
 *     `li v1,240` in the posZ load-delay slot.
 *   - The spawn sequence is wrapped in `do { } while (0)`. The loop notes
 *     end the basic block after MoveActorAlongHeading, so `best = 0` starts the next
 *     block instead of drifting above the spawn call, where reorg would put
 *     it in the jalr delay slot ahead of `move a1,s4`.
 */

#include "actor.h"
#include "level_actors.h"
#include "vecmath.h"
#include "level_globals.h"
#include "level_update_actors.h"


/* Progress-per-tick byte of animation `anim` in the actor's model. */
#define TYPE_DESC(a, id)                                                     \
  (g_apActorMeshTable[(a)->type]->m_Animations[(id)]->m_ProgressPerTick)

/* Step a countdown timer; non-zero when it fires. */

/* Mark an item collected; the second argument is the actor or its id. */



#define AS_I32(sym) (((I32 *)&(sym))->v)

/* Type 421: life chest. Shakes and pops its lid on a timer, spawning a
 * child actor; breaks into fragments when damaged. */

#define LIFT_STEP(st) (((st)->phase - 0x1A0) >> 1)

/* 0x18-byte spherical camera coordinates; g_aCameraXformPresets holds the presets. */

#define CAM g_anWorldToCameraRotMtx


extern int GetGroundTriggerRegionType(void);


/* Glow node owned by Sparx; follows its anchor position. */

/* Shift a vector left by n. */

/* ==== Helper macros ====
 *
 * Absolute value. The two forms differ only at zero; both occur. */
#define ABS(x) ((x) >= 0 ? (x) : -(x))
#define ABS2(x) ((x) > 0 ? (x) : -(x))
#define SPY_DZ(a) ((a)->posZ - (a)->floorOffset - SPY.posZ)

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

#define SPY g_anSpyroWorldPosBlock

/* ==== level_30 per-class state blocks ====
 *
 * Gnorc Gnexus is the hub for Gnasty's world: the balloonist, the vortex
 * pads, the two boss-door gnorcs and the shared fodder/debris classes. Each
 * block below is the ->state record of one actor class, named after the case
 * that uses it. */

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

/* Cases 255/256, 310/311, 257, 309 — debris shards thrown by a break. */
typedef struct ShardState {
  short vel[3];  /* 0x00 */
  short spin[3]; /* 0x06 pitch / yaw / roll per frame */
  int life;      /* 0x0C */
  int floor;     /* 0x10 */
} ShardState;

extern volatile unsigned char
    g_bSpyroPersistentEulerZ;          /* SPY.bodyRotZ, absolute form (A240) */
extern int g_nLevelReadyFlag[]; /* balloon sequence stage (== SPY.u164) */

#define g_nSpyroRequestVelocityY g_anSpyroRequestVelocity.y


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


/* Case 328 -- floor-snapped marker. */

/* ==== Camera and Spyro globals used by the camera arms ==== */

#define sixteen 0x10

/* Types 288/289: bouncing debris. Flies on its velocity until its life runs
 * out; type 288 tumbles, 289 sways as it falls. */

/* ==== Level 7 classes ==== */

/* from level_34 */
#define PATH_X(p, i) (*(int *)((p) + ((i) << 4) + 0x8))
/* from level_34 */
#define PATH_Y(p, i) (*(int *)((p) + ((i) << 4) + 0xC))
/* from level_34 */
#define PATH_Z(p, i) (*(int *)((p) + ((i) << 4) + 0x10))
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
#define AS_PTRU8(sym) (((PtrU8 *)&(sym))->v)
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
typedef struct ExitState {
  PortalDesc *portal; /* 0x00 */
  int centre[3];      /* 0x04 */
  int pad10;          /* 0x10 */
  int dest;           /* 0x14 */
  int preset;         /* 0x18 */
} ExitState;
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
#define ENTER_POSE4(a, n)                                                      \
  (a)->mode = (n);                                                            \
  if ((a)->nextAnim == (n)) {                                                     \
    continue;                                                                  \
  }                                                                            \
  ANIM_ADVANCE(a, n);                                                          \
  continue
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
extern int TweenActorAlongPath(Actor *actor, PathData *path, int *a, int *b, int c,
                         int d);
/* from level_34 */
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
/* from level_34 */
/* from level_34 */
/* from level_34 */
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
/* from level_34 */
/* from level_34 */
/* from level_34 */
#define CAM_LOOK (*(CamLook *)&g_anSpyroRequestVelocity)
/* from level_34 */
/* from level_34 */
/* from level_34 */
#define AS_PTRV(sym) (((PtrV *)&(sym))->v)
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
/* from level_34 */
#define FACE4(a, h, s, t)                                                      \
  ((int (*)(Actor *, int, int, int))TurnActorYawTowardAngle)(a, h, s, t)
/* from level_34 */
#define RESET_IDLE(a)                                                          \
  (a)->mode = 0;                                                              \
  ANIM_RESET(a, 0);                                                            \
  continue
/* from level_34 */
#define PATH_PT(p, i) ((int *)((p) + (((i) << 4) + 0x8)))
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
#define ANIM_WORD(a) (*(int *)&(a)->anim)
/* from level_34 */
#define SPY_FACING(a)                                                          \
  AbsAngleDelta8(spy->bodyRotZ, ArcTan2((a)->posX - spy->posX,            \
                                             (a)->posY - spy->posY, 0))
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
#define FLY_Z(p, i) (*(int *)((char *)(p) + ((i) << 4) + 0x10))
/* from level_34 */
/* from level_34 */
#define FP_X(p, i) (*(int *)((char *)(p) + ((i) << 4) + 0x8))
/* from level_34 */
#define FP_Y(p, i) (*(int *)((char *)(p) + ((i) << 4) + 0xC))
/* from level_34 */
#define FP_PT(p, i) ((int *)((char *)(p) + ((i) << 4) + 0x8))
/* from level_34 */
/* from level_34 */
/* from level_34 */
/* from level_34 */
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
/* from level_1 */
extern int g_nCameraActiveTargetPitch, g_nCameraActiveTargetDistance, g_nCameraActiveTargetRoll, g_nCameraActiveTargetPitchBias, g_nCameraActiveTargetYawBias;
/* from level_10 */
/* from level_25 */
/* from level_25 */
#define L25_UNLINK(st)                                                         \
  if ((st)->link >= 0) {                                                       \
    g_pActorListBase[(st)->link].subMode &= 0xFB;                                      \
  }
/* from level_25 */
#define L25_DROP(st)                                                           \
  *(st)->slot = 0;                                                             \
  L25_UNLINK(st)

/* ==== Level 21 classes ==== */
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

/* Type 57 state (boulder and its shards). */
typedef struct L21Rock {
  short vx;                /* 0x00 */
  short vy;                /* 0x02 */
  short vz;                /* 0x04 */
  short u06;               /* 0x06 */
  short u08;               /* 0x08 */
  short life;              /* 0x0A */
  unsigned char spin[3];   /* 0x0C */
  unsigned char heading;   /* 0x0F */
} L21Rock;

/* Type 100 state (boulder thrower). */
typedef struct L21State100 {
  int partner;     /* 0x00 index into g_pActorListBase, -1 none */
  int rider;       /* 0x04 index into g_pActorListBase */
  int u08;         /* 0x08 */
  int u0C;         /* 0x0C sound handle */
  int u10;         /* 0x10 */
  int u14;         /* 0x14 */
  int u18[3];      /* 0x18 */
  FlyPath *path;   /* 0x24 */
  int u28;         /* 0x28 flags */
  int u2C;         /* 0x2C timer */
  int u30;         /* 0x30 heading */
  int u34;         /* 0x34 speed */
  int u38;         /* 0x38 */
  int u3C;         /* 0x3C */
  Actor *rock;     /* 0x40 boulder being carried */
  int u44;         /* 0x44 sight range */
  int u48;         /* 0x48 throw range */
  int u4C[3];      /* 0x4C trigger box */
  int u58;         /* 0x58 */
  int u5C[2];      /* 0x5C */
  int u64[6];      /* 0x64 trigger box */
  int u7C;         /* 0x7C */
  int u80;         /* 0x80 */
  int u84;         /* 0x84 */
  int u88;         /* 0x88 */
  int *u8C;        /* 0x8C */
  int u90;         /* 0x90 */
  int u94;         /* 0x94 */
} L21State100;

/* Type 101 state (gatekeeper). */
typedef struct L21State101 {
  int partner;     /* 0x00 index into g_pActorListBase, -1 none */
  int rider;       /* 0x04 index into g_pActorListBase */
  int u08;         /* 0x08 */
  int u0C;         /* 0x0C sound handle */
  int u10;         /* 0x10 */
  int u14;         /* 0x14 */
  int mode;        /* 0x18 */
  int u1C;         /* 0x1C timer */
  int u20[3];      /* 0x20 */
  int u2C;         /* 0x2C heading */
  int u30;         /* 0x30 speed */
  int u34;         /* 0x34 */
  FlyPath *path;   /* 0x38 */
  int u3C[6];      /* 0x3C trigger box */
  int u54[6];      /* 0x54 trigger box */
  int u6C;         /* 0x6C */
  int idx;         /* 0x70 next key */
  int range[2];    /* 0x74 release range per key (units of 0x400) */
  int keys[2];     /* 0x7C key actor indices, -1 released */
  int count;       /* 0x84 */
  int *u88;        /* 0x88 */
  int u8C;         /* 0x8C */
  int u90;         /* 0x90 facing to test, -1 any */
} L21State101;

/* Type 293/294 state (path runner): runs its path ahead of Spyro, leads
 * him to a goal, and jumps or scatters when struck. */
typedef struct L21Runner {
  int speed;           /* 0x00 flee speed */
  unsigned char *path; /* 0x04 current path: [0] count, [1] cur, nodes at +8 */
  unsigned char *home; /* 0x08 home path */
  int knock;           /* 0x0C knock-back speed */
  int knockAng;        /* 0x10 knock-back heading */
  int lift;            /* 0x14 vertical speed */
  int target;          /* 0x18 index into g_pActorListBase, -1 none */
  int pathInit;        /* 0x1C */
  int camTimer;        /* 0x20 */
  unsigned int mode;   /* 0x24 0/1 = which goal flag it drives */
  int range;           /* 0x28 */
  int timer;           /* 0x2C */
  int init;            /* 0x30 */
  int volA[6];         /* 0x34 trigger box */
  int volB[6];         /* 0x4C trigger box */
  int camDone;         /* 0x64 */
} L21Runner;
#define RUN_FLAGS(p) (*(int *)((p) + ((p)[1] << 4) + 0x14))
#define ROCK(st) (*(Actor **)((char *)(st) + 0x40))

/* ==== Level 22 classes ==== */

/* Type 82 state: a thrown shot. */
typedef struct L22Shot {
  int vel[3];             /* 0x00 */
  short timer;            /* 0x0C */
  unsigned char heading;  /* 0x0E */
  unsigned char mode;     /* 0x0F 1 = spent, just times out */
} L22Shot;

/* Type 210 state: a bobbing platform. */
typedef struct L22Bob {
  int z;      /* 0x00 phase */
  int u04;    /* 0x04 */
  int period; /* 0x08 */
  int baseZ;  /* 0x0C */
} L22Bob;

/* Type 70 state: a lobbed bolt. */
typedef struct L22Bolt {
  int speed;     /* 0x00 */
  int vz;        /* 0x04 */
  int dvz;       /* 0x08 gravity */
  int u0C;       /* 0x0C */
  short timer;   /* 0x10 */
  short heading; /* 0x12 */
} L22Bolt;

/* Type 65 state: a pod that raises a ring of bobbing platforms (210) and
 * lobs shots (82) at its partner's target. */
typedef struct L22Pod {
  int timer;      /* 0x00 */
  int period;     /* 0x04 */
  int u08;        /* 0x08 */
  int u0C;        /* 0x0C */
  int *counter;   /* 0x10 shared live-pod count */
  int initCount;  /* 0x14 */
  int init;       /* 0x18 */
  int partner;    /* 0x1C index into g_pActorListBase */
  int shotTimer;  /* 0x20 */
  Actor *kids[4]; /* 0x24 */
} L22Pod;

/* Type 48 state: Metalhead. Three phases, each ending in a run along the
 * path to the next arena spot. */
typedef struct L22Boss {
  int phase;            /* 0x00 */
  unsigned char *path;  /* 0x04 */
  int timer;            /* 0x08 attack cooldown */
  int timer2;           /* 0x0C */
  int pathEnd[2];       /* 0x10 per-phase last path node */
  int range[2];         /* 0x18 per-phase stomp range (units of 0x400) */
  int choice;           /* 0x20 0 none, 1 stomp, 2 shots, 3 bolt */
  int shots;            /* 0x24 */
  int count;            /* 0x28 */
  int aim[3];           /* 0x2C */
  int hand[3];          /* 0x38 */
  int seen;             /* 0x44 */
  int init;             /* 0x48 */
  int camTimer;         /* 0x4C */
  int u50;              /* 0x50 */
} L22Boss;
/* An animation change that keeps the current animation (no 3C/3E promotion). */
#define ANIM_THEN(a, n)                                                        \
  if ((a)->nextAnim != (n)) {                                                     \
    g_nActorAnimDone = 0;                                                            \
    (a)->frameProgress = sixteen;                                                      \
    (a)->frameRate = sixteen;                                                      \
    (a)->nextAnim = (n);                                                          \
    (a)->nextFrame = 0;                                                            \
    StubActorHook(a);                                                          \
  }

/* ==== Level 28 classes ==== */

/* Type 47 state: a thrown rock, carried by its partner until released. */
typedef struct L28Rock {
  unsigned char timer;  /* 0x00 */
  unsigned char spinX;  /* 0x01 */
  unsigned char spinY;  /* 0x02 */
  unsigned char mode;   /* 0x03 1 = held, 0x63 = idle */
  short speed;          /* 0x04 */
  short u06;            /* 0x06 */
  short vz;             /* 0x08 */
  short timer2;         /* 0x0A */
  Actor *partner;       /* 0x0C the thrower */
} L28Rock;

/* Type 150 state: Jacques. Hops between the stops of his path ring and
 * hands rocks (47) to throw. */
typedef struct L28Boss {
  PathData *path;          /* 0x00 */
  int u04;                 /* 0x04 */
  int u08;                 /* 0x08 */
  unsigned char stops[7];  /* 0x0C path nodes he rests at */
  unsigned char pad13;
  int idx;                 /* 0x14 current stop */
  int init;                /* 0x18 */
  int u1C[3];              /* 0x1C */
  int u28[3];              /* 0x28 */
  int timer;               /* 0x34 */
  int u38;                 /* 0x38 */
  Actor *u3C;              /* 0x3C */
  int u40;                 /* 0x40 */
  Actor *child;            /* 0x44 rock in hand */
  int u48;                 /* 0x48 */
  int u4C;                 /* 0x4C */
  int u50;                 /* 0x50 */
  int u54;                 /* 0x54 */
} L28Boss;

/* Types 45/46 state: a charging ram (45 is the bigger one). */
typedef struct L28Ram {
  int init;     /* 0x00 */
  int mode;     /* 0x04 nonzero = starts awake */
  int knock;    /* 0x08 knock-back speed */
  int lift;     /* 0x0C */
  int u10;      /* 0x10 */
  int heading;  /* 0x14 */
  int u18;      /* 0x18 */
  int u1C;      /* 0x1C */
  int u20;      /* 0x20 */
  int home[3];  /* 0x24 */
  int timer3;   /* 0x30 */
  int timer2;   /* 0x34 */
  int charged;  /* 0x38 */
  int u3C;      /* 0x3C */
  int saved;    /* 0x40 state to return to */
  int heavy;    /* 0x44 */
  int timer;    /* 0x48 cooldown */
} L28Ram;

/* Type 126 state: a path walker that can carry a rider and sings while
 * it rests. */
typedef struct L28Walker {
  int init;            /* 0x00 */
  int mode;            /* 0x04 0..6; 3 = carried, 6 = absent */
  PathData *path;      /* 0x08 */
  int next;            /* 0x0C state to enter after the turn, 0x63 = done */
  int u10[3];          /* 0x10 */
  int u1C[5];          /* 0x1C */
  int sound;           /* 0x30 */
  int period;          /* 0x34 */
  int timer;           /* 0x38 */
  int u3C;             /* 0x3C */
  int startCur;        /* 0x40 */
  int partner;         /* 0x44 index into g_pActorListBase */
  int flag;            /* 0x48 */
  int u4C;             /* 0x4C */
  int rideMode;        /* 0x50 1 = held, 2 = thrown */
  int rider;           /* 0x54 index into g_pActorListBase */
  int speed;           /* 0x58 */
  int vz;              /* 0x5C */
  int dvz;             /* 0x60 */
  int call;            /* 0x64 */
} L28Walker;

/* ==== Level 24 classes ==== */

/* A trail point (as level 33's type 169). */
typedef struct L24Trail {
  int pos[3]; /* 0x00 */
  int u0C[3]; /* 0x0C */
  int age;    /* 0x18 */
} L24Trail;   /* 0x1C */

/* Type 104 state: a comet that leaves a fading trail and drops sparks. */
typedef struct L24Comet {
  short life;           /* 0x00 */
  short lifeOn;         /* 0x02 */
  L24Trail *trail;      /* 0x04 */
  short speed;          /* 0x08 */
  short vz;             /* 0x0A */
  short spark;          /* 0x0C spark timer */
  short dvz;            /* 0x0E */
  short timer;          /* 0x10 */
  unsigned char steps;  /* 0x12 extra steps this frame */
  unsigned char count;  /* 0x13 trail points */
} L24Comet;

/* Type 91 state: a cannon. Fires comets (104) at Spyro's targets or at the
 * actor its trigger names; each comet keeps its trail in one of three
 * buffers here. */
typedef struct L24Cannon {
  int steps;      /* 0x00 comet sub-steps */
  int link;       /* 0x04 index into g_pActorListBase (its trigger) */
  int target;     /* 0x08 index into g_pActorListBase */
  int u0C;        /* 0x0C */
  int timer;      /* 0x10 */
  int heading;    /* 0x14 */
  int camFlag;    /* 0x18 */
  int u1C;        /* 0x1C */
  int idx;        /* 0x20 trail buffer in use */
  int init;       /* 0x24 */
  char trails[1]; /* 0x28 */
} L24Cannon;

/* Types 92/130 state: a guard whose animations start at `base`. */
typedef struct L24Guard {
  int knock;           /* 0x00 knock-back speed */
  int heading;         /* 0x04 knock-back heading */
  int lift;            /* 0x08 */
  int base;            /* 0x0C first animation */
  int kind;            /* 0x10 */
  int timer;           /* 0x14 */
  int vol[6];          /* 0x18 trigger box */
  unsigned char *path; /* 0x30 */
  int flags;           /* 0x34 */
  int init;            /* 0x38 */
  int range;           /* 0x3C <0 = waiting */
  int partner;         /* 0x40 index into g_pActorListBase */
  int msg;             /* 0x44 written into the partner's state */
  int range2;          /* 0x48 */
  int timer2;          /* 0x4C */
  int reload;          /* 0x50 */
  int spin;            /* 0x54 */
} L24Guard;

/* Type 130 state: the bigger guard (as type 92's, with two followers). */
typedef struct L24Guard2 {
  int knock;           /* 0x00 */
  int heading;         /* 0x04 */
  int lift;            /* 0x08 */
  int base;            /* 0x0C */
  int kind;            /* 0x10 */
  int timer;           /* 0x14 */
  int vol[6];          /* 0x18 */
  unsigned char *path; /* 0x30 */
  int flags;           /* 0x34 */
  int init;            /* 0x38 */
  int range;           /* 0x3C */
  int partner;         /* 0x40 */
  int msg;             /* 0x44 */
  int range2;          /* 0x48 */
  int timer2;          /* 0x4C */
  int reload;          /* 0x50 */
  int follow1;         /* 0x54 index into g_pActorListBase, -1 none */
  int follow2;         /* 0x58 */
  int spin;            /* 0x5C */
  int u60;             /* 0x60 */
  int flip;            /* 0x64 */
  int sound;           /* 0x68 */
} L24Guard2;
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

/* ==== Level 27 classes ==== */

/* Type 135 state: a falling chunk. */
typedef struct L27Chunk {
  short vx;               /* 0x00 */
  short vy;               /* 0x02 */
  short vz;               /* 0x04 */
  unsigned char b06;      /* 0x06 */
  unsigned char noFloor;  /* 0x07 */
  short u08;              /* 0x08 */
  short age;              /* 0x0A */
  short grav;             /* 0x0C */
  short u0E;              /* 0x0E */
  unsigned char spin[3];  /* 0x10 */
} L27Chunk;

/* Type 136 state: a breakable wall made of four parts. */
typedef struct L27Wall {
  Actor *part[4]; /* 0x00 */
  int kind;       /* 0x10 1 = only a flame-charge breaks it */
  int init;       /* 0x14 */
  int u18;        /* 0x18 */
} L27Wall;

/* Type 134 state: a rock thrower. */
typedef struct L27Thrower {
  int heading;          /* 0x00 knock-back heading */
  int speed;            /* 0x04 */
  int lift;             /* 0x08 */
  int flags;            /* 0x0C bit 0 = watch, bit 1 = escort */
  int vol[6];           /* 0x10 trigger box */
  int timer;            /* 0x28 */
  int range;            /* 0x2C */
  int range2;           /* 0x30 */
  unsigned char *path;  /* 0x34 */
  int partner;          /* 0x38 index into g_pActorListBase */
  int u3C;              /* 0x3C */
  int link;             /* 0x40 index into g_pActorListBase, -1 none */
  int trig;             /* 0x44 */
} L27Thrower;

/* Type 203 state: a guard that walks a fence segment. */
typedef struct L27Fence {
  int u00;    /* 0x00 */
  int onPath; /* 0x04 1 = keep to the segment in path */
  int wake;   /* 0x08 */
  int *path;  /* 0x0C segment ends at [2],[3] and [6],[7] */
  int u10;    /* 0x10 */
  int timer;  /* 0x14 */
  int link;   /* 0x18 index into g_pActorListBase, -1 none */
  int u1C;    /* 0x1C */
  int timer0; /* 0x20 timer reload */
} L27Fence;

/* Type 215 state: one of the guard's six pieces. */
typedef struct L27Piece {
  int u00;   /* 0x00 */
  short lift; /* 0x04 */
} L27Piece;

/* A trigger box for IsPointInRotatedBox. */
typedef struct L27Box {
  int u00; /* 0x00 */
  int u04; /* 0x04 */
  int z;   /* 0x08 */
  int on;  /* 0x0C */
  int u10; /* 0x10 */
  int u14; /* 0x14 */
} L27Box;

/* Type 204 state: the fence-guard captain. Wakes the guards (203) whose
 * boxes Spyro has entered and sends them in one at a time. */
typedef struct L27Captain {
  unsigned char *path; /* 0x000 */
  int u04[7];          /* 0x004 */
  int link[7];         /* 0x020 the guards, index into g_pActorListBase, -1 none */
  int cur;             /* 0x03C */
  L27Box box[7];       /* 0x040 one per guard */
  int hit[7];          /* 0x0E8 */
  int heading;         /* 0x104 */
  int speed;           /* 0x108 */
  int u10C;            /* 0x10C */
  L27Box reset;        /* 0x110 */
  int timer;           /* 0x128 */
  int stage;           /* 0x12C */
  int puff;            /* 0x130 */
  L27Box all1;         /* 0x134 */
  L27Box all2;         /* 0x14C */
} L27Captain;

/* Type 237 state: a fireball. */
typedef struct L6Fireball {
  int speed;   /* 0x00 */
  int vz;      /* 0x04 */
  int grav;    /* 0x08 */
  short age;   /* 0x0C ticks alive, or the fade count once spent */
  short spent; /* 0x0E */
  int link;    /* 0x10 index into g_pActorListBase to wake on impact, -1 none */
  short delay; /* 0x14 ticks before it can hit Spyro */
} L6Fireball;

/* Type 225 state: a cannon. */
typedef struct L6Cannon {
  int link;    /* 0x00 index into g_pActorListBase (its trigger) */
  int u04;     /* 0x04 */
  int u08;     /* 0x08 */
  int hold;    /* 0x0C ticks until the trigger is released */
  int u10;     /* 0x10 */
  int heading; /* 0x14 */
  int timer;   /* 0x18 */
  int camFlag; /* 0x1C */
  int init;    /* 0x20 */
  int u24;     /* 0x24 */
} L6Cannon;

/* Types 214/216 state: a Peace Keeper soldier (kind picks the behaviour). */
typedef struct L6Keeper {
  char pad00[0x20];
  unsigned char *path; /* 0x20 */
  int kind;            /* 0x24 */
  int timer;           /* 0x28 */
  int timer2;          /* 0x2C */
  int home[4];         /* 0x30 */
  int speed;           /* 0x40 */
  int link;            /* 0x44 index into g_pActorListBase */
  int link2;           /* 0x48 index into g_pActorListBase, -1 none */
  int volley;          /* 0x4C */
  int phase;           /* 0x50 */
  int knockDir;        /* 0x54 */
  int knock;           /* 0x58 */
  int seen;            /* 0x5C */
  int bold;            /* 0x60 */
  int gone;            /* 0x64 */
  int vol[6];          /* 0x68 */
} L6Keeper;

/* ==== The function ==== */
#define spy (&SPY)
#define one 1
void Level6_UpdateActors(void) {
  Actor **actorList;
  Actor *actor;
  char *base;
  int dt;

  /* Scratch vectors. */
  int g056[3];
  int g072[3];
  int g088[3];
  int g104[3];
  int g120[3];
  int g136[3];
  int g152[3][3];
  int g192[3];
  int g208[3];
  int g224[3];
  int g240[3];
  int g256[3];
  int g272[3];
  int g288[3];
  int g304[3];
  int g320[2];
  int g328[3];
  int g344[3];
  int g360[3];
  int g376[3];
  int g392[3];
  int g408[3];
  int g424[3];
  int g440[3];
  int g456[3];
  int g472[3];

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
    if (actor->mode >= 0x80) {
      continue;
    }

    g_nActorAnimDone = actor->animFlags & 2;
    g_nActorAnimNewFrame = actor->animFlags & 1;
    dt = g_nFrameStep;
    g_nActorFrameDelta = dt;

    switch (actor->type) {
    /* Portal name text: shown while the camera faces the portal. */
    case 1: {
      int shouldSpawnText;
      int cameraAngleDiff;
      int distance = ApproxDist2D(&actor->posX, &CAM.posX);

      if (distance < (actor->mode ? 12 : 11) * 1024) {
        shouldSpawnText = actor->mode == 0;

        actor->mode = one;

        cameraAngleDiff = AbsAngleDelta8(
            actor->rotZ,
            ArcTan2(CAM.posX - actor->posX, CAM.posY - actor->posY, 0));

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
            (parent->mode >= 0x80 || (parent->dropKind & 0x80))) {
          Actor *scan;
          int self;

          parent->dropKind = actor->dropKind;
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

            if (st->dropToSpyro != 0) {
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
                SAMPLE_PITCH(g_pLevelSampleBankHeader[0x37]) +
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
                SAMPLE_PITCH(g_pLevelSampleBankHeader[0x37]);
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
              if (WrapAngleDelta8(spy->bodyRotZ,
                                ArcTan2(actor->posX - spy->posX,
                                              actor->posY - spy->posY, 0)) <
                  0) {
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

      if (actor->cullRadius & 0x80) {
        break;
      }

      switch (actor->mode) {
      case 0: {
        if (TickCountdownByFrameDelta(&st->idleTimer, 2) == 0 || actor->drawn) {
          int refHeight, rotated, moved;

          if (TickCountdownByFrameDelta(&st->headingTimer, 1) != 0) {
            int r = RandomSignedInRange(30, 90);
            if (GetRandomU32() & 1) {
              r = -r;
            }

            st->heading = WrapAddAngle8(st->heading, r);
            st->headingTimer = RandomIntInRange(60, 140);
          }

          if (TickCountdownByFrameDelta(&st->climbTimer, 1) != 0) {
            st->vz = RandomSignedInRange(10, 25);
            st->climbTimer = RandomIntInRange(80, 140);
          }

          if (st->vz >= 128) {
            actor->posZ = actor->posZ + (st->vz - 256);
          } else {
            actor->posZ = actor->posZ + st->vz;
          }

          refHeight = actor->posZ - ProbeActorGroundHeight(actor);

          if (refHeight < 300) {
            st->vz = RandomIntInRange(10, 25);
            st->climbTimer = RandomIntInRange(80, 140);
          }

          if (1024 < refHeight && refHeight < 2048) {
            st->vz = -RandomIntInRange(10, 25);
            st->climbTimer = RandomIntInRange(80, 140);
          }

          rotated = TurnActorYawTowardAngle(actor, st->heading, 4, 0xA, 1);
          moved = MoveActorAlongHeading(actor, 30, 0, 50, 1);

          if (rotated && moved) {
            st->heading = WrapAddAngle8(st->heading, RandomIntInRange(98, 158));
            st->headingTimer = RandomIntInRange(30, 80);
          }

          if (ApproxDist2D(&actor->posX, &st->tgtX) > 2000) {
            int angle = ArcTan2(st->tgtX - actor->posX,
                                      st->tgtY - actor->posY, 0);

            st->heading = angle;
            st->headingTimer = 40;
          }
        } else {
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      case 1: {
        SteerActorAroundPoint(actor, &spy->posX, 0x320,
                      WrapAddAngle8(ArcTan2(actor->posX - spy->posX,
                                                  actor->posY - spy->posY, 0),
                                    st->dir * 8),
                      8, 0x82, 0xE, 0x80, 0xFF, 0xFF, 0, 0, 0);

        if (g_pLevelOverlayInitRecord == 0 || g_pLevelOverlayInitRecord->subMode == 99) {
          actor->mode = 0;
        } else {
          int refHeight;

          if (TickCountdownByFrameDelta(&st->climbTimer, 1)) {
            st->vz = RandomSignedInRange(5, 20);
            st->climbTimer = RandomIntInRange(40, 80);
          }

          if (st->vz >= 128) {
            actor->posZ = (actor->posZ) + (st->vz - 256);
          } else {
            actor->posZ = actor->posZ + st->vz;
          }

          refHeight = actor->posZ - ProbeActorGroundHeight(actor);

          if (refHeight < 200) {
            st->vz = RandomIntInRange(5, 20);
            st->climbTimer = RandomIntInRange(40, 80);
          }

          if (1000 < refHeight && refHeight < 2048) {
            st->vz = -RandomIntInRange(5, 20);
            st->climbTimer = RandomIntInRange(40, 80);
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
      char *b33;

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
        if (actor->dropKind == 0xFF) {
          actor->dropKind = 0xF;
          SpawnActorDeathDrop(actor, 3, 0, 0);
          MarkActorCommittedKill(actor);
        }
        actor->mode = 10;
        continue;
      }

      if (actor->dropKind != 0xFF && st->egg == 0 && actor->mode != 10) {
        st->egg = g_pfnLevelSpawnActor(0x22, actor);
        st->egg->otBias = 0;
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
        actor->otBias = 2;
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
        actor->otBias = 0;
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
            st->egg->otBias = 4;
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
        if (st->path->nodes[st->path->cur].speed > 0) {
          top = st->path->nodes[st->path->cur].speed;
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
        if (st->path->nodes[st->path->cur].speed > 0) {
          st->speed = st->path->nodes[st->path->cur].speed;
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
        b33 = (char *)&g_nSpyroWorldPosZ;
        if (*(int *)b33 - st->path->nodes[10].z < -0x5DC &&
            ApproxDist2D(&actor->posX, (int *)(b33 - 8)) < 0x1800) {
          st->vz = 0x1A4;
          st->path->cur = 9;
          actor->mode = 0x2B;
          continue;
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

          CopyVector(g056, &SPY.posX);
          cb = LookupCosine(g_bSpyroPersistentEulerZ * 0x10);
          cs = LookupSine(g_bSpyroPersistentEulerZ * 0x10);
          x = cb * 0xB5;
          y = cs * 0xB5;
          g056[0] += (y + x) * 4 >> 12;
          g056[1] += (int)((LookupSine(g_bSpyroPersistentEulerZ * 0x10) * 0xB5) -
                          (LookupCosine(g_bSpyroPersistentEulerZ * 0x10) * 0xB5U)) *
                        4 >>
                    12;
          g056[2] += 0x300;
          VectorToSVectorShr2(st->apex, g056);
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
          SVectorToVectorShl2(g072, st->centre);
          SVectorToVectorShl2(g088, st->apex);
          SubtractVector(g088, g088, g072);
          ScaleVector3Sat(g088, g088, st->phase);
          RShiftVector3(g088, 6);
          AddVector(&actor->posX, g072, g088);
          actor->rotZ += g_nFrameStep * 4;
        } else {
          CopyVector(g104, &SPY.posX);
          g104[0] += LookupCosine(g_bSpyroPersistentEulerZ * 0x10) >> 3;
          g104[1] += LookupSine(g_bSpyroPersistentEulerZ * 0x10) >> 3;
          g104[2] += 0x300;
          VectorToSVectorShr2(st->centre, g104);
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
        SubtractVector(g120, &actor->posX, &CAM.posX);
        WorldToCameraRotate(g120, g120);
        g_pfnLevelEmitParticles(0x10, 0x4D, g120, 0);
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
        g104[0] = GetRandomU32() & 3;
        g104[1] = GetRandomU32() & 3;
        g104[2] = 0x14;
        g_pfnLevelEmitParticles(1, 1, &actor->posX, g104);
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
          CopyVector(g056, &actor->posX);
          g056[2] += 0x400;
          FindGroundHeightBelow(g056, 0x10000);
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
            UnpackWorldCollisionTri(st->node, g152[0]);
            AddVector(g152[0], g152[0], g152[1]);
            AddVector(g152[0], g152[0], g152[2]);
            actor->posX = g152[0][0] / 3;
            actor->posY = g152[0][1] / 3;
            actor->posZ = g152[0][2] / 3;
            RefreshActorDerivedState(actor, 2);
            SnapActorToGround(actor);
            EncodeCachedVecToActorDirCode(actor);
          } else if (st->mode == 3) {
            CopyVector(g136, &actor->posX);
            g136[2] += 0x400;
            RefreshActorDerivedState(actor, 2);
            if (FindGroundHeightBelow(g136, 0x1000) > 0) {
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

          ScaleVector3Sat(g192, (int *)st, g_nFrameStep);
          RShiftVector3(g192, 1);
          dist = VectorLength(g192, 1);

          if (dist >= 0xDD) {
            ScaleVector3ByRatio(g192, dist, 0xDC);
          }

          at = &actor->posX;
          AddVector(g192, at, g192);

          if (st->vel[2] >= -0xDB) {
            st->vel[2] -= g_nFrameStep * 5;
          }

          if (g192[2] < 0) {
            if (actor->type != 14 && actor->type != 15) {
              CollectPickupActor(actor);
            }
            DespawnActorRecord(actor);
            continue;
          }

          g192[2] += 0xF0;

          if (CollideSphereWithWorldAndActors(g192, 0xF0, 0xF0) != 0) {
            if (GetGroundTriggerRegionType() == 0) {
              if (actor->type != 14 && actor->type != 15) {
                CollectPickupActor(actor);
              }
              DespawnActorRecord(actor);
              continue;
            }

            CopyVector(g208, &g_anCollisionProbeVec);
            CopyVector(at, &g_anCollisionResolvedPos);
            actor->posZ -= 0xF0;

            if (st->bounce == 0) {
              int floor = FindGroundHeightBelow(g192, 0x400);
              int slope =
                  (signed char)ArcTan2_8bit(g208[2], VectorLength(g208, 0));

              if ((g192[2] - 0x190) < floor && slope < 0x18) {
                actor->subMode = 1;
                CopyVector(at, g192);
                actor->posZ = floor;
                if (actor->type != 15) {
                  st->lean = -ArcTan2_8bit(
                      IntegerSqrt((g208[0] * g208[0]) + (g208[2] * g208[2])),
                      g208[1]);
                  st->roll = -ArcTan2_8bit(g208[2], g208[0]);
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

            if (ReflectVectorOffNormal((int *)st, g208, (int *)st) != 0) {
              st->bounce -= 1;
              st->vel[0] = (st->vel[0] >> 3) + (GetRandomU32() & 0x3F) - 0x20;
              st->vel[1] = (st->vel[1] >> 3) + (GetRandomU32() & 0x3F) - 0x20;
              st->vel[2] = (st->vel[2] >> 2) + (GetRandomU32() & 0xF);
            }
          } else {
            g192[2] -= 0xF0;
            CopyVector(at, g192);
            FindGroundHeightBelow(g192, 0x10000);
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
            SubtractVector(g224, &spy->posX, (int *)st);
            RShiftVector3(g224, 5);

            if (VectorLength(g224, 1) >= 0x1E1) {
              CopyVector(&actor->posX, &spy->posX);
              st->timer = 0x20;
            } else {
              ScaleVector3Sat(g224, g224, st->timer);
              AddVector(&actor->posX, (int *)st, g224);
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
        actor->collision = 0;
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
          actor->collision = 0;

          if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x2000) {
            if (ABS2(actor->posZ - actor->floorOffset - SPY.posZ) < 2048) {
              if (st->flag24 == 0 || tgtB->mode == 1) {
                st->flag24 = 0;
                actor->posZ = st->baseZ + 0x80;
                actor->rotZ = ArcTan2(SPY.posX - actor->posX,
                                             SPY.posY - actor->posY, 0) -
                               128;
                actor->cullRadius = 0x20;
                actor->mode = 3;
                actor->scale = 0x60;

                g_pfnLevelEmitParticles(0x18, 0x50, &actor->posX, (void *)0xC);

                RefreshActorDerivedState(actor, 1);
                FindGroundHeightBelow(&actor->posX, 0x1000);
                EncodeCachedVecToActorDirCode(actor);
                actor->groundCode |= 0x80000000;
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

          SubtractVector(g136, st->tgt, &actor->posX);

          angleDiff = SignedAngleDelta8(st->angle, actor->rotZ);

          cosWeight1 = LookupCosine((st->t20 - 1) << 7);
          cosDelta = cosWeight1 - LookupCosine(st->t20 << 7);
          totalWeight = LookupCosine((st->t20 - 1) << 7) + 0x1000;

          actor->posX += (g136[0] * cosDelta) / totalWeight;
          actor->posY += ((g136[1] * cosDelta) / totalWeight);
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
          actor->cullRadius = 0;
          actor->drawn = 0;
          actor->mode = 2;
          st->timer = 16;

          g_pfnLevelEmitParticles(0x18, 0x50, &actor->posX, (void *)0xC);

          actor->groundCode &= 0x7FFFFFFF;
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
        g136[0] = 0;
        g136[1] = 0x64;
        g136[2] = 0;
        RotateVectorByMatrix(&actor->mtx[0], g136, g136);
        AddVector(g136, g136, &actor->posX);
        g_pfnLevelEmitParticles(2, 0x42, g136, 0);
        g136[0] = 0;
        g136[1] = -0x64;
        g136[2] = 0;
        ApplyActiveGteRotation(g136, g136);
        AddVector(g136, g136, &actor->posX);
        g_pfnLevelEmitParticles(2, 0x42, g136, 0);
      }

      if (g_nLevelReadyFlag[0] >= 3) {
        FxNode *fx = st->smoke;

        if (fx == 0) {
          fx = AllocWorldSparkle();
          st->smoke = fx;
          if (fx != 0) {
            fx->anchor = &actor->posX;
            st->smoke->size = 0x40;
            st->smoke->offX = 0;
            st->smoke->offY = 0;
            st->smoke->offZ = 0;
            st->smoke->r = 0xC0;
            st->smoke->g = 0xC0;
            st->smoke->b = 0x60;
            st->smoke->kind = 9;
            st->smoke->desc = &g_anSparkleGlowDesc;
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

            CopyVector(g056, &st->gnorc->posX);

            if (high == 0) {
              g056[0] -=
                  (unsigned short)g_anCosineLut[st->gnorc->rotZ] << 16 >> 20;
              g056[1] -=
                  (unsigned short)g_anSineLut[st->gnorc->rotZ] << 16 >> 20;
            } else {
              g056[0] -=
                  (unsigned short)g_anCosineLut[st->gnorc->rotZ] << 16 >> 22;
              g056[1] -=
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

            SubtractVector(g056, g056, &actor->posX);
            dist = VectorLength(g056, 1);

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
              ScaleVector3ByRatio(g056, dist, limit);
            } else {
              ScaleVector3ByRatio(g056, dist, reach - 5);
            }
            AddVector(&actor->posX, &actor->posX, g056);
            TurnActorYawTowardAngle(actor, ArcTan2(g056[0], g056[1], 0), 0xA, 0, 0);
            actor->rotY = ArcTan2(VectorLength(g056, 0), g056[2], 0);
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

            SubtractVector(g240, &st->gnorc->posX, &actor->posX);
            dist = VectorLength(g240, 1);

            if (dist < 0x200) {
              ge->timer = 0;
              ge->lean = GetRandomU32() & 0xE;
              ge->roll = GetRandomU32() & 0xE;
              ge->spin = GetRandomU32() & 0xE;
              CopyVector((int *)ge, &st->gnorc->posX);
              st->gnorc->subMode = 3;

              if (st->gnorc->type == 14) {
                g_pfnLevelEmitParticles(1, 0xC, actor, g_dwBurstColorType14);
              } else if (st->gnorc->type == 15) {
                g_pfnLevelEmitParticles(1, 0xC, actor, g_dwBurstColorType15);
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
                ScaleVector3ByRatio(g240, dist, lim);
              } else {
                ScaleVector3ByRatio(g240, dist, 0x122);
              }
              AddVector(&actor->posX, &actor->posX, g240);
              TurnActorYawTowardAngle(actor, ArcTan2(g240[0], g240[1], 0), 0xA, 0, 0);
              actor->rotY = ArcTan2(VectorLength(g240, 0), g240[2], 0);
              actor->rotY = StepAngle8Toward(actor->rotY & 0xFF, 0, 0x30);
            }
            break;
          }
          }
        } else {
          /* Nobody in the basket: drift on the wind. */
          actor->cullRadius = 0x10;

          if (st->timer <= 0) {
            g136[0] = -0x258;
            g136[1] = (GetRandomU32() & 0x310) - 0x188;
            g136[2] = g_nCameraCurrentMode == 3 ? (GetRandomU32() & 0x7F) + 0x64
                                     : (GetRandomU32() & 0x1FF) - 0xC8;
            do {
            } while (0);
            st->drift[0] = g136[0];
            st->drift[1] = g136[1];
            st->drift[2] = g136[2];
            st->timer = GetRandomU32() & 0x7B;
          } else {
            int *at;

            st->timer -= g_nFrameStep;
            do {
            } while (0);

            if (g_nCameraCurrentMode == 3) {
              g136[0] = st->drift[0] + (g_nSpyroMotionSpeed >> 2);
              g136[1] = st->drift[1];
              g136[2] = st->drift[2];
              if (g136[2] < 0x64) {
                g136[2] = 0x64;
              }
            } else {
              if (g_nCameraCurrentMode == 0x80000009) {
                g136[0] = st->drift[0] - 0x200;
              } else {
                g136[0] = st->drift[0];
              }
              g136[1] = st->drift[1];
              g136[2] = st->drift[2];
            }

            RotateVectorByMatrix(&SPY.u034, g136, g136);
            AddVector(g136, g136, &SPY.posX);
            at = &actor->posX;
            SubtractVector(g136, g136, at);
            RShiftVector3(g136, 2);
            AddVector(at, at, g136);

            if (CollideSphereWithWorldAndActors(at, 0x100, 0x100) != 0) {
              CopyVector(at, &g_anCollisionResolvedPos);
            }

            if (g136[2] >= 0x21) {
              g136[2] = 0x20;
            }
            if (g136[2] < -0x20) {
              g136[2] = -0x20;
            }

            actor->rotX = 0;
            actor->rotY = g136[2];
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

    /* Type 146: a breakable prop. A flame breaks it at once; a charge needs
     * a second hit within half a second. */
    case 146: {
      int *st = actor->state;

      if ((actor->flags & 0x10000) && actor->mode < 2) {
        actor->mode = 2;
        ANIM_RESET(actor, 2);
        continue;
      }
      if ((actor->flags & 0x80000) && actor->mode < 2) {
        if (st[0] != 0) {
          if (TickCountdownByFrameDelta(st, 4) != 0) {
            actor->mode = 2;
            ANIM_RESET(actor, 2);
            continue;
          }
        } else {
          st[0] = 0x1E;
        }
      }
      switch (actor->mode) {
      case 1:
        if (g_nActorAnimDone != 0) {
          actor->mode = 0;
          ANIM_RESET(actor, 0);
          continue;
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
      continue;
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
            actor->scale = 0x40;
            actor->cullRadius = 0;
            actor->updateDist = 0;
            actor->drawn = 0;
            actor->groundCode = 0;
            actor->visZone = 0xFF;
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

    /* Type 174: the cage that carries Spyro (CageState). Shakes when hit;
     * once its flyer is freed, Spyro steps in and it lifts him up and away,
     * bursting when the ride ends. */
    case 174: {
      CageState *st = actor->state;
      Actor *fly = &g_pActorListBase[st->idx];

      if (actor->flags & 0x80000) {
        actor->subMode = 2;
        st->shake = 0xD2;
      }

      if (actor->subMode < 2) {
        if (st->shake != 0) {
          st->shake += g_nFrameStep;
          if (st->shake < 0x40) {
            int dx;
            int dy;

            actor->rotX = g_aShakeRotOffsets[st->shake >> 1].a + st->rotX;
            actor->rotY = g_aShakeRotOffsets[st->shake >> 1].b + st->rotY;
            dx = (signed char)g_aShakeRotOffsets[st->shake >> 1].a;
            dy = (signed char)g_aShakeRotOffsets[st->shake >> 1].b;
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
        } else if (actor->flags & 0xB0000) {
          st->shake = 1;
          st->rotX = actor->rotX;
          st->rotY = actor->rotY;
          st->restZ = actor->posZ;
        }
      }

      switch (actor->subMode) {
      case 0:
        if (g_nLevelEggCount == 0 || g_nLevelEggCount == 3) {
          g_nLevelEggCount = 0;
          actor->updateDist = sixteen;
          fly->updateDist = 0x40;
          fly->cullRadius = 0x18;
        } else {
          g_nLevelEggCount = 1;
          actor->subMode = 1;
          fly->scale = 0x40;
          fly->groundCode = 0;
          fly->visZone = 0xFF;
          fly->subMode = 2;
        }
        break;

      case 1: {
        int height;
        int drop;

        if (g_nLevelEggCount == 1 &&
            (SPY.state == 0 || SPY.state == 1 || SPY.state == 0x15 ||
             SPY.state == 2) &&
            ApproxDist2D(&actor->posX, &spy->posX) < 0x600) {
          height = actor->posZ - actor->floorOffset;
          drop = height - SPY.posZ;
          if ((drop > 0 ? drop < 0x200 : (SPY.posZ - height) < 0x200) &&
              AbsAngleDelta8(spy->bodyRotZ,
                            ArcTan2(actor->posX - spy->posX,
                                          actor->posY - spy->posY, 0)) < 0x30 &&
              AbsAngleDelta8(actor->rotZ,
                            ArcTan2(spy->posX - actor->posX,
                                          spy->posY - actor->posY, 0)) < 0x30) {
            spy->controlFlags = 0x80002000;
            ResetSpyroLinearMotion();
            ZeroVector(&spy->headLook);
            actor->subMode = 2;
            if (st->shake > 0) {
              st->shake = 0;
              actor->rotX = st->rotX;
              actor->rotY = st->rotY;
              actor->posZ = st->restZ;
            }
            fly->rotX = 0;
            fly->rotY = 0;
            fly->rotZ = spy->bodyRotZ + 0x80;
          }
        }
        break;
      }

      case 2:
        if (g_dwPadPressed & 0x40) {
          st->shake = 0xD2;
        }
        g_dwSpyroRequestMask = 0x80002000;
        fly->cullRadius = 0x18;
        st->shake += g_nFrameStep;
        g_nGameplayBlocked = 1;

        if (st->shake < 0xC0) {
          g256[0] = 0x200;
          g256[1] = 0;
          g256[2] = 0x140;
          RotateVectorByMatrix(actor->mtx, g256, g256);
          AddVector(g256, g256, &actor->posX);
          SubtractVector(g256, g256, &spy->posX);
          g256[2] -= 0x100;
          RShiftVector3(g256, 5);
          ScaleVector3Sat(g256, g256, st->shake);
          DivideVector3(g256, g256, 6);
          AddVector(g256, g256, &spy->posX);
          g256[2] += 0x100;
          g256[2] += g_anSineLut[(st->shake << 7) / 192] >> 3;
          CopyVector(&fly->posX, g256);
          g_nCameraScriptStickRequestY[0] = g_anSineLut[(st->shake << 7) / 192] >> 3;

          if (st->shake < 0x40) {
            int rot;

            fly->rotY = -st->shake;
            rot = g_bSpyroPersistentEulerZ + 0x80;
            fly->rotZ = rot + st->shake * 4;
          } else if (st->shake < 0x80) {
            int rot;

            fly->rotY = 0xC0;
            rot = g_bSpyroPersistentEulerZ + 0x80;

            fly->rotZ = rot + (st->shake - 0x40) * 3;
          } else {
            int turn = AbsAngleDelta8(actor->rotZ,
                                     (unsigned char)(g_bSpyroPersistentEulerZ + 0x40)) *
                       (st->shake - 0x80);
            int rot = g_bSpyroPersistentEulerZ + 0x40;

            fly->rotZ = rot + turn / 64;
          }
        } else if (st->shake < 0xD0) {
          fly->rotZ = actor->rotZ;
          g272[0] = 0x200 - (st->shake - 0xC0) * 8;
          g272[1] = 0;
          g272[2] = 0x140;
          RotateVectorByMatrix(actor->mtx, g272, g272);
          AddVector(&fly->posX, g272, &actor->posX);
        }

        if (st->shake >= 0xD0 ||
            !(SPY.state == 0 || SPY.state == 1 || SPY.state == 0x15 ||
              SPY.state == 2 || SPY.state == 3)) {
          int i;

          fly->cullRadius = 0;
          fly->updateDist = 0;
          fly->drawn = 0;
          g_nGameplayBlocked = 0;
          actor->soundRange = 0x20;
          PlayActorMeshVoice(actor, 0, 0);

          for (i = 0; i < 4; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(0x136, actor);
            g_pfnLevelSpawnActor(0x137, actor);
          }

          for (i = 0; i < 10; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(0x135, actor);
          }

          g_pfnLevelEmitParticles(0x20, 0x46, &actor->posX, (void *)0x18);
          g_pThiefEggDrop = SpawnActorDeathDrop(actor, 1, 0, 0);
          MarkActorCommittedKill(actor);
          DespawnActorRecord(actor);
          g_nLevelEggCount = 2;
          g_nCameraScriptStickRequestX = 0;
          g_nCameraScriptStickRequestY[0] = 0;
          g_nSpyroSwingEulerPitchTarget = 0;
        }

        RefreshActorDerivedState(fly, 4);
        if (st->shake % 4 < 2) {
          g_pfnLevelEmitParticles(1, 0xC, (int *)fly, (void *)0x8080);
        } else {
          g_pfnLevelEmitParticles(1, 0xC, (int *)fly, (void *)0x5008080);
        }
        break;
      }

      st->handle = SelectActorLodBand(actor, st->handle);
      actor->flags = 0;
      continue;
    }

    case 188: {
      if (actor->mode == 0) {
        if (ApproxDist2D(&actor->posX, &SPY.posX) >= 0x1000) {
          actor->mode = 1;
        }
      } else if ((SPY.state == 0 || SPY.state == 1 || SPY.state == 0x15 ||
                  SPY.state == 2 || SPY.state == 0xB) &&
                 ApproxDist2D(&actor->posX, &spy->posX) < 0x780 &&
                 AbsAngleDelta8(SPY.bodyRotZ,
                               ArcTan2(actor->posX - spy->posX,
                                             actor->posY - g_nSpyroWorldPosY, 0)) <
                     0x20 &&
                 AbsAngleDelta8(actor->rotZ,
                               ArcTan2(spy->posX - actor->posX,
                                             g_nSpyroWorldPosY - actor->posY, 0)) <
                     0x28) {
        actor->mode = 0;
        SPY.controlFlags = 0x80002000;
        ResetSpyroLinearMotion();
        ZeroVector(&spy->headLook);
        g_pfnLevelBeginActorCutscene(actor);
        g_abWorldBalloonistState[1] = 2;

        if (g_nWorldTreasureCollected < 0x4B0) {
          if (g_abWorldBalloonistState[2] == 0) {
            g_nGamestate0cDialogPage = 6;
            g_abWorldBalloonistState[2] = 1;
          } else {
            g_nGamestate0cDialogPage = 8;
          }
        } else if (g_abWorldBalloonistState[2] < 2) {
          if (g_abWorldBalloonistState[2] == 0) {
            g_nGamestate0cDialogPage = 9;
          } else {
            g_nGamestate0cDialogPage = 0xA;
          }
          g_abWorldBalloonistState[2] = 2;
        } else {
          g_nGamestate0cDialogPage = 0x1E;
          g_nGamestate0cDialogVariant = (int)GetRandomU32() % 3;
        }
      }
      break;
    }

    /* Type 193: wandering critter; knocked away when hit. */
    case 193: {
      L26Critter *st = actor->state;

      if ((actor->flags & 0xB0000) && actor->mode != 1) {
        actor->flags = 0;
        st->knock = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                                actor->posY - spy->posY, 0),
                                  spy->bodyRotZ, 0x20, 0x40);
        {
          if (actor->flags & 0x10000) {
            st->speed = 0xC8;
          } else {
            st->speed = 0x190;
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

    /* Wooden chest (BreakState). */

    /* Wooden chest (BreakState). */
    case 194: {
      BreakState *st;
      int i;

      st = (BreakState *)actor->state;
      if (st->snapToGround != 0) {
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

    /* Wobbling chest (WobbleState): plays a recoil pose, then breaks. */
    case 195: {
      WobbleState *st = actor->state;

      if (st->snapToGround != 0) {
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

        actor->soundRange = 0x20;
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

    /* Types 214/216: Peace Keeper soldiers. Each `kind` patrols, guards or
     * escorts in its own way; all can be knocked down, and the gunners lob
     * fireballs (237) at Spyro or at a partner's target. */
    case 214:
    case 216: {
      L6Keeper *st = actor->state;

      if (actor->flags != 0 && actor->mode == 0xB &&
          g_pActorListBase[st->link].collision != 0) {
        actor->flags = 0;
      }
      if ((actor->flags & 0xB0000) && actor->mode != 3) {
        actor->flags = 0;
        if (actor->mode != 0xB || actor->cullRadius != 0) {
          st->knockDir = StepAndLerpAngle8(
              ArcTan2(actor->posX - g_anSpyroWorldPos, actor->posY - g_nSpyroWorldPosY,
                            0),
              g_bSpyroPersistentEulerZ, 0x20, 0x40);
          st->knock = 0xFA;
          if ((unsigned)(st->kind - 2) < 2 || st->kind == 8) {
            Actor *a = &g_pActorListBase[st->link];

            if (a->nextAnim != 0) {
              ANIM_ADVANCE(a, 0);
            }
          }
          if (actor->mode < 0xA && (st->kind == 4 || st->kind == 10)) {
            ReplaceActorGroupState(actor, 0xA, 0);
            ReplaceActorGroupState(actor, 0xA, 2);
            ReplaceActorGroupState(actor, 0xA, 4);
            ReplaceActorGroupState(actor, 0xA, 1);
            ReplaceActorGroupState(actor, 0xA, 0x64);
          }
          SpawnActorDeathDrop(actor, 3, 0, 0);
          MarkActorCommittedKill(actor);
          actor->mode = 3;
          ANIM_RESET(actor, 3);
          continue;
        }
      }
      if (st->gone != 0) {
        st->gone = 0;
        if (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < 0x2000) {
          if (actor->dropKind != 0xFF) {
            SpawnActorDeathDrop(actor, 1, 0, 0);
            MarkActorCommittedKill(actor);
          }
          DespawnActorRecord(actor);
          continue;
        }
      }
      CopyVector(g288, &actor->posX);
      g288[2] += 0x400;
      FindGroundHeightBelow(g288, 0x10000);
      EncodeCachedVecToActorDirCode(actor);

      switch (actor->mode) {
      case 0:
        switch (st->kind) {
        case 0:
          if (TickCountdownByFrameDelta(&st->timer, 4) == 0) {
            TrackEntityYawTowardSpyro(actor, 4, 0, 0);
          } else {
            ANIM_GO(actor, 1);
            if (PivotActorAlongPath(actor, st->path, 0x80, 0x46, 0, 4, 0x14, 0xFF,
                              5) != 0) {
              st->timer = RandomIntInRange(0x78, 0xB4);
              ANIM_GO(actor, 6);
            }
          }
          if (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < 0x2000) {
            actor->mode = 4;
            continue;
          }
          break;
        case 1:
        case 2:
          if (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < 0x2800) {
            SetActorGroupStateExcept(actor, 4, 3);
          }
          break;
        case 3:
          ANIM_GO(actor, 8);
          actor->mode = 9;
          continue;
        case 4:
          if (TickCountdownByFrameDelta(&st->timer, 4) == 0) {
            TrackEntityYawTowardSpyro(actor, 4, 0, 0);
          } else {
            ANIM_GO(actor, 1);
            if (PivotActorAlongPath(actor, st->path, 0x80, 0x46, 0, 4, 0x14, 0xFF,
                              5) != 0) {
              st->timer = RandomIntInRange(0x78, 0xB4);
              ANIM_GO(actor, 6);
            }
          }
          if (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < 0x2000) {
            ReplaceActorGroupState(actor, 4, 0);
          }
          break;
        case 5:
        case 6:
          ANIM_GO(actor, 1);
          SnapActorToGround(actor);
          UpdateActorWander(actor, st);
          if (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < 0x2000) {
            ReplaceActorGroupState(actor, 0xA, 0);
          }
          break;
        case 8:
          actor->mode = 8;
          continue;
        case 9:
          TrackEntityYawTowardSpyro(actor, 4, 0, 0);
          if (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < 0x1800) {
            ReplaceActorGroupState(actor, 1, 0);
          }
          break;
        case 10:
          if (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < 0x2000) {
            ReplaceActorGroupState(actor, 4, 0);
          }
          break;
        case 11:
          TrackEntityYawTowardSpyro(actor, 4, 0, 0);
          if (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < 0x44C) {
            ENTER_POSE(actor, 2);
          }
          break;
        }
        break;

      case 1:
        switch (st->kind) {
        case 0:
        case 1:
        case 9: {
          int d = ApproxDist2D(&actor->posX, &g_anSpyroWorldPos);

          ANIM_GO(actor, 1);
          if (TickCountdownByFrameDelta(&st->timer2, 4) != 0) {
            TrackEntityYawTowardSpyro(actor, 4, 0, 0);
            if (MoveActorAlongHeading(actor, 0x46, 0x12C, 0x12C, 0x15) != 0) {
              actor->mode = 100;
              continue;
            }
            if (d < 0x44C) {
              ENTER_POSE(actor, 2);
            }
            if (st->kind == 9) {
              if (ApproxDist2D(&actor->posX, st->home) > 0x1000) {
                actor->mode = 100;
                continue;
              }
            } else if (ApproxDist2D(&actor->posX, st->home) > 0x1800) {
              actor->mode = 100;
              continue;
            }
          }
          break;
        }
        case 2: {
          Actor *a = &g_pActorListBase[st->link];
          int h = ArcTan2(a->posX - actor->posX, a->posY - actor->posY, 0);
          int d = ApproxDist2D(&actor->posX, &a->posX);

          if (actor->subMode == 0) {
            actor->subMode = 1;
            st->speed = 0x32;
          }
          if (TurnActorYawTowardAngle(actor, h, 8, 0x28, 1) != 0) {
            if (st->speed < 0xAA) {
              st->speed += 0x1E;
            }
            MoveActorAlongHeading(actor, st->speed, 0, 0, 5);
            if (d < 0x5DC) {
              st->timer = 0x50;
              actor->mode = 8;
              continue;
            }
          }
          break;
        }
        case 4:
        case 10: {
          int d = ApproxDist2D(&actor->posX, &g_anSpyroWorldPos);

          if (TickCountdownByFrameDelta(&st->timer2, 4) != 0) {
            if (IsPointInRotatedBox(&actor->posX, st->vol) != 0) {
              actor->mode = 100;
              continue;
            }
            TrackEntityYawTowardSpyro(actor, 4, 0, 0);
            if (MoveActorAlongHeading(actor, 0x46, 0x12C, 0x12C, 5) != 0) {
              actor->mode = 100;
              continue;
            }
            if (d < 0x44C) {
              ENTER_POSE(actor, 2);
            }
            if (ApproxDist2D(&actor->posX, st->home) > 0x3000) {
              actor->mode = 100;
              continue;
            }
          }
          break;
        }
        }
        break;

      case 2:
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (g_nActorAnimDone != 0) {
          actor->mode = 100;
          continue;
        }
        g_anSpyroRequestVelocity.x = -(g_anSineLut[actor->rotZ] << 7) >> 12;
        g_nSpyroRequestVelocityY = (g_anCosineLut[actor->rotZ] << 7) >> 12;
        g_nSpyroRequestVelocityZ = 0;
        break;

      case 3:
        if (st->knock != 0) {
          MoveActorAlongAngle(actor, st->knockDir, st->knock, 0x12C, 0x12C, 5);
          st->knock -= 0x14;
          if (st->knock < 0) {
            st->knock = 0;
          }
        }
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x18);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case 4:
        if (st->kind == 2) {
          actor->frameRate = 0x30;
        }
        if (TrackEntityYawTowardSpyro(actor, 6, 0xF, 1) != 0) {
          if (st->kind == 10) {
            ANIM_GO(actor, 0xF);
          } else {
            ANIM_GO(actor, 4);
          }
        }
        if ((g_nActorAnimDone != 0 && actor->anim == 4) || actor->anim == 0xF) {
          ENTER_POSE(actor, 1);
        }
        break;

      case 8: {
        Actor *a = &g_pActorListBase[st->link];
        int h;
        int h2;
        int r;

        ApproxDist2D(&actor->posX, &g_anSpyroWorldPos);
        h = ArcTan2(a->posX - actor->posX, a->posY - actor->posY, 0);
        h2 = WrapAddAngle8(
            ArcTan2(g_anSpyroWorldPos - a->posX, g_nSpyroWorldPosY - a->posY, 0), 0x80);
        r = SteerActorAroundPoint(actor, &a->posX, 0x5DC, h2, 4, 0x32, 0xE, 0x80, 0xFF,
                          0xFF, 0, 0, 0xD);
        TurnActorYawTowardAngle(a, h, 4, 0, 0);
        if (r < 0xA) {
          ANIM_GO(actor, 0);
        } else {
          ANIM_GO(actor, 1);
        }
        if (*(unsigned short *)&actor->anim == 0x101 && g_nActorAnimNewFrame != 0 &&
            actor->frame == 0x10) {
          actor->frame = 5;
          actor->nextFrame = 6;
          actor->frameProgress = 0;
        }
        if (a->anim < 3) {
          if (r >= 0xB) {
            if (WrapAngleDelta8(h, h2) < 0) {
              if (a->nextAnim != 1) {
                ANIM_ADVANCE(a, 1);
              }
            } else if (a->nextAnim != 2) {
              ANIM_ADVANCE(a, 2);
            }
          } else if (a->nextAnim != 0) {
            ANIM_ADVANCE(a, 0);
          }
        }
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          int d = ApproxDist2D(&actor->posX, &g_anSpyroWorldPos);

          if (actor->drawn) {
            st->seen += g_nFrameStep;
          } else {
            st->seen = 0;
          }
          if (r < 4 && d > 0x1400 && d < 0x3C00 && st->seen > 0x50 &&
              CheckLineOfSightSegment(&actor->posX, &g_anSpyroWorldPos) != 0) {
            Actor *o = g_pfnLevelSpawnActor(0xED, actor);
            L6Fireball *fs = o->state;

            CopyVector(&o->posX, &a->posX);
            o->rotZ = a->rotZ;
            MoveActorAlongHeading(o, 0x5DC, 0, 0, 0);
            o->posZ += 0x4E2;
            o->rotZ = ArcTan2(spy->posX - o->posX,
                                     spy->posY - o->posY, 0);
            CopyVector(g304, &spy->posX);
            g304[2] -= 0x12C;
            fs->speed = 0xB4;
            fs->grav = -5;
            fs->vz = ComputeLobVelocityZ(&o->posX, g304, fs->speed, fs->grav, 0);
            fs->age = 0xB4;
            fs->spent = 0;
            fs->link = -1;
            fs->delay = 0;
            st->timer = 0xB4;
            if (st->link2 != -1) {
              Actor *b = &g_pActorListBase[st->link2];

              ((int *)b->state)[10] = 0x5A;
            }
            if (a->anim != 3) {
              ANIM_RESET(a, 3);
            }
          }
        }
        break;
      }

      case 9: {
        Actor *a = &g_pActorListBase[st->link];
        Actor *b = &g_pActorListBase[st->link2];

        TurnActorYawTowardAngle(actor,
                      ArcTan2(a->posX - actor->posX,
                                    a->posY - actor->posY, 0),
                      4, 0, 0);
        if (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < 0x2000) {
          if (b->mode < 0x80) {
            ((int *)b->state)[10] = 0xB4;
            if (b->nextAnim != 8) {
              ANIM_ADVANCE(b, 8);
            }
            b->mode = 8;
          }
          st->timer = 0xB4;
          ENTER_POSE(actor, 8);
        }
        if (st->volley != 0) {
          Actor *c = &g_pActorListBase[((int *)a->state)[1]];
          int *cs = c->state;

          if (TickCountdownByFrameDelta(&st->volley, 4) == 2) {
            int h = ArcTan2(c->posX - actor->posX, c->posY - actor->posY,
                                  0);
            Actor *o = g_pfnLevelSpawnActor(0xED, actor);
            L6Fireball *fs = o->state;

            CopyVector(&o->posX, &a->posX);
            o->rotZ = h;
            MoveActorAlongHeading(o, 0x546, 0, 0, 0);
            o->posZ += 0x546;
            CopyVector(g304, &c->posX);
            g304[2] += 0x52D;
            g304[0] += (g_anCosineLut[c->rotZ] * 0x433) >> 12;
            g304[1] += (g_anSineLut[c->rotZ] * 0x433) >> 12;
            fs->speed = 0xB4;
            fs->grav = -5;
            fs->vz = ComputeLobVelocityZ(&o->posX, g304, fs->speed, fs->grav, g320);
            fs->spent = 1;
            fs->age = g320[0] + 1;
            fs->link = -1;
            fs->delay = 0;
            o->cullRadius = 0x30;
            cs[3] = g320[0] + 1;
            if (a->anim != 3) {
              ANIM_RESET(a, 3);
            }
          }
        }
        break;
      }

      case 10: {
        Actor *a = &g_pActorListBase[st->link];

        if (st->phase == 0) {
          if (GetRandomU32() & 1) {
            st->bold = 1;
          }
          st->timer = RandomIntInRange(0, 0x28);
          st->phase = 1;
          break;
        }
        if (st->phase == 1) {
          int r = TrackEntityYawTowardSpyro(actor, 6, 0xA, 1);

          if (TickCountdownByFrameDelta(&st->timer, 4) != 0 && r != 0) {
            ANIM_GO(actor, 9);
          }
          if (g_nActorAnimDone != 0 && actor->anim == 9) {
            st->phase = 2;
            ANIM_SET(actor, 1);
          }
          break;
        }
        if (actor->type == 0xD8 && a->mode >= 0x80) {
          actor->subMode = 1;
          actor->mode = 0xB;
          ANIM_RESET(actor, 0xB);
          continue;
        }
        if (st->kind == 6 && actor->type == 0xD6) {
          st->kind = 2;
          actor->mode = 1;
          continue;
        }
        CopyVector(g304, &a->posX);
        g304[0] += (g_anCosineLut[a->rotZ] * 0x271) >> 10;
        g304[1] += (g_anSineLut[a->rotZ] * 0x271) >> 10;
        if (ApproxDist2D(&actor->posX, g304) < 0x200) {
          if (TurnActorYawTowardAngle(actor,
                            ArcTan2(a->posX - actor->posX,
                                          a->posY - actor->posY, 0),
                            6, 6, 1) != 0) {
            actor->subMode = 0;
            actor->mode = 0x10;
            ANIM_RESET(actor, 0x10);
            continue;
          }
        } else if (TurnActorYawTowardAngle(actor,
                                 ArcTan2(g304[0] - actor->posX,
                                               g304[1] - actor->posY, 0),
                                 6, 0x14, 1) != 0) {
          if (st->speed < 0x82) {
            st->speed += 0x14;
          }
          MoveActorAlongHeading(actor, st->speed, 0, 0, 4);
        }
        break;
      }

      case 11: {
        Actor *a = &g_pActorListBase[st->link];

        if (a->anim < 2) {
          actor->cullRadius = 0;
          break;
        }
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (actor->subMode == 0) {
          CopyVector(&actor->posX, &a->posX);
          actor->cullRadius = 0x20;
          actor->subMode = 1;
          actor->rotZ = ArcTan2(g_anSpyroWorldPos - actor->posX,
                                       g_nSpyroWorldPosY - actor->posY, 0);
          ANIM_SET(actor, 0xC);
        }
        if (actor->anim == 0x11) {
          if (AbsAngleDelta8(g_bSpyroPersistentEulerZ,
                            ArcTan2(actor->posX - g_anSpyroWorldPos,
                                          actor->posY - g_nSpyroWorldPosY, 0)) >=
                  0x17 ||
              st->bold != 0) {
            if (g_nActorAnimNewFrame != 0 && actor->nextFrame >= 0x32) {
              actor->frame = 0x24;
              actor->nextFrame = 0x25;
              actor->frameProgress = 0;
            }
          } else {
            if (actor->nextFrame >= 0x25 && actor->nextFrame < 0x32) {
              actor->frame = 0x32;
              actor->nextFrame = 0x33;
              actor->frameProgress = 0;
            }
            if (g_nActorAnimDone != 0) {
              ANIM_SET(actor, 0xC);
            }
          }
        } else if (g_nActorAnimDone != 0 &&
                   (AbsAngleDelta8(g_bSpyroPersistentEulerZ,
                                  ArcTan2(actor->posX - g_anSpyroWorldPos,
                                                actor->posY - g_nSpyroWorldPosY, 0)) >=
                        0x17 ||
                    st->bold != 0)) {
          ANIM_SET(actor, 0x11);
          actor->frame = 0x16;
          actor->nextFrame = 0x17;
          actor->frameProgress = 0;
        }
        break;
      }

      case 16: {
        Actor *a = &g_pActorListBase[st->link];

        if (actor->nextFrame == 0xB) {
          actor->cullRadius = 0;
          actor->mode = 0xB;
          ANIM_RESET(actor, 0xB);
          continue;
        }
        if (ApproxDist2D(&actor->posX, &a->posX) > 0x64) {
          MoveActorAlongAngle(actor,
                        ArcTan2(a->posX - actor->posX,
                                      a->posY - actor->posY, 0),
                        0x64, 0, 0, 0);
        } else {
          CopyVector(&actor->posX, &a->posX);
        }
        if (actor->frame >= 5 && a->mode == 0) {
          if (a->anim != 1) {
            ANIM_RESET(a, 1);
          }
          a->mode = 1;
        }
        break;
      }

      case 100:
        ANIM_GO(actor, 1);
        if (TurnActorYawTowardAngle(actor,
                          ArcTan2(st->home[0] - actor->posX,
                                        st->home[1] - actor->posY, 0),
                          4, 0x14, 1) != 0) {
          MoveActorAlongHeading(actor, 0x5A, 0, 0, 5);
        }
        if (ApproxDist2D(&actor->posX, st->home) < 0x80) {
          ENTER_POSE(actor, 0);
        }
        break;
      }
      RefreshActorDerivedState(actor, 1);
      if (actor->cullRadius == 0) {
        actor->collision = 0;
      }
      continue;
    }

    /* Type 225: a cannon. Watches for Spyro to stand in its sights and
     * blasts him, then lobs a fireball (237) at the nearest target. */
    case 225: {
      L6Cannon *st = actor->state;
      Actor *p = &g_pActorListBase[st->link];

      RefreshActorDerivedState(actor, 4);
      if (st->hold != 0 && --st->hold == 0) {
        ((int *)p->state)[19] = 0x3C;
        ANIM_GO(actor, 3);
      }
      if (actor->nextAnim >= 3 && actor->nextFrame >= 8) {
        ANIM_GO(actor, 0);
      }
      if (st->init == 0) {
        st->init = one;
        st->heading = actor->rotZ << 4;
      }
      if (st->camFlag != 0) {
        g_dwSpyroRequestMask = 0x80001000;
        g_pScriptedCameraActor = actor;
      }

      switch (actor->mode) {
      case 0:
        if (p->mode >= 0x80 || (p->nextAnim == 3 && p->nextFrame >= 0x15)) {
          int range = 0x4E2;
          int a = 0x20;
          int b = 0x5A;

          if (st->camFlag != 0) {
            range = 0x578;
            a = 0x2C;
            b = 0x4B;
          }
          if (actor->flags & 0x10000) {
            actor->flags = 0;
            g304[0] = -0x3E8;
            g304[1] = 0;
            g304[2] = 0x172;
            RefreshActorDerivedState(actor, 4);
            RotateVectorByMatrix(actor->mtx, g304, g304);
            AddVector(g304, g304, &actor->posX);
            if (AbsAngleDelta8(ArcTan2(g304[0] - g_anSpyroWorldPos,
                                            g304[1] - g_nSpyroWorldPosY, 0),
                              g_bSpyroPersistentEulerZ) < 0x30) {
              actor->mode = 1;
              continue;
            }
          }
          st->camFlag = 0;
          if (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < range) {
            int d1 = WrapAngleDelta8(actor->rotZ, *(unsigned char *)&g_bSpyroPersistentEulerZ);
            int ad = ABS(d1);

            if (0x40 - a < ad && ad < a + 0x40) {
              int d2 = WrapAngleDelta8(ArcTan2(g_anSpyroWorldPos - actor->posX,
                                                   g_nSpyroWorldPosY - actor->posY, 0),
                                     actor->rotZ);

              if (b < ABS(d2) &&
                  ((d1 < 0 && d2 < 0) || (d1 > 0 && d2 > 0))) {
                st->camFlag = one;
              }
            }
          }
        } else {
          actor->flags = 0;
          st->heading = actor->rotZ << 4;
        }
        break;

      case 1:
        st->timer = 0x3C;
        PlayActorMeshVoice(actor, 0, (int)&st->u24);
        actor->mode = 2;
        continue;

      case 2:
        if (st->timer >= 0xB) {
          int i;

          actor->flags = 0;
          RefreshActorDerivedState(actor, 4);
          GetActorAttachPoint(actor, 3, g328);
          g344[0] = -0x10;
          g344[1] = 0;
          g344[2] = -0xE;
          RotateVectorByMatrix(actor->mtx, g344, g344);
          i = 0;
          do {
            i++;
            g344[0] += (GetRandomU32() & 0xF) - 8;
            g344[1] += (GetRandomU32() & 0xF) - 8;
            g_pfnLevelEmitParticles(1, 0x4A, g328, (void *)g344);
          } while (i < 5);
        }
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          StopSoundVoicesByOwner(actor, 1);
          actor->mode = 3;
          continue;
        }
        break;

      case 3: {
        Actor *o;
        L6Fireball *cs;
        Actor *best;
        Actor *q;
        int bestScore;
        int ang;
        int d;
        int h;

        do {
          o = g_pfnLevelSpawnActor(0xED, actor);
          cs = o->state;
          CopyVector(&o->posX, &actor->posX);
          o->rotZ = actor->rotZ;
          MoveActorAlongHeading(o, 0x546, 0, 0, 0);
        } while (0);
        best = 0;
        h = 0xF0;
        o->posZ += 0x47E;
        cs->grav = -5;
        cs->link = -1;
        cs->speed = h;
        cs->vz = h;
        cs->age = 0;
        cs->spent = 0;
        cs->delay = 0;
        o->cullRadius = 0x34;
        o->updateDist = 0xFF;
        bestScore = 0x7530;
        actor->flags = 0;
        for (q = g_pActorListBase; q < g_pActorPoolDynBase; q++) {
          if (q == actor || q->mode >= 0x80 || q->type == 0xFA ||
              q->type == 0x6E || q->type == 0xE1) {
            continue;
          }
          if ((q->collision == 0 || q->collision >= 0) && q->type != 0x1A1) {
            continue;
          }
          SubtractVector(g360, &q->posX, &actor->posX);
          d = ABS(g360[0]) + ABS(g360[1]);
          if (d > 0xBFFF) {
            continue;
          }
          ang = (ArcTan2(q->posX - actor->posX, q->posY - actor->posY,
                               0) -
                 actor->rotZ) &
                0xFF;
          if (ang > 0x80) {
            ang -= 0x100;
          }
          ang = ABS(ang);
          if (ang >= 7) {
            continue;
          }
          d = VectorLength(g360, 0);
          if (d >= 0x6800) {
            continue;
          }
          if (d > 0x4800) {
            d -= 0x4800;
          } else if (d < 0x3800) {
            d = 0x3800 - d;
          } else {
            d = 0;
          }
          d += ang << 12;
          if (q->type == 0x1A1) {
            d >>= 2;
          }
          if (d < bestScore) {
            best = q;
            bestScore = d;
          }
        }
        if (best != 0) {
          o->rotZ = ArcTan2(best->posX - actor->posX,
                                   best->posY - actor->posY, 0);
          cs->grav = -5;
          SubtractVector(g360, &best->posX, &o->posX);
          ang = VectorLength(g360, 0);
          h = ang - g360[2];
          if (h > 0x400) {
            bestScore = ((ang * 5 >> 8) * ang / h) >> 1;
            if ((unsigned)(bestScore - 0x11) <= 0xFFFEE) {
              bestScore = IntegerSqrt(bestScore) << 4;
              bestScore = (bestScore * 0x108) >> 8;
              cs->vz = bestScore;
              cs->speed = bestScore;
              cs->delay = ang / bestScore - 4;
              if (best->type == 0x1A1) {
                cs->link = best - g_pActorListBase;
              }
            }
          }
        }
        ANIM_GO(actor, 3);
        actor->mode = 0;
        continue;
      }
      }
      RefreshActorDerivedState(actor, 1);
      continue;
    }

    /* Type 237: a fireball. Flies on its heading under gravity, bursts on
     * walls or on Spyro, and wakes its linked actor when it hits. */
    case 237: {
      L6Fireball *st = actor->state;
      int i;

      actor->rotY = WrapAddAngle8(actor->rotY, 3);
      switch (actor->mode) {
      case 0:
        actor->posX += (st->speed * g_anCosineLut[actor->rotZ]) >> 12;
        actor->posY += (st->speed * g_anSineLut[actor->rotZ]) >> 12;
        if (st->spent != 0) {
          if (st->age == 0) {
            DespawnActorRecord(actor);
            continue;
          }
          st->age--;
        } else {
          st->age += g_nFrameStep;
          if (st->age >= 0x169) {
            DespawnActorRecord(actor);
            continue;
          }
          if (st->delay != 0) {
            st->delay--;
          }
          if (CollideSphereWithWorldAndActors(&actor->posX, 0x168, 0x168) != 0) {
            PlayActorMeshVoice(actor, 0, 0);
            actor->mode = 1;
            if (st->link != -1) {
              g_pActorListBase[st->link].subMode = 1;
            }
          }
          if (st->age >= 9 && st->delay == 0 &&
              DispatchActorContactAtSphere(&actor->posX, 0x400, 0, 0x80000, actor, 2) != 0) {
            PlayActorMeshVoice(actor, 0, 0);
            actor->mode = 1;
            if (st->link != -1) {
              g_pActorListBase[st->link].subMode = 1;
            }
          }
        }
        actor->posZ += st->vz;
        st->vz += st->grav;
        g304[0] = 0;
        g304[1] = 0;
        g304[2] = 0;
        g_pfnLevelEmitParticles(1, 1, &actor->posX, g304);
        break;

      case 1:
        for (i = 0; i < 6; i++) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0x1CA, actor);
          g_pfnLevelSpawnActor(0x1CB, actor);
        }
        for (i = 0; i < 8; i++) {
          g328[0] = (GetRandomU32() & 0x3E) - 0x1F;
          g328[1] = (GetRandomU32() & 0x3E) - 0x1F;
          g328[2] = GetRandomU32() & 0xF;
          CopyVector(g376, g328);
          LShiftVector3(g376, 2);
          AddVector(g376, g376, &actor->posX);
          g_pfnLevelEmitParticles(1, 0xD, g376, g328);
        }
        g_pfnLevelEmitParticles(0xA, 0x46, &actor->posX, (void *)0x10);
        DispatchActorContactAtSphere(&actor->posX, 0x800, 0, 0x80000, actor, 2);
        DespawnActorRecord(actor);
        continue;
      }
      RefreshActorDerivedState(actor, 2);
      continue;
    }

    /* Crystal dragon statue (DragonStatueState): shakes, then counts the rescue when Spyro is close. */
    case 250: {
      DragonStatueState *st = actor->state;
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
          SubtractVector(g304, at, &spy->posX);
          g304[2] = (g304[2] * 3) >> 2;

          if (VectorLength(g304, 1) < 0x440) {
            int facing;

            facing =
                st->partner != -1 ? g_pActorListBase[st->partner].rotZ : st->facing;
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

            if (st->cutsceneId == st->reward) {
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
              g_nSpyroRequestVelocityY = g_nSpyroMotionY >> 8;
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

      if (!(actor->cullRadius & 0x80) && (actor->cullRadius != 0)) {

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

      actor->soundRange = st->volume;

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

        g304[0] = (GetRandomU32() & 0xFE) - 0x7F;
        g304[1] = (GetRandomU32() & 0xFE) - 0x7F;
        g304[2] = (GetRandomU32() & 0xFE) - 0x40;
        AddVector(g304, g304, &actor->posX);
        g_pfnLevelEmitParticles(1, 0x42, g304, (int *)1);
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
            g328[0] = (GetRandomU32() & 0xFE) - 0x7F;
            g328[1] = (GetRandomU32() & 0xFE) - 0x7F;
            g328[2] = (GetRandomU32() & 0xFE) - 0x40;
            AddVector(g328, g328, &actor->posX);
            g_pfnLevelEmitParticles(1, 0x42, g328, (int *)1);
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
            scan->killGroup &= 0x7F;
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

      actor->soundRange = 0x20;
      switch (actor->mode) {
      case 0:
        if (actor->subMode == 0) {
          *(int *)&actor->tint0 = g_anTintByDropKind[actor->dropKind];
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
          st->prize->cullRadius = 0;
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

        st->prize->cullRadius = 0x18;
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
          ZeroVector(g392);
          g_pfnLevelEmitParticles(1, 0, &st->prize->posX, g392);
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
            CopyVector(g408, &st->prize->posX);
            g408[0] += g_anCosineLut[j * 43] >> 6;
            g408[1] += g_anSineLut[j * 43] >> 6;
            g408[2] += 0x28;
            g424[0] = g_anCosineLut[j * 43] >> 7;
            g424[1] = g_anSineLut[j * 43] >> 7;
            g424[2] = 0x10;
            g_pfnLevelEmitParticles(1, 0, g408, g424);
          }
          DespawnActorRecord(st->prize);
          actor->killGroup &= 0x7F;
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
    case 332:      switch (actor->mode) {
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

    /* Types 363/364: a one-shot effect that is removed when its animation
     * ends. */
    case 363:
    case 364:
    case 405:
    case 477:      if (g_nActorAnimDone != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      break;

    /* Exit vortex (PortalState). */
    case 398: {
      PortalState *st = actor->state;
      PortalDesc *p;
      int mag;
      short progressed;
      switch (actor->mode) {
      case 1: {

        SPY.controlFlags =
            (0x80000000 | 0x2000 | 0x4000 | 0x100 | 0x40 | 0x4 | 0x2 | 0x1);
        SPY.u248 = actor->subMode;
        SPY.pPortal = st->desc;
        SPY.u244 = 0x60;
        SPY.u218 = 5;
        SPY.u027 = 0x7F;

        p = st->desc;
        AddVector(g392, p->posA, p->posB);
        RShiftVector3(g392, 1);

        SubtractVector(&SPY.u208, g392, &SPY.posX);
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
          CAM.nextMode = (0x80000000 | 0x10 | 0x2);
          SPY.u218 = 0xF;
          g_nCdStreamState = 0;
          g_nLevelTransitionTallyActive = 1;
          g_nLevelTransitionPhase = 0;
          g_nGamestate = 1;
          g_nGamePaused = 1;
          SPY.u206 = st->dest;
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
          if (st->cameraPreset >= 0) {
            CAM.preset = &g_aCameraXformPresets[st->cameraPreset];
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
          CAM.nextMode = (0x80000000 | 0x10 | 0x2);
          g_nCdStreamState = 0;
          g_nLevelTransitionTallyActive = 1;
          g_nLevelTransitionPhase = 0;
          g_nGamestate = 1;
          g_nGamePaused = 1;
          SPY.u206 = st->dest;
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
          if (st->cameraPreset >= 0) {
            CAM.preset = &g_aCameraXformPresets[st->cameraPreset];
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

    /* Type 401: breakable rock. Shakes when struck; a charge shatters it
     * into rubble and dust and removes it. */
    case 401: {
      L31Rock *st = actor->state;

      if (st->shake != 0) {
        st->shake += g_nFrameStep;
        if (st->shake < 0x40) {
          int dx;
          int dy;

          actor->rotX = g_aShakeRotOffsets[st->shake >> 1].a + st->rotX;
          actor->rotY = g_aShakeRotOffsets[st->shake >> 1].b + st->rotY;
          dx = (signed char)g_aShakeRotOffsets[st->shake >> 1].a;
          dy = (signed char)g_aShakeRotOffsets[st->shake >> 1].b;
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

        actor->soundRange = 0x20;
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

    /* Type 417: a reward burst. Once triggered it plays its sound, throws up
     * to four sets of sparks (while the spawn budget allows) and goes. */
    case 417: {
      int *st = actor->state;
      int i;

      if (st[0] == 1 && actor->dropKind == 0xFF) {
        ModifyWorldChunkAnimFlags(0, 0xFC, 0);
        DespawnActorRecord(actor);
        continue;
      }
      if (actor->subMode != 1) {
        continue;
      }
      if (st[0] == 0) {
        DespawnActorRecord(actor);
        continue;
      }
      if (st[0] != 1) {
        continue;
      }
      PlaySoundEffect(g_pLevelSampleBankHeader[0x1C], actor, 0x10, 0);
      CommitActorKillGems(0, actor);
      for (i = 0; i < 4; i++) {
        if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
          break;
        }
        g_pfnLevelSpawnActor(0x1C6, actor);
        g_pfnLevelSpawnActor(0x1C7, actor);
        g_pfnLevelSpawnActor(0x1C8, actor);
      }
      g_pfnLevelEmitParticles(8, 0x17, &actor->posX, 0);
      ModifyWorldChunkAnimFlags(0, 0xFC, 0);
      DespawnActorRecord(actor);
      continue;
    }

    /* Life chest (LiftState). */
    case 421: {
      LiftState *st = actor->state;
      Actor *rider;
      int i;
      int a;
      int b;

      if (st->rideMode == 1) {
        SnapActorToGround(actor);
        EncodeCachedVecToActorDirCode(actor);
      }

      if (actor->mode == 0) {
        st->phase += g_nFrameStep;

        if (st->phase >= 0x1E1) {
          st->phase = 0;
          actor->rotX = st->savedRotX;
          actor->rotY = st->savedRotY;
          actor->posZ = st->savedPosZ;
          actor->mode = 1;
          actor->frameProgress = 8;
          actor->frameRate = 8;
          actor->nextAnim = 1;

          st->passenger = g_pfnLevelSpawnActor(0x1A6, actor);
          st->passenger->rotX = actor->rotX;
          st->passenger->rotY = actor->rotY;
          st->passenger->rotZ = actor->rotZ;
          st->passenger->frameRate = 0x10;
          st->passenger->updateDist = 0;
          st->passenger->cullRadius = 0;
          st->passenger->drawn = 0;
          st->passenger->otBias = 5;

          PlayActorMeshVoice(actor, 0, 0);
        } else if (st->phase >= 0x1A0) {
          actor->rotX = g_aShakeRotOffsets[LIFT_STEP(st)].a + st->savedRotX;
          actor->rotY = g_aShakeRotOffsets[LIFT_STEP(st)].b + st->savedRotY;
          a = (signed char)g_aShakeRotOffsets[LIFT_STEP(st)].a;
          b = (signed char)g_aShakeRotOffsets[LIFT_STEP(st)].b;
          if (a < 0) {
            a = -a;
          }
          if (b < 0) {
            b = -b;
          }
          actor->posZ = st->savedPosZ + (a + b) * 6;

          if (actor->sfxSlot == 0x7F) {
            PlayActorMeshVoice(actor, 1, 0);
          }
        } else if (st->phase >= 0x19C) {
          /* Just before the shake: remember the rest pose. */
          st->savedRotX = actor->rotX;
          st->savedRotY = actor->rotY;
          st->savedPosZ = actor->posZ;
          if (st->rideMode == 1) {
            st->phase = 0;
          }
        }
      } else {
        if (actor->anim == 1) {
          st->phase += g_nFrameStep;
          st->passenger->cullRadius = 0x10;
          st->passenger->updateDist = 0xFF;
        }

        if (st->phase >= 0xE6) {
          DespawnActorRecord(st->passenger);
          st->phase = GetRandomU32() & 0xFF;
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
      if (st->rideMode == 1) {
        ((unsigned char *)rider->state)[0xE] = st->rideMode;
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
        g408[0] = ((short)g_anCosineLut[i * 32] >> 8) * 3;
        g408[1] = ((short)g_anSineLut[i * 32] >> 8) * 3;
        g408[2] = 0x18;
        g_pfnLevelEmitParticles(1, 0, &actor->posX, g408);
      }

      g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (void *)0x18);

      if (actor->mode == 1) {
        DespawnActorRecord(st->passenger);
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
          g408[0] = (g_anCosineLut[bearing] * 3) >> 4;
          g408[1] = (g_anSineLut[bearing] * 3) >> 4;
          g408[2] = 0;
          g440[0] = (unsigned short)g_anCosineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          g440[1] = (unsigned short)g_anSineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          g440[2] = 0;
          bearing = (bearing + 0x80) & 0xFF;
          actor->rotZ = bearing + ((g_anCosineLut[actor->subMode] * 3) >> 9);
          ScaleVector3Sat(g456, g440, st->radius - 1);
          LShiftVector3(g440, 1);
          arm = st->radius - 1;
          g456[2] += (g_anCosineLut[arm * 2] * 3) >> 3;
          g472[0] = g408[0] * g_anCosineLut[arm * 2];
          g472[1] = g408[1] * g_anCosineLut[arm * 2];
          g472[2] = 0;
          bearing = arm << 1;
          bearing = (bearing - (st->phase * 4)) & 0xFF;
          ScaleVector3Sat(g440, g440, st->phase);
          SubtractVector(g456, g456, g440);
          swing = &g_anCosineLut[bearing];
          actor->posX = g408[0] * *swing;
          actor->posY = g408[1] * *swing;
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, g472);
          RShiftVector3(&actor->posX, 0xA);
          AddVector(&actor->posX, &actor->posX, g408);
          AddVector(&actor->posX, &actor->posX, &st->parent->posX);
          SubtractVector(&actor->posX, &actor->posX, g456);
          actor->posZ = actor->posZ + ((*swing * 3) >> 3) + 0x600;
        }
      } else {
        unsigned char now = actor->subMode + 8;

        actor->subMode = now;
        actor->rotZ = actor->floorOffset + ((g_anCosineLut[now] * 3) >> 9);
      }

      break;
    }

    /* Types 454-456: falling sparks. Fall (faster than 458's) until they
     * land, time out or leave the visible set. */
    case 454:
    case 455:
    case 456: {
      SparkState *st = actor->state;

      if (TickCountdownByFrameDelta(&st->life, 4) == 0 && actor->posZ >= 0x7D0 &&
          actor->drawn) {
        actor->posX += st->vel[0];
        actor->posY += st->vel[1];
        st->vel[2] -= 0x10;
        if (st->vel[2] < -0x140) {
          st->vel[2] = -0x140;
        }
        actor->posZ += st->vel[2];
        actor->rotX += st->spin0;
        actor->rotY += st->spin1;
        actor->rotZ += st->spin2;
        st->life--;
        continue;
      }
      DespawnActorRecord(actor);
      continue;
    }

    /* Types 458/459: tumbling debris. Bounces off walls and floors,
     * falling and spinning until its life runs out. */
    case 458:
    case 459: {
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
        if (st->vel[2] < -128) {
          st->vel[2] = -128;
        }
        actor->posZ += st->vel[2];
        actor->rotX += st->spin0;
        actor->rotY += st->spin1;
        actor->rotZ += st->spin2;
        st->life--;
        continue;
      }
      DespawnActorRecord(actor);
      continue;
    }
}
  }
}
