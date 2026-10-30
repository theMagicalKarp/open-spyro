/* Level29_SpawnActor (0x80082028, level_29_dream_weavers_icy_flight
 * overlay, 0xd6c bytes).
 *
 * Spawn one actor ("moby") of type `type`, parented to `parent`.
 *
 * Allocates a pool record (AllocActorRecordSlot), stamps the
 * type into +0x36 and the parent's pool index into +0x56, then runs a per-type
 * init arm: every arm starts from InitActorRenderDefaults (InitActorRenderDefaults) and
 * ends by placing the new actor -- most copy the parent's position
 * (CopyVector) and either register it in the spatial grid
 * (InsertActorIntoSpatialGrid) or hide its render record (HideActorRenderRecord).  With no parent
 * the position falls back to Spyro's (g_anSpyroWorldPos).
 *
 * Icy Flight is a flight level, so the arm set is the flight one shared with
 * levels 5/11/17/23 (thermal rings 0x104..0x10D, the race-object arms around
 * 0x159..0x15B and the archway/gate group 0x1AA..0x1C3), minus the
 * "attitude-copy" arms the other four carry.  Four arms are unique to it:
 *  - 0x4E: bare init, straight into the spatial grid.
 *  - 0xE8: spawn 0x400 above the parent, then aim at Spyro -- ArcTan2 of the
 *    horizontal offset (ArcTan2) fed through StepAndLerpAngle8 with Spyro's
 *    8-bit body yaw.
 *  - 0x167..0x169: a scatter burst on a random 8-bit heading, read out of the
 *    unit-circle tables (g_anCosineLut/g_anSineLut) and shifted down 7, with a
 *    coin-flipped +-0x5A vertical kick and randomised tumble rates.
 *  - 0x16A: launch off one of four diagonal quadrants (+-0x200, +-0x200,
 *    0x280) picked by two random bits, rotated into the parent's orientation
 *    (RotateVectorByMatrix) and added to the parent's position.
 */

#include "vecmath.h"
#include "level_globals.h"
#include "level_spawn_actor.h"
#include "actor.h"
#include "main_shared.h"
#include "level_emit_particles.h"

/* Spin-and-run helper state block (type 0xE8; SpinRunState in the updater). */
typedef struct AimState {
  int heading;   /* 0x00 */
  int zVel;      /* 0x04 */
  int moveSpeed; /* 0x08 */
  int timer;     /* 0x0C */
} AimState;



