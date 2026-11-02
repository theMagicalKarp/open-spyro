/* MATCHED 2026-08-27-1, with level_5_8008249C / level_17_80081F0C /
 * level_23_8008223C. Wall B19 (the call-arg copy hoisted into a same-block
 * store batch) is RETIRED: the "real basic-block boundary between the stores
 * and the call" the old park note asked for is a plain `do { ... } while (0);`
 * around the store batch -- its loop notes split the block, so the arg copy's
 * cost-1 TRUE dependence on the call can no longer outrank the stores' cost-0
 * REG_DEP_ANTI edges, and `move a0,rec` falls back into the jal delay slot with
 * the 0x44..0x46 copy batched into v0/v1/a0. An explicit goto/label does not
 * work (jump.c deletes it before sched1); tens of thousands of permuter
 * iterations did not, because permutation cannot synthesise a block boundary.
 */

#include "vecmath.h"
#include "level_globals.h"
#include "level_spawn_actor.h"
#include "actor.h"
#include "main_shared.h"
#include "level_emit_particles.h"

/* Level17_SpawnActor (0x80081F0C, level_17_magic_crafters_crystal_flight
 * overlay, 0xb94 bytes).
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



/* NOTE: the function body below is decomp-permuter output, spliced in
   by the 2026-08-14-1 unattended permuter session for its PARTIAL-BYTE
   gain only. It reads worse than the hand-written form it replaced and
   its inline comments are lost. The hand-written original is recoverable
   with `git show HEAD:src/overlays/level_17_magic_crafters_crystal_flight/Level17_SpawnActor.c.wip`; this body came from
   build/permuter/nonmatchings/Level17_SpawnActor/output-325-1/source.c. */
