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
 *    into an `anim` temp above the four `rec->unk3C..3F = 0` stores (which are
 *    themselves in original order 3D, 3C, 3F, 3E), so the load chain schedules
 *    ahead of the store batch and `sb anim,65(rec)` still lands in the `j`
 *    delay slot.
 * 3. Arm 0x26's `if (parent->type == type)` must compare against the LITERAL
 *    0x26. Comparing against the `type` parameter keeps it live in `s0` across
 *    the arm and rotates s0/s1/s2 through 10 insns; the literal rematerialises
 *    as `li v0,38` exactly as the original does.
 */

#include "vecmath.h"

/* Level16_SpawnActor (0x80083AB4, level_16_magic_crafters_blowhard
 * overlay, 0xD7C bytes).
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
 */

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
  int unk38;           /* 0x38 */
  unsigned char unk3C; /* 0x3C */
  unsigned char unk3D;
  unsigned char unk3E;
  unsigned char unk3F;
  unsigned char unk40; /* 0x40 */
  unsigned char unk41;
  unsigned char unk42;
  unsigned char unk43;
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
typedef struct DebrisState {
  short vel[3]; /* 0x00 */
  short spinX;  /* 0x06 */
  short spinY;  /* 0x08 */
  short spinZ;  /* 0x0A */
  int unk0C;
  int unk10;
} DebrisState;

/* Whirlwind state block (type 0x25). */
typedef struct WindState {
  Actor *owner; /* 0x00 */
  short unk04;
  short unk06;
  int unk08;
  int unk0C;
  unsigned char unk10;
} WindState;

/* Turret-shot state block (type 0x27). */
typedef struct AimState {
  int unk00;
  Actor *owner; /* 0x04 */
  int unk08;
} AimState;

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
extern int RandomIntInRange(int lo, int hi);       /* RandomInRange */
extern int g_anCameraPos;     /* turret aim target X */
extern int g_nCameraPosY;     /* turret aim target Y */
extern int g_nSpyroWorldPosY;     /* g_anSpyroWorldPos[1] */

