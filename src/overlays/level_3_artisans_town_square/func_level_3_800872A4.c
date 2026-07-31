/* func_level_3_800872A4 (0x800872A4, level_3_artisans_town_square
 * overlay, 0xB7C bytes).
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
extern int func_80037EA0(int lo, int hi);       /* RandomInRange */

extern Actor *D_80075828;  /* g_pActorListBase */
extern int D_80078A58[];   /* g_anSpyroWorldPos */
extern int D_80078B4C[3];  /* g_anSpyroVelocity */
extern int D_80077058;     /* g_nGemPickupSubstate */
extern short D_8006F3A0[]; /* launch-direction table, x */
extern short D_8006F3A2[]; /* launch-direction table, z */
extern int D_80078BD4[3];  /* g_anSpyroFirstContactPoint */
extern int D_80078BE0[3];  /* g_anSpyroFirstContactNormal */

Actor *func_level_3_800872A4(int type, Actor *parent) {
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

  case 0xEA:
    func_8003A720(rec);
    func_80017700(&rec->posX, D_80078BD4);
    rec->unk46 = func_80016AB4(D_80078BE0[0], D_80078BE0[1], 0) + 0x40;
    func_800526A8(rec);
    break;

  case 0x43 ... 0x45:
  case 0xFF ... 0x101:
  case 0x135 ... 0x137:
  case 0x1A7 ... 0x1A9: {
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

  case 0xFB: {
    ShotState *st = rec->state;
    int dir[3];
    int unused[2];
    int i;

    func_8003A720(rec);
    rec->unk50 = 0x20;
    rec->unk52 = 0xFF;
    func_800529CC(rec);
    rec->unk4C = 0;
    rec->unk4D = 0;
    rec->unk4E = 0;
    rec->unk4F = 0xE;
    if (D_80077058 == 3) {
      rec->unk57 = 0x14;
    } else if (D_80077058 == 1) {
      rec->unk57 = 0x30;
    }
    i = func_8006272C() & 7;
    dir[0] = D_8006F3A0[i * 2];
    dir[1] = 0;
    dir[2] = D_8006F3A2[i * 2];
    func_80017048(parent->unk20, dir, dir);
    dir[0] -= 0x3F - (func_8006272C() & 0x7F);
    dir[1] -= 0x3F - (func_8006272C() & 0x7F);
    dir[2] -= 0x3F - (func_8006272C() & 0x7F);
    func_80017758(&rec->posX, &parent->posX, dir);
    func_80017700(st->vel, dir);
    func_800176C8(st->vel, 2);
    st->vel[0] -= 0x7F - (func_8006272C() & 0xFF);
    st->vel[1] -= 0x7F - (func_8006272C() & 0xFF);
    st->vel[2] -= 0x7F - (func_8006272C() & 0xFF);
    rec->unk44 = func_8006272C();
    rec->unk45 = func_8006272C();
    rec->unk46 = func_8006272C();
    st->unk10 = func_8006272C() & 0xF;
    st->unk11 = func_8006272C() & 0xF;
    st->unk12 = func_8006272C() & 0xF;
    st->unk0C = parent->posZ;
    st->unk13 = (func_8006272C() & 3) + 0x10;
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

  case 0x188:
    func_8003A720(rec);
    rec->unk47 = 5;
    if (parent != 0) {
      func_80017700(&rec->posX, &parent->posX);
    } else {
      func_80017700(&rec->posX, D_80078A58);
    }
    func_800526A8(rec);
    break;

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

  case 0x1F6: {
    ShardState *st = rec->state;
    int yaw;
    int pitch;

    func_8003A720(rec);
    func_80017700(&rec->posX, &parent->posX);
    func_800526A8(rec);
    yaw = func_8006272C() & 0xFFF;
    pitch = func_8006272C() & 0x7FF;
    if (pitch < 0x80) {
      pitch = 0x80;
    }
    if (pitch > 0x780) {
      pitch = 0x780;
    }
    st->vel[0] = (func_80016CB0(pitch) >> 5) * (func_80016CB0(yaw) >> 12);
    st->vel[1] = (func_80016CB0(pitch) >> 5) * (func_80016C58(yaw) >> 12);
    st->vel[2] = func_80016C58(pitch) >> 5;
    if (st->vel[2] < 0x20) {
      st->vel[2] = 0x20;
    }
    st->vel[2] += 0x20;
    rec->unk46 = func_8006272C();
    if ((int)(func_8006272C() & 0xFF) >= 0x81) {
      rec->unk45 = (func_8006272C() & 0x1C) - 0x10;
      st->unk10 = 1;
      if (st->vel[2] > 0x20) {
        st->vel[2] = 0x20;
      }
    }
    rec->posX += st->vel[0] * 4;
    rec->posY += st->vel[1] * 4;
    rec->posZ += st->vel[2] * 4;
    st->spinX = (func_8006272C() & 0x1F) - 0x10;
    st->spinY = (func_8006272C() & 0x1F) - 0x10;
    st->spinZ = (func_8006272C() & 0x1F) - 0x10;
    st->unk0C = 0x32 - func_80037EA0(0, 0x14);
    st->unk0D = 0xA;
    st->unk0E = 4;
    break;
  }
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
