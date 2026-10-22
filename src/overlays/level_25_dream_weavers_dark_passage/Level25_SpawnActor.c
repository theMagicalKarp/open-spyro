/* MATCHED 2026-08-28 — the last of the 36 spawn dispatchers, and the last
 * standing §B19 site. The residue (4/794: the 0x7C arm's `move a0,rec` sat
 * between the first and second byte store, and the third temp fell to a1) fell
 * to a NEW placement of the A232 block splitter: a `do { } while (0);` around
 * the FIRST TWO stores of the batch, so the block boundary lands BETWEEN store
 * 2 and store 3 — not around the whole batch.
 *
 * Why the whole-batch barrier failed here and this one works. Wrapping all
 * three stores leaves the call block as `{move a0,rec; jal}`, which is
 * byte-identical to the head of the shared tail at 0x800857C8, so jump.c
 * cross-jumps it away and the function comes out one insn short (404/794).
 * Splitting after store 2 leaves `{sb 0x46, sw st->owner, move a0,rec, j}`:
 * store 3's value now arrives from the PREVIOUS block, so it is a priority-1
 * straggler exactly like the arg copy, and rank_for_schedule's LUID-descending
 * tiebreak emits the two in source order — the original's order. The block is
 * three insns long, so there is no tail for jump.c to merge.
 *
 * Generalises to: when a whole-batch A232 barrier over-merges into a shared
 * tail, move the boundary INTO the batch. Leaving one store behind gives the
 * following block a same-priority straggler to lose to, at zero insn cost.
 */

#include "vecmath.h"
#include "level_globals.h"
#include "level_spawn_actor.h"

/* Level25_SpawnActor (0x80084B94, level_25_dream_weavers_dark_passage
 * overlay, 0xc68 bytes).
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
 *    the g_anLaunchDirTable/g_anLaunchDirTableZ table, rotate it into the parent's orientation
 *    (RotateVectorByMatrix = RotateVectorByMatrix), jitter each component by
 *    +-0x3F, offset the spawn point along it (AddVector = AddVector) and
 *    seed the shot's velocity (RShiftVector3 = RShiftVector3) with a further
 *    +-0x7F of jitter plus randomised spin rates and fuse.
 *  - 0x16B..0x16C: drop 0x200 and snap to the ground below when it is within
 *    0x800, otherwise back off by 0x200.
 *
 * With no parent the position falls back to Spyro's (g_anSpyroWorldPos).
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
  int unk38[3];        /* 0x38 */
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

/* Debris state block (types 0x43..0x45, 0xFF..0x101, 0x135..0x137). */
/* Parent-anchored tracker state block. */
typedef struct TrackState {
  Actor *owner;        /* 0x00 */
  int unk04;
  int unk08;
} TrackState;

extern Actor *AllocActorRecordSlot(void);
extern void InitActorRenderDefaults(Actor *rec);
extern void InsertActorIntoSpatialGrid(Actor *rec);
extern void HideActorRenderRecord(Actor *rec);
extern void EncodeCachedVecToActorDirCode(Actor *rec);          /* EncodeCachedVecToActorDir */

extern Actor *g_pActorListBase;
extern int g_anSpyroWorldPos[];
extern int g_nSpyroWorldPosY;     /* g_anSpyroWorldPos[1] */
extern int abs(int n);
extern void GetActorAttachPoint(Actor *parent, int slot, int *dst);

Actor *Level25_SpawnActor(int type, Actor *parent) {
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

  case 0x22:
    InitActorRenderDefaults(rec);
    rec->unk50 = 0x20;
    rec->updateDist = 0xFF;
    if (parent != 0) {
      CopyVector(&rec->posX, &parent->posX);
    }
    HideActorRenderRecord(rec);
    break;

  case 0x37: {
    FlameState *st = rec->state;
    int *pos;
    int turn;

    InitActorRenderDefaults(rec);
    pos = &rec->posX;
    GetActorAttachPoint(parent, 0, pos);
    rec->rotX = 0;
    turn = ArcTan2(ApproxDist2D(pos, (int *)g_anSpyroWorldPos),
                         g_nSpyroWorldPosZ - rec->posZ, 0);
    if (turn > 0x80) {
      turn -= 0x100;
    }
    if (turn < -0x10) {
      turn = -0x10;
    }
    if (turn > 0x10) {
      turn = 0x10;
    }
    rec->rotY = turn;
    rec->rotZ = ArcTan2(g_anSpyroWorldPos[0] - rec->posX, g_nSpyroWorldPosY - rec->posY, 0);
    InsertActorIntoSpatialGrid(rec);
    st->unk00 = 0x8C;
    break;
  }

  case 0x38: {
    FlameState *st = rec->state;
    int *pos;
    int turn;

    InitActorRenderDefaults(rec);
    pos = &rec->posX;
    GetActorAttachPoint(parent, 0, pos);
    rec->rotX = 0;
    turn = ArcTan2(ApproxDist2D(pos, (int *)g_anSpyroWorldPos),
                         g_nSpyroWorldPosZ - rec->posZ, 0);
    if (turn > 0x80) {
      turn -= 0x100;
    }
    if (turn < -0x10) {
      turn = -0x10;
    }
    if (turn > 0x10) {
      turn = 0x10;
    }
    rec->rotY = turn;
    rec->rotZ = ArcTan2(g_anSpyroWorldPos[0] - rec->posX, g_nSpyroWorldPosY - rec->posY, 0);
    InsertActorIntoSpatialGrid(rec);
    st->unk00 = 0xF0;
    break;
  }

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

  case 0x7C: {
    TrackState *st = rec->state;
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
    } while (0);
    rec->rotZ = dz;
    st->owner = parent;
    InsertActorIntoSpatialGrid(rec);
    break;
  }

  case 0x98 ... 0x99:
    InitActorRenderDefaults(rec);
    rec->unk50 = 0x20;
    rec->updateDist = 0xFF;
    HideActorRenderRecord(rec);
    rec->unk4C = 0;
    rec->unk4D = 0;
    rec->unk4E = 0;
    rec->unk4F = 0xD;
    break;

  case 0x9B: {
    FlameState *st = rec->state;
    int *pos;
    int turn;
    int pdir;
    int diff;
    int delta;

    InitActorRenderDefaults(rec);
    pos = &rec->posX;
    GetActorAttachPoint(parent, 0, pos);
    rec->rotX = 0;
    turn = ArcTan2(ApproxDist2D(pos, (int *)g_anSpyroWorldPos),
                         g_nSpyroWorldPosZ - rec->posZ, 0);
    if (turn > 0x80) {
      turn -= 0x100;
    }
    if (turn < -0x10) {
      turn = -0x10;
    }
    if (turn > 0x10) {
      turn = 0x10;
    }
    rec->rotY = turn;
    rec->rotZ = ArcTan2(g_anSpyroWorldPos[0] - rec->posX, g_nSpyroWorldPosY - rec->posY, 0);
    pdir = parent->rotZ;
    diff = rec->rotZ - pdir;
    delta = (unsigned char)diff;
    if (delta > 0x80) {
      delta -= 0x100;
    }
    if (abs(delta) > 0x10) {
      rec->rotZ = pdir;
    }
    InsertActorIntoSpatialGrid(rec);
    st->unk00 = 0xF0;
    break;
  }

  case 0x43 ... 0x45:
  case 0x97:
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
      rec->renderList |= 0x80;
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
    rec->updateDist = 0xFF;
    HideActorRenderRecord(rec);
    rec->unk4C = 0;
    rec->unk4D = 0;
    rec->unk4E = 0;
    rec->unk4F = 0xE;
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
