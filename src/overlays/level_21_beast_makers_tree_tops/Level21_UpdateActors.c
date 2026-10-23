/* Level21_UpdateActors -- per-frame actor update for level 21 (Tree
 * Tops). 0x8007B698, 46,240 bytes.
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class. Most
 * classes are shared with levels 7/10/12/13/16/26/33; the level-21 classes
 * are the boulder and its shards (57), the boulder thrower (100/102), the
 * gatekeeper (101) and the path runner (293/294) that leads Spyro to a goal
 * and raises a per-goal flag. The roaming ram (453) is level 18/20's 466
 * without the voice channel.
 *
 * Load-bearing source forms:
 *   - Level 7's frame base (actor in s4, spy = base - 8 in fp). The scratch
 *     arrays are declared in the original's slot order and named by sp offset
 *     (f056..f592); arms that share a slot share the name.
 *   - Case 100: `st->rock = 0` before the heading store, vz stored after vy,
 *     the camera z through CAM_LOOK, and the state-2 exit test as one &&
 *     chain (the branchy ?: range test only survives jump.c there).
 *   - Case 293: the goal-arrival `last` index as a ternary (cse must not keep
 *     st->mode across it), the spark loop's angle as an int `& 0xFF`, and the
 *     mode-1 arrival test written flag-clear-first.
 *   - Case 57: the floor delta through two temps (fold reassociates the
 *     one-expression form into `(h + 0x50) - z`).
 */

#include "actor.h"
#include "level_actors.h"
#include "vecmath.h"
#include "level_globals.h"
#include "level_update_actors.h"

/* Progress-per-tick byte of animation `anim` in the actor's model. */
#define TYPE_DESC(a, id)                                                     \
  (g_apActorMeshTable[(a)->type]->m_Animations[(id)]->m_ProgressPerTick)

#define AS_I32(sym) (((I32 *)&(sym))->v)

/* Type 421: life chest. Shakes and pops its lid on a timer, spawning a
 * child actor; breaks into fragments when damaged. */

#define LIFT_STEP(st) (((st)->phase - 0x1A0) >> 1)

/* 0x18-byte spherical camera coordinates; g_aCameraXformPresets holds the presets. */

#define CAM g_anWorldToCameraRotMtx

extern int GetGroundTriggerRegionType(void);

/* Glow node owned by Sparx; follows its anchor position. */

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

#define PATH_X(p, i) (*(int *)((p) + ((i) << 4) + 0x8))
#define PATH_Y(p, i) (*(int *)((p) + ((i) << 4) + 0xC))
#define PATH_Z(p, i) (*(int *)((p) + ((i) << 4) + 0x10))
#define AS_PTRU8(sym) (((PtrU8 *)&(sym))->v)
typedef struct ExitState {
  PortalDesc *portal; /* 0x00 */
  int centre[3];      /* 0x04 */
  int pad10;          /* 0x10 */
  int dest;           /* 0x14 */
  int preset;         /* 0x18 */
} ExitState;
#define ENTER_POSE4(a, n)                                                      \
  (a)->mode = (n);                                                            \
  if ((a)->nextAnim == (n)) {                                                     \
    continue;                                                                  \
  }                                                                            \
  ANIM_ADVANCE(a, n);                                                          \
  continue
extern int TweenActorAlongPath(Actor *actor, PathData *path, int *a, int *b, int c,
                         int d);
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
#define CAM_LOOK (*(CamLook *)&g_anSpyroRequestVelocity)
#define AS_PTRV(sym) (((PtrV *)&(sym))->v)
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
#define FACE4(a, h, s, t)                                                      \
  ((int (*)(Actor *, int, int, int))TurnActorYawTowardAngle)(a, h, s, t)
#define RESET_IDLE(a)                                                          \
  (a)->mode = 0;                                                              \
  ANIM_RESET(a, 0);                                                            \
  continue
#define PATH_PT(p, i) ((int *)((p) + (((i) << 4) + 0x8)))
#define ANIM_WORD(a) (*(int *)&(a)->anim)
#define SPY_FACING(a)                                                          \
  AbsAngleDelta8(spy->bodyRotZ, ArcTan2((a)->posX - spy->posX,            \
                                             (a)->posY - spy->posY, 0))
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
#define ANIM_RETIME(a)                                                         \
  if (ABS2((a)->nextFrame - (a)->frame) < 3) {                                     \
    (a)->frameRate = TYPE_DESC(a, (a)->anim);                                     \
  }
#define ANIM_JUMP(a, n, f)                                                     \
  g_nActorAnimDone = 0;                                                              \
  (a)->frameProgress = sixteen;                                                        \
  (a)->frameRate = sixteen;                                                        \
  (a)->anim = (a)->nextAnim;                                                     \
  (a)->nextAnim = (n);                                                            \
  (a)->frame = (a)->nextFrame;                                                     \
  (a)->nextFrame = (f);                                                            \
  StubActorHook(a)
#define FLY_Z(p, i) (*(int *)((char *)(p) + ((i) << 4) + 0x10))
#define FP_X(p, i) (*(int *)((char *)(p) + ((i) << 4) + 0x8))
#define FP_Y(p, i) (*(int *)((char *)(p) + ((i) << 4) + 0xC))
#define FP_PT(p, i) ((int *)((char *)(p) + ((i) << 4) + 0x8))
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
extern int g_nCameraActiveTargetPitch, g_nCameraActiveTargetDistance, g_nCameraActiveTargetRoll, g_nCameraActiveTargetPitchBias, g_nCameraActiveTargetYawBias;
#define L25_UNLINK(st)                                                         \
  if ((st)->link >= 0) {                                                       \
    g_pActorListBase[(st)->link].subMode &= 0xFB;                                      \
  }
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
extern int g_anLevelRunnerArrived[]; /* per-goal "runner arrived" flags */
extern int g_nSpawnAnchorValid[];
#define RUN_FLAGS(p) (*(int *)((p) + ((p)[1] << 4) + 0x14))
#define ROCK(st) (*(Actor **)((char *)(st) + 0x40))

