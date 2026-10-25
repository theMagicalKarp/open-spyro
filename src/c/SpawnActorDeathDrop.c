#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "spyro_state.h"

/* The tumbling sub-record hung off the drop's ->state pointer. */
struct Debris {
  int vel[3];                        /* 0x00 */
  char pad0c[2];                     /* 0x0c */
  unsigned char live;                /* 0x0e */
  char pad0f[2];                     /* 0x0f */
  unsigned char spinX, spinY, spinZ; /* 0x11 */
};

/* Spawn the drop that a broken/flamed actor leaves behind (0x8003abc0,
   1308 bytes).  `src` is the actor that died; `kind` selects how the drop is
   thrown.  An actor whose dropKind (low 7 bits) is 1..0x7e drops that actor
   type, once (killGroup bit 7 latches it).  Any other actor rolls: 2 in 25
   an extra life (ACTOR_TYPE_EXTRA_LIFE); else, while g_nSparxHealth < 3,
   10 in 25 a butterfly (ACTOR_TYPE_BUTTERFLY), or Sparx himself when he is
   gone (no g_pSparxActor) and the health is not negative; else a Sparx food
   (ACTOR_TYPE_SPARX_FOOD).  Extra lives, butterflies and Sparx are returned
   as spawned.  Anything else is thrown: kinds 0/2/6 land on the source (or
   `alt`) position with a small XY jitter, kind 1 and any kind-3/4 that fails
   its Spyro proximity test scatter over a wider XY box, kind 5 drops in place
   with zero velocity, and kinds 3/4 that pass attach straight onto the source
   with a random tumble.  The thrown path probes the landing spot for ground
   and a walkable slope, integrates the ballistic arc at 10 units of gravity
   per frame to count the flight time, and divides the offset by that count to
   get the launch velocity. */
Actor *SpawnActorDeathDrop(Actor *src, int kind, int *hint, int *alt) {
  int land[4];   /* sp+0x18 */
  int throw_[4]; /* sp+0x28 */
  Actor *rec;
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

  id = src->dropKind & 0x7f;
  if ((unsigned int)(id - 1) < 0x7e) {
    if (src->killGroup & 0x80) {
      return 0;
    }
    src->killGroup |= 0x80;
  } else {
    int roll;
    id = ACTOR_TYPE_SPARX_FOOD;
    roll = (int)(((GetRandomU32() & 0xfff) * 25) >> 10);
    if (roll < 2) {
      id = ACTOR_TYPE_EXTRA_LIFE;
    } else if (g_nSparxHealth < 3 && roll < 0xc) {
      id = ACTOR_TYPE_BUTTERFLY;
    }
  }

  if (id == ACTOR_TYPE_BUTTERFLY) {
    if (g_pSparxActor != 0 || g_anSparxHealthBlock[0] < 0) {
      rec = (*(struct Actor * (*)(int, struct Actor *))
                 g_pfnLevelSpawnActor)(ACTOR_TYPE_BUTTERFLY, src);
    } else {
      rec = (*(struct Actor * (*)(int, struct Actor *))
                 g_pfnLevelSpawnActor)(ACTOR_TYPE_SPARX, src);
      PlayActorMeshVoice(rec, 0, 0);
      g_anSparxHealthBlock[0] = 1;
      g_pSparxActor = rec;
    }
    return rec;
  }

  if (id == ACTOR_TYPE_EXTRA_LIFE) {
    rec = (*(struct Actor * (*)(int, struct Actor *))
               g_pfnLevelSpawnActor)(ACTOR_TYPE_EXTRA_LIFE, src);
    dbg = (struct Debris *)rec->state;
    dbg->live = 1;
    return rec;
  }

  rec = (*(struct Actor * (*)(int, struct Actor *)) g_pfnLevelSpawnActor)(id,
                                                                          src);
  dbg = (struct Debris *)rec->state;

  if (kind == 0) {
    CopyVector(throw_, &src->posX);
    vz = 0x8c;
  } else if (kind == 6) {
    CopyVector(throw_, &src->posX);
    vz = 0x12c;
    throw_[0] -= 0x100 - (int)(GetRandomU32() & 0x1ff);
    throw_[1] -= 0x100 - (int)(GetRandomU32() & 0x1ff);
  } else if (kind == 1) {
    CopyVector(throw_, &src->posX);
    vz = 0x8c;
    throw_[0] -= 0x200 - (int)(GetRandomU32() & 0x3ff);
    throw_[1] -= 0x200 - (int)(GetRandomU32() & 0x3ff);
  } else if (kind == 2) {
    CopyVector(throw_, alt);
    vz = 0x8c;
  } else if (kind == 3 || kind == 4) {
    if ((kind == 4 || g_nSpyroState == SPYRO_ST_CHARGE ||
         g_nSpyroState == SPYRO_ST_SUPERCHARGE_AIR ||
         g_nSpyroState == SPYRO_ST_CHARGE_AIR ||
         g_nSpyroState == SPYRO_ST_SUPERCHARGE) &&
        (kind == 4 ||
         (ApproxDist2D(&rec->posX, g_anSpyroWorldPos) < 0x800 &&
          (d = rec->posZ - rec->floorOffset, e = d - g_nSpyroWorldPosZ,
           e > 0 ? e < 0x400 : g_nSpyroWorldPosZ - d < 0x400)))) {
      int *p = &rec->posX;
      CopyVector(p, &src->posX);
      rec->posZ += 0x100;
      CopyVector(dbg->vel, p);
      dbg->spinX = GetRandomU32() & 0xe;
      dbg->spinY = GetRandomU32() & 0xe;
      dbg->spinZ = GetRandomU32() & 0xe;
      rec->subMode = 3;
      rec->updateDist = 0xff;
      return rec;
    }
    CopyVector(throw_, &src->posX);
    vz = 0x8c;
    throw_[0] -= 0x200 - (int)(GetRandomU32() & 0x3ff);
    throw_[1] -= 0x200 - (int)(GetRandomU32() & 0x3ff);
  } else if (kind == 5) {
    CopyVector(&rec->posX, &src->posX);
    rec->posZ += 0x100;
    ZeroVector(dbg->vel);
    return rec;
  }

  if (hint != 0) {
    CopyVector(land, hint);
  } else {
    CopyVector(land, &src->posX);
    land[2] += 0x100;
  }

  throw_[2] += 0x400;
  hit = FindGroundHeightBelow(throw_, 0x800);
  throw_[2] -= 0x400;
  ang = (signed char)ArcTan2_8bit(g_anCollisionProbeVec[2],
                                  VectorLength(g_anCollisionProbeVec, 0));
  if (hit == 0 || ang >= 0x18 ||
      DispatchActorContactAtSphere(throw_, 0xc8, 0, 0, 0, 0) != 0) {
    CopyVector(throw_, &src->posX);
  }
  n = 0;

  z = land[2];
  v = vz;
  while (v > 0 || throw_[2] < z) {
    z += v;
    v -= 10;
    n += 1;
  }

  CopyVector(&rec->posX, land);
  SubtractVector(throw_, throw_, land);
  DivideVector3(throw_, throw_, n);
  throw_[2] = vz;
  CopyVector(dbg->vel, throw_);
  dbg->live = 1;
  return rec;
}
