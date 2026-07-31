/* func_level_33_80082F24 (0x80082F24, level_33_gnastys_world_gnasty_gnorc
 * overlay, 0x7CC bytes).
 *
 * Spawn one actor ("moby") of type `type`, parented to `parent`.
 *
 * Allocates a pool record (func_800524C4 = AllocActorRecordSlot), stamps the
 * type into +0x36 and the parent's pool index into +0x56 (the 0x58-byte-record
 * pointer difference, clamped away when it is not a real pool slot), then runs
 * a per-type init arm. Every arm starts from func_8003A720
 * (InitActorRenderDefaults) and ends by placing the new actor: most copy the
 * parent's position (func_80017700 = CopyVector) and either register it in the
 * spatial grid (func_800526A8) or hide its render record (func_800529CC).
 *
 * The interesting arms:
 *  - 0xE / 0x53..0x57 (Gnasty's cauldron flames): fill the flame state block,
 *    take a ground probe (func_8004D5EC) 0x400 above the spawn point, encode
 *    the cached direction (func_800533D0) and select the per-type flame
 *    variant byte at +0x4F.
 *  - 0x43..0x45 / 0x97 / 0xFF..0x101 / 0x135..0x137 (debris / loot bursts):
 *    give the fragment a random unit velocity built from a random yaw
 *    (0xFFF) and pitch (0x7FF) through LookupCosine/LookupSine, add a sixth
 *    of Spyro's velocity (D_80078B4C..54) when the parent is flagged 0x20000,
 *    nudge the spawn point along it, and randomise the tumble rates. The
 *    0x135..0x137 sub-range additionally gets the 0xA18618 tint and the
 *    +0x4B bit 7 flag.
 *  - 0x16B..0x16C / 0x1FB..0x1FC: drop 0x200 and snap to the ground below
 *    when it is within 0x800, otherwise back off by 0x200.
 *
 * With no parent the position falls back to Spyro's (D_80078A58).
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

/* Debris state block (types 0x43..0x45, 0x97, 0xFF..0x101, 0x135..0x137). */
typedef struct DebrisState {
  short vel[3]; /* 0x00 */
  short spinX;  /* 0x06 */
  short spinY;  /* 0x08 */
  short spinZ;  /* 0x0A */
  int unk0C;
  int unk10;
} DebrisState;

extern Actor *func_800524C4(void);              /* AllocActorRecordSlot */
extern void func_8003A720(Actor *rec);          /* InitActorRenderDefaults */
extern void func_80017700(int *dst, int *src);  /* CopyVector */
extern void func_800526A8(Actor *rec);          /* InsertActorIntoSpatialGrid */
extern void func_800529CC(Actor *rec);          /* HideActorRenderRecord */
extern void func_800533D0(Actor *rec);          /* EncodeCachedVecToActorDir */
extern int func_8004D5EC(int *pos, int drop);   /* FindGroundHeightBelow */
extern unsigned int func_8006272C(void);        /* GetRandomU32 */
extern int func_80016CB0(int angle);            /* LookupCosine */
extern int func_80016C58(int angle);            /* LookupSine */

extern Actor *D_80075828;  /* g_pActorListBase */
extern int D_80078A58[];   /* g_anSpyroWorldPos */
extern int D_80078B4C[3];  /* g_anSpyroVelocity */

