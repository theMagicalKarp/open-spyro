/* Level29_SpawnActor (0x80082028, level_29_dream_weavers_icy_flight
 * overlay, 0xd6c bytes).
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
 *    (RotateVectorByMatrix = RotateVectorByMatrix) and added to the parent's position.
 */

#include "vecmath.h"

typedef struct Actor {
  void *state;         /* 0x00 per-type state block */
  int unk04;           /* 0x04 */
  int unk08;           /* 0x08 */
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
  unsigned char unk43;
  unsigned char rotX; /* 0x44 */
  unsigned char rotY;
  unsigned char rotZ;
  unsigned char unk47;
  unsigned char mode;
  unsigned char subMode; /* 0x49 */
  unsigned char unk4A;
  unsigned char unk4B; /* 0x4B */
  unsigned char unk4C; /* 0x4C */
  unsigned char unk4D;
  unsigned char unk4E;
  unsigned char unk4F;
  unsigned char unk50; /* 0x50 */
  unsigned char drawn;
  unsigned char updateDist; /* 0x52 */
  unsigned char unk53;
  unsigned char sfxSlot;
  unsigned char unk55;
  unsigned char poolIndex; /* 0x56 owner pool index */
  unsigned char unk57;
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

/* Aim/lob state block (type 0xE8). */
typedef struct AimState {
  int unk00;
  int unk04;
  int unk08;
  int unk0C;
} AimState;

extern Actor *AllocActorRecordSlot(void);
extern void InitActorRenderDefaults(Actor *rec);
extern void InsertActorIntoSpatialGrid(Actor *rec);
extern void HideActorRenderRecord(Actor *rec);
extern void EncodeCachedVecToActorDirCode(Actor *rec);          /* EncodeCachedVecToActorDir */
extern int FindGroundHeightBelow(int *pos, int drop);
extern int RandomIntInRange(int lo, int hi);       /* RandomInRange */
extern int StepAndLerpAngle8(int yaw, unsigned char pitch, int speed, int lift);
extern int g_nSpyroWorldPosY;     /* g_anSpyroWorldPos[1] */

extern Actor *g_pActorListBase;
extern int g_anSpyroWorldPos[];
extern int g_anSpyroVelocity[3];
extern int g_nGemPickupSubstate;
extern short g_anLaunchDirTable[]; /* launch-direction table, x */
extern short g_anLaunchDirTableZ[]; /* launch-direction table, z */
extern int g_anSpyroFirstContactPoint[3];
extern unsigned char **g_apActorMeshTable[];  /* per-type actor asset table */
extern int g_nSpyroWorldPosZ;     /* g_anSpyroWorldPos[2] */
extern void (*g_pfnLevelEmitParticles)(int kind, int count, int *pos, int spread); /* SpawnPuff */
extern int g_anSpyroFirstContactNormal[3];
extern int g_nSpyroGroundHeightZ;     /* g_nSpyroGroundHeight */
extern short g_anCosineLut[]; /* unit-circle x table (8-bit angle) */
extern short g_anSineLut[]; /* unit-circle y table (8-bit angle) */
extern unsigned char g_bSpyroPersistentEulerZ;  /* g_bSpyroBodyYaw (8-bit) */
extern int g_nActiveWorldIndex;     /* active flight-race slot index */
extern int g_anWorldPersistentFlags[];   /* per-slot flight-race object table */
extern void DespawnActorRecord(Actor *rec);          /* ReleaseActorRecordSlot */

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
    rec->unk4B |= 0x80;
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

  case 0xE8:
  {
    AimState *st = rec->state;

    InitActorRenderDefaults(rec);
    CopyVector(&rec->posX, &parent->posX);
    rec->posZ += 0x400;
    InsertActorIntoSpatialGrid(rec);
    st->unk04 = 0xC8;
    st->unk08 = 0x15E;
    st->unk00 = StepAndLerpAngle8(
        ArcTan2(rec->posX - g_anSpyroWorldPos[0], rec->posY - g_nSpyroWorldPosY, 0),
        g_bSpyroPersistentEulerZ, 0x28, 0x40);
    st->unk0C = 0x78;
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
      rec->unk4B |= 0x80;
    }
    break;
  }

  case 0x1E1 ... 0x1E2: {
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

  case 0x185:
  case 0x18A:
  case 0x1EA ... 0x1EB:
    InitActorRenderDefaults(rec);
    rec->unk50 = 0;
    rec->posX = -0x64;
    rec->posY = 0x1E;
    rec->posZ = 0x1000;
    HideActorRenderRecord(rec);
    rec->unk47 = 0x20;
    rec->unk4C = 0;
    rec->unk4D = 0;
    rec->unk4E = 0;
    rec->unk4F = 0;
    break;

  case 0x18F:
    InitActorRenderDefaults(rec);
    rec->unk50 = 0xFF;
    rec->posX = 0x1CC;
    rec->posY = 0x28;
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
