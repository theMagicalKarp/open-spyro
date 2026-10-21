/* func_level_19_8008465C (0x8008465C, level_19_beast_makers_terrace_village
 * overlay, 0xd50 bytes).
 *
 * Spawn one actor ("moby") of type `type`, parented to `parent`.
 *
 * Allocates a pool record (AllocActorRecordSlot = AllocActorRecordSlot), stamps the
 * type into +0x36 and the parent's pool index into +0x56 (the 0x58-byte-record
 * pointer difference, clamped away when it is not a real pool slot), then runs
 * a per-type init arm. Every arm starts from InitActorRenderDefaults
 * (InitActorRenderDefaults) and ends by placing the new actor: most copy the
 * parent's position (CopyVector = CopyVector) and either register it in the
 * spatial grid (InsertActorIntoSpatialGrid) or hide its render record (HideActorRenderRecord).
 *
 * The interesting arms:
 *  - 0xE / 0x53..0x57 (flame spawns): fill the flame state block,
 *    take a ground probe (FindGroundHeightBelow) 0x400 above the spawn point, encode
 *    the cached direction (EncodeCachedVecToActorDirCode) and select the per-type flame
 *    variant byte at +0x4F.
 *  - 0x43..0x45 / 0xFF..0x101 / 0x135..0x137 (debris bursts):
 *    give the fragment a random unit velocity built from a random yaw
 *    (0xFFF) and pitch (0x7FF) through LookupCosine/LookupSine, add a sixth
 *    of Spyro's velocity (g_anSpyroVelocity..54) when the parent is flagged 0x20000,
 *    nudge the spawn point along it, and randomise the tumble rates. The
 *    0x135..0x137 sub-range additionally gets the 0xA18618 tint and the
 *    +0x4B bit 7 flag.
 *  - 0xFB (Metalhead's mortar shot): pick one of eight launch directions from
 *    the D_8006F3A0/D_8006F3A2 table, rotate it into the parent's orientation
 *    (RotateVectorByMatrix = RotateVectorByMatrix), jitter each component by
 *    +-0x3F, offset the spawn point along it (AddVector = AddVector) and
 *    seed the shot's velocity (RShiftVector3 = RShiftVector3) with a further
 *    +-0x7F of jitter plus randomised spin rates and fuse.
 *  - 0x16B..0x16C: drop 0x200 and snap to the ground below when it is within
 *    0x800, otherwise back off by 0x200.
 *
 * With no parent the position falls back to Spyro's (g_anSpyroWorldPos).
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
  int unk20[5];        /* 0x20 */
  short unk34;         /* 0x34 */
  short type;          /* 0x36 */
  int unk38[3];        /* 0x38 */
  unsigned char unk44; /* 0x44 */
  unsigned char unk45;
  unsigned char unk46;
  unsigned char unk47;
  unsigned char unk48;
  unsigned char unk49; /* 0x49 */
  unsigned char unk4A;
  unsigned char unk4B; /* 0x4B */
  unsigned char unk4C; /* 0x4C */
  unsigned char unk4D;
  unsigned char unk4E;
  unsigned char unk4F;
  unsigned char unk50; /* 0x50 */
  unsigned char unk51;
  unsigned char unk52; /* 0x52 */
  unsigned char unk53;
  unsigned char unk54;
  unsigned char unk55;
  unsigned char unk56; /* 0x56 owner pool index */
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
/* Parent-anchored tracker state block. */
typedef struct TrackState {
  Actor *owner;        /* 0x00 */
  int unk04;
  int unk08;
} TrackState;

typedef struct DebrisState {
  short vel[3]; /* 0x00 */
  short spinX;  /* 0x06 */
  short spinY;  /* 0x08 */
  short spinZ;  /* 0x0A */
  int unk0C;
  int unk10;
} DebrisState;

extern Actor *AllocActorRecordSlot(void);
extern void InitActorRenderDefaults(Actor *rec);
extern void InsertActorIntoSpatialGrid(Actor *rec);
extern void HideActorRenderRecord(Actor *rec);
extern void EncodeCachedVecToActorDirCode(Actor *rec);          /* EncodeCachedVecToActorDir */
extern int FindGroundHeightBelow(int *pos, int drop);

