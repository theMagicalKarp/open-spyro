/* MATCHED 2026-08-27-1. This was the last WALL in the 36-overlay spawn-
 * dispatcher vein, and both of its blockers turned out to be source forms.
 *
 * 1. B13 (`lh` + `sra` unreachable) is RETIRED for the `>>` case. Writing
 *    `table[i] >> 2` with a LITERAL shift count lets combine fuse the
 *    sign-extension into the shift, so cc1 emits `lhu; sll 16; sra 18` -- one
 *    insn over, which is what made the override overflow its slot. Routing the
 *    count through a plain `int` local assigned `2` earlier in the same arm
 *    (`n = 2; ... table[i] >> n;`) leaves the extension as its own insn and cc1
 *    emits `lh; sra 2`. The local costs nothing (it is constant-propagated) but
 *    it must be a SEPARATE local per shift site: one local feeding two shifts
 *    stays in a register and degrades both to `srav`.
 * 2. The arm-0x25 tail needed the `g_apActorMeshTable[type][0xE][0xC]` lookup hoisted
 *    into an `anim` temp above the four `rec->anim..nextFrame = 0` stores (which are
 *    themselves in original order 3D, 3C, 3F, 3E), so the load chain schedules
 *    ahead of the store batch and `sb anim,65(rec)` still lands in the `j`
 *    delay slot.
 * 3. Arm 0x26's `if (parent->type == type)` must compare against the LITERAL
 *    0x26. Comparing against the `type` parameter keeps it live in `s0` across
 *    the arm and rotates s0/s1/s2 through 10 insns; the literal rematerialises
 *    as `li v0,38` exactly as the original does.
 */

#include "vecmath.h"
#include "level_globals.h"
#include "level_spawn_actor.h"
#include "actor.h"
#include "emit_record.h"

/* Level16_SpawnActor (0x80083AB4, level_16_magic_crafters_blowhard
 * overlay, 0xD7C bytes).
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

/* Rim-rider state block (type 0x25; Level16_UpdateActors views it as
 * RimState). */
typedef struct WindState {
  Actor *owner;       /* 0x00 */
  short reach;        /* 0x04 */
  short range;        /* 0x06 */
  int seenXY;         /* 0x08 Spyro's last-seen x/y >> 4 (two shorts) */
  int child;          /* 0x0C type 0x27 helper actor */
  unsigned char mode; /* 0x10 */
} WindState;

/* Rim-rider helper state block (type 0x27; HelperState in the updater). */
typedef struct AimState {
  int timer;    /* 0x00 */
  Actor *owner; /* 0x04 */
  int beat;     /* 0x08 */
} AimState;

extern int g_anCameraPos;     /* scalar view of camera X: the spawned prop turns to face the camera */

extern int g_anSpyroWorldPos[];
extern unsigned char **g_apActorMeshTable[];  /* per-type actor asset table */

/* NOTE: the function body below is decomp-permuter output, spliced in
   by the 2026-08-14-1 unattended permuter session for its PARTIAL-BYTE
   gain only. It reads worse than the hand-written form it replaced and
   its inline comments are lost. The hand-written original is recoverable
   with `git show HEAD:src/overlays/level_16_magic_crafters_blowhard/Level16_SpawnActor.c.wip`; this body came from
   build/permuter/nonmatchings/Level16_SpawnActor/output-815-1/source.c. */
