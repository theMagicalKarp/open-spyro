/* MATCHED 2026-08-27-1. B19 (the call-arg copy hoisted into a same-block store
 * batch) is RETIRED: the escape the old park note called for -- "a real basic
 * block boundary before the call" -- is a plain `do { ... } while (0);` around
 * the store batch. The loop notes split the block, so sched1 can no longer let
 * the arg copy's cost-1 TRUE dependence on the call outrank the stores'
 * cost-0 REG_DEP_ANTI edges, and `move a0,s4` drops back into the jal's delay
 * slot with the 0x44..0x46 copy batched into v0/v1/a0. Applied at the two
 * attitude-copy arms (0x176 and 0x177..0x182) only; ~29,000 permuter
 * iterations per family member had bottomed out on this same shelf because
 * permutation cannot synthesise a block boundary.
 */

#include "vecmath.h"
#include "level_globals.h"

/* Level5_SpawnActor (0x8008249C, level_5_artisans_sunny_flight overlay,
 * 0xe94 bytes).
 *
 * Spawn one actor ("moby") of type `type`, parented to `parent`.
 *
 * Allocates a pool record (AllocActorRecordSlot = AllocActorRecordSlot), stamps the
 * type into +0x36 and the parent's pool index into +0x56, then runs a per-type
 * init arm: every arm starts from InitActorRenderDefaults (InitActorRenderDefaults) and
 * ends by placing the new actor -- most copy the parent's position
 * (CopyVector = CopyVector) and either register it in the spatial grid
 * (InsertActorIntoSpatialGrid) or hide its render record (HideActorRenderRecord).  With no parent
 * the position falls back to Spyro's (g_anSpyroWorldPos).
 *
 * Sunny Flight carries the full flight arm set shared with levels 11/17/23/29.
 */

typedef struct Actor {
  void *state;         /* 0x00 per-type state block */
  int unk04;           /* 0x04 */
  int collision;           /* 0x08 */
  int posX;            /* 0x0C */
  int posY;            /* 0x10 */
  int posZ;            /* 0x14 */
  int flags;           /* 0x18 */
  int unk1C;           /* 0x1C */
  int mtx[5];        /* 0x20 */
  short gridCell;         /* 0x34 */
  short type;          /* 0x36 */
  int unk38;           /* 0x38 */
  unsigned char anim; /* 0x3C */
  unsigned char nextAnim;
  unsigned char frame;
  unsigned char nextFrame;
  unsigned char frameProgress; /* 0x40 */
  unsigned char frameRate;
  unsigned char animFlags;
  unsigned char group;
  unsigned char rotX; /* 0x44 */
  unsigned char rotY;
  unsigned char rotZ;
  unsigned char unk47;
  unsigned char mode;
  unsigned char subMode; /* 0x49 */
  unsigned char visZone;
  unsigned char renderList; /* 0x4B */
  unsigned char unk4C; /* 0x4C */
  unsigned char unk4D;
  unsigned char unk4E;
  unsigned char unk4F;
  unsigned char unk50; /* 0x50 */
  unsigned char drawn;
  unsigned char updateDist; /* 0x52 */
  unsigned char dropKind;
  unsigned char sfxSlot;
  unsigned char soundRange;
  unsigned char poolIndex; /* 0x56 owner pool index */
  unsigned char scale;
} Actor; /* 0x58 */

/* Flame state block (types 0xE, 0xF, 0x53..0x57). */
typedef struct FlameState {
  int unk00;
  int unk04;
  int unk08;
  unsigned char unk0C;
  unsigned char unk0D;
  unsigned char unk0E;
  unsigned char unk0F;
  unsigned char unk10;
  unsigned char unk11;
  unsigned char unk12;
  unsigned char unk13;
  unsigned char unk14;
} FlameState;

/* Mortar-shot state block (type 0xFB). */
typedef struct ShotState {
  int vel[3];  /* 0x00 */
  int unk0C;   /* 0x0C */
  unsigned char unk10;
  unsigned char unk11;
  unsigned char unk12;
  unsigned char unk13;
} ShotState;

/* Beam/link state block (type 0x10). */
typedef struct BeamState {
  int unk00;
  int anchor[3]; /* 0x04 */
  unsigned char unk10;
  unsigned char unk11;
  unsigned char unk12;
  unsigned char unk13;
  short unk14;
} BeamState;

/* Spin state block (type 0x78). */
typedef struct SpinState {
  int unk00;
  short unk04;
  short unk06;
  short unk08;
  short unk0A;
  int unk0C;
  int unk10;
} SpinState;