Actor *Level17_SpawnActor(int type, Actor *parent)
{
  Actor *rec;
  int owner;
  rec = AllocActorRecordSlot();
  rec->type = type;
  if ((parent == 0) || (((unsigned int) (owner = parent - g_pActorListBase)) >= 0x100))
  {
    owner = 0;
  }
  rec->poolIndex = owner;
  switch (type)
  {
    case ACTOR_TYPE_SPARX_FOOD:
    {
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
      rec->subMode = COLLECT_BOUNCE;
      rec->cullRadius = 0x18;
      rec->updateDist = 0x10;
      rec->rotX = 0x20;
      rec->rotY = 0;
      rec->rotZ = 0;
      if (parent != 0)
      {
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

    case ACTOR_TYPE_BUTTERFLY:
    {
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

    case ACTOR_TYPE_AIMED_SHOT:
    {
      int *st;
      int dir;
      int d[3];
      InitActorRenderDefaults(rec);
      dir = parent->rotZ;
      rec->rotZ = dir;
      rec->posX = parent->posX + (g_anCosineLut[dir] >> 4);
      rec->posY = parent->posY + (g_anSineLut[rec->rotZ] >> 4);
      rec->posZ = parent->posZ;
      d[0] = g_anSpyroWorldPos[0] - rec->posX;
      d[1] = g_nSpyroWorldPosY - rec->posY;
      d[2] = g_nSpyroWorldPosZ - rec->posZ;
      ScaleVector3ByRatio(d, VectorLength(d, VEC_LEN_XYZ), 0xA0);
      InsertActorIntoSpatialGrid(rec);
      st = rec->state;
      CopyVector(st, d);
      st[3] = 0x50;
      break;
    }

    case ACTOR_TYPE_DRAGON_EGG:
      InitActorRenderDefaults(rec);
      rec->cullRadius = 0x20;
      rec->updateDist = 0xFF;
      if (parent != 0)
    {
      CopyVector(&rec->posX, &parent->posX);
    }
      HideActorRenderRecord(rec);
      break;

    case ACTOR_TYPE_EXTRA_LIFE:

    case ACTOR_TYPE_GEM_1 ... ACTOR_TYPE_GEM_25:
    {
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
      if (parent->type == ACTOR_TYPE_FOLLOWER)
      {
        st->bounces = 2;
      }
      else
      {
        st->bounces = 3;
      }
      pos = &rec->posX;
      st->sparkle = 0xFF;
      rec->subMode = COLLECT_BOUNCE;
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
      if (rec->type == ACTOR_TYPE_EXTRA_LIFE)
      {
        rec->tintMode = 0xC;
      }
      if (rec->type == ACTOR_TYPE_GEM_1)
      {
        rec->tintMode = 1;
      }
      if (rec->type == ACTOR_TYPE_GEM_2)
      {
        rec->tintMode = 2;
      }
      if (rec->type == ACTOR_TYPE_GEM_5)
      {
        rec->tintMode = 3;
      }
      if (rec->type == ACTOR_TYPE_GEM_10)
      {
        rec->tintMode = 4;
      }
      if (rec->type == ACTOR_TYPE_GEM_25)
      {
        rec->tintMode = 5;
      }
      break;
    }

    case ACTOR_TYPE_SPARX:
    {
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
      if (parent != 0)
      {
        CopyVector(&rec->posX, &parent->posX);
      }
      break;
    }

    case ACTOR_TYPE_FLIGHT_CHEST_SHARD ... ACTOR_TYPE_FLIGHT_CHEST_SHARD + 2:
    {
      FragmentState *st = rec->state;
      int yaw;
      int pitch;
      InitActorRenderDefaults(rec);
      rec->cullRadius = 0x20;
      CopyVector(&rec->posX, &parent->posX);
      InsertActorIntoSpatialGrid(rec);
      yaw = GetRandomU32() & 0xFFF;
      pitch = GetRandomU32() & 0x7FF;
      st->vel[0] = ((LookupCosine(pitch) >> 5) * LookupCosine(yaw)) >> 12;
      st->vel[1] = ((LookupCosine(pitch) >> 5) * LookupSine(yaw)) >> 12;
      st->vel[2] = LookupSine(pitch) >> 5;
      if (parent->flags & ACTOR_HIT_CHARGE)
      {
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
      if (((unsigned int) (((unsigned short) rec->type) - 0x135)) < 3)
      {
        *((int *) (&rec->tint0)) = 0xA18618;
        rec->renderList |= ACTOR_RENDER_SECONDARY;
      }
      break;
    }

    case ACTOR_TYPE_SPARK_DEBRIS ... ACTOR_TYPE_SPARK_DEBRIS + 2:
    {
      FragmentState *st = rec->state;
      int yaw;
      int pitch;
      InitActorRenderDefaults(rec);
      rec->cullRadius = 0x20;
      CopyVector(&rec->posX, &parent->posX);
      InsertActorIntoSpatialGrid(rec);
      yaw = GetRandomU32() & 0xFFF;
      pitch = GetRandomU32() & 0x7FF;
      st->vel[0] = ((LookupCosine(pitch) >> 5) * LookupCosine(yaw)) >> 12;
      st->vel[1] = ((LookupCosine(pitch) >> 5) * LookupSine(yaw)) >> 12;
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

    case ACTOR_TYPE_GEM_DIGIT_0 ... ACTOR_TYPE_GEM_DIGIT_0 + 9:

    case ACTOR_TYPE_GLYPH_SLASH:

    case ACTOR_TYPE_GLYPH_PERIOD:
    {
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

    case ACTOR_TYPE_TIME_BONUS ... ACTOR_TYPE_TIME_BONUS + 2:
      if (g_anFlightClearTimes[g_nActiveWorldIndex] != 0)
    {
      HideActorRenderRecord(rec);
      DespawnActorRecord(rec);
      rec = 0;
    }
    else
    {
      InitActorRenderDefaults(rec);
      if (parent != 0)
      {
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

    case ACTOR_TYPE_FLIGHT_TARGET_SHARD:
    {
      int dx;
      int dy;
      int dz;
      InitActorRenderDefaults(rec);
      CopyVector(&rec->posX, &parent->posX);
      dx = (signed char) parent->rotX;
      dy = (signed char) parent->rotY;
      dz = (signed char) parent->rotZ;
      rec->rotX = dx;
      rec->rotY = dy;
      rec->rotZ = dz;
 do { InsertActorIntoSpatialGrid(rec); rec->renderList = 0xBF; rec->tint0 = parent->tint0; rec->tint1 = parent->tint1; rec->tint2 = parent->tint2; rec->tintMode = parent->tintMode; } while (0);
      rec->cullRadius = parent->cullRadius;
      break;
    }

    case ACTOR_TYPE_FLIGHT_TARGET_SHARD + 1 ... ACTOR_TYPE_FLIGHT_TARGET_SHARD + 12:
    {
      int dx;
      int dy;
      int dz;
      InitActorRenderDefaults(rec);
      CopyVector(&rec->posX, &parent->posX);
      dx = (signed char) parent->rotX;
      dy = (signed char) parent->rotY;
      dz = (signed char) parent->rotZ;
      do {
      rec->rotX = dx;
      rec->rotY = dy;
      rec->rotZ = dz;
      } while (0);
      InsertActorIntoSpatialGrid(rec);
      rec->renderList = 0xBF;
      rec->tint0 = parent->tint0;
      rec->tint1 = parent->tint1;
      rec->tint2 = parent->tint2;
      rec->tintMode = parent->tintMode;
      break;
    }

    case ACTOR_TYPE_TALLY_ICON_RING ... ACTOR_TYPE_TALLY_ICON_CHEST: /* flight tally icons */

    case ACTOR_TYPE_TALLY_ICON_BANKER:
      InitActorRenderDefaults(rec);
      rec->posX = -0x64;
      rec->posY = 0x1E;
      rec->cullRadius = 0;
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

    case ACTOR_TYPE_HAZARD0_ENTER_FX:
    {
      int ground;
      int z;
      int gap;
      InitActorRenderDefaults(rec);
      if (parent != 0)
      {
        CopyVector(&rec->posX, &parent->posX);
      }
      else
      {
        CopyVector(&rec->posX, g_anSpyroWorldPos);
      }
      rec->posZ += 0x200;
      ground = FindGroundHeightBelow(&rec->posX, 0x800);
      z = rec->posZ;
      gap = ground - z;
      if (gap < 0)
      {
        gap = -gap;
      }
      if (gap < 0x800)
      {
        rec->posZ = ground;
      }
      else
      {
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
      if (parent != 0)
    {
      CopyVector(&rec->posX, &parent->posX);
    }
    else
    {
      CopyVector(&rec->posX, g_anSpyroWorldPos);
    }
      InsertActorIntoSpatialGrid(rec);
      break;

  }

  return rec;
}
