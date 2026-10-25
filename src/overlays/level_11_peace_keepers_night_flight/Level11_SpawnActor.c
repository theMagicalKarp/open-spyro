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
#include "actor_type.h"
#include "level_globals.h"
#include "level_spawn_actor.h"
#include "level_emit_particles.h"

/* Level11_SpawnActor (0x800819BC, level_11_peace_keepers_night_flight
 * overlay, 0x944 bytes).
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
   with `git show HEAD:src/overlays/level_11_peace_keepers_night_flight/Level11_SpawnActor.c.wip`; this body came from
   build/permuter/nonmatchings/Level11_SpawnActor/output-305-1/source.c. */
Actor *Level11_SpawnActor(int type, Actor *parent)
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
    case 0xF:
    {
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
      if (parent != 0)
      {
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

    case 0x10:
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

    case 0x22:
      InitActorRenderDefaults(rec);
      rec->cullRadius = 0x20;
      rec->updateDist = 0xFF;
      if (parent != 0)
    {
      CopyVector(&rec->posX, &parent->posX);
    }
      HideActorRenderRecord(rec);
      break;

    case 0xE:

    case 0x53 ... 0x57:
    {
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
      if (parent->type == 0xD)
      {
        st->bounces = 2;
      }
      else
      {
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

    case 0x78:
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

    case 0x1DE ... 0x1E0:
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
        rec->renderList |= 0x80;
      }
      break;
    }

    case 0x104 ... 0x10D:

    case 0x115:

    case 0x147:
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

    case 0x159 ... 0x15B:
      if (g_anWorldPersistentFlags[g_nActiveWorldIndex] != 0)
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

    case 0x176:
    {
      int dx;
      int dy;
      int dz; /* the original copies the euler bytes with lb */
      InitActorRenderDefaults(rec);
      CopyVector(&rec->posX, &parent->posX);
      dx = (signed char)parent->rotX;
      dy = (signed char)parent->rotY;
      dz = (signed char)parent->rotZ;
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
      rec->cullRadius = parent->cullRadius;
      break;
    }

    case 0x177 ... 0x182:
    {
      int dx;
      int dy;
      int dz;
      InitActorRenderDefaults(rec);
      CopyVector(&rec->posX, &parent->posX);
      dx = (signed char)parent->rotX;
      dy = (signed char)parent->rotY;
      dz = (signed char)parent->rotZ;
      rec->rotX = dx;
      rec->rotY = dy;
      rec->rotZ = dz;
 do { InsertActorIntoSpatialGrid(rec); rec->renderList = 0xBF; rec->tint0 = parent->tint0; rec->tint1 = parent->tint1; } while (0);
      rec->tint2 = parent->tint2;
      rec->tintMode = parent->tintMode;
      break;
    }

    case 0x183 ... 0x185:

    case 0x1EB:
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

    case 0x18F:
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

    case 0x195:

    case 0x1DD:
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

    case 0x4C:

    case 0x1AA ... 0x1C3:
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