/* Debris state block (types 0x43..0x45, 0xFF..0x101, 0x135..0x137). */
typedef struct DebrisState {
  short vel[3]; /* 0x00 */
  short spinX;  /* 0x06 */
  short spinY;  /* 0x08 */
  short spinZ;  /* 0x0A */
  int unk0C;
  int unk10;
} DebrisState;

/* Thrown-shard state block (types 0x120..0x121). */
typedef struct ShardState {
  short vel[3]; /* 0x00 */
  short spinX;  /* 0x06 */
  short spinY;  /* 0x08 */
  short spinZ;  /* 0x0A */
  unsigned char unk0C;
  unsigned char unk0D;
  short unk0E;
  int unk10;
} ShardState;

extern Actor *AllocActorRecordSlot(void);
extern void InitActorRenderDefaults(Actor *rec);
extern void InsertActorIntoSpatialGrid(Actor *rec);
extern void HideActorRenderRecord(Actor *rec);
extern void EncodeCachedVecToActorDirCode(Actor *rec);          /* EncodeCachedVecToActorDir */
extern int FindGroundHeightBelow(int *pos, int drop);
extern int g_nSpyroWorldPosY;     /* g_anSpyroWorldPos[1] */

extern Actor *g_pActorListBase;
extern int g_anSpyroWorldPos[];
extern void (*g_pfnLevelEmitParticles)(int kind, int count, int *pos, int spread); /* SpawnPuff */
extern short g_anCosineLut[]; /* unit-circle x table (8-bit angle) */
extern short g_anSineLut[]; /* unit-circle y table (8-bit angle) */
extern int g_anWorldPersistentFlags[];   /* per-slot flight-race object table */
extern void DespawnActorRecord(Actor *rec);          /* ReleaseActorRecordSlot */

