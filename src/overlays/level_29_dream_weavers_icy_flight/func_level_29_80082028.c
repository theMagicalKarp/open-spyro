/* func_level_29_80082028 (0x80082028, level_29_dream_weavers_icy_flight
 * overlay, 0xd6c bytes).
 *
 * Spawn one actor ("moby") of type `type`, parented to `parent`.
 *
 * Allocates a pool record (func_800524C4 = AllocActorRecordSlot), stamps the
 * type into +0x36 and the parent's pool index into +0x56, then runs a per-type
 * init arm: every arm starts from func_8003A720 (InitActorRenderDefaults) and
 * ends by placing the new actor -- most copy the parent's position
 * (func_80017700 = CopyVector) and either register it in the spatial grid
 * (func_800526A8) or hide its render record (func_800529CC).  With no parent
 * the position falls back to Spyro's (D_80078A58).
 *
 * Icy Flight is a flight level, so the arm set is the flight one shared with
 * levels 5/11/17/23 (thermal rings 0x104..0x10D, the race-object arms around
 * 0x159..0x15B and the archway/gate group 0x1AA..0x1C3), minus the
 * "attitude-copy" arms the other four carry.  Four arms are unique to it:
 *  - 0x4E: bare init, straight into the spatial grid.
 *  - 0xE8: spawn 0x400 above the parent, then aim at Spyro -- ArcTan2 of the
 *    horizontal offset (func_80016AB4) fed through func_80038178 with Spyro's
 *    8-bit body yaw.
 *  - 0x167..0x169: a scatter burst on a random 8-bit heading, read out of the
 *    unit-circle tables (D_8006CC78/D_8006CBF8) and shifted down 7, with a
 *    coin-flipped +-0x5A vertical kick and randomised tumble rates.
 *  - 0x16A: launch off one of four diagonal quadrants (+-0x200, +-0x200,
 *    0x280) picked by two random bits, rotated into the parent's orientation
 *    (func_80017048 = RotateVectorByMatrix) and added to the parent's position.
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

extern Actor *func_800524C4(void);              /* AllocActorRecordSlot */
extern void func_8003A720(Actor *rec);          /* InitActorRenderDefaults */
extern void func_80017700(int *dst, int *src);  /* CopyVector */
extern void func_800526A8(Actor *rec);          /* InsertActorIntoSpatialGrid */
extern void func_800529CC(Actor *rec);          /* HideActorRenderRecord */
extern void func_800533D0(Actor *rec);          /* EncodeCachedVecToActorDir */
extern int func_8004D5EC(int *pos, int drop);   /* FindGroundHeightBelow */
extern void func_80017048(int *mtx, int *src, int *dst); /* RotateVectorByMtx */
extern void func_80017758(int *dst, int *a, int *b);     /* AddVector */
extern void func_800176C8(int *vec, int shift);          /* RShiftVector3 */
extern unsigned int func_8006272C(void);        /* GetRandomU32 */
extern int func_80016CB0(int angle);            /* LookupCosine */
extern int func_80016C58(int angle);            /* LookupSine */
extern int func_80016AB4(int y, int x, int mode);         /* ArcTan2 */
extern int func_80017990(int *a, int *b);       /* HorizontalDistance */
extern int func_800171FC(int *v, int mode);     /* VectorLength */
extern void func_800175B8(int *v, int len, int scale);  /* ScaleVectorToLength */
extern int func_80037EA0(int lo, int hi);       /* RandomInRange */
extern int func_80038178(int yaw, unsigned char pitch, int speed, int lift);
extern int D_80078A5C;     /* g_anSpyroWorldPos[1] */

extern Actor *D_80075828;  /* g_pActorListBase */
extern int D_80078A58[];   /* g_anSpyroWorldPos */
extern int D_80078B4C[3];  /* g_anSpyroVelocity */
extern int D_80077058;     /* g_nGemPickupSubstate */
extern short D_8006F3A0[]; /* launch-direction table, x */
extern short D_8006F3A2[]; /* launch-direction table, z */
extern int D_80078BD4[3];  /* g_anSpyroFirstContactPoint */
extern unsigned char **D_80076378[];  /* per-type actor asset table */
extern int D_80078A60;     /* g_anSpyroWorldPos[2] */
extern void (*D_800758E4)(int kind, int count, int *pos, int spread); /* SpawnPuff */
extern int D_80078BE0[3];  /* g_anSpyroFirstContactNormal */
extern int D_80078AF8;     /* g_nSpyroGroundHeight */
extern short D_8006CC78[]; /* unit-circle x table (8-bit angle) */
extern short D_8006CBF8[]; /* unit-circle y table (8-bit angle) */
extern unsigned char D_80078A66;  /* g_bSpyroBodyYaw (8-bit) */
extern int D_8007595C;     /* active flight-race slot index */
extern int D_80078618[];   /* per-slot flight-race object table */
extern void func_80052568(Actor *rec);          /* ReleaseActorRecordSlot */