extern Actor *g_pActorListBase;
extern int g_anSpyroWorldPos[];
extern int g_anSpyroVelocity[3];
extern int g_nGemPickupSubstate;
extern short D_8006F3A0[]; /* launch-direction table, x */
extern short D_8006F3A2[]; /* launch-direction table, z */
extern int g_anSpyroFirstContactPoint[3];
extern int g_anSpyroFirstContactNormal[3];
extern int func_80037EA0(int lo, int hi);       /* RandomInRange */
extern void func_80052D64(Actor *parent, int slot, int *dst); /* GetActorAttachPoint */
extern unsigned short g_nCameraEulerPitch;  /* tracked world yaw (byte) */
extern short g_nCameraEulerYaw;           /* tracked world pitch */
extern void func_80052D64(Actor *parent, int slot, int *dst); /* PlaceActorAtMountPoint */         /* ArcTan2 */

Actor *func_level_19_8008465C(int type, Actor *parent) {
  Actor *rec;
  int owner;

  rec = AllocActorRecordSlot();
  rec->type = type;
  if (parent == 0 || (unsigned int)(owner = parent - g_pActorListBase) >= 0x100) {
    owner = 0;
  }
  rec->unk56 = owner;

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
    rec->unk49 = 2;
    rec->unk50 = 0x18;
    rec->unk52 = 0x10;
    rec->unk44 = 0x20;
    rec->unk45 = 0;
    rec->unk46 = 0;
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

  case 0x20: {
    TrackState *st = rec->state;

    InitActorRenderDefaults(rec);
    st->owner = parent;
    func_80052D64(parent, 0, &rec->posX);
    InsertActorIntoSpatialGrid(rec);
    rec->unk45 = g_nCameraEulerPitch;
    rec->unk46 = (g_nCameraEulerYaw + 0x400) >> 4;
    break;
  }

  case 0x22:
    InitActorRenderDefaults(rec);
    rec->unk50 = 0x20;
    rec->unk52 = 0xFF;
    if (parent != 0) {
      CopyVector(&rec->posX, &parent->posX);
    }
    HideActorRenderRecord(rec);
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
    rec->unk49 = 2;
    rec->unk50 = 0x18;
    rec->unk52 = 0x40;
    rec->unk44 = 0x20;
    rec->unk45 = 0;
    rec->unk46 = 0;
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

  case 0x5D: {
    DebrisState *st = rec->state;
    int yaw;
    int pitch;
    int speed;

    InitActorRenderDefaults(rec);
    rec->unk50 = 0x20;
    CopyVector(&rec->posX, &parent->posX);
    InsertActorIntoSpatialGrid(rec);
    if (parent->type == 0x23) {
      DebrisState *pst = parent->state;

      yaw = ((pst->unk0C + func_80037EA0(-0x32, 0x32)) & 0xFF) << 4;
      pitch = GetRandomU32() & 0x7FF;
      if (LookupCosine(pitch) >> 4 > 0) {
        speed = LookupCosine(pitch) >> 4;
      } else {
        speed = -(LookupCosine(pitch) >> 4);
      }
      st->vel[0] = speed * LookupCosine(yaw) >> 12;
      st->vel[1] = speed * LookupSine(yaw) >> 12;
      st->vel[2] = LookupSine(pitch) >> 4;
      st->unk0C = func_80037EA0(0x23, 0x32);
      rec->unk57 = 0x20 - func_80037EA0(6, 0x14);
      st->vel[0] += g_anSpyroVelocity[0] >> 6;
      st->vel[1] += g_anSpyroVelocity[1] >> 6;
      st->vel[2] += g_anSpyroVelocity[2] >> 6;
    } else {
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
      st->unk0C = 0x40 - (GetRandomU32() & 0xF);
    }
    rec->posX += st->vel[0] * 4;
    rec->posY += st->vel[1] * 4;
    rec->posZ += st->vel[2] * 4;
    st->spinX = GetRandomU32() & 0xF;
    st->spinY = GetRandomU32() & 0xF;
    st->spinZ = GetRandomU32() & 0xF;
    rec->unk44 += (unsigned char)st->spinX * 4;
    rec->unk45 += (unsigned char)st->spinY * 4;
    rec->unk46 += (unsigned char)st->spinZ * 4;
    st->unk10 = parent->posZ - 0x40;
    *(int *)&rec->unk4C = 0x1000000;
    rec->unk4B |= 0x80;
    break;
  }

  case 0x78: {
    SpinState *st = rec->state;

    InitActorRenderDefaults(rec);
    InsertActorIntoSpatialGrid(rec);
    rec->unk49 = 0;
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

  case 0xEA:
    InitActorRenderDefaults(rec);
    CopyVector(&rec->posX, g_anSpyroFirstContactPoint);
    rec->unk46 = ArcTan2(g_anSpyroFirstContactNormal[0], g_anSpyroFirstContactNormal[1], 0) + 0x40;
    InsertActorIntoSpatialGrid(rec);
    break;

  case 0x43 ... 0x45:
  case 0xFF ... 0x101:
  case 0x135 ... 0x137:
  case 0x1A7 ... 0x1A9: {
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

  case 0xFB: {
    ShotState *st = rec->state;
    int dir[3];
    int unused[2];
    int i;

    InitActorRenderDefaults(rec);
    rec->unk50 = 0x20;
    rec->unk52 = 0xFF;
    HideActorRenderRecord(rec);
    rec->unk4C = 0;
    rec->unk4D = 0;
    rec->unk4E = 0;
    rec->unk4F = 0xE;
    if (g_nGemPickupSubstate == 3) {
      rec->unk57 = 0x14;
    } else if (g_nGemPickupSubstate == 1) {
      rec->unk57 = 0x30;
    }
    i = GetRandomU32() & 7;
    dir[0] = D_8006F3A0[i * 2];
    dir[1] = 0;
    dir[2] = D_8006F3A2[i * 2];
    RotateVectorByMatrix(parent->unk20, dir, dir);
    dir[0] -= 0x3F - (GetRandomU32() & 0x7F);
    dir[1] -= 0x3F - (GetRandomU32() & 0x7F);
    dir[2] -= 0x3F - (GetRandomU32() & 0x7F);
    AddVector(&rec->posX, &parent->posX, dir);
    CopyVector(st->vel, dir);
    RShiftVector3(st->vel, 2);
    st->vel[0] -= 0x7F - (GetRandomU32() & 0xFF);
    st->vel[1] -= 0x7F - (GetRandomU32() & 0xFF);
    st->vel[2] -= 0x7F - (GetRandomU32() & 0xFF);
    rec->unk44 = GetRandomU32();
    rec->unk45 = GetRandomU32();
    rec->unk46 = GetRandomU32();
    st->unk10 = GetRandomU32() & 0xF;
    st->unk11 = GetRandomU32() & 0xF;
    st->unk12 = GetRandomU32() & 0xF;
    st->unk0C = parent->posZ;
    st->unk13 = (GetRandomU32() & 3) + 0x10;
    break;
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

  case 0x157: {
    TrackState *st = rec->state;

    InitActorRenderDefaults(rec);
    CopyVector(&rec->posX, &parent->posX);
    st->owner = parent;
    st->unk04 = 0;
    st->unk08 = 0;
    rec->unk50 = 0;
    InsertActorIntoSpatialGrid(rec);
    rec->unk51 = 0;
    break;
  }

  case 0x188:
    InitActorRenderDefaults(rec);
    rec->unk47 = 5;
    if (parent != 0) {
      CopyVector(&rec->posX, &parent->posX);
    } else {
      CopyVector(&rec->posX, g_anSpyroWorldPos);
    }
    InsertActorIntoSpatialGrid(rec);
    break;

  case 0x1FB ... 0x1FC: {
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
    rec->unk52 = 0xFF;
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