Actor *Level5_SpawnActor(int type, Actor *parent) {
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
    FlameState *st = rec->state;

    InitActorRenderDefaults(rec);
    st->unk08 = 0x8C;
    st->unk10 = 3;
    st->unk14 = 0xFF;
    st->unk00 = 0;
    st->unk04 = 0;
    st->unk0E = 0;
    st->unk0F = 0;
    st->unk11 = 0;
    st->unk12 = 0;
    st->unk13 = 0;
    rec->subMode = 2;
    rec->unk50 = 0x18;
    rec->updateDist = 0x10;
    rec->rotX = 0x20;
    rec->rotY = 0;
    rec->rotZ = 0;
    if (parent != 0) {
      CopyVector(&rec->posX, &parent->posX);
    }
    InsertActorIntoSpatialGrid(rec);
    rec->unk4C = 0;
    rec->unk4D = 0;
    rec->unk4E = 0;
    rec->unk4F = 1;
    rec->renderList |= 0x80;
    break;
  }

  case 0x10: {
    BeamState *st = rec->state;
    int *pos;

    InitActorRenderDefaults(rec);
    InsertActorIntoSpatialGrid(rec);
    pos = &rec->posX;
    CopyVector(pos, &parent->posX);
    rec->posZ += 0x200;
    CopyVector(st->anchor, pos);
    st->unk13 = 0;
    st->unk12 = 0;
    st->unk14 = 0x708;
    break;
  }

  case 0x11: {
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
    ScaleVector3ByRatio(d, VectorLength(d, 1), 0xA0);
    InsertActorIntoSpatialGrid(rec);
    st = rec->state;
    CopyVector(st, d);
    st[3] = 0x50;
    break;
  }

  case 0x22:
    InitActorRenderDefaults(rec);
    rec->unk50 = 0x20;
    rec->updateDist = 0xFF;
    if (parent != 0) {
      CopyVector(&rec->posX, &parent->posX);
    }
    HideActorRenderRecord(rec);
    break;

  case 0x4E:
    InitActorRenderDefaults(rec);
    InsertActorIntoSpatialGrid(rec);
    break;

  case 0xE:
  case 0x53 ... 0x57: {
    FlameState *st = rec->state;
    int *pos;
    int probe[3];

    InitActorRenderDefaults(rec);
    st->unk00 = 0;
    st->unk04 = 0;
    st->unk08 = 0x8C;
    st->unk0E = 0;
    st->unk0F = 0;
    st->unk11 = 0;
    st->unk12 = 0;
    st->unk13 = 0;
    if (parent->type == 0xD) {
      st->unk10 = 2;
    } else {
      st->unk10 = 3;
    }
    pos = &rec->posX;
    st->unk14 = 0xFF;
    rec->subMode = 2;
    rec->unk50 = 0x18;
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
    rec->unk4C = 0;
    rec->unk4D = 0;
    rec->unk4E = 0;
    if (rec->type == 0xE) {
      rec->unk4F = 0xC;
    }
    if (rec->type == 0x53) {
      rec->unk4F = 1;
    }
    if (rec->type == 0x54) {
      rec->unk4F = 2;
    }
    if (rec->type == 0x55) {
      rec->unk4F = 3;
    }
    if (rec->type == 0x56) {
      rec->unk4F = 4;
    }
    if (rec->type == 0x57) {
      rec->unk4F = 5;
    }
    break;
  }

  case 0x78: {
    SpinState *st = rec->state;

    InitActorRenderDefaults(rec);
    InsertActorIntoSpatialGrid(rec);
    rec->subMode = 0;
    st->unk00 = 0;
    st->unk08 = 0;
    st->unk06 = 0;
    st->unk04 = 0;
    st->unk0C = 0;
    st->unk10 = 0;
    if (parent != 0) {
      CopyVector(&rec->posX, &parent->posX);
    }
    break;
  }

  case 0x1DE ... 0x1E0: {
    DebrisState *st = rec->state;
    int yaw;
    int pitch;

    InitActorRenderDefaults(rec);
    rec->unk50 = 0x20;
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
    st->unk10 = parent->posZ - 0x40;
    st->unk0C = 0x40 - (GetRandomU32() & 0xF);
    if ((unsigned int)((unsigned short)rec->type - 0x135) < 3) {
      *(int *)&rec->unk4C = 0xA18618;
      rec->renderList |= 0x80;
    }
    break;
  }

  case 0x1E1 ... 0x1E3: {
    DebrisState *st = rec->state;
    int yaw;
    int pitch;

    InitActorRenderDefaults(rec);
    rec->unk50 = 0x20;
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
    st->unk10 = parent->posZ - 0x40;
    st->unk0C = 0x40 - (GetRandomU32() & 0xF);
    break;
  }

  case 0x167 ... 0x169:
  {
    DebrisState *st = rec->state;
    int seed;

    InitActorRenderDefaults(rec);
    rec->unk50 = 0x20;
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
    st->unk10 = parent->posZ - 0x40;
    st->unk0C = 0x40 - (GetRandomU32() & 0xF);
    break;
  }

  case 0x16A:
  {
    DebrisState *st = rec->state;
    int *pst = parent->state;
    int dir[3];

    InitActorRenderDefaults(rec);
    rec->unk50 = 0x20;
    InsertActorIntoSpatialGrid(rec);
    CopyVector((int *)st, pst + 2);
    st->spinY = 0;
    st->spinZ = 0;
    st->vel[2] += 0x40;
    st->unk10 = parent->posZ - 0x40;
    st->unk0C = 0x40 - (GetRandomU32() & 0xF);
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
    rec->unk4F = 2;
    rec->unk4C = 0;
    rec->unk4D = 0;
    rec->unk4E = 0;
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
      rec->unk50 = 0x40;
      HideActorRenderRecord(rec);
      rec->unk4C = 0;
      rec->unk4D = 0;
      rec->unk4E = 0;
      rec->unk4F = 2;
    }
    break;

  case 0x176: {
    int dx;
    int dy;
    int dz;

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
    rec->unk4C = parent->unk4C;
    rec->unk4D = parent->unk4D;
    rec->unk4E = parent->unk4E;
    rec->unk4F = parent->unk4F;
    rec->unk50 = parent->unk50;
    break;
  }

  case 0x177 ... 0x182: {
    int dx;
    int dy;
    int dz;

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
    rec->unk4C = parent->unk4C;
    rec->unk4D = parent->unk4D;
    rec->unk4E = parent->unk4E;
    rec->unk4F = parent->unk4F;
    break;
  }

  case 0x183 ... 0x185:
  case 0x189:
  case 0x1EA:
    InitActorRenderDefaults(rec);
    rec->posX = -0x64;
    rec->posY = 0x1E;
    rec->unk50 = 0;
    rec->posZ = 0x1000;
    HideActorRenderRecord(rec);
    rec->unk47 = 0x20;
    rec->unk4C = 0;
    rec->unk4D = 0;
    rec->unk4E = 0;
    rec->unk4F = 0;
    break;

  case 0x190:
  case 0x195:
  case 0x1DD: {
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
    rec->unk50 = 0x20;
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
