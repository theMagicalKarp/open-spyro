/* Level14_SpawnActor (0x8008883C, level_14_magic_crafters_high_caves
 * overlay, 0xC18 bytes).
 *
 * Spawn one actor ("moby") of type `type`, parented to `parent`.
 *
 * Allocates a pool record (AllocActorRecordSlot), stamps the
 * type into ->type and the parent's pool index into ->poolIndex (the 0x58-byte-record
 * pointer difference, clamped away when it is not a real pool slot), then runs
 * a per-type init arm. Every arm starts from InitActorRenderDefaults
 * (InitActorRenderDefaults) and ends by placing the new actor: most copy the
 * parent's position (CopyVector) and either register it in the
 * spatial grid (InsertActorIntoSpatialGrid) or hide its render record (HideActorRenderRecord).
 *
 * The interesting arms:
 *  - 0xE / 0x53..0x57 (collectible drops: life statue, life orb, gems): fill the pickup state block,
 *    take a ground probe (FindGroundHeightBelow) 0x400 above the spawn point, encode
 *    the cached direction (EncodeHitNormalToActorGroundCode) and select the per-type
 *    tintMode preset (0xE: 0xC, 0x53..0x57: 1..5).
 *  - 0x43..0x45 / 0xFF..0x101 / 0x135..0x137 (debris bursts):
 *    give the fragment a random unit velocity built from a random yaw
 *    (0xFFF) and pitch (0x7FF) through LookupCosine/LookupSine, add a sixth
 *    of Spyro's velocity (g_anSpyroVelocity..54) when the parent is flagged 0x20000,
 *    nudge the spawn point along it, and randomise the tumble rates. The
 *    0x135..0x137 sub-range additionally gets the 0xA18618 tint and the
 *    ACTOR_RENDER_SECONDARY renderList bit.
 *  - 0xFB (dragon-statue burst shard, ticked by TickStatueBurstShard): pick one of eight launch directions from
 *    the g_anLaunchDirTable/g_anLaunchDirTableZ table, rotate it into the parent's orientation
 *    (RotateVectorByMatrix), jitter each component by
 *    +-0x3F, offset the spawn point along it (AddVector) and
 *    seed the shard's velocity (RShiftVector3) with a further
 *    +-0x7F of jitter plus randomised spin rates and life.
 *  - 0x16B..0x16C: drop 0x200 and snap to the ground below when it is within
 *    0x800, otherwise back off by 0x200.
 *
 * With no parent the position falls back to Spyro's (g_anSpyroWorldPos).
 */

#include "rot_billboard.h"
#include "vecmath.h"
#include "level_globals.h"
#include "level_spawn_actor.h"
#include "actor.h"

extern volatile int g_anSpyroWorldPos[];
extern volatile int g_nSpyroWorldPosY;     /* g_anSpyroWorldPos[1] */

Actor *Level14_SpawnActor(int type, Actor *parent) {
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

  case ACTOR_TYPE_BOULDER_DEBRIS + 1 ... ACTOR_TYPE_BOULDER_DEBRIS + 2:
    InitActorRenderDefaults(rec);
    rec->cullRadius = CULL_RADIUS(0x2000);
    rec->updateDist = ACTOR_UPDATE_ALWAYS;
    HideActorRenderRecord(rec);
    rec->tint0 = 0;
    rec->tint1 = 0;
    rec->tint2 = 0;
    rec->tintMode = TINT_BOULDER_SPARK;
    break;

  case ACTOR_TYPE_SEED_POD_SHARD ... ACTOR_TYPE_SEED_POD_SHARD + 2:
  case ACTOR_TYPE_BOULDER_DEBRIS:
  case ACTOR_TYPE_WOOD_CHEST_SHARD ... ACTOR_TYPE_WOOD_CHEST_SHARD + 2:
  case ACTOR_TYPE_SPARKLE_SHARD ... ACTOR_TYPE_WOOD_SHARD + 1: {
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

  case ACTOR_TYPE_HOMING_WISP: {
    WispState *st = rec->state;
    int turn;
    int dx;
    int dz;
    int dist;

    InitActorRenderDefaults(rec);
    rec->posX = parent->posX + (g_anCosineLut[parent->rotZ] >> 1);
    rec->posY = parent->posY + (g_anSineLut[parent->rotZ] >> 1);
    rec->posZ = parent->posZ + 0x300;
    dx = g_anSpyroWorldPos[0] - rec->posX;
    turn = (ArcTan2(dx, g_nSpyroWorldPosY - rec->posY, ATAN_ANGLE8) - parent->rotZ) & 0xFF;
    if (turn > 0x80) {
      turn -= 0x100;
    }
    if (turn < -0x20) {
      turn = -0x20;
    }
    if (turn > 0x20) {
      turn = 0x20;
    }
    rec->rotZ = parent->rotZ + turn;
    dist = ApproxDist2D(&rec->posX, (int *)g_anSpyroWorldPos);
    dz = rec->posZ - 0x164;
    turn = (ArcTan2(dist, g_nSpyroGroundHeightZ - dz, ATAN_ANGLE8) - parent->rotY) & 0xFF;
    if (turn > 0x80) {
      turn -= 0x100;
    }
    if (turn < -0x30) {
      turn = -0x30;
    }
    if (turn > 0x30) {
      turn = 0x30;
    }
    rec->rotY = parent->rotY + turn;
    st->timer = 0x5A;
    st->target = 0;
    InsertActorIntoSpatialGrid(rec);
    break;
  }
  case ACTOR_TYPE_WAYPOINT_RUNNER: {
    WaypointRunnerState *st = rec->state;
    RunnerSpawnerState *pst = parent->state;
    int *pos;
    int probe[3];
    int unused[2];

    InitActorRenderDefaults(rec);
    pos = &rec->posX;
    CopyVector(pos, &parent->posX);
    st->spawner = parent;
    st->path = pst->path;
    ZeroVector(st->smoothVel);
    rec->cullRadius = CULL_RADIUS(0x2000);
    InsertActorIntoSpatialGrid(rec);
    rec->groundCode = -1;
    CopyVector(probe, pos);
    probe[2] += GROUND_PROBE_LIFT;
    FindGroundHeightBelow(probe, GROUND_PROBE_FAR);
    EncodeHitNormalToActorGroundCode(rec);
    break;
  }

  case ACTOR_TYPE_FALLING_DEBRIS: {
    FallingDebrisState *st = rec->state;

    InitActorRenderDefaults(rec);
    CopyVector(&rec->posX, &parent->posX);
    st->vel[0] = (GetRandomU32() & 0x7E) - 0x3F;
    st->vel[1] = (GetRandomU32() & 0x7E) - 0x3F;
    st->vel[2] = (GetRandomU32() & 0x7E) - 0x10;
    st->life = (GetRandomU32() & 0x1F) + 0x20;
    InsertActorIntoSpatialGrid(rec);
    break;
  }
  case ACTOR_TYPE_SPIN_CHEST_LID:
    InitActorRenderDefaults(rec);
    rec->otBias = 5;
    if (parent != 0) {
      CopyVector(&rec->posX, &parent->posX);
    } else {
      CopyVector(&rec->posX, (int *)g_anSpyroWorldPos);
    }
    InsertActorIntoSpatialGrid(rec);
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
      CopyVector(&rec->posX, (int *)g_anSpyroWorldPos);
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
      CopyVector(&rec->posX, (int *)g_anSpyroWorldPos);
    }
    InsertActorIntoSpatialGrid(rec);
    break;
  }

  return rec;
}
