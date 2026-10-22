/* Level13_EmitParticles (0x8008C9D8, level_13_magic_crafters_alpine_ridge
 * overlay, 0x10f4 bytes).
 *
 * Push `count` particle records of effect `type` onto the emit list (the
 * 0x20-byte record stream that RasterizeEmitList walks each frame).
 *
 * AllocEmitListRecord(class): hands back the next 0x20-byte
 * slot and stamps the record class at +0x01.  The class picks the record
 * layout: 2/3/6 are sparks (short position at +4, a velocity at +0x18 and an
 * RGB triple at +0xC), 0 is a dust mote (short position + short velocity at
 * +0x10) and 1/4 are streaks (two short endpoints at +4 / +0xA and two RGBA
 * quads at +0x10 / +0x14).
 *
 * `pos` is the emitter position (an int 3-vector) for every effect except
 * 0xC, where it is the source actor and the offset is rotated out of the
 * actor's own matrix.  `arg3` is per-effect: a velocity vector for 0/1, a
 * packed RGB for 0xC, a scale for 0x1A/0x1B, and a flag for 0x21/0x42.
 *
 * MATCHED 2026-08-21, generated from the donor library: arms 0x4, 0x5, 0x8
 * and 0x16 all came from the same session's decodes (0x4/0x5 off level_15,
 * 0x8 off level_16, 0x16 off level_12) and needed no rework.  Arm 0x15's
 * `unk10 = 0` moved below `unk11` -- without it the arm keeps both stores
 * inline instead of cross-jumping the shared `sb t0,0x11; sb zero,0x10` block
 * and the overlay is 2 insns too long to link.
 */

#include "vecmath.h"

typedef struct Emit {   /* one 0x20-byte emit-list record */
  unsigned char type;   /* 0x00 effect id */
  unsigned char cls;    /* 0x01 record class (stamped by the allocator) */
  unsigned char phase;  /* 0x02 */
  unsigned char unk03;  /* 0x03 */
  union {
    struct {                     /* classes 2 / 3 / 6 -- spark */
      short pos[3];              /* 0x04 */
      unsigned char life;        /* 0x0A */
      unsigned char seed;        /* 0x0B */
      unsigned char r;           /* 0x0C */
      unsigned char g;           /* 0x0D */
      unsigned char b;           /* 0x0E */
      unsigned char fx;          /* 0x0F */
      unsigned char unk10;       /* 0x10 */
      unsigned char unk11;       /* 0x11 */
      short pad[3];              /* 0x12 */
      short vel[3];              /* 0x18 */
      short unk1E;               /* 0x1E */
    } spark;
    struct {                     /* class 0 -- dust mote */
      unsigned short pos[3];     /* 0x04 */
      unsigned char life;        /* 0x0A */
      unsigned char seed;        /* 0x0B */
      unsigned char r;           /* 0x0C */
      unsigned char g;           /* 0x0D */
      unsigned char b;           /* 0x0E */
      unsigned char fx;          /* 0x0F */
      short vel[3];              /* 0x10 */
    } dust;
    struct {                     /* class 2 -- ribbon (2nd point at 0x12) */
      short pos[3];              /* 0x04 */
      unsigned char life;        /* 0x0A */
      unsigned char seed;        /* 0x0B */
      unsigned char r;           /* 0x0C */
      unsigned char g;           /* 0x0D */
      unsigned char b;           /* 0x0E */
      unsigned char fx;          /* 0x0F */
      unsigned char unk10;       /* 0x10 */
      unsigned char unk11;       /* 0x11 */
      short pos2[3];             /* 0x12 */
      unsigned char unk18;       /* 0x18 */
      unsigned char unk19;       /* 0x19 */
    } ribbon;
    struct {                     /* class 2 -- band (shorts at 0x18 / 0x1A) */
      short pos[3];              /* 0x04 */
      unsigned char life;        /* 0x0A */
      unsigned char seed;        /* 0x0B */
      unsigned char r;           /* 0x0C */
      unsigned char g;           /* 0x0D */
      unsigned char b;           /* 0x0E */
      unsigned char fx;          /* 0x0F */
      unsigned char unk10;       /* 0x10 */
      unsigned char unk11;       /* 0x11 */
      short pos2[3];             /* 0x12 */
      short unk18;               /* 0x18 */
      short unk1A;               /* 0x1A */
      unsigned char unk1C;       /* 0x1C */
      unsigned char unk1D;       /* 0x1D */
    } band;
    struct {                     /* class 0 -- smear (2nd point at 0x10) */
      short pos[3];              /* 0x04 */
      unsigned char life;        /* 0x0A */
      unsigned char seed;        /* 0x0B */
      unsigned char r;           /* 0x0C */
      unsigned char g;           /* 0x0D */
      unsigned char b;           /* 0x0E */
      unsigned char fx;          /* 0x0F */
      short pos2[3];             /* 0x10 */
      short pad16;               /* 0x16 */
      unsigned char unk18;       /* 0x18 */
      unsigned char unk19;       /* 0x19 */
    } smear;
    struct {                     /* class 3 -- actor-tracking plume */
      unsigned short pos[3];     /* 0x04 */
      unsigned char life;        /* 0x0A */
      unsigned char seed;        /* 0x0B */
      unsigned char r;           /* 0x0C */
      unsigned char g;           /* 0x0D */
      unsigned char b;           /* 0x0E */
      unsigned char fx;          /* 0x0F */
      unsigned char unk10;       /* 0x10 */
      unsigned char unk11;       /* 0x11 */
      short pad[3];              /* 0x12 */
      int *src;                  /* 0x18 the actor position it tracks */
      short unk1C;               /* 0x1C */
      short unk1E;               /* 0x1E */
    } plume;
    struct {                     /* class 3 -- owner-tracked spark */
      unsigned short pos[3];     /* 0x04 */
      unsigned char life;        /* 0x0A */
      unsigned char seed;        /* 0x0B */
      unsigned char r;           /* 0x0C */
      unsigned char g;           /* 0x0D */
      unsigned char b;           /* 0x0E */
      unsigned char fx;          /* 0x0F */
      unsigned char unk10;       /* 0x10 */
      unsigned char unk11;       /* 0x11 */
      short pad[3];              /* 0x12 */
      short vel[3];              /* 0x18 */
      unsigned char owner;       /* 0x1E actor index into the pool */
      unsigned char unk1F;       /* 0x1F */
    } track;
    struct {                     /* classes 1 / 4 -- streak */
      unsigned short a[3];       /* 0x04 head */
      unsigned short b[3];       /* 0x0A tail */
      unsigned char c0[4];       /* 0x10 head colour */
      unsigned char c1[4];       /* 0x14 tail colour */
    } line;
  } u;
} Emit;