extern Actor *g_pActorListBase;
extern int g_anSpyroWorldPos[];
extern int g_anSpyroVelocity[3];
extern int g_nGemPickupSubstate;
extern short D_8006F3A0[]; /* launch-direction table, x */
extern short D_8006F3A2[]; /* launch-direction table, z */
extern int g_anSpyroFirstContactPoint[3];
extern unsigned char **g_apActorMeshTable[];  /* per-type actor asset table */
extern int g_nSpyroWorldPosZ;     /* g_anSpyroWorldPos[2] */
extern void (*g_pfnLevelOverlayParticleSpawn)(int kind, int count, int *pos, int spread); /* SpawnPuff */
extern int g_anSpyroFirstContactNormal[3];
extern int g_nSpyroGroundHeightZ;     /* g_nSpyroGroundHeight */
extern short g_anCosineLut[]; /* unit-circle x table (8-bit angle) */
extern short g_anSineLut[]; /* unit-circle y table (8-bit angle) */

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
  int new_var;
  int new_var2;
  rec = AllocActorRecordSlot();
  rec->type = type;
  if ((parent == 0) || (((unsigned int) (owner = parent - g_pActorListBase)) >= 0x100))
  {
    owner = 0;
  }
  rec->unk56 = owner;
  switch (type)
  {
    case 0xF:
    {
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
      if (parent != 0)
      {
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

    case 0x10:
    {
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
      rec->unk52 = 0xFF;
      if (parent != 0)
    {
      CopyVector(&rec->posX, &parent->posX);
    }
      HideActorRenderRecord(rec);
      break;

    case 0x25:
    {
      WindState *st = rec->state;
      int probe[3];
      int anim;
      new_var = 2;
      new_var2 = 2;
      InitActorRenderDefaults(rec);
      rec->unk1C = -1;
      CopyVector(probe, &rec->posX);
      probe[2] += 0x400;
      FindGroundHeightBelow(probe, 0x10000);
      EncodeCachedVecToActorDirCode(rec);
      switch (parent->unk48)
      {
        case 0x48:
          st->unk10 = 1;
          st->unk04 = 0x40;
          st->unk06 = 0xE00;
          break;

        case 0x52:
          st->unk10 = 2;
          st->unk04 = 0x100;
          st->unk06 = 0x2000;
          break;

        case 0x5C:
          st->unk10 = 3;
          st->unk04 = 0x140;
          st->unk06 = 0x2000;
          break;

      }

      rec->posX = parent->posX + (g_anCosineLut[parent->unk46] >> new_var);
      rec->posY = parent->posY + (g_anSineLut[parent->unk46] >> new_var2);
      rec->posZ = parent->posZ;
      st->owner = parent;
      st->unk0C = 0;
      rec->unk48 = 0;
      anim = g_apActorMeshTable[rec->type][0xE][0xC];
      rec->unk3D = 0;
      rec->unk3C = 0;
      rec->unk3F = 0;
      rec->unk3E = 0;
      rec->unk41 = anim;
      InsertActorIntoSpatialGrid(rec);
      break;
    }

    case 0x26:
    {
      int *st = rec->state;
      int *pos;
      int turn;
      InitActorRenderDefaults(rec);
      pos = &rec->posX;
      CopyVector(pos, &parent->posX);
      if (parent->type == 0x26)
      {
        rec->unk46 = parent->unk46;
        rec->unk48 = 3;
        rec->unk40 = 0;
        rec->unk41 = g_apActorMeshTable[rec->type][0x11][0xC];
        rec->unk3C = 3;
        rec->unk3D = 3;
        *st = 3;
      }
      else
        if (parent->unk3F >= 6)
      {
        rec->posX += LookupCosine(parent->unk46 << 4) >> 2;
        rec->posY += LookupSine(parent->unk46 << 4) >> 2;
        rec->posZ += 0x12C;
        rec->unk46 = parent->unk46;
        turn = (ArcTan2(ApproxDist2D(pos, g_anSpyroWorldPos), g_nSpyroWorldPosZ - rec->posZ, 0) - parent->unk45) & 0xFF;
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
        rec->unk45 = parent->unk45 + turn;
        *st = 0x80;
        rec->unk48 = 2;
        rec->unk40 = 0;
        rec->unk41 = g_apActorMeshTable[rec->type][0x10][0xC];
        rec->unk3C = 2;
        rec->unk3D = 2;
        g_pfnLevelOverlayParticleSpawn(4, 7, pos, 0x10);
        InsertActorIntoSpatialGrid(rec);
        break;
      }
      else
      {
        if (parent->unk3F >= 2)
        {
          rec->posZ += 0x12C;
          rec->unk46 = parent->unk46;
          rec->unk48 = 1;
          rec->unk40 = 0;
          rec->unk41 = g_apActorMeshTable[rec->type][0xF][0xC];
          rec->unk3C = 1;
          rec->unk3D = 1;
        }
        else
        {
          rec->posZ += 0x180;
          rec->unk46 = parent->unk46;
        }
        *st = 2;
      }
      InsertActorIntoSpatialGrid(rec);
      break;
    }

    case 0x27:
    {
      AimState *st = rec->state;
      InitActorRenderDefaults(rec);
      CopyVector(&rec->posX, &parent->posX);
      InsertActorIntoSpatialGrid(rec);
      rec->unk46 = ArcTan2(g_anCameraPos - rec->posX, g_nCameraPosY - rec->posY, 0);
      rec->posZ -= 0x400;
      st->unk00 = 0x40;
      st->owner = parent;
      st->unk08 = 1;
      break;
    }

    case 0xE:

    case 0x53 ... 0x57:
    {
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
      if (parent->type == 0xD)
      {
        st->unk10 = 2;
      }
      else
      {
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
      if (rec->type == 0xE)
      {
        rec->unk4F = 0xC;
      }
      if (rec->type == 0x53)
      {
        rec->unk4F = 1;
      }
      if (rec->type == 0x54)
      {
        rec->unk4F = 2;
      }
      if (rec->type == 0x55)
      {
        rec->unk4F = 3;
      }
      if (rec->type == 0x56)
      {
        rec->unk4F = 4;
      }
      if (rec->type == 0x57)
      {
        rec->unk4F = 5;
      }
      break;
    }

    case 0x78:
    {
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
      if (parent != 0)
      {
        CopyVector(&rec->posX, &parent->posX);
      }
      break;
    }

    case 0x98 ... 0x99:
      InitActorRenderDefaults(rec);
      rec->unk50 = 0x20;
      rec->unk52 = 0xFF;
      HideActorRenderRecord(rec);
      rec->unk4C = 0;
      rec->unk4D = 0;
      rec->unk4E = 0;
      rec->unk4F = 0xD;
      break;

    case 0x43 ... 0x45:

    case 0x97:

    case 0xFF ... 0x101:

    case 0x135 ... 0x137:
    {
      DebrisState *st = rec->state;
      int yaw;
      int pitch;
      InitActorRenderDefaults(rec);
      rec->unk50 = 0x20;
      CopyVector(&rec->posX, &parent->posX);
      InsertActorIntoSpatialGrid(rec);
      yaw = GetRandomU32() & 0xFFF;
      pitch = GetRandomU32() & 0x7FF;
      st->vel[0] = ((LookupCosine(pitch) >> 5) * LookupCosine(yaw)) >> 12;
      st->vel[1] = ((LookupCosine(pitch) >> 5) * LookupSine(yaw)) >> 12;
      st->vel[2] = LookupSine(pitch) >> 5;
      if (parent->flags & 0x20000)
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
      st->unk10 = parent->posZ - 0x40;
      st->unk0C = 0x40 - (GetRandomU32() & 0xF);
      if (((unsigned int) (((unsigned short) rec->type) - 0x135)) < 3)
      {
        *((int *) (&rec->unk4C)) = 0xA18618;
        rec->unk4B |= 0x80;
      }
      break;
    }

    case 0xFB:
    {
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
      if (g_nGemPickupSubstate == 3)
      {
        rec->unk57 = 0x14;
      }
      else
        if (g_nGemPickupSubstate == 1)
      {
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

    case 0x147:
    {
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
      rec->unk50 = 0x20;
      rec->unk52 = 0xFF;
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