Actor *Level16_SpawnActor(int type, Actor *parent)
{
  Actor *rec;
  int owner;
  int shiftX;
  int shiftY;
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

    case ACTOR_TYPE_RIM_RIDER:
    {
      WindState *st = rec->state;
      int probe[3];
      int anim;
      shiftX = 2;
      shiftY = 2;
      InitActorRenderDefaults(rec);
      rec->groundCode = -1;
      CopyVector(probe, &rec->posX);
      probe[2] += 0x400;
      FindGroundHeightBelow(probe, 0x10000);
      EncodeCachedVecToActorDirCode(rec);
      switch (parent->mode)
      {
        case 0x48:
          st->mode = 1;
          st->reach = 0x40;
          st->range = 0xE00;
          break;

        case 0x52:
          st->mode = 2;
          st->reach = 0x100;
          st->range = 0x2000;
          break;

        case 0x5C:
          st->mode = 3;
          st->reach = 0x140;
          st->range = 0x2000;
          break;

      }

      rec->posX = parent->posX + (g_anCosineLut[parent->rotZ] >> shiftX);
      rec->posY = parent->posY + (g_anSineLut[parent->rotZ] >> shiftY);
      rec->posZ = parent->posZ;
      st->owner = parent;
      st->child = 0;
      rec->mode = 0;
      anim = g_apActorMeshTable[rec->type][0xE][0xC];
      rec->nextAnim = 0;
      rec->anim = 0;
      rec->nextFrame = 0;
      rec->frame = 0;
      rec->frameRate = anim;
      InsertActorIntoSpatialGrid(rec);
      break;
    }

    case ACTOR_TYPE_SPLIT_SHOT:
    {
      int *st = rec->state;
      int *pos;
      int turn;
      InitActorRenderDefaults(rec);
      pos = &rec->posX;
      CopyVector(pos, &parent->posX);
      if (parent->type == ACTOR_TYPE_SPLIT_SHOT)
      {
        rec->rotZ = parent->rotZ;
        rec->mode = SPLITSHOT_FRAGMENT;
        rec->frameProgress = 0;
        rec->frameRate = g_apActorMeshTable[rec->type][0x11][0xC];
        rec->anim = 3;
        rec->nextAnim = 3;
        *st = 3;
      }
      else
        if (parent->nextFrame >= 6)
      {
        rec->posX += LookupCosine(parent->rotZ << 4) >> 2;
        rec->posY += LookupSine(parent->rotZ << 4) >> 2;
        rec->posZ += 0x12C;
        rec->rotZ = parent->rotZ;
        turn = (ArcTan2(ApproxDist2D(pos, g_anSpyroWorldPos), g_nSpyroWorldPosZ - rec->posZ, ATAN_ANGLE8) - parent->rotY) & 0xFF;
        if (turn > 0x80)
        {
          turn -= 0x100;
        }
        if (turn < (-0x10))
        {
          turn = -0x10;
        }
        if (turn > 0x10)
        {
          turn = 0x10;
        }
        rec->rotY = parent->rotY + turn;
        *st = 0x80;
        rec->mode = SPLITSHOT_FLY;
        rec->frameProgress = 0;
        rec->frameRate = g_apActorMeshTable[rec->type][0x10][0xC];
        rec->anim = 2;
        rec->nextAnim = 2;
        g_pfnLevelEmitParticles(4, EMIT_DRIFT_SPARKLE, pos, 0x10);
        InsertActorIntoSpatialGrid(rec);
        break;
      }
      else
      {
        if (parent->nextFrame >= 2)
        {
          rec->posZ += 0x12C;
          rec->rotZ = parent->rotZ;
          rec->mode = SPLITSHOT_RAISE;
          rec->frameProgress = 0;
          rec->frameRate = g_apActorMeshTable[rec->type][0xF][0xC];
          rec->anim = 1;
          rec->nextAnim = 1;
        }
        else
        {
          rec->posZ += 0x180;
          rec->rotZ = parent->rotZ;
        }
        *st = 2;
      }
      InsertActorIntoSpatialGrid(rec);
      break;
    }

    case ACTOR_TYPE_RIM_RIDER_HELPER:
    {
      AimState *st = rec->state;
      InitActorRenderDefaults(rec);
      CopyVector(&rec->posX, &parent->posX);
      InsertActorIntoSpatialGrid(rec);
      rec->rotZ = ArcTan2(g_anCameraPos - rec->posX, g_nCameraPosY - rec->posY, ATAN_ANGLE8);
      rec->posZ -= 0x400;
      st->timer = 0x40;
      st->owner = parent;
      st->beat = 1;
      break;
    }

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
      st->sub = 0;
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

    case ACTOR_TYPE_SEED_POD_SHARD ... ACTOR_TYPE_SEED_POD_SHARD + 2:

    case ACTOR_TYPE_BOULDER_DEBRIS:

    case ACTOR_TYPE_WOOD_CHEST_SHARD ... ACTOR_TYPE_WOOD_CHEST_SHARD + 2:

    case ACTOR_TYPE_SPARKLE_SHARD ... ACTOR_TYPE_WOOD_SHARD + 1:
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

    case ACTOR_TYPE_STATUE_SHARD:
    {
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
      if (g_nDragonRescueSubstate == DRAGON_RESCUE_TURN)
      {
        rec->scale = 0x14;
      }
      else
        if (g_nDragonRescueSubstate == DRAGON_RESCUE_SWEEP)
      {
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
