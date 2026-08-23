#include "globals.h"

extern unsigned int GetRandomU32(void);
extern int ApproxDist2D(int *a, int *b);
extern int FindGroundHeightBelow(int *pt, int z);
extern int VectorLength(int *vec, int include_z);
extern signed char ArcTan2_8bit(int a, int b);
extern int DispatchActorContactAtSphere(int *pos, int group, int a, int b,
                                        int *actor, int c);
extern void func_8003851C(void *rec, int a, int b);
extern void func_800177F8(int *dst, int *src, int n);

extern int g_nSpyroWorldPosZ;         /* 0x80078a60 — g_anSpyroWorldPos[2] */
extern int g_anLevelReadyFlagBlock[]; /* 0x80078bbc — g_nLevelReadyFlag */

/* The tumbling sub-record hung off an actor's +0x00 pointer. */
struct Debris {
  int vel[3];                        /* 0x00 */
  char pad0c[2];                     /* 0x0c */
  unsigned char live;                /* 0x0e */
  char pad0f[2];                     /* 0x0f */
  unsigned char spinX, spinY, spinZ; /* 0x11 */
};

struct Actor {
  struct Debris *debris; /* 0x00 */
  char pad04[8];         /* 0x04 */
  int pos[3];            /* 0x0c */
  char pad18[0x20];      /* 0x18 */
  short zbase;           /* 0x38 */
  unsigned char flags;   /* 0x3a */
  char pad3b[0xe];       /* 0x3b */
  unsigned char state;   /* 0x49 */
  char pad4a[8];         /* 0x4a */
  unsigned char alpha;   /* 0x52 */
  unsigned char kind;    /* 0x53 */
};

/* Spawn the drop that a broken/flamed actor leaves behind (0x8003abc0,
   1308 bytes).  `src` is the actor that died; `kind` selects how the drop is
   thrown.  Actors whose id byte is in 1..0x7e drop a fixed item and are
   latched so they only ever drop once; everything else rolls a gem colour
   (0xf normal, 0xe on a 2-in-25 roll, 0x10 for the "extra life" band while the
   level is still loading).  The 0x10 case doubles as the one-shot key/egg
   record: while none is registered yet and g_nLevelReadyFlag is non-negative
   the drop is built as id 0x78 instead, seeded by func_8003851C, latched into
   g_pLevelOverlayInitRecord and the flag knocked down to 1.
   Otherwise a record is allocated through g_pfnLevelOverlayInitHook and thrown:
   kinds 0/2/6 land on the source (or `alt`) position with a small XY jitter,
   kind 1 and any kind-3/4 that fails its Spyro proximity test scatter over a
   wider XY box, kind 5 drops in place with zero velocity, and kinds 3/4 that
   pass attach straight onto the source with a random tumble.  The thrown path
   probes the landing spot for ground and a walkable slope, integrates the
   ballistic arc at 10 units of gravity per frame to count the flight time, and
   divides the offset by that count to get the launch velocity. */
struct Actor *func_8003ABC0(struct Actor *src, int kind, int *hint, int *alt) {
  int land[4];   /* sp+0x18 */
  int throw_[4]; /* sp+0x28 */
  struct Actor *rec;
  int vz;
  struct Debris *dbg;
  int ang;
  int id;
  int n;
  int hit;
  int z;
  int v;
  int d;
  int e;

  id = src->kind & 0x7f;
  if ((unsigned int)(id - 1) < 0x7e) {
    if (src->flags & 0x80) {
      return 0;
    }
    src->flags |= 0x80;
  } else {
    int roll;
    id = 0xf;
    roll = (int)(((GetRandomU32() & 0xfff) * 25) >> 10);
    if (roll < 2) {
      id = 0xe;
    } else if (g_nLevelReadyFlag < 3 && roll < 0xc) {
      id = 0x10;
    }
  }