typedef struct Actor {
  int *owner;   /* 0x00 */
  char pad04[8];
  int pos[3];   /* 0x0C */
  char pad18[8];
  short mat[9]; /* 0x20 3x3 rotation matrix */
  short pad32;  /* 0x32 */
  short unk34;  /* 0x34 */
  short kind;   /* 0x36 */
  char pad38[0x11];
  unsigned char flag49; /* 0x49 */
  char pad4A[0xE];      /* 0x4A -- the pool stride is 0x58 */
} Actor;

extern void *AllocEmitListRecord(int cls);
extern void VectorToSVectorShr2(void *dst, int *src);          /* vector -> short[3] >>2 */
extern void GetActorAttachPoint(Actor *parent, int slot, int *dst);

extern short g_anCosineLut[];        /* cosine table */
extern short g_anSineLut[];        /* sine table */                 /* GetRandomU32 */

extern Actor *g_pActorListBase;
extern int g_anCameraPos[];          /* camera world position */
extern short g_anCosineLut[];        /* cosine table */
extern short g_anSineLut[];        /* sine table */
extern int g_nGamestate;            /* g_Gamestate */
extern int g_nSpyroHornStrikeVariant;
extern int g_anSpyroWorldPos[];          /* Spyro world position */
extern unsigned char D_80078A66;
extern int g_anSpyroBodyMtx[];          /* Spyro orientation matrix (position at -0x34) */
extern int g_nSpyroGroundHeightZ;
extern int g_nSpyroMotionSpeed;
extern int g_nLevelReadyFlag;
extern int g_nActiveLevelId;
extern unsigned char D_8006E438[];  /* trail colour table, stride 4 */
extern unsigned char D_8006E439[];
extern unsigned char D_8006E43A[];
extern int D_8006E498[];            /* per-step offsets, stride 3 ints */
extern int D_8006E4E0[];            /* per-slot offsets, stride 3 ints */
extern int D_8006E570[];