Actor *func_level_29_80082028(int type, Actor *parent) {
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

  case 0x4E:
    func_8003A720(rec);
    func_800526A8(rec);
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

  case 0xE8:
  {
    AimState *st = rec->state;

    func_8003A720(rec);
    func_80017700(&rec->posX, &parent->posX);
    rec->posZ += 0x400;
    func_800526A8(rec);
    st->unk04 = 0xC8;
    st->unk08 = 0x15E;
    st->unk00 = func_80038178(
        func_80016AB4(rec->posX - D_80078A58[0], rec->posY - D_80078A5C, 0),
        D_80078A66, 0x28, 0x40);
    st->unk0C = 0x78;
    break;
  }

  case 0x1DE ... 0x1E0: {
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

  case 0x1E1 ... 0x1E2: {
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
    st->vel[0] += D_80078B4C[0] >> 6;
    st->vel[1] += D_80078B4C[1] >> 6;
    st->vel[2] += D_80078B4C[2] >> 6;
    rec->posX += st->vel[0] * 4;
    rec->posY += st->vel[1] * 4;
    rec->posZ += st->vel[2] * 4;
    st->spinX = func_8006272C() & 0xF;
    st->spinY = func_8006272C() & 0xF;
    st->spinZ = func_8006272C() & 0xF;
    st->unk10 = parent->posZ - 0x40;
    st->unk0C = 0x40 - (func_8006272C() & 0xF);
    break;
  }

  case 0x167 ... 0x169:
  {
    DebrisState *st = rec->state;
    int seed;

    func_8003A720(rec);
    rec->unk50 = 0x20;
    func_80017700(&rec->posX, &parent->posX);
    func_800526A8(rec);
    seed = func_8006272C();
    rec->unk46 = seed;
    st->vel[0] = D_8006CC78[(unsigned char)seed] >> 7;
    st->vel[1] = D_8006CBF8[rec->unk46] >> 7;
    if (func_8006272C() & 1) {
      st->vel[2] = 0x5A;
    } else {
      st->vel[2] = -0x5A;
      rec->unk44 = 0x80;
    }
    rec->posX += st->vel[0] * 4;
    rec->posY += st->vel[1] * 4;
    rec->posZ += st->vel[2] * 4;
    if (st->vel[2] < 0x14) {
      st->vel[2] = 0x14;
    }
    st->spinX = func_8006272C() & 0xF;
    st->spinY = func_8006272C() & 0xF;
    st->spinZ = func_8006272C() & 0xF;
    st->unk10 = parent->posZ - 0x40;
    st->unk0C = 0x40 - (func_8006272C() & 0xF);
    break;
  }

  case 0x16A:
  {
    DebrisState *st = rec->state;
    int *pst = parent->state;
    int dir[3];

    func_8003A720(rec);
    rec->unk50 = 0x20;
    func_800526A8(rec);
    func_80017700((int *)st, pst + 2);
    st->spinY = 0;
    st->spinZ = 0;
    st->vel[2] += 0x40;
    st->unk10 = parent->posZ - 0x40;
    st->unk0C = 0x40 - (func_8006272C() & 0xF);
    switch ((int)(func_8006272C() & 3)) {
    case 0:
      dir[0] = 0x200;
      dir[1] = 0x200;
      rec->unk46 = 0x40;
      st->spinX = 0x10;
      break;
    case 1:
      dir[0] = 0x200;
      dir[1] = -0x200;
      rec->unk46 = 0xC0;
      st->spinX = -0x10;
      break;
    case 2:
      dir[0] = -0x200;
      dir[1] = 0x200;
      rec->unk46 = 0x40;
      st->spinX = 0x10;
      break;
    case 3:
      dir[0] = -0x200;
      dir[1] = -0x200;
      rec->unk46 = 0xC0;
      st->spinX = -0x10;
      break;
    }
    rec->unk46 += parent->unk46;
    dir[2] = 0x280;
    func_80017048(parent->unk20, dir, dir);
    func_80017758(&rec->posX, dir, &parent->posX);
    return rec;
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

  case 0x159 ... 0x15B:
    if (D_80078618[D_8007595C] != 0) {
      func_800529CC(rec);
      func_80052568(rec);
      rec = 0;
    } else {
      func_8003A720(rec);
      if (parent != 0) {
        func_80017700(&rec->posX, &parent->posX);
      }
      rec->unk50 = 0x40;
      func_800529CC(rec);
      rec->unk4C = 0;
      rec->unk4D = 0;
      rec->unk4E = 0;
      rec->unk4F = 2;
    }
    break;

  case 0x185:
  case 0x18A:
  case 0x1EA ... 0x1EB:
    func_8003A720(rec);
    rec->unk50 = 0;
    rec->posX = -0x64;
    rec->posY = 0x1E;
    rec->posZ = 0x1000;
    func_800529CC(rec);
    rec->unk47 = 0x20;
    rec->unk4C = 0;
    rec->unk4D = 0;
    rec->unk4E = 0;
    rec->unk4F = 0;
    break;

  case 0x18F:
    func_8003A720(rec);
    rec->unk50 = 0xFF;
    rec->posX = 0x1CC;
    rec->posY = 0x28;
    rec->posZ = 0x1000;
    func_800529CC(rec);
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
