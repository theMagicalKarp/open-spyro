/* Level12_SpawnActor (0x8008B2C0, level_12_magic_crafters_home
 * overlay, 0xBD8 bytes).
 *
 * Spawn one actor ("moby") of type `type`, parented to `parent`.
 *
 * Allocates a pool record (AllocActorRecordSlot), stamps the
 * type into ->type and the parent's pool index into ->poolIndex, then runs a per-type
 * init arm: every arm starts from InitActorRenderDefaults (InitActorRenderDefaults) and
 * ends by placing the new actor -- most copy the parent's position
 * (CopyVector) and either register it in the spatial grid
 * (InsertActorIntoSpatialGrid) or hide its render record (HideActorRenderRecord).  With no parent
 * the position falls back to Spyro's (g_anSpyroWorldPos).
 */

#include "rot_billboard.h"
#include "vecmath.h"
#include "level_globals.h"
#include "level_spawn_actor.h"
#include "actor.h"
#include "model.h"
#include "level_emit_particles.h"
#include "emit_record.h"


Actor *Level12_SpawnActor(int type, Actor *parent) {
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
    PickupState *st = rec->state;

    InitActorRenderDefaults(rec);
    st->vel[2] = PICKUP_DROP_POP_VZ;
    st->bounces = PICKUP_DROP_BOUNCES;
    st->sparkle = ROT_BILLBOARD_NONE;
    st->vel[0] = 0;
    st->vel[1] = 0;
    st->restMode = COLLREST_STILL;
    st->timer = 0;
    st->pitch = 0;
    st->yaw = 0;
    st->roll = 0;
    rec->subMode = COLLECT_BOUNCE;
    rec->cullRadius = CULL_RADIUS(0x1800);
    rec->updateDist = UPDATE_DIST(0x4000);
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
    rec->tintMode = TINT_WHITE;
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
    CopyVector(st->home, pos);
    st->climbTimer = 0;
    st->headingTimer = 0;
    st->offscreenTimer = BUTTERFLY_OFFSCREEN_FRAMES;
    break;
  }

  case ACTOR_TYPE_DRAGON_EGG:
    InitActorRenderDefaults(rec);
    rec->cullRadius = CULL_RADIUS(0x2000);
    rec->updateDist = ACTOR_UPDATE_ALWAYS;
    if (parent != 0) {
      CopyVector(&rec->posX, &parent->posX);
    }
    HideActorRenderRecord(rec);
    break;

  case ACTOR_TYPE_SPLIT_SHOT: {
    int *timer = rec->state;
    int *pos;
    int turn;

    InitActorRenderDefaults(rec);
    pos = &rec->posX;
    CopyVector(pos, &parent->posX);
    if (parent->type == type) {
      rec->rotZ = parent->rotZ;
      rec->mode = SPLITSHOT_FRAGMENT;
      rec->frameProgress = 0;
      rec->frameRate = g_apActorMeshTable[rec->type]->m_Animations[SPLITSHOT_FRAGMENT]->m_ProgressPerTick;
      rec->anim = SPLITSHOT_FRAGMENT;
      rec->nextAnim = SPLITSHOT_FRAGMENT;
      *timer = 3;
    } else if (parent->nextFrame >= 6) {
      rec->posX += LookupCosine(parent->rotZ << 4) >> 2;
      rec->posY += LookupSine(parent->rotZ << 4) >> 2;
      rec->posZ += 0x12C;
      rec->rotZ = parent->rotZ;
      turn = (ArcTan2(ApproxDist2D(pos, g_anSpyroWorldPos), g_nSpyroWorldPosZ - rec->posZ,
                            ATAN_ANGLE8) -
              parent->rotY) &
             ANGLE8_MASK;
      if (turn > ANGLE8_HALF) {
        turn -= ANGLE8_TURN;
      }
      if (turn < -0x10) {
        turn = -0x10;
      }
      if (turn > 0x10) {
        turn = 0x10;
      }
      rec->rotY = parent->rotY + turn;
      *timer = 0x80;
      rec->mode = SPLITSHOT_FLY;
      rec->frameProgress = 0;
      rec->frameRate = g_apActorMeshTable[rec->type]->m_Animations[SPLITSHOT_FLY]->m_ProgressPerTick;
      rec->anim = SPLITSHOT_FLY;
      rec->nextAnim = SPLITSHOT_FLY;
      g_pfnLevelEmitParticles(4, EMIT_DRIFT_SPARKLE, pos, 0x10);
      InsertActorIntoSpatialGrid(rec);
      break;
    } else {
      if (parent->nextFrame >= 2) {
        rec->posZ += 0x12C;
        rec->rotZ = parent->rotZ;
        rec->mode = SPLITSHOT_RAISE;
        rec->frameProgress = 0;
        rec->frameRate = g_apActorMeshTable[rec->type]->m_Animations[SPLITSHOT_RAISE]->m_ProgressPerTick;
        rec->anim = SPLITSHOT_RAISE;
        rec->nextAnim = SPLITSHOT_RAISE;
      } else {
        rec->posZ += 0x180;
        rec->rotZ = parent->rotZ;
      }
      *timer = 2;
    }
    InsertActorIntoSpatialGrid(rec);
    break;
  }
  case ACTOR_TYPE_EXTRA_LIFE:
  case ACTOR_TYPE_GEM_1 ... ACTOR_TYPE_GEM_25: {
    PickupState *st = rec->state;
    int *pos;
    int probe[3];

    InitActorRenderDefaults(rec);
    st->vel[0] = 0;
    st->vel[1] = 0;
    st->vel[2] = PICKUP_DROP_POP_VZ;
    st->restMode = COLLREST_STILL;
    st->timer = 0;
    st->pitch = 0;
    st->yaw = 0;
    st->roll = 0;
    if (parent->type == ACTOR_TYPE_FOLLOWER) {
      st->bounces = PICKUP_DROP_FOLLOWER_BOUNCES;
    } else {
      st->bounces = PICKUP_DROP_BOUNCES;
    }
    pos = &rec->posX;
    st->sparkle = ROT_BILLBOARD_NONE;
    rec->subMode = COLLECT_BOUNCE;
    rec->cullRadius = CULL_RADIUS(0x1800);
    rec->updateDist = UPDATE_DIST(0x10000);
    rec->rotX = 0x20;
    rec->rotY = 0;
    rec->rotZ = 0;
    CopyVector(pos, &parent->posX);
    HideActorRenderRecord(rec);
    rec->groundCode = -1;
    CopyVector(probe, pos);
    probe[2] += GROUND_PROBE_LIFT;
    FindGroundHeightBelow(probe, GROUND_PROBE_FAR);
    EncodeHitNormalToActorGroundCode(rec);
    rec->tint0 = 0;
    rec->tint1 = 0;
    rec->tint2 = 0;
    if (rec->type == ACTOR_TYPE_EXTRA_LIFE) {
      rec->tintMode = TINT_EXTRA_LIFE;
    }
    if (rec->type == ACTOR_TYPE_GEM_1) {
      rec->tintMode = TINT_GEM_1;
    }
    if (rec->type == ACTOR_TYPE_GEM_2) {
      rec->tintMode = TINT_GEM_2;
    }
    if (rec->type == ACTOR_TYPE_GEM_5) {
      rec->tintMode = TINT_GEM_5;
    }
    if (rec->type == ACTOR_TYPE_GEM_10) {
      rec->tintMode = TINT_GEM_10;
    }
    if (rec->type == ACTOR_TYPE_GEM_25) {
      rec->tintMode = TINT_GEM_25;
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

  case ACTOR_TYPE_SEED_POD_SHARD ... ACTOR_TYPE_SEED_POD_SHARD + 2:
  case ACTOR_TYPE_WOOD_CHEST_SHARD ... ACTOR_TYPE_WOOD_CHEST_SHARD + 2:
  case ACTOR_TYPE_SPARKLE_SHARD ... ACTOR_TYPE_WOOD_SHARD + 1:
  case ACTOR_TYPE_LIFE_CHEST_SHARD ... ACTOR_TYPE_LIFE_CHEST_SHARD + 2: {
    TumbleState *st = rec->state;
    int yaw;
    int pitch;

    InitActorRenderDefaults(rec);
    rec->cullRadius = CULL_RADIUS(0x2000);
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
    st->spin[0] = GetRandomU32() & 0xF;
    st->spin[1] = GetRandomU32() & 0xF;
    st->spin[2] = GetRandomU32() & 0xF;
    st->floorZ = parent->posZ - 0x40;
    st->life = 0x40 - (GetRandomU32() & 0xF);
    if ((unsigned int)((unsigned short)rec->type - ACTOR_TYPE_SPARKLE_SHARD) < 3) {
      *(int *)&rec->tint0 = ACTOR_TINT_NEUTRAL;
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
    rec->cullRadius = CULL_RADIUS(0x2000);
    rec->updateDist = ACTOR_UPDATE_ALWAYS;
    HideActorRenderRecord(rec);
    rec->tint0 = 0;
    rec->tint1 = 0;
    rec->tint2 = 0;
    rec->tintMode = TINT_STATUE_SHARD;
    if (g_nDragonRescueSubstate == DRAGON_RESCUE_TURN) {
      rec->scale = STATUE_SHARD_TURN_SCALE;
    } else if (g_nDragonRescueSubstate == DRAGON_RESCUE_SWEEP) {
      rec->scale = STATUE_SHARD_SWEEP_SCALE;
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
    st->life = (GetRandomU32() & 3) + STATUE_SHARD_LIFE;
    break;
  }

  case ACTOR_TYPE_GEM_DIGIT_0 ... ACTOR_TYPE_GEM_DIGIT_0 + 9:
  case ACTOR_TYPE_GLYPH_SLASH:
  case ACTOR_TYPE_GLYPH_PERIOD: {
    int *st = rec->state;

    InitActorRenderDefaults(rec);
    HideActorRenderRecord(rec);
    rec->tintMode = TINT_GOLD;
    rec->tint0 = 0;
    rec->tint1 = 0;
    rec->tint2 = 0;
    *st = 0x40;
    break;
  }

  case ACTOR_TYPE_SPIN_CHEST_LID:
    InitActorRenderDefaults(rec);
    rec->otBias = 5;
    if (parent != 0) {
      CopyVector(&rec->posX, &parent->posX);
    } else {
      CopyVector(&rec->posX, g_anSpyroWorldPos);
    }
    InsertActorIntoSpatialGrid(rec);
    break;

  case ACTOR_TYPE_PORTAL_GUIDE:
    InitActorRenderDefaults(rec);
    rec->cullRadius = ACTOR_CULL_ALWAYS;
    rec->posX = 0x1CC;
    rec->posY = 0x28;
    rec->posZ = 0x1000;
    HideActorRenderRecord(rec);
    rec->otBias = OT_BIAS_SCREEN;
    rec->tint0 = 0;
    rec->tint1 = 0;
    rec->tint2 = 0;
    rec->tintMode = 0;
    break;

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
    rec->posZ += HAZARD_FX_PROBE_RAISE;
    ground = FindGroundHeightBelow(&rec->posX, HAZARD_FX_SNAP_RANGE);
    z = rec->posZ;
    gap = ground - z;
    if (gap < 0) {
      gap = -gap;
    }
    if (gap < HAZARD_FX_SNAP_RANGE) {
      rec->posZ = ground;
    } else {
      rec->posZ = z - HAZARD_FX_PROBE_RAISE;
    }
    InsertActorIntoSpatialGrid(rec);
    break;
  }

  case ACTOR_TYPE_GLYPH_COMMA:
  case ACTOR_TYPE_LETTER_A ... ACTOR_TYPE_LETTER_A + 25:
    InitActorRenderDefaults(rec);
    rec->cullRadius = CULL_RADIUS(0x2000);
    rec->updateDist = ACTOR_UPDATE_ALWAYS;
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