void Level13_EmitParticles(int count, int type, int *pos, int arg3) {
  int v0[3];
  int vt[3];
  int v1[3];
  int v2[3];
  int v3[3];
  int v4[3];
  Emit *rec;
  int t;
  int z;
  int i;
  int k;
  int v;
  int c;

  for (i = 0; i < count; i++) {
    k = i * 4;
    switch (type) {
    case 0x0: {
      rec = (Emit *)AllocEmitListRecord(2);
      rec->type = type;
      rec->phase = 0;
      rec->unk03 = 1;
      VectorToSVectorShr2(rec->u.spark.pos, pos);
      VectorToSVectorShr2(rec->u.spark.vel, (int *)arg3);
      rec->u.spark.unk1E = 0;
      rec->u.spark.life = 0x18;
      rec->u.spark.seed = GetRandomU32();
      rec->u.spark.r = 0x80;
      rec->u.spark.g = 0x80;
      rec->u.spark.b = 0x80;
      rec->u.spark.fx = 0x2E;
      rec->u.spark.unk11 = 4;
      rec->u.spark.unk10 = 0;
      break;
    }
    case 0x1:
      rec = (Emit *)AllocEmitListRecord(2);
      rec->type = type;
      rec->phase = 0;
      rec->unk03 = 1;
      VectorToSVectorShr2(rec->u.spark.pos, pos);
      VectorToSVectorShr2(rec->u.spark.vel, (int *)arg3);
      rec->u.spark.unk1E = 0;
      if (g_nGamestate == 8) {
        rec->u.spark.life = 8;
      } else {
        rec->u.spark.life = 0x10;
      }
      rec->u.spark.seed = GetRandomU32();
      rec->u.spark.r = 0x60;
      rec->u.spark.g = 0x60;
      rec->u.spark.b = 0x60;
      rec->u.spark.fx = 0x2E;
      rec->u.spark.unk11 = 4;
      rec->u.spark.unk10 = 0;
      break;
    case 0x2: {
      rec = (Emit *)AllocEmitListRecord(2);
      rec->type = type;
      rec->phase = 0;
      rec->unk03 = 1;
      VectorToSVectorShr2(rec->u.spark.pos, pos);
      v0[0] = (GetRandomU32() & 0x3F) - 0x20;
      v0[1] = (GetRandomU32() & 0x3F) - 0x20;
      v0[2] = 0x1E;
      VectorToSVectorShr2(rec->u.spark.vel, v0);
      rec->u.spark.unk1E = 0;
      rec->u.spark.life = 0x20;
      rec->u.spark.seed = GetRandomU32();
      rec->u.spark.r = 0x80;
      rec->u.spark.g = 0x80;
      rec->u.spark.b = 0x80;
      rec->u.spark.fx = 0x2E;
      rec->u.spark.unk11 = 4;
      rec->u.spark.unk10 = 0;
      break;
    }
    case 0x4: {
      int t1, t2, t3;
      int u1, u2, u3;
      rec = (Emit *)AllocEmitListRecord(3);
      rec->type = type;
      rec->phase = GetRandomU32() & 7;
      rec->unk03 = 1;
      VectorToSVectorShr2(rec->u.plume.pos, pos);
      t1 = GetRandomU32() & 0x7E;
      u1 = rec->u.plume.pos[0] - 0x3F;
      rec->u.plume.pos[0] = u1 + t1;
      t2 = GetRandomU32() & 0x7E;
      u2 = rec->u.plume.pos[1] - 0x3F;
      rec->u.plume.pos[1] = u2 + t2;
      t3 = GetRandomU32() & 0x7E;
      u3 = rec->u.plume.pos[2] - 0x3F;
      rec->u.plume.src = (int *)arg3;
      rec->u.plume.unk1E = 0;
      rec->u.plume.pos[2] = u3 + t3;
      rec->u.plume.life = (GetRandomU32() & 0xF) + 0x18;
      rec->u.plume.seed = (GetRandomU32() & 7) + 0xF;
      rec->u.plume.r = 0x80;
      rec->u.plume.g = 0x80;
      rec->u.plume.b = 0x80;
      rec->u.plume.fx = 0x2C;
      rec->u.plume.unk11 = 4;
      rec->u.plume.unk10 = 0;
      break;
    }
    case 0x5: {
      int t1, t2, t3;
      int u1, u2, u3;
      rec = (Emit *)AllocEmitListRecord(3);
      rec->type = type;
      rec->phase = GetRandomU32() & 7;
      rec->unk03 = 1;
      VectorToSVectorShr2(rec->u.track.pos, pos);
      t1 = GetRandomU32() & 0x3E;
      u1 = rec->u.track.pos[0] - 0x1F;
      rec->u.track.pos[0] = u1 + t1;
      t2 = GetRandomU32() & 0x3E;
      u2 = rec->u.track.pos[1] - 0x1F;
      rec->u.track.pos[1] = u2 + t2;
      t3 = GetRandomU32() & 0x3E;
      u3 = rec->u.track.pos[2] - 0x1F;
      rec->u.track.vel[0] = 0;
      rec->u.track.vel[1] = 0;
      rec->u.track.vel[2] = 0;
      rec->u.track.pos[2] = u3 + t3;
      rec->u.track.life = (GetRandomU32() & 7) + 0xC;
      rec->u.track.seed = (GetRandomU32() & 7) + 0xA;
      rec->u.track.r = 0x80;
      rec->u.track.g = 0x80;
      rec->u.track.b = 0x80;
      rec->u.track.fx = 0x2C;
      rec->u.track.unk11 = 4;
      rec->u.track.unk10 = 0;
      break;
    }
    case 0x8: {
      int t1, t2, t3;
      int u1, u2, u3;
      rec = (Emit *)AllocEmitListRecord(3);
      rec->type = type;
      rec->phase = GetRandomU32() & 3;
      rec->unk03 = 1;
      GetActorAttachPoint((Actor *)pos, GetRandomU32() & 1, vt);
      VectorToSVectorShr2(rec->u.track.pos, vt);
      t1 = GetRandomU32() & 0xE;
      u1 = rec->u.track.pos[0] - 7;
      rec->u.track.pos[0] = u1 + t1;
      t2 = GetRandomU32() & 0xE;
      u2 = rec->u.track.pos[1] - 7;
      rec->u.track.pos[1] = u2 + t2;
      t3 = GetRandomU32() & 0xE;
      u3 = rec->u.track.pos[2] - 7;
      rec->u.track.pos[2] = u3 + t3;
      SubtractVector(vt, vt, ((Actor *)pos)->pos);
      vt[2] = vt[2] - 0x384;
      SetVector3Magnitude(vt, 0x80);
      VectorToSVectorShr2(rec->u.track.vel, vt);
      rec->u.track.owner = (Actor *)arg3 - g_pActorListBase;
      rec->u.track.unk1F = 0;
      rec->u.track.life = (GetRandomU32() & 0xF) + 0x18;
      rec->u.track.seed = (GetRandomU32() & 7) + 0xF;
      rec->u.track.fx = 0x2C;
      rec->u.track.unk11 = 4;
      rec->u.track.unk10 = 0;
      switch ((int)(GetRandomU32() & 3)) {
      case 0:
      case 1:
        rec->u.track.r = 0x7F;
        rec->u.track.g = 0x7F;
        rec->u.track.b = 0x20;
        break;
      case 2:
        rec->u.track.r = 0x40;
        rec->u.track.g = 0x7F;
        rec->u.track.b = 0x40;
        break;
      case 3:
        rec->u.track.r = 0x7F;
        rec->u.track.g = 0x7F;
        rec->u.track.b = 0x7F;
        break;
      }
      break;
    }
    case 0x9: {
      rec = (Emit *)AllocEmitListRecord(2);
      rec->type = type;
      rec->phase = 0;
      rec->unk03 = 1;
      VectorToSVectorShr2(rec->u.spark.pos, pos);
      VectorToSVectorShr2(rec->u.spark.vel, (int *)arg3);
      rec->u.spark.unk1E = 0;
      rec->u.spark.life = 0x18;
      rec->u.spark.seed = GetRandomU32();
      if (g_nSpyroMotionSpeed >= 0x5781) {
        rec->u.spark.r = 0xFF;
        rec->u.spark.g = 0;
        rec->u.spark.b = 0;
      } else if (g_nSpyroMotionSpeed >= 0x4241) {
        rec->u.spark.r = 0xF0;
        rec->u.spark.g = 0x60;
        rec->u.spark.b = 0;
      } else if (g_nSpyroMotionSpeed >= 0x3681) {
        rec->u.spark.r = 0xE0;
        rec->u.spark.g = 0xE0;
        rec->u.spark.b = 0;
      } else {
        rec->u.spark.r = 0x80;
        rec->u.spark.g = 0x80;
        rec->u.spark.b = 0x80;
      }
      rec->u.spark.fx = 0x2E;
      rec->u.spark.unk11 = 4;
      rec->u.spark.unk10 = 0;
      break;
    }
    case 0xA: {
      rec = (Emit *)AllocEmitListRecord(3);
      rec->type = type;
      rec->phase = GetRandomU32() & 0x1F;
      rec->unk03 = 1;
      VectorToSVectorShr2(rec->u.spark.pos, g_anSpyroWorldPos);
      rec->u.spark.life = 0x18;
      rec->u.spark.seed = 0xF;
      rec->u.spark.r = D_8006E438[k];
      rec->u.spark.g = D_8006E439[k];
      rec->u.spark.b = D_8006E43A[k];
      rec->u.spark.fx = 0x2C;
      rec->u.spark.unk11 = 4;
      rec->u.spark.unk10 = 0;
      rec->u.spark.vel[0] = ((count - i) << 12) / 5;
      break;
    }
    case 0xC: {
      rec = (Emit *)AllocEmitListRecord(2);
      rec->type = type;
      rec->unk03 = 1;
      rec->u.spark.unk11 = 4;
      if (((Actor *)pos)->kind == 0x22) {
        CopyVector(v1, ((Actor *)pos)->pos);
      } else if (((Actor *)pos)->kind == 0xAD || ((Actor *)pos)->kind == 0xB5) {
        if ((arg3 >> 24) < 6) {
          RotateVectorByMatrix((int *)((Actor *)pos)->mat, &D_8006E4E0[(arg3 >> 24) * 3], v1);
          AddVector(v1, v1, ((Actor *)pos)->pos);
        } else {
          rec->cls = 5;
          rec->u.spark.unk11 = 0x16;
          CopyVector(v1, &D_8006E4E0[(arg3 >> 24) * 3]);
        }
      } else if (((Actor *)pos)->kind == 0x78) {
        RotateVectorByMatrix((int *)((Actor *)pos)->mat, D_8006E570, v1);
        AddVector(v1, v1, ((Actor *)pos)->pos);
      } else {
        RotateVectorByMatrix((int *)((Actor *)pos)->mat, &D_8006E498[i * 3], v1);
        AddVector(v1, v1, ((Actor *)pos)->pos);
      }
      VectorToSVectorShr2(rec->u.spark.pos, v1);
      rec->u.spark.g = arg3 >> 8;
      rec->u.spark.r = arg3;
      rec->u.spark.b = arg3 >> 16;
      rec->u.spark.fx = 0x2E;
      rec->u.spark.life = (GetRandomU32() & 7) + 0x10;
      rec->u.spark.seed = GetRandomU32();
      rec->u.spark.unk10 = 0;
      break;
    }
    case 0x10: {
      rec = (Emit *)AllocEmitListRecord(2);
      rec->type = type;
      rec->phase = 0;
      rec->unk03 = 1;
      VectorToSVectorShr2(rec->u.spark.pos, pos);
      VectorToSVectorShr2(rec->u.spark.vel, pos + 3);
      rec->u.spark.unk1E = 0;
      rec->u.spark.life = ((int *)arg3)[0];
      rec->u.spark.seed = GetRandomU32();
      c = ((int *)arg3)[1] * 8;
      rec->u.spark.r = c;
      rec->u.spark.fx = 0x2E;
      rec->u.spark.unk11 = 4;
      rec->u.spark.unk10 = 0;
      rec->u.spark.g = c;
      rec->u.spark.b = rec->u.spark.r;
      break;
    }
    case 0x11: {
      rec = (Emit *)AllocEmitListRecord(2);
      rec->type = type;
      rec->phase = 0;
      rec->unk03 = 1;
      VectorToSVectorShr2(rec->u.spark.pos, pos);
      VectorToSVectorShr2(rec->u.spark.vel, pos + 3);
      rec->u.spark.unk1E = 0;
      rec->u.spark.life = 0x18;
      rec->u.spark.seed = GetRandomU32();
      rec->u.spark.r = ((int *)arg3)[0];
      rec->u.spark.g = ((int *)arg3)[1];
      rec->u.spark.b = ((int *)arg3)[2];
      rec->u.spark.fx = 0x2E;
      rec->u.spark.unk11 = 4;
      rec->u.spark.unk10 = 0;
      break;
    }
    case 0x15: {
      rec = (Emit *)AllocEmitListRecord(2);
      rec->type = type;
      rec->unk03 = 1;
      VectorToSVectorShr2(rec->u.ribbon.pos, pos);
      VectorToSVectorShr2(rec->u.ribbon.pos2, pos);
      if ((arg3 & 0x1F) < 0x10) {
        v = arg3 * 2;
      } else {
        v = arg3 * 2 - 0x80;
      }
      rec->u.ribbon.unk18 = v;
      rec->u.ribbon.unk19 = 8;
      rec->u.ribbon.r = ~((arg3 & 0xF) << 4);
      rec->u.ribbon.g = 0xFF;
      rec->u.ribbon.b = -0x80 - ((arg3 & 0xF) << 3);
      rec->u.ribbon.life = 0x30 - ((arg3 & 0xF) * 2);
      rec->u.ribbon.seed = arg3 * 2;
      rec->u.ribbon.fx = 0x2E;
      rec->u.ribbon.unk11 = 2;
      rec->u.ribbon.unk10 = 0;
      break;
    }
    case 0x16: {
      int d;
      d = ApproxDist2D(pos, g_anCameraPos);
      if (d > 0x7FFF) {
        break;
      }
      rec = (Emit *)AllocEmitListRecord(2);
      rec->type = type;
      rec->phase = GetRandomU32() & 0xF;
      rec->unk03 = 1;
      VectorToSVectorShr2(rec->u.spark.pos, pos);
      rec->u.spark.pos[0] = rec->u.spark.pos[0] + ((((int)(GetRandomU32() & 0xFFF) - 0x7FF) * 300) >> 12);
      rec->u.spark.pos[1] = rec->u.spark.pos[1] + ((((int)(GetRandomU32() & 0xFFF) - 0x7FF) * 300) >> 12);
      arg3 = (arg3 + (int)(GetRandomU32() & 0x1E) - 0xF) & 0xFF;
      rec->u.spark.vel[0] = (g_anCosineLut[arg3] * 7) >> 11;
      rec->u.spark.vel[1] = (g_anSineLut[arg3] * 7) >> 11;
      rec->u.spark.vel[2] = 8;
      rec->u.spark.unk1E = (GetRandomU32() & 2) - 1;
      rec->u.spark.life = 0xE0;
      rec->u.spark.seed = GetRandomU32();
      if (d >= 0x6001) {
        d = (0x8000 - d) >> 7;
      } else {
        d = 0x40;
      }
      rec->u.spark.r = d >> 1;
      rec->u.spark.g = d >> 1;
      rec->u.spark.b = d;
      rec->u.spark.fx = 0x2E;
      rec->u.spark.unk11 = 0xC;
      rec->u.spark.unk10 = 0;
      break;
    }
    case 0x18: {
      rec = (Emit *)AllocEmitListRecord(2);
      rec->type = type;
      rec->unk03 = 1;
      VectorToSVectorShr2(rec->u.band.pos, pos);
      VectorToSVectorShr2(rec->u.band.pos2, pos);
      v = 0x30;
      z = i * 16;
      rec->u.band.life = v;
      rec->u.band.r = 0x80;
      rec->u.band.g = 0x80;
      rec->u.band.b = 0x80;
      rec->u.band.seed = z;
      rec->u.band.fx = 0x2E;
      rec->u.band.unk11 = 8;
      rec->u.band.unk18 = i * 32;
      rec->u.band.unk10 = 0;
      rec->u.band.unk1A = 0x10;
      rec->u.band.unk1C = z;
      rec->u.band.unk1D = i;
      break;
    }
    case 0x21: {
      int ta, tb;
      int ua, ub;
      rec = (Emit *)AllocEmitListRecord(2);
      rec->type = type;
      rec->phase = 0;
      rec->unk03 = 1;
      if (arg3 != 0) {
        v2[0] = 0x164;
        v2[1] = 0;
        v2[2] = 0;
        RotateVectorByMatrix(g_anSpyroBodyMtx, v2, v2);
        AddVector(v2, v2, g_anSpyroBodyMtx - 13);
      } else {
        CopyVector(v2, g_anSpyroWorldPos);
        v2[2] = g_nSpyroGroundHeightZ;
        ta = GetRandomU32() & 0x3E;
        ua = v2[0] - 0x1F;
        v2[0] = ua + ta;
        tb = GetRandomU32() & 0x3E;
        ub = v2[1] - 0x1F;
        v2[1] = ub + tb;
      }
      VectorToSVectorShr2(rec->u.spark.pos, v2);
      VectorToSVectorShr2(rec->u.spark.vel, pos);
      rec->u.spark.unk1E = 0;
      rec->u.spark.life = 0x10;
      rec->u.spark.seed = GetRandomU32();
      rec->u.spark.r = 0x20;
      rec->u.spark.g = 0x20;
      rec->u.spark.b = 0x20;
      rec->u.spark.fx = 0x2E;
      rec->u.spark.unk11 = 4;
      rec->u.spark.unk10 = 0;
      break;
    }
    case 0x42: {
      rec = (Emit *)AllocEmitListRecord(0);
      rec->type = type;
      rec->phase = GetRandomU32() & 0xF;
      rec->unk03 = 1;
      rec->u.dust.pos[0] = (pos[0] >> 2) + (GetRandomU32() & 0xF) - 8;
      rec->u.dust.pos[1] = (pos[1] >> 2) + (GetRandomU32() & 0xF) - 8;
      rec->u.dust.pos[2] = (pos[2] >> 2) + (GetRandomU32() & 0xF) - 8;
      rec->u.dust.vel[0] = 0;
      rec->u.dust.vel[1] = 0;
      rec->u.dust.vel[2] = 0;
      rec->u.dust.life = 2;
      if (arg3 == 1 || (arg3 == 0 && g_nLevelReadyFlag >= 3)) {
        rec->u.dust.r = 0xFF;
        rec->u.dust.g = 0xFF;
        rec->u.dust.b = 0;
      } else {
        switch (GetRandomU32() & 7) {
        case 0:
          rec->u.dust.r = 0xFF;
          rec->u.dust.g = 0;
          rec->u.dust.b = 0;
          break;
        case 1:
          rec->u.dust.r = 0xFF;
          rec->u.dust.g = 0xFF;
          rec->u.dust.b = 0;
          break;
        case 2:
          rec->u.dust.r = 0;
          rec->u.dust.g = 0xFF;
          rec->u.dust.b = 0;
          break;
        case 3:
          rec->u.dust.r = 0;
          rec->u.dust.g = 0xFF;
          rec->u.dust.b = 0xFF;
          break;
        case 4:
          rec->u.dust.r = 0;
          rec->u.dust.g = 0;
          rec->u.dust.b = 0xFF;
          break;
        case 5:
          rec->u.dust.r = 0xFF;
          rec->u.dust.g = 0;
          rec->u.dust.b = 0xFF;
          break;
        case 6:
          rec->u.dust.r = 0xFF;
          rec->u.dust.g = 0x80;
          rec->u.dust.b = 0;
          break;
        case 7:
          rec->u.dust.r = 0;
          rec->u.dust.g = 0x80;
          rec->u.dust.b = 0xFF;
          break;
        }
      }
      rec->u.dust.fx = 0x40;
      break;
    }
    case 0x46: {
      rec = (Emit *)AllocEmitListRecord(1);
      rec->type = type;
      rec->phase = GetRandomU32() & 0xF;
      rec->unk03 = 1;
      v3[0] = (arg3 * ((int)(GetRandomU32() & 0xFFF) - 0x800)) >> 12;
      v3[1] = (arg3 * ((int)(GetRandomU32() & 0xFFF) - 0x800)) >> 12;
      v3[2] = (arg3 * (int)(GetRandomU32() & 0xFFF)) >> 12;
      rec->u.line.a[0] = (pos[0] >> 2) + v3[0] * 4;
      rec->u.line.a[1] = (pos[1] >> 2) + v3[1] * 4;
      rec->u.line.a[2] = (pos[2] >> 2) + v3[2] * 4;
      rec->u.line.b[0] = rec->u.line.a[0] + v3[0] * 2;
      rec->u.line.b[1] = rec->u.line.a[1] + v3[1] * 2;
      z = v3[2];
      rec->u.line.c1[3] = 2;
      rec->u.line.c0[0] = 0xE0;
      rec->u.line.c0[1] = 0xC0;
      rec->u.line.c0[3] = 0x50;
      rec->u.line.c0[2] = 0;
      rec->u.line.c1[0] = 0xFF;
      rec->u.line.c1[1] = 0xFF;
      rec->u.line.c1[2] = 0x80;
      rec->u.line.b[2] = rec->u.line.a[2] + z * 2;
      break;
    }
    case 0x47: {
      rec = (Emit *)AllocEmitListRecord(1);
      rec->type = type;
      rec->phase = GetRandomU32() & 7;
      rec->unk03 = 1;
      v3[0] = (((int)(GetRandomU32() & 0xFFF) - 0x800) * 16) >> 12;
      v3[1] = (((int)(GetRandomU32() & 0xFFF) - 0x800) * 16) >> 12;
      v3[2] = (((int)(GetRandomU32() & 0xFFF) - 0x800) * 16) >> 12;
      rec->u.line.a[0] = (pos[0] >> 2) + v3[0] * 4;
      rec->u.line.a[1] = (pos[1] >> 2) + v3[1] * 4;
      rec->u.line.a[2] = (pos[2] >> 2) + v3[2] * 4;
      rec->u.line.b[0] = rec->u.line.a[0] + v3[0];
      rec->u.line.b[1] = rec->u.line.a[1] + v3[1];
      rec->u.line.b[2] = rec->u.line.a[2] + v3[2];
      rec->u.line.c1[3] = 2;
      rec->u.line.c0[0] = 0xE0;
      rec->u.line.c0[1] = 0xE0;
      rec->u.line.c0[2] = 0x60;
      rec->u.line.c1[0] = 0xFF;
      rec->u.line.c1[1] = 0xFF;
      rec->u.line.c1[2] = 0xFF;
      rec->u.line.c0[3] = 0x50;
      break;
    }
    case 0x4C: {
      int w;
      rec = (Emit *)AllocEmitListRecord(0);
      rec->type = type;
      rec->unk03 = 1;
      VectorToSVectorShr2(rec->u.smear.pos, pos);
      VectorToSVectorShr2(rec->u.smear.pos2, pos);
      if ((arg3 & 0x1F) < 0x10) {
        w = arg3 * 2;
      } else {
        w = arg3 * 2 - 0x80;
      }
      rec->u.smear.unk18 = w;
      rec->u.smear.unk19 = 8;
      rec->u.smear.r = ~((arg3 & 0xF) << 4);
      rec->u.smear.g = 0xFF;
      rec->u.smear.b = -0x80 - ((arg3 & 0xF) << 3);
      rec->u.smear.fx = 0x40;
      rec->u.smear.life = 2;
      break;
    }
    case 0x4D: {
      int t;
      int u;
      rec = (Emit *)AllocEmitListRecord(4);
      rec->type = type;
      rec->phase = GetRandomU32() & 0xF;
      VectorToSVectorShr2(rec->u.line.b, pos);
      VectorToSVectorShr2(rec->u.line.a, pos);
      t = GetRandomU32() & 0xFFF;
      rec->u.line.a[0] = rec->u.line.a[0] + (LookupCosine(t) >> 8);
      rec->u.line.a[1] = rec->u.line.a[1] + (LookupSine(GetRandomU32() & 0x7FF) >> 8);
      u = LookupSine(t);
      rec->u.line.c1[3] = 0x7F;
      rec->u.line.c0[1] = 0x40;
      rec->u.line.c0[2] = 0x60;
      rec->u.line.c1[2] = 0xC0;
      rec->u.line.c0[3] = 0x50;
      rec->u.line.c0[0] = 0x80;
      rec->u.line.c1[0] = 0xFF;
      rec->u.line.c1[1] = 0x80;
      rec->u.line.a[2] = rec->u.line.a[2] + (u >> 8);
      break;
    }
    case 0x4E: {
      int t1, t2, t3;
      int u1, u2, u3;
      rec = (Emit *)AllocEmitListRecord(0);
      rec->type = type;
      rec->phase = GetRandomU32() & 0x1F;
      rec->unk03 = 1;
      VectorToSVectorShr2(rec->u.dust.pos, pos);
      t1 = GetRandomU32() & 0x3F;
      u1 = rec->u.dust.pos[0] - 0x20;
      rec->u.dust.pos[0] = u1 + t1;
      t2 = GetRandomU32() & 0x3F;
      u2 = rec->u.dust.pos[1] - 0x20;
      rec->u.dust.pos[1] = u2 + t2;
      t3 = GetRandomU32() & 0x3F;
      u3 = rec->u.dust.pos[2] - 0x20;
      rec->u.dust.pos[2] = u3 + t3;
      rec->u.dust.vel[0] = (GetRandomU32() & 2) - 1;
      rec->u.dust.vel[1] = (GetRandomU32() & 2) - 1;
      rec->u.dust.vel[2] = -(GetRandomU32() & 1);
      rec->u.dust.r = GetRandomU32() | 0x80;
      rec->u.dust.g = (GetRandomU32() & 0x7F) + 0x60;
      if (rec->u.dust.r < rec->u.dust.g) {
        rec->u.dust.g = rec->u.dust.r;
      }
      rec->u.dust.b = (GetRandomU32() & 0x3F) + 0x40;
      rec->u.dust.fx = 0x40;
      rec->u.dust.life = 2;
      break;
    }
    case 0x4F: {
      int t4, t5, t6;
      int u4, u5, u6;
      rec = (Emit *)AllocEmitListRecord(1);
      rec->type = type;
      rec->phase = GetRandomU32() & 0xF;
      rec->unk03 = 1;
      VectorToSVectorShr2(rec->u.line.b, pos);
      AddVector(v3, pos, (int *)arg3);
      t4 = GetRandomU32() & 0x3F;
      u4 = v3[0] - 0x20;
      v3[0] = u4 + t4;
      t5 = GetRandomU32() & 0x3F;
      u5 = v3[1] - 0x20;
      v3[1] = u5 + t5;
      t6 = GetRandomU32() & 0x7F;
      u6 = v3[2] - 0x40;
      v3[2] = u6 + t6;
      VectorToSVectorShr2(rec->u.line.a, v3);
      rec->u.line.c1[3] = 2;
      if (g_nSpyroHornStrikeVariant == 0) {
        rec->u.line.c0[0] = GetRandomU32() | 0xC0;
        rec->u.line.c0[1] = (GetRandomU32() & 0x3F) - 0x60;
        rec->u.line.c0[2] = 0;
        rec->u.line.c1[0] = GetRandomU32() | 0xE0;
        rec->u.line.c1[1] = GetRandomU32() | 0xE0;
        rec->u.line.c1[2] = 0x80;
      } else {
        rec->u.line.c0[0] = GetRandomU32() | 0xC0;
        rec->u.line.c0[1] = (GetRandomU32() & 0x3F) + 0x40;
        rec->u.line.c0[2] = (GetRandomU32() & 0x3F) + 0x40;
        rec->u.line.c1[0] = GetRandomU32() | 0xE0;
        rec->u.line.c1[1] = (GetRandomU32() & 0x1F) - 0x60;
        rec->u.line.c1[2] = (GetRandomU32() & 0x1F) - 0x60;
      }
      rec->u.line.c0[3] = 0x50;
      break;
    }
    case 0x50: {
      rec = (Emit *)AllocEmitListRecord(1);
      rec->type = type;
      rec->phase = GetRandomU32() & 0xF;
      rec->unk03 = 1;
      VectorToSVectorShr2(rec->u.line.a, pos);
      v4[0] = (arg3 * ((int)(GetRandomU32() & 0xFFF) - 0x800)) >> 12;
      v4[1] = (arg3 * ((int)(GetRandomU32() & 0xFFF) - 0x800)) >> 12;
      v4[2] = (arg3 * (int)(GetRandomU32() & 0xFFF)) >> 12;
      rec->u.line.a[0] = rec->u.line.a[0] + v4[0] * 4;
      rec->u.line.a[1] = rec->u.line.a[1] + v4[1] * 4;
      rec->u.line.a[2] = rec->u.line.a[2] + v4[2] * 4;
      rec->u.line.b[0] = rec->u.line.a[0] + v4[0] * 2;
      rec->u.line.b[1] = rec->u.line.a[1] + v4[1] * 2;
      z = v4[2];
      rec->u.line.c1[3] = 2;
      rec->u.line.c0[0] = 0xE0;
      rec->u.line.c0[1] = 0xC0;
      rec->u.line.c0[3] = 0x50;
      rec->u.line.c0[2] = 0;
      rec->u.line.c1[0] = 0xFF;
      rec->u.line.c1[1] = 0xFF;
      rec->u.line.c1[2] = 0x80;
      rec->u.line.b[2] = rec->u.line.a[2] + z * 2;
      break;
    }
    }
  }
}

