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
   type, once (killGroup bit 7 latches it).  Any other actor rolls 0..99: 2%
   an extra life (ACTOR_TYPE_EXTRA_LIFE); else, while g_nSparxHealth <
   SPARX_GOLD, 10% a butterfly (ACTOR_TYPE_BUTTERFLY), or Sparx himself
   when he is gone (no g_pSparxActor) and the health is not negative; else a
   Sparx food (ACTOR_TYPE_SPARX_FOOD).  Extra lives, butterflies and Sparx are
   returned as spawned.  Anything else is thrown by `kind` (enum DropThrow,
   actor_type.h): DROP_HOP / DROP_AT_POINT land on the source (or `alt`)
   position, DROP_HIGH_HOP with a small XY jitter, DROP_SCATTER and a
   DROP_TUMBLE_IF_NEAR that fails its Spyro proximity test scatter over a
   wider XY box, DROP_IN_PLACE drops in place with zero velocity, and
   DROP_TUMBLE (or a passing DROP_TUMBLE_IF_NEAR) rides off the source with a
   random tumble.  The thrown path probes the landing spot for ground
   and a walkable slope, integrates the ballistic arc at DROP_GRAVITY
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
    if (src->killGroup & ACTOR_KILL_DROPPED) {
      return 0;
    }
    src->killGroup |= ACTOR_KILL_DROPPED;
  } else {
    int roll;
    id = ACTOR_TYPE_SPARX_FOOD;
    roll = (int)(((GetRandomU32() & 0xfff) * 25) >> 10); /* 0..99 */
    if (roll < DROP_LIFE_PERCENT) {
      id = ACTOR_TYPE_EXTRA_LIFE;
    } else if (g_nSparxHealth < SPARX_GOLD && roll < DROP_BUTTERFLY_PERCENT) {
      id = ACTOR_TYPE_BUTTERFLY;
    }
  }

  if (id == ACTOR_TYPE_BUTTERFLY) {
    if (g_pSparxActor != 0 || g_anSparxHealthBlock[0] < SPARX_GONE) {
      rec = g_pfnLevelSpawnActor(ACTOR_TYPE_BUTTERFLY, src);
    } else {
      rec = g_pfnLevelSpawnActor(ACTOR_TYPE_SPARX, src);
      PlayActorMeshVoice(rec, SPARX_VOICE_BUZZ, 0);
      g_anSparxHealthBlock[0] = SPARX_GREEN;
      g_pSparxActor = rec;
    }
    return rec;
  }

  if (id == ACTOR_TYPE_EXTRA_LIFE) {
    rec = g_pfnLevelSpawnActor(ACTOR_TYPE_EXTRA_LIFE, src);
    dbg = (struct Debris *)rec->state;
    dbg->live = 1;
    return rec;
  }

  rec = g_pfnLevelSpawnActor(id, src);
  dbg = (struct Debris *)rec->state;

  if (kind == DROP_HOP) {
    CopyVector(throw_, &src->posX);
    vz = DROP_HOP_VZ;
  } else if (kind == DROP_HIGH_HOP) {
    CopyVector(throw_, &src->posX);
    vz = DROP_HIGH_HOP_VZ;
    throw_[0] -= DROP_HIGH_HOP_JITTER -
                 (int)(GetRandomU32() & (2 * DROP_HIGH_HOP_JITTER - 1));
    throw_[1] -= DROP_HIGH_HOP_JITTER -
                 (int)(GetRandomU32() & (2 * DROP_HIGH_HOP_JITTER - 1));
  } else if (kind == DROP_SCATTER) {
    CopyVector(throw_, &src->posX);
    vz = DROP_HOP_VZ;
    throw_[0] -= DROP_SCATTER_JITTER -
                 (int)(GetRandomU32() & (2 * DROP_SCATTER_JITTER - 1));
    throw_[1] -= DROP_SCATTER_JITTER -
                 (int)(GetRandomU32() & (2 * DROP_SCATTER_JITTER - 1));
  } else if (kind == DROP_AT_POINT) {
    CopyVector(throw_, alt);
    vz = DROP_HOP_VZ;
  } else if (kind == DROP_TUMBLE_IF_NEAR || kind == DROP_TUMBLE) {
    if ((kind == DROP_TUMBLE || g_nSpyroState == SPYRO_ST_CHARGE ||
         g_nSpyroState == SPYRO_ST_SUPERCHARGE_AIR ||
         g_nSpyroState == SPYRO_ST_CHARGE_AIR ||
         g_nSpyroState == SPYRO_ST_SUPERCHARGE) &&
        (kind == DROP_TUMBLE ||
         (ApproxDist2D(&rec->posX, g_anSpyroWorldPos) < DROP_TUMBLE_RANGE &&
          (d = rec->posZ - rec->floorOffset, e = d - g_nSpyroWorldPosZ,
           e > 0 ? e < DROP_TUMBLE_DZ
                 : g_nSpyroWorldPosZ - d < DROP_TUMBLE_DZ)))) {
      int *p = &rec->posX;
      CopyVector(p, &src->posX);
      rec->posZ += DROP_LIFT;
      CopyVector(dbg->vel, p);
      dbg->spinX = GetRandomU32() & 0xe;
      dbg->spinY = GetRandomU32() & 0xe;
      dbg->spinZ = GetRandomU32() & 0xe;
      rec->subMode = COLLECT_HOMING;
      rec->updateDist = ACTOR_UPDATE_ALWAYS;
      return rec;
    }
    CopyVector(throw_, &src->posX);
    vz = DROP_HOP_VZ;
    throw_[0] -= DROP_SCATTER_JITTER -
                 (int)(GetRandomU32() & (2 * DROP_SCATTER_JITTER - 1));
    throw_[1] -= DROP_SCATTER_JITTER -
                 (int)(GetRandomU32() & (2 * DROP_SCATTER_JITTER - 1));
  } else if (kind == DROP_IN_PLACE) {
    CopyVector(&rec->posX, &src->posX);
    rec->posZ += DROP_LIFT;
    ZeroVector(dbg->vel);
    return rec;
  }

  if (hint != 0) {
    CopyVector(land, hint);
  } else {
    CopyVector(land, &src->posX);
    land[2] += DROP_LIFT;
  }

  throw_[2] += GROUND_PROBE_LIFT;
  hit = FindGroundHeightBelow(throw_, 0x800);
  throw_[2] -= 0x400;
  ang = (signed char)ArcTan2_8bit(
      g_anCollisionHitNormal[2],
      VectorLength(g_anCollisionHitNormal, VEC_LEN_XY));
  if (hit == 0 || ang >= DROP_MAX_SLOPE ||
      DispatchActorContactAtSphere(throw_, DROP_CLEAR_RADIUS, 0, 0, 0,
                                   CONTACT_STOP_AT_SOLID) != 0) {
    CopyVector(throw_, &src->posX);
  }
  n = 0;

  z = land[2];
  v = vz;
  while (v > 0 || throw_[2] < z) {
    z += v;
    v -= DROP_GRAVITY;
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