  if (id == 0x10) {
    if (g_pLevelOverlayInitRecord != 0 || g_anLevelReadyFlagBlock[0] < 0) {
      rec = (*(struct Actor * (*)(int, struct Actor *))
                 g_pfnLevelOverlayInitHook)(0x10, src);
    } else {
      rec = (*(struct Actor * (*)(int, struct Actor *))
                 g_pfnLevelOverlayInitHook)(0x78, src);
      func_8003851C(rec, 0, 0);
      g_anLevelReadyFlagBlock[0] = 1;
      g_pLevelOverlayInitRecord = rec;
    }
    return rec;
  }

  if (id == 0xe) {
    rec = (*(struct Actor * (*)(int, struct Actor *))
               g_pfnLevelOverlayInitHook)(0xe, src);
    dbg = rec->debris;
    dbg->live = 1;
    return rec;
  }

  rec = (*(struct Actor * (*)(int, struct Actor *))
             g_pfnLevelOverlayInitHook)(id, src);
  dbg = rec->debris;

  if (kind == 0) {
    CopyVector(throw_, src->pos);
    vz = 0x8c;
  } else if (kind == 6) {
    CopyVector(throw_, src->pos);
    vz = 0x12c;
    throw_[0] -= 0x100 - (int)(GetRandomU32() & 0x1ff);
    throw_[1] -= 0x100 - (int)(GetRandomU32() & 0x1ff);
  } else if (kind == 1) {
    CopyVector(throw_, src->pos);
    vz = 0x8c;
    throw_[0] -= 0x200 - (int)(GetRandomU32() & 0x3ff);
    throw_[1] -= 0x200 - (int)(GetRandomU32() & 0x3ff);
  } else if (kind == 2) {
    CopyVector(throw_, alt);
    vz = 0x8c;
  } else if (kind == 3 || kind == 4) {
    if ((kind == 4 || g_nSpyroState == 0xb || g_nSpyroState == 0x18 ||
         g_nSpyroState == 0x14 || g_nSpyroState == 0x2c) &&
        (kind == 4 || (ApproxDist2D(rec->pos, g_anSpyroWorldPos) < 0x800 &&
                       (d = rec->pos[2] - rec->zbase, e = d - g_nSpyroWorldPosZ,
                        e > 0 ? e < 0x400 : g_nSpyroWorldPosZ - d < 0x400)))) {
      int *p = rec->pos;
      CopyVector(p, src->pos);
      rec->pos[2] += 0x100;
      CopyVector(dbg->vel, p);
      dbg->spinX = GetRandomU32() & 0xe;
      dbg->spinY = GetRandomU32() & 0xe;
      dbg->spinZ = GetRandomU32() & 0xe;
      rec->state = 3;
      rec->alpha = 0xff;
      return rec;
    }
    CopyVector(throw_, src->pos);
    vz = 0x8c;
    throw_[0] -= 0x200 - (int)(GetRandomU32() & 0x3ff);
    throw_[1] -= 0x200 - (int)(GetRandomU32() & 0x3ff);
  } else if (kind == 5) {
    CopyVector(rec->pos, src->pos);
    rec->pos[2] += 0x100;
    ZeroVector(dbg->vel);
    return rec;
  }

  if (hint != 0) {
    CopyVector(land, hint);
  } else {
    CopyVector(land, src->pos);
    land[2] += 0x100;
  }

  throw_[2] += 0x400;
  hit = FindGroundHeightBelow(throw_, 0x800);
  throw_[2] -= 0x400;
  ang = ArcTan2_8bit(g_anCollisionProbeVec[2],
                     VectorLength(g_anCollisionProbeVec, 0));
  if (hit == 0 || ang >= 0x18 ||
      DispatchActorContactAtSphere(throw_, 0xc8, 0, 0, 0, 0) != 0) {
    CopyVector(throw_, src->pos);
  }
  n = 0;

  z = land[2];
  v = vz;
  while (v > 0 || throw_[2] < z) {
    z += v;
    v -= 10;
    n += 1;
  }

  CopyVector(rec->pos, land);
  SubtractVector(throw_, throw_, land);
  func_800177F8(throw_, throw_, n);
  throw_[2] = vz;
  CopyVector(dbg->vel, throw_);
  dbg->live = 1;
  return rec;
}