/* ==== The function ==== */
#define one 1
void Level21_UpdateActors(void) {
  Actor **actorList;
  Actor *actor;
  SpyroObj *spy;
  char *base;
  int dt;

  /* Scratch vectors, in the original's slot order (named by sp offset). */
  int f056[3];
  int f072[3];
  int f088[3];
  int f104[3];
  int f120[3];
  int f136[3];
  int f152[3];
  int f168[3];
  int f184[3];
  int f200[3];
  int f216[3];
  int f232[3];
  int f248[3];
  int f264[3];
  int f280[3];
  int f296[3];
  int f312[3][3];
  int f352[3];
  int f368[3];
  int f384[3];
  int f400[3];
  int f416[3];
  int f432[3];
  int f448[7];
  int f480[3];
  int f496[3];
  int f512[3];
  int f528[3];
  int f544[3];
  int f560[3];
  int f576[3];
  int f592[3];

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
    spy = (SpyroObj *)(base - 8);

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
        int top = g_nSpyroWorldPosZ;
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
            g_nSpyroRequestTargetZ = ACTOR_RAW(actor, int, posZ) + 0x4000;
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

          CopyVector(f056, &spy->posX);
          cb = LookupCosine(g_bSpyroPersistentEulerZ * 0x10);
          cs = LookupSine(g_bSpyroPersistentEulerZ * 0x10);
          x = cb * 0xB5;
          y = cs * 0xB5;
          f056[0] += (y + x) * 4 >> 12;
          f056[1] += (int)((LookupSine(g_bSpyroPersistentEulerZ * 0x10) * 0xB5) -
                          (LookupCosine(g_bSpyroPersistentEulerZ * 0x10) * 0xB5U)) *
                        4 >>
                    12;
          f056[2] += 0x300;
          VectorToSVectorShr2(st->apex, f056);
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
          SVectorToVectorShl2(f072, st->centre);
          SVectorToVectorShl2(f088, st->apex);
          SubtractVector(f088, f088, f072);
          ScaleVector3Sat(f088, f088, st->phase);
          RShiftVector3(f088, 6);
          AddVector(&actor->posX, f072, f088);
          actor->rotZ += g_nFrameStep * 4;
        } else {
          CopyVector(f104, &spy->posX);
          f104[0] += LookupCosine(g_bSpyroPersistentEulerZ * 0x10) >> 3;
          f104[1] += LookupSine(g_bSpyroPersistentEulerZ * 0x10) >> 3;
          f104[2] += 0x300;
          VectorToSVectorShr2(st->centre, f104);
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
        SubtractVector(f120, &actor->posX, &CAM.posX);
        WorldToCameraRotate(f120, f120);
        g_pfnLevelEmitParticles(0x10, 0x4D, f120, 0);
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

    /* Type 57: boulder. Rolls down to the floor and bursts into shards
     * (more type-57 actors in state 1) that bounce off the scenery. */
    case 57: {
      L21Rock *st = actor->state;

      switch (actor->mode) {
      case 0: {
        int n;

        if (actor->subMode == 0) {
          continue;
        }
        {
          int h = ProbeActorGroundHeight(actor);
          int z = actor->posZ - 0x50;

          st->vz = h - z;
        }
        if (st->vz > 0x50) {
          st->vz = 0x50;
        }
        if (st->vz < -0x50) {
          st->vz = -0x50;
        }
        if (ABS(st->vz) < 0x32) {
          st->vz = 0;
        }
        if (st->life > 0xA) {
          st->vz = 0;
        }
        CopyVector(f104, &actor->posX);
        actor->posX += st->vx;
        actor->posY += st->vy;
        actor->posZ += st->vz;
        actor->rotX += st->spin[0];
        actor->rotY += st->spin[1];
        actor->rotZ += st->spin[2];
        st->life += g_nFrameStep;
        if (actor->subMode != 2 && st->life < 0x79 &&
            RaiseSpyroTriggerInRadius(&actor->posX, 0xDC, 0x86) == 0 &&
            CastRayWorldAndActors(f104, &actor->posX) == 0) {
          continue;
        }
        for (n = 0; n < 8; n++) {
          Actor *o;
          L21Rock *q;

          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          o = g_pfnLevelSpawnActor(0x39, actor);
          o->mode = 1;
          if (o->anim != 1) {
            ANIM_RESET(o, 1);
          }
          q = o->state;
          if (actor->subMode == 2) {
            int a = (st->heading + RandomIntInRange(-0x28, 0x28)) & 0xFF;
            int s = RandomIntInRange(0x96, 0xFA);

            q->vx = (s * g_anCosineLut[a]) >> 12;
            q->vy = (s * g_anSineLut[a]) >> 12;
          } else {
            q->vx = RandomIntInRange(-0x96, 0x96);
            q->vy = RandomIntInRange(-0x96, 0x96);
          }
          q->vz = RandomIntInRange(-0x78, 0xFA);
          q->life = RandomIntInRange(0x50, 0x78);
          o->rotX = GetRandomU32();
          o->rotY = GetRandomU32();
          o->rotZ = GetRandomU32();
          q->spin[0] = RandomSignedInRange(7, 0xF);
          q->spin[1] = RandomSignedInRange(7, 0xF);
          q->spin[2] = RandomSignedInRange(7, 0xF);
        }
        g_anSpyroRequestVelocity.x = (st->vx * 3) >> 3;
        g_nSpyroRequestVelocityZ = 0x1E;
        g_nSpyroRequestVelocityY = (st->vy * 3) >> 3;
        DespawnActorRecord(actor);
        continue;
      }

      case 1:
        if (actor->drawn == 0) {
          DespawnActorRecord(actor);
          continue;
        }
        actor->rotX += st->spin[0];
        actor->rotY += st->spin[1];
        actor->rotZ += st->spin[2];
        st->vz -= 0x14;
        if (st->vz < -0xA0) {
          st->vz = -0xA0;
        }
        f056[0] = st->vx;
        f056[1] = st->vy;
        f056[2] = st->vz;
        CopyVector(f104, &actor->posX);
        AddVector(&actor->posX, &actor->posX, f056);
        if (TickCountdownByFrameDelta(&st->life, 2) != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        if (CollideSphereWithWorldAndActors(&actor->posX, 0x100, 0x100) == 0) {
          continue;
        }
        CopyVector(f136, &g_anCollisionProbeVec);
        if (ReflectVectorOffNormal(f056, f136, f056) == 0) {
          continue;
        }
        st->vx = (f056[0] * 7) >> 3;
        st->vy = (f056[1] * 7) >> 3;
        st->vz = (f056[2] * 7) >> 3;
        continue;
      }
      continue;
    }

    /* Type 100 (and 102): boulder thrower. Patrols its path, picks up a
     * boulder (57) and hurls it, rides its partner, and charges Spyro. */
    case 100: {
      L21State100 *st = actor->state;
      int f;

      if (actor->type == 0x66) {
        TickActorFlameGlow(actor);
      }
      f = actor->flags;
      if ((f & 0xF0000) && actor->mode != 3 &&
          (actor->type != 0x66 || f != 0x10000)) {
        Actor *c;

        if (f & 0x40000) {
          st->u34 = 0;
          st->u3C = 0x28;
        } else {
          if (f == 0x20000) {
            st->u34 = 0x15E;
          } else {
            st->u34 = 0x118;
          }
          st->u30 = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                                actor->posY - spy->posY, 0),
                                  spy->bodyRotZ, 0x20, 0x40);
          if (actor->mode != 0x2B) {
            st->u3C = 0x1E;
          }
        }
        actor->flags = 0;
        c = st->rock;
        if (c != 0 && c->subMode == 0) {
          c->subMode = 2;
          {
            L21Rock *r = c->state;

            st->rock = 0;
            r->heading = st->u30;
          }
        }
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        ENTER_POSE(actor, 3);
      }
      actor->flags = 0;

      if (st->u7C != 0) {
        if (st->u7C == 1) {
          st->u7C = 0;
          actor->mode = 0x28;
          continue;
        }
        if (actor->group != 0 && actor->subMode == 0) {
          continue;
        }
        actor->subMode++;
        st->path->dir = -1;
        while (AbsAngleDelta8(
                   actor->rotZ,
                   ArcTan2(FLY_X(st->path, st->path->cur) - actor->posX,
                                 FLY_Y(st->path, st->path->cur) - actor->posY,
                                 0)) >= 0x40) {
          st->path->cur++;
        }
        st->u7C = 0;
        actor->mode = 0x32;
        continue;
      }

      if (st->u28 == 2) {
        if (st->path->cur != 0 &&
            IsPointInRotatedBox((int *)&g_anSpyroWorldPos, st->u4C) != 0) {
          st->path->cur = 0;
          CopyVector(&actor->posX,
                        PATH_PT((unsigned char *)st->path, 0));
          SnapActorToGround(actor);
        } else if (st->path->cur != 1 &&
                   IsPointInRotatedBox((int *)&g_anSpyroWorldPos, st->u64) != 0) {
          st->path->cur = 1;
          CopyVector(&actor->posX,
                        PATH_PT((unsigned char *)st->path, 1));
          SnapActorToGround(actor);
        }
      }

      actor->flags = 0;
      switch (actor->mode) {
      case 0:
        if (actor->subMode != 0) {
          ENTER_POSE(actor, actor->subMode);
        }
        if (st->u10 != 0) {
          ENTER_POSE(actor, 2);
        }
        break;

      case 1:
        if (g_nActorAnimDone == 0) {
          break;
        }
        if (st->u08 != 0) {
          SetActorGroupSubstate(actor, 1);
          ModifyWorldChunkAnimFlags(st->u0C, 0xFC, 0);
        }
        if (st->u28 & 1) {
          ENTER_POSE(actor, 5);
        }
        actor->mode = 2;
        ANIM_RESET(actor, 2);
        continue;

      case 2:
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if ((st->u28 & 1) &&
            (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x1F4
                               : -SPY_DZ(actor) < 0x1F4) &&
            ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) < st->u48) {
          goto pose5;
        }
        if (st->u58 != 0 &&
            IsPointInRotatedBox((int *)&g_anSpyroWorldPos, st->u4C) != 0) {
        pose5:
          ENTER_POSE(actor, 5);
        }
        if (TickCountdownByFrameDelta(&st->u2C, 4) != 0 &&
            ApproxDist2D(&actor->posX, &spy->posX) < st->u44 &&
            (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x1F4
                               : -SPY_DZ(actor) < 0x1F4) &&
            CheckSpyroLineOfSightTo(&actor->posX) != 0) {
          ENTER_POSE(actor, 4);
        }
        break;

      case 3:
        if (DriveActorTrackedMove(actor, &st->u34, st->u30, &st->u3C, 0xC, 0xA) ==
                3 &&
            st->u34 == 0 && st->u94 == 0) {
          PlayActorMeshVoice(actor, 0, 0);
          st->u94 = 1;
        }
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x18);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case 4:
        TrackEntityYawTowardSpyro(actor, 8, 0, 0);
        if ((unsigned)(actor->nextFrame - 3) < 5 && st->rock == 0) {
          st->rock = g_pfnLevelSpawnActor(0x39, actor);
          st->rock->subMode = 0;
        }
        if (st->rock != 0 && st->rock->subMode == 0) {
          RefreshActorDerivedState(actor, 4);
          GetActorAttachPoint(actor, 0, &st->rock->posX);
          st->rock->rotZ = ArcTan2(spy->posX - st->rock->posX,
                                          spy->posY - st->rock->posY, 0);
          if (actor->nextFrame >= 0x10) {
            L21Rock *q = st->rock->state;

            q->life = 0;
            q->vx = (g_anCosineLut[st->rock->rotZ] * 0x4B) >> 10;
            q->vy = (g_anSineLut[st->rock->rotZ] * 0x4B) >> 10;
            q->vz = 0;
            st->rock->subMode = 1;
            st->rock = 0;
            q->spin[0] = RandomSignedInRange(7, 0xF);
            q->spin[1] = RandomSignedInRange(7, 0xF);
            q->spin[2] = RandomSignedInRange(7, 0xF);
          }
        }
        if (g_nActorAnimDone != 0) {
          st->u2C = 0x78;
          if (actor->type == 0x66) {
            st->u2C = 0x50;
          }
          st->rock = 0;
          ENTER_POSE(actor, 2);
        }
        break;

      case 5:
        st->u28 &= ~1;
        if (ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) < 0x1400) {
          ENTER_POSE(actor, 4);
        }
        if (PivotActorAlongPath(actor, st->path, 0x100, 0x8C, 0, 8, 0x28, 0xFF, 5) ==
            0x100) {
          ENTER_POSE(actor, 2);
        }
        break;

      case 40:
        ANIM_GO(actor, 2);
        if (actor->subMode == 0) {
          break;
        }
        st->u80 = ApproxDist2D(&actor->posX, &g_pActorListBase[st->partner].posX);
        actor->mode = 0x29;
        continue;

      case 41: {
        int h = g_pActorListBase[st->partner].rotZ;

        ANIM_GO(actor, 5);
        if (SteerActorAroundPoint(actor, &g_pActorListBase[st->partner].posX, st->u80, h, 4,
                          0x64, 0xE, 0x80, 0xFF, 0xFF, 0, 0, 0) < 6) {
          g_pActorListBase[st->partner].subMode = 1;
          actor->subMode = 0;
          actor->mode = 0x2A;
          continue;
        }
        break;
      }

      case 42:
        ANIM_GO(actor, 2);
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        if (actor->subMode != 0) {
          int d = ApproxDist2D(&actor->posX, &spy->posX);

          ANIM_GO(actor, 2);
          st->u30 = ArcTan2(spy->posX - actor->posX,
                                  SPY.posY - actor->posY, 0);
          st->u34 = 0x15E;
          st->u3C = d >> 7;
          if (d < 0x1800 ||
              (d < 0x2800 && !(SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x3E9
                                                 : -SPY_DZ(actor) < 0x3E9))) {
            st->u3C = 0;
          }
          if (st->u3C >= 0x6F) {
            st->u3C = 0x6E;
          }
          st->u84 = RandomSignedInRange(8, 0xF);
          st->u88 = RandomSignedInRange(8, 0xF);
          st->u2C = 0xF0;
          actor->mode = 0x2B;
          continue;
        } else {
          int h;

          if (g_pActorListBase[st->partner].subMode == 0) {
            break;
          }
          h = g_pActorListBase[st->partner].rotZ;
          if (AbsAngleDelta8(
                  h, ArcTan2(actor->posX - g_pActorListBase[st->partner].posX,
                                   actor->posY - g_pActorListBase[st->partner].posY,
                                   0)) < 0xD) {
            break;
          }
          g_pActorListBase[st->partner].subMode = 0;
          actor->mode = 0x29;
          continue;
        }

      case 43: {
        int r;

        CopyVector(f136, &actor->posX);
        r = DriveActorTrackedMove(actor, &st->u34, st->u30, &st->u3C, 0, 0x12);
        if (r == 3) {
          st->u3C = 0x50;
        }
        actor->updateDist = 0xFF;
        actor->probeDist = 0xFF;
        TurnActorYawTowardAngle(actor, st->u30, 6, 0, 0);
        ANIM_GO(actor, 7);
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x2BC &&
            (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x2BC
                               : -SPY_DZ(actor) < 0x2BC) &&
            g_nSpyroState != 0xB && g_nSpyroState != 0x14) {
          int a = StepAngle8Toward(ArcTan2(g_anSpyroWorldPos - actor->posX,
                                              g_nSpyroWorldPosY - actor->posY, 0),
                                st->u30, 0x20)
                  << 4;

          g_dwSpyroTriggerEventFlags |= 0x86;
          g_anSpyroRequestVelocity.x = (LookupCosine(a) * 0x6E) >> 12;
          g_nSpyroRequestVelocityY = (LookupSine(a) * 0x6E) >> 12;
          CAM_LOOK.z = 0x28;
          st->u30 = ArcTan2(actor->posX - g_anSpyroWorldPos,
                                  actor->posY - g_nSpyroWorldPosY, 0);
          st->u34 = 0xDC;
          st->u3C = 0x3C;
          SpawnActorDeathDrop(actor, 4, 0, 0);
          ENTER_POSE(actor, 3);
        }
        if (TickCountdownByFrameDelta(&st->u2C, 4) != 0 || r == 2 ||
            ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) > 0x9470) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x18);
          SpawnActorDeathDrop(actor, 4, 0, 0);
          MarkActorCommittedKill(actor);
          DespawnActorRecord(actor);
          continue;
        } else {
          Actor **lp = g_apSaveMenuAnimActorList;
          Actor *o;

          while ((o = *lp++) != 0) {
            if (o->type == 0x41 &&
                (actor->posX - o->posX > 0 ? actor->posX - o->posX < 0xBB8
                                           : o->posX - actor->posX < 0xBB8) &&
                (actor->posY - o->posY > 0 ? actor->posY - o->posY < 0xBB8
                                           : o->posY - actor->posY < 0xBB8) &&
                ApproxDist2D(&actor->posX, &o->posX) < 0x384 &&
                AbsAngleDelta8(st->u30,
                              ArcTan2(o->posX - actor->posX,
                                            o->posY - actor->posY, 0)) < 0x28) {
              o->flags |= 0xC0000;
            }
          }
        }
        break;
      }

      case 50:
        if (st->u14 != 0) {
          actor->cullRadius = 0x28;
          actor->mode++;
        }
        break;

      case 51: {
        int r = PivotActorAlongPath(actor, st->path, 0x100, 0x78, 0, 8, 0x80, 0xFF, 5);

        ANIM_GO(actor, 5);
        if (r == 0x100) {
          actor->mode++;
        }
        break;
      }

      case 52: {
        Actor *p = &g_pActorListBase[st->rider];

        if (p->subMode != 0) {
          st->u30 = ArcTan2(p->posX - actor->posX, p->posY - actor->posY,
                                  0);
          st->u34 = 0x5A;
          st->u3C = 0x78;
          p->subMode = 2;
          actor->mode++;
          break;
        }
        ANIM_GO(actor, 2);
        TurnActorYawTowardAngle(actor,
                      ArcTan2(p->posX - actor->posX,
                                    p->posY - actor->posY, 0),
                      6, 0, 0);
        break;
      }

      case 53: {
        Actor *p = &g_pActorListBase[st->rider];

        TurnActorYawTowardAngle(actor, st->u30, 6, 0, 0);
        ANIM_GO(actor, 6);
        RefreshActorDerivedState(p, 4);
        GetActorAttachPoint(p, 0, f168);
        GetActorAttachPoint(p, 1, f184);
        AddVector(f152, f168, f184);
        RShiftVector3(f152, 1);
        DriveActorTrackedMove(actor, &st->u34, st->u30, &st->u3C, 0, sixteen);
        if (actor->posZ < f152[2]) {
          CopyVector(&actor->posX, f152);
          actor->mode++;
        }
        break;
      }

      case 54: {
        Actor *p = &g_pActorListBase[st->rider];

        TurnActorYawTowardAngle(actor, st->u30, 6, 0, 0);
        RefreshActorDerivedState(p, 4);
        GetActorAttachPoint(p, 0, f216);
        GetActorAttachPoint(p, 1, f232);
        AddVector(f200, f216, f232);
        RShiftVector3(f200, 1);
        CopyVector(&actor->posX, f200);
        if (p->nextFrame >= 0xE) {
          if (st->partner != -1) {
            ((int *)g_pActorListBase[st->partner].state)[5] = 1;
          }
          *st->u8C = 0;
          st->u30 = ArcTan2(spy->posX - actor->posX,
                                  spy->posY - actor->posY, 0);
          st->u34 = 0x1C2;
          st->u2C = 0xB4;
          st->u3C = 0;
          actor->mode = 0x2B;
        }
        break;
      }
      }
      RefreshActorDerivedState(actor, 1);
      continue;
    }

    /* Type 101: gatekeeper. Guards a run of keys (g_pActorListBase actors) and
     * releases them as Spyro approaches; rides its partner and charges. */
    case 101: {
      L21State101 *st = actor->state;
      int f = actor->flags;

      if ((f & 0xD0000) && actor->mode != 5) {
        if (f & 0x40000) {
          st->u30 = 0;
          st->u34 = 0x28;
        } else {
          st->u30 = 0xF0;
          st->u2C = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                                actor->posY - spy->posY, 0),
                                  spy->bodyRotZ, 0x20, 0x40);
        }
        if (actor->mode != 0x2B) {
          st->u34 = 0x1E;
        }
        actor->flags = 0;
        SpawnActorDeathDrop(actor, 1, 0, 0);
        MarkActorCommittedKill(actor);
        ENTER_POSE(actor, 5);
      }

      if (st->u6C != 0) {
        if (st->u6C == 1) {
          st->u6C = 0;
          actor->mode = 0x28;
          continue;
        }
        if (actor->group != 0 && actor->subMode == 0) {
          continue;
        }
        actor->subMode++;
        st->path->dir = -1;
        while (AbsAngleDelta8(
                   actor->rotZ,
                   ArcTan2(FLY_X(st->path, st->path->cur) - actor->posX,
                                 FLY_Y(st->path, st->path->cur) - actor->posY,
                                 0)) >= 0x40) {
          st->path->cur++;
        }
        st->u6C = 0;
        actor->mode = 0x32;
        continue;
      }

      if (st->mode == 2) {
        if (st->path->cur != 0 &&
            IsPointInRotatedBox((int *)&g_anSpyroWorldPos, st->u3C) != 0) {
          st->path->cur = 0;
          CopyVector(&actor->posX,
                        PATH_PT((unsigned char *)st->path, 0));
          SnapActorToGround(actor);
        } else if (st->path->cur != 1 &&
                   IsPointInRotatedBox((int *)&g_anSpyroWorldPos, st->u54) != 0) {
          st->path->cur = 1;
          CopyVector(&actor->posX,
                        PATH_PT((unsigned char *)st->path, 1));
          SnapActorToGround(actor);
        }
      }

      switch (actor->mode) {
      case 0:
        if (actor->subMode == 0 && st->u10 == 0) {
          break;
        }
        if (actor->type == 0xAA) {
          actor->mode = 2;
          ANIM_RESET(actor, 2);
          continue;
        }
        ENTER_POSE(actor, 1);

      case 1:
        if (g_nActorAnimDone == 0) {
          break;
        }
        if (st->u08 != 0) {
          SetActorGroupSubstate(actor, 2);
          ModifyWorldChunkAnimFlags(st->u0C, 0xFC, 0);
        }
        actor->mode = 2;
        ANIM_RESET(actor, 2);
        continue;

      case 2:
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) < 0x1C00) {
          ENTER_POSE(actor, 3);
        }
        break;

      case 3: {
        int d = ApproxDist2D(&actor->posX, &spy->posX);
        int dz = SPY_DZ(actor);

        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (d > 0x2400) {
          ENTER_POSE(actor, 2);
        }
        if (TickCountdownByFrameDelta(&st->u1C, 4) != 0 && d < 0xD48 &&
            (unsigned)(dz + 0x577) < 0x4AF) {
          ENTER_POSE(actor, 4);
        }
        break;
      }

      case 4: {
        int d = ApproxDist2D(&actor->posX, &spy->posX);

        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        if ((unsigned)(actor->nextFrame - 6) < 4) {
          int r = (actor->nextFrame - 5) * 0x3E8;

          if (r < 0x3E8) {
            r = 0x3E8;
          }
          if (r >= 0xDAD) {
            r = 0xDAC;
          }
          if (d < r && SPY_DZ(actor) < -0xC8) {
            g_dwSpyroTriggerEventFlags |= 0x86;
            g_anSpyroRequestVelocity.x = (LookupCosine(actor->rotZ << 4) * 0x8C) >> 12;
            g_nSpyroRequestVelocityY = (LookupSine(actor->rotZ << 4) * 0x8C) >> 12;
            g_nSpyroRequestVelocityZ = 0x46;
          }
        }
        if (g_nActorAnimDone != 0) {
          st->u1C = 0x78;
          ENTER_POSE(actor, 3);
        }
        break;
      }

      case 5:
        DriveActorTrackedMove(actor, &st->u30, st->u2C, &st->u34, 0xE, 0xA);
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x34);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case 40:
        ANIM_GO(actor, 2);
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (st->idx < st->count &&
            ApproxDist2D(&actor->posX, &spy->posX) <
                (st->range[0] + 6) << 10 &&
            actor->drawn != 0 &&
            (st->u90 == -1 ||
             AbsAngleDelta8(st->u90,
                           ArcTan2(spy->posX - actor->posX,
                                         SPY.posY - actor->posY, 0)) < 0x1C)) {
          g_pActorListBase[st->keys[st->idx]].subMode = 1;
          actor->mode++;
          break;
        }
        if (st->idx < st->count) {
          break;
        }
        ENTER_POSE(actor, 2);

      case 41:
        ANIM_GO(actor, 2);
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (actor->subMode == 0) {
          break;
        }
        if (ApproxDist2D(&actor->posX, &spy->posX) >=
            st->range[st->idx] << 10) {
          break;
        }
        if (AbsAngleDelta8(g_bSpyroPersistentEulerZ,
                          ArcTan2(actor->posX - spy->posX,
                                        actor->posY - SPY.posY, 0)) >= 0x30) {
          break;
        }
        if (actor->drawn == 0) {
          break;
        }
        if (CheckSpyroLineOfSightTo(&actor->posX) == 0) {
          break;
        }
        actor->subMode = 0;
        actor->mode++;
        break;

      case 42: {
        int k;

        ANIM_GO(actor, 4);
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        if (actor->nextFrame < 6) {
          break;
        }
        k = st->keys[st->idx];
        if (k == -1) {
          break;
        }
        g_pActorListBase[k].subMode = 1;
        st->keys[st->idx] = -1;
        if (++st->idx < st->count) {
          g_pActorListBase[st->keys[st->idx]].subMode = 1;
        }
        actor->mode = 0x2C;
        continue;
      }

      case 43: {
        int r;

        CopyVector(f136, &actor->posX);
        r = DriveActorTrackedMove(actor, &st->u30, st->u2C, &st->u34, 0, 0x12);
        actor->updateDist = 0xFF;
        if (r == 3) {
          st->u34 = 0x5A;
        }
        ANIM_GO(actor, 8);
        actor->probeDist = 0xFF;
        TurnActorYawTowardAngle(actor, st->u2C, 6, 0, 0);
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x3E8 &&
            (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x320
                               : -SPY_DZ(actor) < 0x320)) {
          int a = StepAngle8Toward(ArcTan2(spy->posX - actor->posX,
                                              spy->posY - actor->posY, 0),
                                st->u2C, 0x20)
                  << 4;

          spy->damageFlags |= 0x86;
          spy->u208 = (LookupCosine(a) * 0x6E) >> 12;
          spy->u20C = (LookupSine(a) * 0x6E) >> 12;
          spy->u210 = 0x28;
          st->u2C = ArcTan2(actor->posX - spy->posX,
                                  actor->posY - spy->posY, 0);
          st->u30 = 0xF0;
          st->u34 = 0x3C;
          SpawnActorDeathDrop(actor, 4, 0, 0);
          ENTER_POSE(actor, 5);
        }
        if (TickCountdownByFrameDelta(&st->u1C, 4) != 0 || r == 2 ||
            ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) > 0x9470) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x34);
          SpawnActorDeathDrop(actor, 4, 0, 0);
          MarkActorCommittedKill(actor);
          DespawnActorRecord(actor);
          continue;
        } else {
          Actor **lp = g_apSaveMenuAnimActorList;
          Actor *o;

          while ((o = *lp++) != 0) {
            if (o->type == 0x41 &&
                (actor->posX - o->posX > 0 ? actor->posX - o->posX < 0xBB8
                                           : o->posX - actor->posX < 0xBB8) &&
                (actor->posY - o->posY > 0 ? actor->posY - o->posY < 0xBB8
                                           : o->posY - actor->posY < 0xBB8) &&
                ApproxDist2D(&actor->posX, &o->posX) < 0x4B0 &&
                AbsAngleDelta8(st->u2C,
                              ArcTan2(o->posX - actor->posX,
                                            o->posY - actor->posY, 0)) < 0x28) {
              o->flags |= 0xC0000;
            }
          }
        }
        break;
      }

      case 44:
        if (g_nActorAnimDone == 0) {
          break;
        }
        if (st->idx < st->count) {
          actor->mode = 0x29;
          continue;
        }
        ENTER_POSE(actor, 2);

      case 50:
        if (st->u14 != 0) {
          actor->cullRadius = 0x28;
          actor->mode++;
        }
        break;

      case 51: {
        int r = PivotActorAlongPath(actor, st->path, 0x100, 0x78, 0, 8, 0x80, 0xFF, 5);

        ANIM_GO(actor, 6);
        if (r == 0x100) {
          actor->mode++;
        }
        break;
      }

      case 52: {
        Actor *p = &g_pActorListBase[st->rider];

        if (p->subMode != 0) {
          st->u2C = ArcTan2(p->posX - actor->posX, p->posY - actor->posY,
                                  0);
          st->u30 = 0x5A;
          st->u34 = 0x78;
          p->subMode = 2;
          actor->mode++;
          break;
        }
        ANIM_GO(actor, 2);
        TurnActorYawTowardAngle(actor,
                      ArcTan2(p->posX - actor->posX,
                                    p->posY - actor->posY, 0),
                      6, 0, 0);
        break;
      }

      case 53: {
        Actor *p = &g_pActorListBase[st->rider];

        TurnActorYawTowardAngle(actor, st->u2C, 6, 0, 0);
        ANIM_GO(actor, 7);
        RefreshActorDerivedState(p, 4);
        GetActorAttachPoint(p, 0, f168);
        GetActorAttachPoint(p, 1, f248);
        AddVector(f216, f168, f248);
        RShiftVector3(f216, 1);
        DriveActorTrackedMove(actor, &st->u30, st->u2C, &st->u34, 0, sixteen);
        if (actor->posZ < f216[2]) {
          CopyVector(&actor->posX, f216);
          actor->mode++;
        }
        break;
      }

      case 54: {
        Actor *p = &g_pActorListBase[st->rider];

        TurnActorYawTowardAngle(actor, st->u2C, 6, 0, 0);
        RefreshActorDerivedState(p, 4);
        GetActorAttachPoint(p, 0, f280);
        GetActorAttachPoint(p, 1, f296);
        AddVector(f264, f280, f296);
        RShiftVector3(f264, 1);
        CopyVector(&actor->posX, f264);
        if (p->nextFrame >= 0xE) {
          if (st->partner != -1) {
            ((int *)g_pActorListBase[st->partner].state)[5] = 1;
          }
          *st->u88 = 0;
          st->u2C = ArcTan2(spy->posX - actor->posX,
                                  spy->posY - actor->posY, 0);
          st->u30 = 0x1F4;
          st->u1C = 0xB4;
          st->u34 = 0;
          actor->mode = 0x2B;
        }
        break;
      }
      }
      RefreshActorDerivedState(actor, 1);
      continue;
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
        f136[0] = GetRandomU32() & 3;
        f136[1] = GetRandomU32() & 3;
        f136[2] = 0x14;
        g_pfnLevelEmitParticles(1, 1, &actor->posX, f136);
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

        CopyVector(f232, &actor->posX);
        f232[2] += 0x400;
        FindGroundHeightBelow(f232, 0x10000);
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

            UnpackWorldCollisionTri(st->groundTri, f312[0]);

            AddVector(f312[0], f312[0], f312[1]);
            AddVector(f312[0], f312[0], f312[2]);

            actor->posX = f312[0][0] / 3;
            actor->posY = f312[0][1] / 3;
            actor->posZ = f312[0][2] / 3;

            RefreshActorDerivedState(actor, 2);
            SnapActorToGround(actor);
            EncodeCachedVecToActorDirCode(actor);

          } else if (st->sub == 3) {
            int colIndex;

            CopyVector(f280, &actor->posX);
            f280[2] += 0x400;

            RefreshActorDerivedState(actor, 2);

            if (FindGroundHeightBelow(f280, 0x1000) > 0) {
              colIndex = g_nLastCollisionHitChunkId;
              st->sub = 2;
              st->groundTri = colIndex;
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

        ScaleVector3Sat(f168, st->vel, g_nFrameStep);
        RShiftVector3(f168, 1);

        speed = VectorLength(f168, 1);

        if (220 < speed) {
          ScaleVector3ByRatio(f168, speed, 220);
        }

        AddVector(f168, &actor->posX, f168);

        if (-220 < st->vel[2]) {
          st->vel[2] -= g_nFrameStep * 5;
        }

        if (f168[2] < 0) {
          if (actor->type != 14 && actor->type != 15) {
            CollectPickupActor(actor);
          }
          DespawnActorRecord(actor);
          continue;
        } else {
          f168[2] += 240;

          if (CollideSphereWithWorldAndActors(f168, 240, 240)) {
            if (GetGroundTriggerRegionType() == 0) {
              if (actor->type != 14 && actor->type != 15) {
                CollectPickupActor(actor);
              }
              DespawnActorRecord(actor);
              continue;
            }

            CopyVector(f352, &g_anCollisionProbeVec);
            CopyVector(&actor->posX, &g_anCollisionResolvedPos);
            actor->posZ -= 0xF0;

            if (st->count == 0) {
              int groundHeight = FindGroundHeightBelow(f168, 0x400);
              int groundAngle =
                  (signed char)ArcTan2_8bit(f352[2], VectorLength(f352, 0));

              if ((f168[2] - 400) < groundHeight && groundAngle < 24) {
                actor->subMode = 1;
                CopyVector(&actor->posX, f168);
                actor->posZ = groundHeight;

                if (actor->type != 0xF) {

                  st->pitch = -ArcTan2_8bit(
                      IntegerSqrt((f352[0] * f352[0]) + (f352[2] * f352[2])),
                      f352[1]);
                  st->yaw = -ArcTan2_8bit(f352[2], f352[0]);

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

              if (ReflectVectorOffNormal(st->vel, f352, st->vel)) {
                st->count--;

                st->vel[0] = (st->vel[0] >> 3) + (GetRandomU32() & 0x3F) - 32;
                st->vel[1] = (st->vel[1] >> 3) + (GetRandomU32() & 0x3F) - 32;
                st->vel[2] = (st->vel[2] >> 2) + (GetRandomU32() & 0xF);
              }
            }

          } else {
            f168[2] -= 240;
            CopyVector(&actor->posX, f168);
            FindGroundHeightBelow(f168, 0x10000);
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

          SubtractVector(f368, &spy->posX, st->vel);
          RShiftVector3(f368, 5);

          if (VectorLength(f368, 1) > 480) {
            CopyVector(&actor->posX, &spy->posX);
            st->timer = 32;
          } else {
            ScaleVector3Sat(f368, f368, st->timer);
            AddVector(&actor->posX, st->vel, f368);
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

    /* Lift plate (PlateState): rises once its trigger actor is gone, waits,
     * then drops back. */
    case 110: {
      PlateState *st = actor->state;
      Actor *trigger = &g_pActorListBase[st->trigger];
      Actor *rider = &g_pActorListBase[st->rider];

      switch (actor->mode) {
      case 0:
        actor->collision = 0;
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
          actor->collision = 0;
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
                actor->cullRadius = 0x20;
                actor->mode = 3;
                actor->scale = 0x60;
                g_pfnLevelEmitParticles(0x18, 0x50, at, (int *)0xC);
                RefreshActorDerivedState(actor, 1);
                FindGroundHeightBelow(at, 0x1000);
                EncodeCachedVecToActorDirCode(actor);
                actor->groundCode |= 0x80000000;
              }
            }
          }
        }
        break;

      case 3:
        if (actor->scale >= 0x21) {
          actor->scale -= 4;
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
          SubtractVector(f232, st->from, here);
          turn = SignedAngleDelta8(st->yaw, actor->rotZ);
          w0 = LookupCosine((st->step - 1) << 7);
          wSpan = w0 - LookupCosine(st->step << 7);
          wDen = LookupCosine((st->step - 1) << 7) + 0x1000;
          actor->posX += (f232[0] * wSpan) / wDen;
          actor->posY += (f232[1] * wSpan) / wDen;
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
        if (actor->scale < 0x60) {
          actor->scale += 4;
          actor->rotZ += 8;
        } else {
          actor->cullRadius = 0;
          actor->drawn = 0;
          actor->mode = 2;
          st->timer = 0x10;
          g_pfnLevelEmitParticles(0x18, 0x50, &actor->posX, (int *)0xC);
          actor->groundCode &= 0x7FFFFFFF;
        }
        break;
      }

      break;
    }

    /* The balloonist (BalloonState): smokes, waits for his gnorc, then flies
     * Spyro out once the world is finished. */
    case 120: {
      BalloonState *st = actor->state;

      if (g_nLevelReadyFlag[0] >= 2) {
        f232[0] = 0;
        f232[1] = 0x64;
        f232[2] = 0;
        RotateVectorByMatrix(&actor->mtx[0], f232, f232);
        AddVector(f232, f232, &actor->posX);
        g_pfnLevelEmitParticles(2, 0x42, f232, 0);
        f232[0] = 0;
        f232[1] = -0x64;
        f232[2] = 0;
        ApplyActiveGteRotation(f232, f232);
        AddVector(f232, f232, &actor->posX);
        g_pfnLevelEmitParticles(2, 0x42, f232, 0);
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

            CopyVector(f280, &st->gnorc->posX);

            if (high == 0) {
              f280[0] -=
                  (unsigned short)g_anCosineLut[st->gnorc->rotZ] << 16 >> 20;
              f280[1] -=
                  (unsigned short)g_anSineLut[st->gnorc->rotZ] << 16 >> 20;
            } else {
              f280[0] -=
                  (unsigned short)g_anCosineLut[st->gnorc->rotZ] << 16 >> 22;
              f280[1] -=
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

            SubtractVector(f280, f280, &actor->posX);
            dist = VectorLength(f280, 1);

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
              ScaleVector3ByRatio(f280, dist, limit);
            } else {
              ScaleVector3ByRatio(f280, dist, reach - 5);
            }
            AddVector(&actor->posX, &actor->posX, f280);
            TurnActorYawTowardAngle(actor, ArcTan2(f280[0], f280[1], 0), 0xA, 0, 0);
            actor->rotY = ArcTan2(VectorLength(f280, 0), f280[2], 0);
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

            SubtractVector(f384, &st->gnorc->posX, &actor->posX);
            dist = VectorLength(f384, 1);

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
                ScaleVector3ByRatio(f384, dist, lim);
              } else {
                ScaleVector3ByRatio(f384, dist, 0x122);
              }
              AddVector(&actor->posX, &actor->posX, f384);
              TurnActorYawTowardAngle(actor, ArcTan2(f384[0], f384[1], 0), 0xA, 0, 0);
              actor->rotY = ArcTan2(VectorLength(f384, 0), f384[2], 0);
              actor->rotY = StepAngle8Toward(actor->rotY & 0xFF, 0, 0x30);
            }
            break;
          }
          }
        } else {
          /* Nobody in the basket: drift on the wind. */
          actor->cullRadius = 0x10;

          if (st->timer <= 0) {
            f232[0] = -0x258;
            f232[1] = (GetRandomU32() & 0x310) - 0x188;
            f232[2] = g_nCameraCurrentMode == 3 ? (GetRandomU32() & 0x7F) + 0x64
                                     : (GetRandomU32() & 0x1FF) - 0xC8;
            do {
            } while (0);
            st->drift[0] = f232[0];
            st->drift[1] = f232[1];
            st->drift[2] = f232[2];
            st->timer = GetRandomU32() & 0x7B;
          } else {
            int *at;

            st->timer -= g_nFrameStep;
            do {
            } while (0);

            if (g_nCameraCurrentMode == 3) {
              f232[0] = st->drift[0] + (g_nSpyroMotionSpeed >> 2);
              f232[1] = st->drift[1];
              f232[2] = st->drift[2];
              if (f232[2] < 0x64) {
                f232[2] = 0x64;
              }
            } else {
              if (g_nCameraCurrentMode == 0x80000009) {
                f232[0] = st->drift[0] - 0x200;
              } else {
                f232[0] = st->drift[0];
              }
              f232[1] = st->drift[1];
              f232[2] = st->drift[2];
            }

            RotateVectorByMatrix(&SPY.u034, f232, f232);
            AddVector(f232, f232, &SPY.posX);
            at = &actor->posX;
            SubtractVector(f232, f232, at);
            RShiftVector3(f232, 2);
            AddVector(at, at, f232);

            if (CollideSphereWithWorldAndActors(at, 0x100, 0x100) != 0) {
              CopyVector(at, &g_anCollisionResolvedPos);
            }

            if (f232[2] >= 0x21) {
              f232[2] = 0x20;
            }
            if (f232[2] < -0x20) {
              f232[2] = -0x20;
            }

            actor->rotX = 0;
            actor->rotY = f232[2];
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
          f400[0] = 0x200;
          f400[1] = 0;
          f400[2] = 0x140;
          RotateVectorByMatrix(actor->mtx, f400, f400);
          AddVector(f400, f400, &actor->posX);
          SubtractVector(f400, f400, &spy->posX);
          f400[2] -= 0x100;
          RShiftVector3(f400, 5);
          ScaleVector3Sat(f400, f400, st->shake);
          DivideVector3(f400, f400, 6);
          AddVector(f400, f400, &spy->posX);
          f400[2] += 0x100;
          f400[2] += g_anSineLut[(st->shake << 7) / 192] >> 3;
          CopyVector(&fly->posX, f400);
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
          f416[0] = 0x200 - (st->shake - 0xC0) * 8;
          f416[1] = 0;
          f416[2] = 0x140;
          RotateVectorByMatrix(actor->mtx, f416, f416);
          AddVector(&fly->posX, f416, &actor->posX);
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
          SubtractVector(f432, at, &spy->posX);
          f432[2] = (f432[2] * 3) >> 2;

          if (VectorLength(f432, 1) < 0x440) {
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

    /* Types 293/294: path runner. Runs its path ahead of Spyro and raises
     * its goal flag on arrival; struck, it is knocked away. */
    case 293:
    case 294: {
      L21Runner *st = actor->state;

      if ((actor->flags & 0xB0000) && actor->mode >= 3 &&
          actor->mode != 4) {
        st->knock = 0x3C;
        if (actor->flags & 0x20000) {
          st->knock = 0x55;
        }
        st->lift = 0x3C;
        st->knockAng = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                                   actor->posY - spy->posY, 0),
                                     spy->bodyRotZ, 0x20, 0x40);
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        ENTER_POSE(actor, 2);
      }
      actor->flags = 0;

      if (st->init == 0) {
        st->init = 1;
        if (IsActorInRunningKillSet(actor) != 0) {
          if (st->mode == 0) {
            actor->group = 0;
            SetActorGroupSubstate(actor, 2);
            actor->group = 1;
            SetActorGroupSubstate(actor, 2);
          } else if (st->mode == 1) {
            actor->group = 2;
            SetActorGroupSubstate(actor, 2);
          }
          actor->group = 0xFF;
          DespawnActorRecord(actor);
          continue;
        }
      }

      if (st->mode == 0 && st->pathInit == 0) {
        st->pathInit = 1;
        if (g_nSpawnAnchorValid[0] != 0) {
          st->camTimer = 0;
        }
        if (IsPointInRotatedBox((int *)spy, st->volB) != 0) {
          actor->group = 0;
          SetActorGroupSubstate(actor, 2);
          actor->group = 1;
          SetActorGroupSubstate(actor, 2);
          actor->group = 0xFF;
          actor->rotZ = ArcTan2(spy->posX - actor->posX,
                                       SPY.posY - actor->posY, 0);
          st->camTimer = 0;
          if (g_anLevelRunnerArrived[st->mode] != 0) {
            CopyVector(&actor->posX,
                          (int *)(st->path + (st->path[0] * 16 - 8)));
            ENTER_POSE(actor, 3);
          }
          st->path = st->home;
          st->path[1] = st->path[0] - 1;
          CopyVector(&actor->posX,
                        (int *)(st->path + (st->path[0] * 16 - 0x18)));
          SnapActorToGround(actor);
          actor->mode = 5;
          continue;
        }
      }
      if (st->mode == 1 && st->pathInit == 0) {
        st->pathInit = 1;
        if (IsPointInRotatedBox((int *)spy, st->volB) != 0) {
          actor->group = 2;
          SetActorGroupSubstate(actor, 2);
          actor->group = 0xFF;
          actor->rotZ = ArcTan2(spy->posX - actor->posX,
                                       SPY.posY - actor->posY, 0);
          if (g_anLevelRunnerArrived[st->mode] != 0) {
            CopyVector(&actor->posX, PATH_PT(st->home, 0));
            SnapActorToGround(actor);
            ENTER_POSE(actor, 3);
          }
          st->path = st->home;
          st->path[1] = st->path[0] - 1;
          CopyVector(&actor->posX,
                        (int *)(st->path + (st->path[0] * 16 - 0x18)));
          SnapActorToGround(actor);
          actor->mode = 5;
          continue;
        }
        actor->mode = 0x14;
        continue;
      }

      {
        int r = 1;

        if (st->camTimer != 0) {
          if (g_nSpyroAirborneFrames == 0) {
            r = TickCountdownByFrameDelta(&st->camTimer, 4);
            st->camDone = 1;
          }
          if (st->camDone == 0) {
            continue;
          }
        }
        if (r == 0) {
          g_dwSpyroRequestMask = 0x80002000;
          g_nGameplayBlocked = 1;
        } else if (r == 2) {
          g_nGameplayBlocked = 0;
        }
      }

      if (actor->nextAnim == 4 && actor->cullRadius != 0) {
        if (actor->nextFrame == 1 && !(GetRandomU32() & 3) &&
            GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[6]) == 0) {
          PlayActorMeshVoice(actor, 6, 0);
        }
        if (actor->nextFrame == 10 && !(GetRandomU32() & 3) &&
            GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[5]) == 0) {
          PlayActorMeshVoice(actor, 5, 0);
        }
      }

      switch (actor->mode) {
      case 0: {
        int spd;
        int t;
        int r;
        int floor;
        int z;

        t = RUN_FLAGS(st->path) & 0xFFC;
        spd = 0xB4;
        if (t != 0) {
          spd = t;
        }
        if (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < 0x1400 && spd < 0xAA) {
          spd = 0xAA;
        }
        r = PivotActorAlongPath(actor, st->path, 0x17C, spd, 0, 8, 0x28, 0xFF, 0);
        floor = ProbeActorGroundHeightRaised(actor, 0xBB8);
        st->lift -= 0x1E;
        if (st->lift < -0x190) {
          st->lift = -0x190;
        }
        z = actor->posZ + st->lift;
        if (z < floor) {
          actor->posZ = floor;
        } else {
          actor->posZ = z;
        }
        if (st->mode == 0 && r == 0x11C &&
            ApproxDist2D(&actor->posX, &spy->posX) < 0x1C00 &&
            ABS2(SPY_DZ(actor)) < 0x898) {
          g_anLevelRunnerArrived[st->mode] = 1;
        }
        if (st->mode == 1 && r == 0x10A &&
            ApproxDist2D(&actor->posX, &spy->posX) < 0x1D4C &&
            ABS2(SPY_DZ(actor)) < 0xBB8) {
          g_anLevelRunnerArrived[st->mode] = 1;
        }
        if (r == 0x100) {
          if (st->mode == 1) {
            if (g_anLevelRunnerArrived[1] == 0) {
              st->path = st->home;
              st->path[1] = 0;
              actor->mode = 4;
              continue;
            }
            actor->mode = 10;
            continue;
          } else if (st->mode == 0) {
            if (g_anLevelRunnerArrived[0] != 0) {
              actor->mode = 3;
              continue;
            }
            st->path = st->home;
            st->path[1] = 0;
            actor->mode = 4;
            continue;
          }
        } else if (r & 0x100) {
          switch (RUN_FLAGS(st->path) & 3) {
          case 1:
            st->lift = 0xDC;
            break;
          case 2:
            st->lift = 0x140;
            break;
          }
        }
        if (st->target != -1 && actor->posZ == floor &&
            ApproxDist2D(&actor->posX, &g_pActorListBase[st->target].posX) <
                0x9C4) {
          actor->mode = 1;
          continue;
        }
        continue;
      }

      case 1:
        ANIM_GO(actor, 3);
        TurnActorYawTowardAngle(actor,
                      ArcTan2(g_pActorListBase[st->target].posX - actor->posX,
                                    g_pActorListBase[st->target].posY - actor->posY,
                                    0),
                      6, 0, 0);
        if (g_nActorAnimDone != 0) {
          g_pActorListBase[st->target].subMode = 1;
          st->target = *(int *)g_pActorListBase[st->target].state;
          ENTER_POSE(actor, 0);
        }
        continue;

      case 2:
        DriveActorTrackedMove(actor, &st->knock, st->knockAng, &st->lift, 0xC, 0xC);
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x18);
          DespawnActorRecord(actor);
          continue;
        }
        continue;

      case 3:
        ANIM_GO(actor, 4);
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (ApproxDist2D(&actor->posX, &spy->posX) < st->range &&
            AbsAngleDelta8(g_bSpyroPersistentEulerZ,
                          ArcTan2(actor->posX - spy->posX,
                                        actor->posY - SPY.posY, 0)) < 0x20) {
          st->path = st->home;
          st->path[1] = 0;
          st->lift = 0;
          actor->mode = 4;
          continue;
        }
        continue;

      case 4: {
        int spd;
        int t;
        int r;
        int floor;
        int z;
        int last;

        ANIM_GO(actor, 0);
        t = RUN_FLAGS(st->path) & 0xFFC;
        spd = 0xF0;
        if (t != 0) {
          spd = t;
        }
        r = PivotActorAlongPath(actor, st->path, 0x17C, spd, 0, 10, 0x28, 0xFF, 0);
        floor = ProbeActorGroundHeightRaised(actor, 0xBB8);
        st->lift -= 10;
        if (st->lift < -0x190) {
          st->lift = -0x190;
        }
        z = actor->posZ + st->lift;
        if (z < floor) {
          actor->posZ = floor;
        } else {
          actor->posZ = z;
        }
        if (spd > 0x118) {
          int k = 0;

          do {
            int a = (actor->rotZ + RandomIntInRange(-7, 7)) & 0xFF;
            CopyVector(f448, &actor->posX);
            k++;
            f448[2] += RandomIntInRange(0, 0x14);
            f448[3] = (g_anCosineLut[a] * 15) >> 10;
            f448[4] = (g_anSineLut[a] * 15) >> 10;
            f448[5] = 10;
            CopyVector(f432, &f448[3]);
            f432[2] = 2;
            LShiftVector3(f432, 3);
            AddVector(f448, f448, f432);
            f448[6] = spd;
            g_pfnLevelEmitParticles(1, 0x19, f448, 0);
          } while (k < 3);
        }
        last = st->mode < 2 ? st->path[0] - 1 : 0;
        if (r == 0x103 && st->mode == 0 && g_anLevelRunnerArrived[0] == 0) {
          st->path[1] = st->path[0] - 1;
          CopyVector(&actor->posX,
                        (int *)(st->path + (st->path[0] * 16 - 0x18)));
          SnapActorToGround(actor);
          actor->mode = 5;
          continue;
        }
        if (r == last + 0x100) {
          if (st->mode < 2) {
            st->path[1] = st->path[0] - 1;
            actor->mode = 5;
            continue;
          }
          st->range = 0;
          actor->mode = 3;
          continue;
        }
        if (r & 0x100) {
          switch (RUN_FLAGS(st->path) & 3) {
          case 1:
            st->lift = 0x17C;
            break;
          case 2:
            if (st->mode == 0) {
              st->lift = 0x104;
            } else if (st->mode == 1) {
              st->lift = 0xD2;
            }
            break;
          case 3:
            st->lift = 0xC8;
            break;
          }
        }
        continue;
      }

      case 5: {
        int r;

        ANIM_GO(actor, 0);
        r = SteerActorAroundPoint(
            actor, PATH_PT(st->path, st->path[1]), st->speed,
            (ArcTan2(spy->posX - PATH_X(st->path, st->path[1]),
                           spy->posY - PATH_Y(st->path, st->path[1]), 0) +
             0x80) &
                0xFF,
            0x1E, 0x64, 0xF, 0x20, 0xFF, 0xFF, 0, 0, 0);
        SnapActorToGround(actor);
        if (r < 0x23 && spy->state != 0xB && spy->state != 0x14) {
          if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
            actor->mode = 6;
            continue;
          }
          continue;
        }
        st->timer = 0x78;
        continue;
      }

      case 6: {
        int r;

        ANIM_GO(actor, 0);
        r = SteerActorAroundPoint(
            actor, PATH_PT(st->path, st->path[1]), st->speed,
            ArcTan2(spy->posX - PATH_X(st->path, st->path[1]),
                          spy->posY - PATH_Y(st->path, st->path[1]), 0),
            5, 0xC8, 0xF, 0x20, 0xFF, 0xFF, 0, 0, 0);
        SnapActorToGround(actor);
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0xFA0) {
          actor->mode = 5;
          continue;
        }
        if (g_nSpyroState == 0xB || g_nSpyroState == 0x14) {
          actor->mode = 5;
          continue;
        }
        if (r < 0x46) {
          actor->mode = 7;
          continue;
        }
        continue;
      }

      case 7:
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        ANIM_GO(actor, 4);
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x12C0) {
          actor->mode = 5;
          continue;
        }
        if (g_nSpyroState == 0xB || g_nSpyroState == 0x14) {
          actor->mode = 5;
          continue;
        }
        continue;

      case 10:
        ANIM_GO(actor, 4);
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (SPY_DZ(actor) < 0 &&
            ApproxDist2D(&actor->posX, (int *)(base - 8)) < 0x1964) {
          actor->mode = 11;
          continue;
        }
        continue;

      case 11:
        ANIM_GO(actor, 0);
        if (TurnActorYawTowardAngle(actor,
                          ArcTan2(PATH_X(st->home, 0) - actor->posX,
                                        PATH_Y(st->home, 0) - actor->posY, 0),
                          8, 0x14, 1) != 0) {
          if (ApproxDist2D(&actor->posX, PATH_PT(st->home, 0)) < 0x12C) {
            actor->mode = 3;
            continue;
          }
          MoveActorAlongHeading(actor, 0x118, 0, 0, 5);
        }
        continue;

      case 20:
        ANIM_GO(actor, 4);
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (IsPointInRotatedBox((int *)&g_anSpyroWorldPos, st->volA) != 0) {
          ENTER_POSE(actor, 0);
        }
        continue;
      }
      continue;
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
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x1000 &&
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
      SubtractVector(f480, &spy->posX, &actor->posX);
      distance = VectorLength(f480, 0);
      if (distance < st->radius && f480[2] >= -0x3FF && f480[2] < st->spanZ &&
          (SPY.state == 0x11 || SPY.posZ + 0xC00 < actor->posZ + st->spanZ)) {
        spyro = &SPY;
        spyro->controlFlags = 0x80000000;
        SPY.pScriptedActor = actor;
        SPY.damageFlags |= 0x800;
        SPY.u200 = actor->posZ + st->spanZ;
        CopyVector(&spy->contact, &actor->posX);
        if (st->flags & 1) {
          spyro->controlFlags |= 0x8000;
        } else {
          g_anCameraActiveTargetParams = g_anCameraParamMode2Front;
          SPY.pCamAnchor = &spy->posX;
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

        f432[0] = (GetRandomU32() & 0xFE) - 0x7F;
        f432[1] = (GetRandomU32() & 0xFE) - 0x7F;
        f432[2] = (GetRandomU32() & 0xFE) - 0x40;
        AddVector(f432, f432, &actor->posX);
        g_pfnLevelEmitParticles(1, 0x42, f432, (int *)1);
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
            f496[0] = (GetRandomU32() & 0xFE) - 0x7F;
            f496[1] = (GetRandomU32() & 0xFE) - 0x7F;
            f496[2] = (GetRandomU32() & 0xFE) - 0x40;
            AddVector(f496, f496, &actor->posX);
            g_pfnLevelEmitParticles(1, 0x42, f496, (int *)1);
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
          ZeroVector(f512);
          g_pfnLevelEmitParticles(1, 0, &st->prize->posX, f512);
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
            CopyVector(f528, &st->prize->posX);
            f528[0] += g_anCosineLut[j * 43] >> 6;
            f528[1] += g_anSineLut[j * 43] >> 6;
            f528[2] += 0x28;
            f544[0] = g_anCosineLut[j * 43] >> 7;
            f544[1] = g_anSineLut[j * 43] >> 7;
            f544[2] = 0x10;
            g_pfnLevelEmitParticles(1, 0, f528, f544);
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

    /* Type 390: spinning chest. Each charge spins it faster (with a rising
     * voice); spun fast enough, or flamed, it bursts. */
    case 390: {
      L26Spinner *st = actor->state;
      int pitch = SAMPLE_PITCH(g_apActorMeshTable[actor->type]->m_Sounds[1]);
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
        st->lid->cullRadius = 0x1A;
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

          if (a == actor || a->mode >= 0x80 || a->collision == 0 ||
              a->collision >= 0) {
            continue;
          }
          SubtractVector(f512, &a->posX, &actor->posX);
          d = ABS2(f512[0]) + ABS2(f512[1]);
          if (d < 0x6000 &&
              (unsigned int)(VectorLength(f512, 0) - 0x1000) <= 0x2000 &&
              (unsigned int)(f512[2] + 0x1000) <= 0x2000) {
            d = AbsAngleDelta12(ArcTan2(actor->posX - spy->posX,
                                            actor->posY - spy->posY, 1),
                              ArcTan2(f512[0], f512[1], 1));
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
        actor->cullRadius = 0x20;
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
        f528[0] = ((short)g_anCosineLut[i * 32] >> 8) * 3;
        f528[1] = ((short)g_anSineLut[i * 32] >> 8) * 3;
        f528[2] = 0x18;
        g_pfnLevelEmitParticles(1, 0, &actor->posX, f528);
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
          f528[0] = (g_anCosineLut[bearing] * 3) >> 4;
          f528[1] = (g_anSineLut[bearing] * 3) >> 4;
          f528[2] = 0;
          f560[0] = (unsigned short)g_anCosineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          f560[1] = (unsigned short)g_anSineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          f560[2] = 0;
          bearing = (bearing + 0x80) & 0xFF;
          actor->rotZ = bearing + ((g_anCosineLut[actor->subMode] * 3) >> 9);
          ScaleVector3Sat(f576, f560, st->radius - 1);
          LShiftVector3(f560, 1);
          arm = st->radius - 1;
          f576[2] += (g_anCosineLut[arm * 2] * 3) >> 3;
          f592[0] = f528[0] * g_anCosineLut[arm * 2];
          f592[1] = f528[1] * g_anCosineLut[arm * 2];
          f592[2] = 0;
          bearing = arm << 1;
          bearing = (bearing - (st->phase * 4)) & 0xFF;
          ScaleVector3Sat(f560, f560, st->phase);
          SubtractVector(f576, f576, f560);
          swing = &g_anCosineLut[bearing];
          actor->posX = f528[0] * *swing;
          actor->posY = f528[1] * *swing;
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, f592);
          RShiftVector3(&actor->posX, 0xA);
          AddVector(&actor->posX, &actor->posX, f528);
          AddVector(&actor->posX, &actor->posX, &st->parent->posX);
          SubtractVector(&actor->posX, &actor->posX, f576);
          actor->posZ = actor->posZ + ((*swing * 3) >> 3) + 0x600;
        }
      } else {
        unsigned char now = actor->subMode + 8;

        actor->subMode = now;
        actor->rotZ = actor->floorOffset + ((g_anCosineLut[now] * 3) >> 9);
      }

      break;
    }

    /* Type 453: roaming ram. Wanders its waypoint ring away from Spyro,
     * charges down its path once he is close, and is knocked back by hits
     * or by its partner. */
    case 453: {
      L18Ram *st = actor->state;

      if ((actor->flags & 0xF0000) && actor->mode != 2) {
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
        if (actor->nextAnim != 2) {
          g_nActorAnimDone = 0;
          actor->frameProgress = sixteen;
          actor->frameRate = sixteen;
          actor->nextAnim = 2;
          actor->nextFrame = 0;
          StubActorHook(actor);
        }
        actor->mode = 2;
        continue;
      }
      actor->flags = 0;
      switch (actor->mode) {
      case 0:
        if (TickCountdownByFrameDelta(&st->call, 4) != 0) {
          st->call = (GetRandomU32() & 0x1F) + 0x50;
          PlayActorMeshVoice(actor, (int)GetRandomU32() % 3, 0);
        }
        if (st->mode == 1 && ApproxDist2D(&actor->posX, &SPY.posX) < 0x1000) {
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

      case 2:
        DriveActorTrackedMove(actor, &st->speed, st->knock, &st->lift, 0xC, 0x10);
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x10);
          DespawnActorRecord(actor);
          continue;
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
}
  }
}