Actor *func_level_33_80082F24(int type, Actor *parent) {
  Actor *rec;
  int owner;

  rec = func_800524C4();
  rec->type = type;
  if (parent == 0 || (unsigned int)(owner = parent - D_80075828) >= 0x100) {
    owner = 0;
  }
  rec->unk56 = owner;

  switch (type) {
  case 0xF: {
    FlameState *st = rec->state;

    func_8003A720(rec);
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
      func_80017700(&rec->posX, &parent->posX);
    }
    func_800526A8(rec);
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

    func_8003A720(rec);
    func_800526A8(rec);
    pos = &rec->posX;
    func_80017700(pos, &parent->posX);
    rec->posZ += 0x200;
    func_80017700(st->anchor, pos);
    st->unk13 = 0;
    st->unk12 = 0;
    st->unk14 = 0x708;
    break;
  }

  case 0x22:
    func_8003A720(rec);
    rec->unk50 = 0x20;
    rec->unk52 = 0xFF;
    if (parent != 0) {
      func_80017700(&rec->posX, &parent->posX);
    }
    func_800529CC(rec);
    break;

  case 0xE:
  case 0x53 ... 0x57: {
    FlameState *st = rec->state;
    int *pos;
    int probe[3];

    func_8003A720(rec);
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
    func_80017700(pos, &parent->posX);
    func_800529CC(rec);
    rec->unk1C = -1;
    func_80017700(probe, pos);
    probe[2] += 0x400;
    func_8004D5EC(probe, 0x10000);
    func_800533D0(rec);
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

    func_8003A720(rec);
    func_800526A8(rec);
    rec->unk49 = 0;
    st->unk00 = 0;
    st->unk08 = 0;
    st->unk06 = 0;
    st->unk04 = 0;
    st->unk0C = 0;
    st->unk10 = 0;
    if (parent != 0) {
      func_80017700(&rec->posX, &parent->posX);
    }
    break;
  }

  case 0x98 ... 0x99:
    func_8003A720(rec);
    rec->unk50 = 0x20;
    rec->unk52 = 0xFF;
    func_800529CC(rec);
    rec->unk4C = 0;
    rec->unk4D = 0;
    rec->unk4E = 0;
    rec->unk4F = 0xD;
    break;

  case 0x43 ... 0x45:
  case 0x97:
  case 0xFF ... 0x101:
  case 0x135 ... 0x137: {
    DebrisState *st = rec->state;
    int yaw;
    int pitch;

    func_8003A720(rec);
    rec->unk50 = 0x20;
    func_80017700(&rec->posX, &parent->posX);
    func_800526A8(rec);
    yaw = func_8006272C() & 0xFFF;
    pitch = func_8006272C() & 0x7FF;
    st->vel[0] = (func_80016CB0(pitch) >> 5) * func_80016CB0(yaw) >> 12;
    st->vel[1] = (func_80016CB0(pitch) >> 5) * func_80016C58(yaw) >> 12;
    st->vel[2] = func_80016C58(pitch) >> 5;
    if (parent->flags & 0x20000) {
      st->vel[0] += D_80078B4C[0] >> 6;
      st->vel[1] += D_80078B4C[1] >> 6;
      st->vel[2] += D_80078B4C[2] >> 6;
    }
    rec->posX += st->vel[0] * 4;
    rec->posY += st->vel[1] * 4;
    rec->posZ += st->vel[2] * 4;
    st->spinX = func_8006272C() & 0xF;
    st->spinY = func_8006272C() & 0xF;
    st->spinZ = func_8006272C() & 0xF;
    st->unk10 = parent->posZ - 0x40;
    st->unk0C = 0x40 - (func_8006272C() & 0xF);
    if ((unsigned int)((unsigned short)rec->type - 0x135) < 3) {
      *(int *)&rec->unk4C = 0xA18618;
      rec->unk4B |= 0x80;
    }
    break;
  }

  case 0x104 ... 0x10D:
  case 0x115:
  case 0x147: {
    int *st = rec->state;

    func_8003A720(rec);
    func_800529CC(rec);
    rec->unk4F = 2;
    rec->unk4C = 0;
    rec->unk4D = 0;
    rec->unk4E = 0;
    *st = 0x40;
    break;
  }

  case 0x16B ... 0x16C:
  case 0x1FB ... 0x1FC: {
    int ground;
    int z;
    int gap;
    func_8003A720(rec);
    if (parent != 0) {
      func_80017700(&rec->posX, &parent->posX);
    } else {
      func_80017700(&rec->posX, D_80078A58);
    }
    rec->posZ += 0x200;
    ground = func_8004D5EC(&rec->posX, 0x800);
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
    func_800526A8(rec);
    break;
  }

  case 0x4C:
  case 0x1AA ... 0x1C3:
    func_8003A720(rec);
    rec->unk50 = 0x20;
    rec->unk52 = 0xFF;
    func_800529CC(rec);
    break;

  default:
    func_8003A720(rec);
    if (parent != 0) {
      func_80017700(&rec->posX, &parent->posX);
    } else {
      func_80017700(&rec->posX, D_80078A58);
    }
    func_800526A8(rec);
    break;
  }

  return rec;
}