/* MATCHED 2026-08-19-2 park closed 2026-08-20-1.  The residue was ONE
 * instruction -- the `li t0,46` that reload emits for arm 0x18's
 * `fx = 0x2E` store -- sitting four slots too late, and the fix is pure
 * statement order.
 *
 * The mechanism, and it generalises (cookbook A220): a constant stored to a
 * field with no register of its own is rematerialised by reload IMMEDIATELY
 * BEFORE its store, so the `li` inherits the store's LUID.  sched2 then hoists
 * it as a low-priority straggler, and stragglers come out in LUID order, so
 * the `li` can never be emitted above a straggler belonging to an EARLIER
 * source statement.  Moving the fx store up does move the `li` up -- but it
 * drags the store with it (stores of equal priority also come out in source
 * order), so the store then lands four slots early instead.  The two are one
 * dial with two ends and neither end alone reaches the original.
 *
 * What breaks the tie is that `unk11 = 8` / `unk18 = i * 32` are NOT free
 * stragglers: their `li v0,8` / `sll v0,i,5` producers are chained to
 * `li v0,48` by v0 reuse, so sched2 keeps that chain (and its stores) in the
 * same relative slots no matter where the statements sit in the source.
 * Writing those two statements AFTER the fx store therefore raises their
 * LUIDs above the `li t0,46` -- putting the reload third in the straggler
 * sequence, exactly where the original has it -- while their stores stay put.
 * Measured: fx at each of source positions 0..7 gives 5,5,8,10,9,8,7,5 and
 * never both ends at once; moving unk11/unk18 below fx gives MATCH.
 *
 * The `v` promotion (arm 0x15's block local carrying 0x30) is still required
 * and still load-bearing, and `fx = 0x2E` must stay a PLAIN LITERAL: a carrier
 * gives the constant two sets, which kills `reg_equiv_constant` and hands the
 * value a0 instead of leaving it to reload's spill register t0.
 *
 * Word-identical (bar branch displacements) to func_level_2_800844A0,
 * func_level_4_8008391C, func_level_10_8008611C and func_level_28_80085254,
 * which are sed-clones of this file.  func_level_3_80088F68 (3,940 B) is the
 * 985-insn variant of the same walk and carries the same arm 0x18.
 */
