/* Level31_SpawnActor (0x80085664, level_31_gnastys_world_gnorc_cove
 * overlay, 0xAE0 bytes).
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
 */

#include "vecmath.h"
#include "level_globals.h"
#include "level_spawn_actor.h"
#include "actor.h"
#include "level_emit_particles.h"

/* Barrel state block (types 0xCA / 0xCF; FloatingBarrelState in the updater). */
typedef struct SeekState {
  short pad00;
  short blast;  /* 0x02 blast pushed Spyro */
  Actor *owner; /* 0x04 rider */
  int pad08;
  int sink;     /* 0x0C sink speed */
  int link;     /* 0x10 index into g_pActorListBase, -1 = none */
  short handle; /* 0x14 for SelectActorLodBand */
} SeekState;



Actor *Level31_SpawnActor(int type, Actor *parent) {
  Actor *rec;
  int owner;

  rec = AllocActorRecordSlot();
  rec->type = type;
  if (parent == 0 || (unsigned int)(owner = parent - g_pActorListBase) >= 0x100) {
    owner = 0;
  }
  rec->poolIndex = owner;

  switch (type) {
  case ACTOR_TYPE_SPARX_FOOD: {
    PickupDropState *st = rec->state;

    InitActorRenderDefaults(rec);
    st->velZ = 0x8C;
    st->bounces = 3;
    st->sparkle = 0xFF;
    st->velX = 0;
    st->velY = 0;
    st->restMode = COLLREST_STILL;
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
    rec->renderList |= ACTOR_RENDER_SECONDARY;
    break;
  }

  case ACTOR_TYPE_BUTTERFLY: {
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

  case ACTOR_TYPE_DRAGON_EGG:
    InitActorRenderDefaults(rec);
    rec->cullRadius = 0x20;
    rec->updateDist = 0xFF;
    if (parent != 0) {
      CopyVector(&rec->posX, &parent->posX);
    }
    HideActorRenderRecord(rec);
    break;

  case ACTOR_TYPE_EXTRA_LIFE:
  case ACTOR_TYPE_GEM_1 ... ACTOR_TYPE_GEM_25: {
    PickupDropState *st = rec->state;
    int *pos;
    int probe[3];

    InitActorRenderDefaults(rec);
    st->velX = 0;
    st->velY = 0;
    st->velZ = 0x8C;
    st->restMode = COLLREST_STILL;
    st->timer = 0;
    st->pitch = 0;
    st->yaw = 0;
    st->roll = 0;
    if (parent->type == ACTOR_TYPE_FOLLOWER) {
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

  case ACTOR_TYPE_SPARX: {
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

  case ACTOR_TYPE_BOULDER_DEBRIS + 1 ... ACTOR_TYPE_BOULDER_DEBRIS + 2:
    InitActorRenderDefaults(rec);
    rec->cullRadius = 0x20;
    rec->updateDist = 0xFF;
    HideActorRenderRecord(rec);
    rec->tint0 = 0;
    rec->tint1 = 0;
    rec->tint2 = 0;
    rec->tintMode = 0xD;
    break;

  case ACTOR_TYPE_FLOATING_BARREL: {
    SeekState *st = rec->state;
    int *pos;
    int probe[3];

    InitActorRenderDefaults(rec);
    pos = &rec->posX;
    CopyVector(pos, &parent->posX);
    st->sink = -0xC;
    st->owner = parent;
    st->blast = 0;
    st->link = -1;
    rec->floorOffset = 0x1CD;
    InsertActorIntoSpatialGrid(rec);
    rec->renderList = 0x10;
    rec->cullRadius = 0x20;
    rec->groundCode = -1;
    CopyVector(probe, pos);
    probe[2] += 0x400;
    FindGroundHeightBelow(probe, 0x10000);
    EncodeCachedVecToActorDirCode(rec);
    break;
  }

  case ACTOR_TYPE_FLOATING_MINE: {
    SeekState *st = rec->state;
    int *pos;
    int probe[3];

    InitActorRenderDefaults(rec);
    pos = &rec->posX;
    CopyVector(pos, &parent->posX);
    st->sink = -0xC;
    st->owner = parent;
    st->blast = 0;
    st->handle = 0;
    st->link = -1;
    rec->floorOffset = 0x1CD;
    InsertActorIntoSpatialGrid(rec);
    rec->renderList = 0x90;
    rec->cullRadius = 0x20;
    *(int *)&rec->tint0 = 0xA18618;
    rec->groundCode = -1;
    CopyVector(probe, pos);
    probe[2] += 0x400;
    FindGroundHeightBelow(probe, 0x10000);
    EncodeCachedVecToActorDirCode(rec);
    break;
  }

  case ACTOR_TYPE_SEED_POD_SHARD ... ACTOR_TYPE_SEED_POD_SHARD + 2:
  case ACTOR_TYPE_BOULDER_DEBRIS:
  case ACTOR_TYPE_WOOD_CHEST_SHARD ... ACTOR_TYPE_WOOD_CHEST_SHARD + 2:
  case ACTOR_TYPE_SPARKLE_SHARD ... ACTOR_TYPE_WOOD_SHARD + 1:
  case ACTOR_TYPE_LIFE_CHEST_SHARD ... ACTOR_TYPE_LIFE_CHEST_SHARD + 2: {
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
      rec->renderList |= ACTOR_RENDER_SECONDARY;
    }
    break;
  }

  case ACTOR_TYPE_STATUE_SHARD: {
    BurstShardState *st = rec->state;
    int dir[3];
    int unused[2];
    int i;

    InitActorRenderDefaults(rec);
    rec->cullRadius = 0x20;
    rec->updateDist = 0xFF;
    HideActorRenderRecord(rec);
    rec->tint0 = 0;
    rec->tint1 = 0;
    rec->tint2 = 0;
    rec->tintMode = 0xE;
    if (g_nDragonRescueSubstate == DRAGON_RESCUE_TURN) {
      rec->scale = 0x14;
    } else if (g_nDragonRescueSubstate == DRAGON_RESCUE_SWEEP) {
      rec->scale = 0x30;
    }
    i = GetRandomU32() & 7;
    dir[0] = g_anLaunchDirTable[i * 2];
    dir[1] = 0;
    dir[2] = g_anLaunchDirTableZ[i * 2];
    RotateVectorByMatrix(parent->mtx, dir, dir);
    dir[0] -= 0x3F - (GetRandomU32() & 0x7F);
    dir[1] -= 0x3F - (GetRandomU32() & 0x7F);
    dir[2] -= 0x3F - (GetRandomU32() & 0x7F);
    AddVector(&rec->posX, &parent->posX, dir);
    CopyVector(st->vel, dir);
    RShiftVector3(st->vel, 2);
    st->vel[0] -= 0x7F - (GetRandomU32() & 0xFF);
    st->vel[1] -= 0x7F - (GetRandomU32() & 0xFF);
    st->vel[2] -= 0x7F - (GetRandomU32() & 0xFF);
    rec->rotX = GetRandomU32();
    rec->rotY = GetRandomU32();
    rec->rotZ = GetRandomU32();
    st->spinX = GetRandomU32() & 0xF;
    st->spinY = GetRandomU32() & 0xF;
    st->spinZ = GetRandomU32() & 0xF;
    st->floorZ = parent->posZ;
    st->life = (GetRandomU32() & 3) + 0x10;
    break;
  }

  case ACTOR_TYPE_GEM_DIGIT_0 ... ACTOR_TYPE_GEM_DIGIT_0 + 9:
  case ACTOR_TYPE_GLYPH_SLASH:
  case ACTOR_TYPE_GLYPH_PERIOD: {
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

  case ACTOR_TYPE_HAZARD1_ENTER_FX ... ACTOR_TYPE_HAZARD1_DEATH_FX:
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

  case ACTOR_TYPE_GLYPH_COMMA:
  case ACTOR_TYPE_LETTER_A ... ACTOR_TYPE_LETTER_A + 25:
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