Actor *Level29_SpawnActor(int type, Actor *parent) {
  Actor *rec;
  int owner;

  rec = AllocActorRecordSlot();
  rec->type = type;
  if (parent == 0 || (unsigned int)(owner = parent - g_pActorListBase) >= 0x100) {
    owner = 0;
  }
  rec->poolIndex = owner;

  switch (type) {
  case 0xF: {
    PickupDropState *st = rec->state;

    InitActorRenderDefaults(rec);
    st->velZ = 0x8C;
    st->bounces = 3;
    st->sparkle = 0xFF;
    st->velX = 0;
    st->velY = 0;
    st->sub = 0;
    st->timer = 0;
    st->pitch = 0;
    st->yaw = 0;
    st->roll = 0;
    rec->subMode = 2;
    rec->cullRadius = 0x18;
    rec->updateDist = 0x10;
    rec->rotX = 0x20;
    rec->rotY = 0;
    rec->rotZ = 0;
    if (parent != 0) {
      CopyVector(&rec->posX, &parent->posX);
    }
    InsertActorIntoSpatialGrid(rec);
    rec->tint0 = 0;
    rec->tint1 = 0;
    rec->tint2 = 0;
    rec->tintMode = 1;
    rec->renderList |= 0x80;
    break;
  }

  case 0x10: {
    ButterflyState *st = rec->state;
    int *pos;

    InitActorRenderDefaults(rec);
    InsertActorIntoSpatialGrid(rec);
    pos = &rec->posX;
    CopyVector(pos, &parent->posX);
    rec->posZ += 0x200;
    CopyVector(st->anchor, pos);
    st->climbTimer = 0;
    st->headingTimer = 0;
    st->idleTimer = 0x708;
    break;
  }

  case 0x22:
    InitActorRenderDefaults(rec);
    rec->cullRadius = 0x20;
    rec->updateDist = 0xFF;
    if (parent != 0) {
      CopyVector(&rec->posX, &parent->posX);
    }
    HideActorRenderRecord(rec);
    break;

  case ACTOR_TYPE_RIDING_FLIGHT_TARGET:
    InitActorRenderDefaults(rec);
    InsertActorIntoSpatialGrid(rec);
    break;

  case 0xE:
  case 0x53 ... 0x57: {
    PickupDropState *st = rec->state;
    int *pos;
    int probe[3];

    InitActorRenderDefaults(rec);
    st->velX = 0;
    st->velY = 0;
    st->velZ = 0x8C;
    st->sub = 0;
    st->timer = 0;
    st->pitch = 0;
    st->yaw = 0;
    st->roll = 0;
    if (parent->type == 0xD) {
      st->bounces = 2;
    } else {
      st->bounces = 3;
    }
    pos = &rec->posX;
    st->sparkle = 0xFF;
    rec->subMode = 2;
    rec->cullRadius = 0x18;
    rec->updateDist = 0x40;
    rec->rotX = 0x20;
    rec->rotY = 0;
    rec->rotZ = 0;
    CopyVector(pos, &parent->posX);
    HideActorRenderRecord(rec);
    rec->groundCode = -1;
    CopyVector(probe, pos);
    probe[2] += 0x400;
    FindGroundHeightBelow(probe, 0x10000);
    EncodeCachedVecToActorDirCode(rec);
    rec->tint0 = 0;
    rec->tint1 = 0;
    rec->tint2 = 0;
    if (rec->type == ACTOR_TYPE_EXTRA_LIFE) {
      rec->tintMode = 0xC;
    }
    if (rec->type == ACTOR_TYPE_GEM_1) {
      rec->tintMode = 1;
    }
    if (rec->type == ACTOR_TYPE_GEM_2) {
      rec->tintMode = 2;
    }
    if (rec->type == ACTOR_TYPE_GEM_5) {
      rec->tintMode = 3;
    }
    if (rec->type == ACTOR_TYPE_GEM_10) {
      rec->tintMode = 4;
    }
    if (rec->type == ACTOR_TYPE_GEM_25) {
      rec->tintMode = 5;
    }
    break;
  }

  case 0x78: {
    SparxState *st = rec->state;

    InitActorRenderDefaults(rec);
    InsertActorIntoSpatialGrid(rec);
    rec->subMode = 0;
    st->timer = 0;
    st->drift[2] = 0;
    st->drift[1] = 0;
    st->drift[0] = 0;
    st->glow = 0;
    st->target = 0;
    if (parent != 0) {
      CopyVector(&rec->posX, &parent->posX);
    }
    break;
  }

  case ACTOR_TYPE_BOBBER_SPINOFF:
  {
    AimState *st = rec->state;

    InitActorRenderDefaults(rec);
    CopyVector(&rec->posX, &parent->posX);
    rec->posZ += 0x400;
    InsertActorIntoSpatialGrid(rec);
    st->zVel = 0xC8;
    st->moveSpeed = 0x15E;
    st->heading = StepAndLerpAngle8(
        ArcTan2(rec->posX - g_anSpyroWorldPos[0], rec->posY - g_nSpyroWorldPosY, 0),
        g_bSpyroPersistentEulerZ, 0x28, 0x40);
    st->timer = 0x78;
    break;
  }

  case 0x1DE ... 0x1E0: {
    FragmentState *st = rec->state;
    int yaw;
    int pitch;

    InitActorRenderDefaults(rec);
    rec->cullRadius = 0x20;
    CopyVector(&rec->posX, &parent->posX);
    InsertActorIntoSpatialGrid(rec);
    yaw = GetRandomU32() & 0xFFF;
    pitch = GetRandomU32() & 0x7FF;
    st->vel[0] = (LookupCosine(pitch) >> 5) * LookupCosine(yaw) >> 12;
    st->vel[1] = (LookupCosine(pitch) >> 5) * LookupSine(yaw) >> 12;
    st->vel[2] = LookupSine(pitch) >> 5;
    if (parent->flags & ACTOR_HIT_CHARGE) {
      st->vel[0] += g_anSpyroVelocity[0] >> 6;
      st->vel[1] += g_anSpyroVelocity[1] >> 6;
      st->vel[2] += g_anSpyroVelocity[2] >> 6;
    }
    rec->posX += st->vel[0] * 4;
    rec->posY += st->vel[1] * 4;
    rec->posZ += st->vel[2] * 4;
    st->spinX = GetRandomU32() & 0xF;
    st->spinY = GetRandomU32() & 0xF;
    st->spinZ = GetRandomU32() & 0xF;
    st->floorZ = parent->posZ - 0x40;
    st->life = 0x40 - (GetRandomU32() & 0xF);
    if ((unsigned int)((unsigned short)rec->type - 0x135) < 3) {
      *(int *)&rec->tint0 = 0xA18618;
      rec->renderList |= 0x80;
    }
    break;
  }

  case 0x1E1 ... 0x1E2: {
    FragmentState *st = rec->state;
    int yaw;
    int pitch;

    InitActorRenderDefaults(rec);
    rec->cullRadius = 0x20;
    CopyVector(&rec->posX, &parent->posX);
    InsertActorIntoSpatialGrid(rec);
    yaw = GetRandomU32() & 0xFFF;
    pitch = GetRandomU32() & 0x7FF;
    st->vel[0] = (LookupCosine(pitch) >> 5) * LookupCosine(yaw) >> 12;
    st->vel[1] = (LookupCosine(pitch) >> 5) * LookupSine(yaw) >> 12;
    st->vel[2] = LookupSine(pitch) >> 5;
    st->vel[0] += g_anSpyroVelocity[0] >> 6;
    st->vel[1] += g_anSpyroVelocity[1] >> 6;
    st->vel[2] += g_anSpyroVelocity[2] >> 6;
    rec->posX += st->vel[0] * 4;
    rec->posY += st->vel[1] * 4;
    rec->posZ += st->vel[2] * 4;
    st->spinX = GetRandomU32() & 0xF;
    st->spinY = GetRandomU32() & 0xF;
    st->spinZ = GetRandomU32() & 0xF;
    st->floorZ = parent->posZ - 0x40;
    st->life = 0x40 - (GetRandomU32() & 0xF);
    break;
  }

  case ACTOR_TYPE_TUMBLE_SHARD ... ACTOR_TYPE_TUMBLE_SHARD + 2:
  {
    FragmentState *st = rec->state;
    int seed;

    InitActorRenderDefaults(rec);
    rec->cullRadius = 0x20;
    CopyVector(&rec->posX, &parent->posX);
    InsertActorIntoSpatialGrid(rec);
    seed = GetRandomU32();
    rec->rotZ = seed;
    st->vel[0] = g_anCosineLut[(unsigned char)seed] >> 7;
    st->vel[1] = g_anSineLut[rec->rotZ] >> 7;
    if (GetRandomU32() & 1) {
      st->vel[2] = 0x5A;
    } else {
      st->vel[2] = -0x5A;
      rec->rotX = 0x80;
    }
    rec->posX += st->vel[0] * 4;
    rec->posY += st->vel[1] * 4;
    rec->posZ += st->vel[2] * 4;
    if (st->vel[2] < 0x14) {
      st->vel[2] = 0x14;
    }
    st->spinX = GetRandomU32() & 0xF;
    st->spinY = GetRandomU32() & 0xF;
    st->spinZ = GetRandomU32() & 0xF;
    st->floorZ = parent->posZ - 0x40;
    st->life = 0x40 - (GetRandomU32() & 0xF);
    break;
  }

  case ACTOR_TYPE_WRECK_EMBER:
  {
    FragmentState *st = rec->state;
    int *pst = parent->state;
    int dir[3];

    InitActorRenderDefaults(rec);
    rec->cullRadius = 0x20;
    InsertActorIntoSpatialGrid(rec);
    CopyVector((int *)st, pst + 2);
    st->spinY = 0;
    st->spinZ = 0;
    st->vel[2] += 0x40;
    st->floorZ = parent->posZ - 0x40;
    st->life = 0x40 - (GetRandomU32() & 0xF);
    switch ((int)(GetRandomU32() & 3)) {
    case 0:
      dir[0] = 0x200;
      dir[1] = 0x200;
      rec->rotZ = 0x40;
      st->spinX = 0x10;
      break;
    case 1:
      dir[0] = 0x200;
      dir[1] = -0x200;
      rec->rotZ = 0xC0;
      st->spinX = -0x10;
      break;
    case 2:
      dir[0] = -0x200;
      dir[1] = 0x200;
      rec->rotZ = 0x40;
      st->spinX = 0x10;
      break;
    case 3:
      dir[0] = -0x200;
      dir[1] = -0x200;
      rec->rotZ = 0xC0;
      st->spinX = -0x10;
      break;
    }
    rec->rotZ += parent->rotZ;
    dir[2] = 0x280;
    RotateVectorByMatrix(parent->mtx, dir, dir);
    AddVector(&rec->posX, dir, &parent->posX);
    return rec;
  }

  case 0x104 ... 0x10D:
  case 0x115:
  case 0x147: {
    int *st = rec->state;

    InitActorRenderDefaults(rec);
    HideActorRenderRecord(rec);
    rec->tintMode = 2;
    rec->tint0 = 0;
    rec->tint1 = 0;
    rec->tint2 = 0;
    *st = 0x40;
    break;
  }

  case 0x159 ... 0x15B:
    if (g_anWorldPersistentFlags[g_nActiveWorldIndex] != 0) {
      HideActorRenderRecord(rec);
      DespawnActorRecord(rec);
      rec = 0;
    } else {
      InitActorRenderDefaults(rec);
      if (parent != 0) {
        CopyVector(&rec->posX, &parent->posX);
      }
      rec->cullRadius = 0x40;
      HideActorRenderRecord(rec);
      rec->tint0 = 0;
      rec->tint1 = 0;
      rec->tint2 = 0;
      rec->tintMode = 2;
    }
    break;

  case 0x185:
  case 0x18A:
  case 0x1EA ... 0x1EB:
    InitActorRenderDefaults(rec);
    rec->cullRadius = 0;
    rec->posX = -0x64;
    rec->posY = 0x1E;
    rec->posZ = 0x1000;
    HideActorRenderRecord(rec);
    rec->otBias = 0x20;
    rec->tint0 = 0;
    rec->tint1 = 0;
    rec->tint2 = 0;
    rec->tintMode = 0;
    break;

  case ACTOR_TYPE_FLAME_FLIGHT_TARGET_SPENT:
    InitActorRenderDefaults(rec);
    rec->cullRadius = 0xFF;
    rec->posX = 0x1CC;
    rec->posY = 0x28;
    rec->posZ = 0x1000;
    HideActorRenderRecord(rec);
    rec->otBias = 0x20;
    rec->tint0 = 0;
    rec->tint1 = 0;
    rec->tint2 = 0;
    rec->tintMode = 0;
    break;

  case ACTOR_TYPE_SPLASH:
  case ACTOR_TYPE_HAZARD0_DEATH_FX:
  case ACTOR_TYPE_HAZARD0_ENTER_FX: {
    int ground;
    int z;
    int gap;
    InitActorRenderDefaults(rec);
    if (parent != 0) {
      CopyVector(&rec->posX, &parent->posX);
    } else {
      CopyVector(&rec->posX, g_anSpyroWorldPos);
    }
    rec->posZ += 0x200;
    ground = FindGroundHeightBelow(&rec->posX, 0x800);
    z = rec->posZ;
    gap = ground - z;
    if (gap < 0) {
      gap = -gap;
    }
    if (gap < 0x800) {
      rec->posZ = ground;
    } else {
      rec->posZ = z - 0x200;
    }
    InsertActorIntoSpatialGrid(rec);
    break;
  }

  case 0x4C:
  case 0x1AA ... 0x1C3:
    InitActorRenderDefaults(rec);
    rec->cullRadius = 0x20;
    rec->updateDist = 0xFF;
    HideActorRenderRecord(rec);
    break;

  default:
    InitActorRenderDefaults(rec);
    if (parent != 0) {
      CopyVector(&rec->posX, &parent->posX);
    } else {
      CopyVector(&rec->posX, g_anSpyroWorldPos);
    }
    InsertActorIntoSpatialGrid(rec);
    break;
  }

  return rec;
}
