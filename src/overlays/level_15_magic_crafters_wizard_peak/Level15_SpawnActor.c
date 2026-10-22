/* Level15_SpawnActor (0x80086DD8, level_15_magic_crafters_wizard_peak
 * overlay, 0xD68 bytes).
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

extern Actor *AllocActorRecordSlot(void);
extern volatile int g_nSpyroWorldPosY;     /* g_anSpyroWorldPos[1] */

extern Actor *g_pActorListBase;
extern volatile int g_anSpyroWorldPos[];
extern unsigned char **g_apActorMeshTable[];  /* per-type actor asset table */

Actor *Level15_SpawnActor(int type, Actor *parent) {
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

  case 0x26: {
    int *st = rec->state;
    int *pos;
    int turn;

    InitActorRenderDefaults(rec);
    pos = &rec->posX;
    CopyVector(pos, &parent->posX);
    if (parent->type == type) {
      rec->rotZ = parent->rotZ;
      rec->mode = 3;
      rec->frameProgress = 0;
      rec->frameRate = g_apActorMeshTable[rec->type][0x11][0xC];
      rec->anim = 3;
      rec->nextAnim = 3;
      *st = 3;
    } else if (parent->nextFrame >= 6) {
      rec->posX += LookupCosine(parent->rotZ << 4) >> 2;
      rec->posY += LookupSine(parent->rotZ << 4) >> 2;
      rec->posZ += 0x12C;
      rec->rotZ = parent->rotZ;
      turn = (ArcTan2(ApproxDist2D(pos, (int *)g_anSpyroWorldPos), g_nSpyroWorldPosZ - rec->posZ,
                            0) -
              parent->rotY) &
             0xFF;
      if (turn > 0x80) {
        turn -= 0x100;
      }
      if (turn < -0x10) {
        turn = -0x10;
      }
      if (turn > 0x10) {
        turn = 0x10;
      }
      rec->rotY = parent->rotY + turn;
      *st = 0x80;
      rec->mode = 2;
      rec->frameProgress = 0;
      rec->frameRate = g_apActorMeshTable[rec->type][0x10][0xC];
      rec->anim = 2;
      rec->nextAnim = 2;
      g_pfnLevelEmitParticles(4, 7, pos, 0x10);
      InsertActorIntoSpatialGrid(rec);
      break;
    } else {
      if (parent->nextFrame >= 2) {
        rec->posZ += 0x12C;
        rec->rotZ = parent->rotZ;
        rec->mode = 1;
        rec->frameProgress = 0;
        rec->frameRate = g_apActorMeshTable[rec->type][0xF][0xC];
        rec->anim = 1;
        rec->nextAnim = 1;
      } else {
        rec->posZ += 0x180;
        rec->rotZ = parent->rotZ;
      }
      *st = 2;
    }
    InsertActorIntoSpatialGrid(rec);
    break;
  }
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
    rec->unk1C = -1;
    CopyVector(probe, pos);
    probe[2] += 0x400;
    FindGroundHeightBelow(probe, 0x10000);
    EncodeCachedVecToActorDirCode(rec);
    rec->tint0 = 0;
    rec->tint1 = 0;
    rec->tint2 = 0;
    if (rec->type == 0xE) {
      rec->tintMode = 0xC;
    }
    if (rec->type == 0x53) {
      rec->tintMode = 1;
    }
    if (rec->type == 0x54) {
      rec->tintMode = 2;
    }
    if (rec->type == 0x55) {
      rec->tintMode = 3;
    }
    if (rec->type == 0x56) {
      rec->tintMode = 4;
    }
    if (rec->type == 0x57) {
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
    st->vz = 0;
    st->vy = 0;
    st->vx = 0;
    st->fx = 0;
    st->rider = 0;
    if (parent != 0) {
      CopyVector(&rec->posX, &parent->posX);
    }
    break;
  }

  case 0x43 ... 0x45:
  case 0xFF ... 0x101:
  case 0x135 ... 0x137:
  case 0x1A7 ... 0x1A9: {
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
    if (parent->flags & 0x20000) {
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

  case 0xFB: {
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
    if (g_nGemPickupSubstate == 3) {
      rec->scale = 0x14;
    } else if (g_nGemPickupSubstate == 1) {
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

  case 0x127: {
    int *st = rec->state;
    int turn;
    int dy;
    int dx;
    int dz;
    int dist;

    InitActorRenderDefaults(rec);
    rec->posX = parent->posX + (g_anCosineLut[parent->rotZ] >> 1);
    rec->posY = parent->posY + (g_anSineLut[parent->rotZ] >> 1);
    rec->posZ = parent->posZ + 0x300;
    dx = g_anSpyroWorldPos[0] - rec->posX;
    dy = g_nSpyroWorldPosY - rec->posY;
    turn = (ArcTan2(dx, dy, 0) - parent->rotZ) & 0xFF;
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
    turn = (ArcTan2(dist, g_nSpyroGroundHeightZ - dz, 0) - parent->rotY) & 0xFF;
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
    st[1] = 0x5A;
    st[0] = 0;
    InsertActorIntoSpatialGrid(rec);
    break;
  }
  case 0x13E: {
    int *st = rec->state;

    InitActorRenderDefaults(rec);
    CopyVector(&rec->posX, &parent->posX);
    st[0] = (GetRandomU32() & 0x7E) - 0x3F;
    st[1] = (GetRandomU32() & 0x7E) - 0x3F;
    st[2] = (GetRandomU32() & 0x7E) - 0x10;
    st[3] = (GetRandomU32() & 0x1F) + 0x20;
    InsertActorIntoSpatialGrid(rec);
    break;
  }
  case 0x188:
    InitActorRenderDefaults(rec);
    rec->unk47 = 5;
    if (parent != 0) {
      CopyVector(&rec->posX, &parent->posX);
    } else {
      CopyVector(&rec->posX, (int *)g_anSpyroWorldPos);
    }
    InsertActorIntoSpatialGrid(rec);
    break;

  case 0x195:
  case 0x1DD: {
    int ground;
    int z;
    int gap;
    InitActorRenderDefaults(rec);
    if (parent != 0) {
      CopyVector(&rec->posX, &parent->posX);
    } else {
      CopyVector(&rec->posX, (int *)g_anSpyroWorldPos);
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
      CopyVector(&rec->posX, (int *)g_anSpyroWorldPos);
    }
    InsertActorIntoSpatialGrid(rec);
    break;
  }

  return rec;
}
