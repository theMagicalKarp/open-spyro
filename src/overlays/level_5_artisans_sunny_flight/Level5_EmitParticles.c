/* Level5_EmitParticles (0x80084128, level_5_artisans_sunny_flight
 * overlay, 0xc98 bytes).
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
 * `arg3 >> 24` is written inline at its three use sites rather than hoisted
 * into a named local: loop.c emits the invariant it lifts out of the loop in
 * SCAN order, so the shift has to be scanned after the 0x80 / 0xFF colour
 * constants that the case-0 / case-9 arms hoist.  A user variable can only
 * become a movable when its set is unconditional (scan_loop case 1), which
 * would put it at the top of the body; a compiler temp qualifies under case 2
 * anywhere in the loop.
 *
 * The levels 5 / 29 variant of the dispatcher: the levels 11/17/23 case list
 * plus the 0xD ember arm and the 0x13 distance-faded haze arm.  Word-identical
 * (bar branch displacements) to Level29_EmitParticles.
 *
 * Verified byte-identical inside the relinked
 * level_5_artisans_sunny_flight.ovl.
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
    struct {                     /* class 2 -- haze puff (word velocity) */
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
      int vel;                   /* 0x18 */
    } puff;
    struct {                     /* classes 1 / 4 -- streak */
      unsigned short a[3];       /* 0x04 head */
      unsigned short b[3];       /* 0x0A tail */
      unsigned char c0[4];       /* 0x10 head colour */
      unsigned char c1[4];       /* 0x14 tail colour */
    } line;
  } u;
} Emit;

typedef struct Actor {
  char pad00[0x0C];
  int pos[3];   /* 0x0C */
  char pad18[8];
  short mat[9]; /* 0x20 3x3 rotation matrix */
  short pad32;  /* 0x32 */
  short unk34;  /* 0x34 */
  short kind;   /* 0x36 */
} Actor;

extern void *AllocEmitListRecord(int cls);
extern void VectorToSVectorShr2(void *dst, int *src);          /* vector -> short[3] >>2 */

extern int g_nGamestate;            /* g_Gamestate */
extern int g_anCameraPos[];          /* world anchor position */
extern int g_nSpyroHornStrikeVariant;
extern int g_anSpyroWorldPos[];          /* Spyro world position */
extern unsigned char g_bSpyroPersistentEulerZ;
extern int g_anSpyroBodyMtx[];          /* Spyro orientation matrix (position at -0x34) */
extern int g_nSpyroGroundHeightZ;
extern int g_nSpyroMotionSpeed;
extern int g_nLevelReadyFlag;
extern unsigned char g_abEmitTrailColors[];  /* trail colour table, stride 4 */
extern unsigned char D_8006E439[];
extern unsigned char D_8006E43A[];
extern int g_anEmitStepOffsets[];            /* per-step offsets, stride 3 ints */
extern int g_anEmitSlotOffsets[];            /* per-slot offsets, stride 3 ints */
extern int D_8006E570[];

void Level5_EmitParticles(int count, int type, int *pos, int arg3) {
  int v0[3];
  int v1[3];
  int v2[3];
  int v3[3];
  Emit *rec;
  int c;
  int i;
  int k;
  int d;

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
      rec->u.spark.r = g_abEmitTrailColors[k];
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
          RotateVectorByMatrix((int *)((Actor *)pos)->mat, &g_anEmitSlotOffsets[(arg3 >> 24) * 3], v1);
          AddVector(v1, v1, ((Actor *)pos)->pos);
        } else {
          rec->cls = 5;
          rec->u.spark.unk11 = 0x16;
          CopyVector(v1, &g_anEmitSlotOffsets[(arg3 >> 24) * 3]);
        }
      } else if (((Actor *)pos)->kind == 0x78) {
        RotateVectorByMatrix((int *)((Actor *)pos)->mat, D_8006E570, v1);
        AddVector(v1, v1, ((Actor *)pos)->pos);
      } else {
        RotateVectorByMatrix((int *)((Actor *)pos)->mat, &g_anEmitStepOffsets[i * 3], v1);
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
    case 0xD: {
      rec = (Emit *)AllocEmitListRecord(2);
      rec->type = type;
      rec->phase = 0;
      rec->unk03 = 1;
      VectorToSVectorShr2(rec->u.spark.pos, pos);
      VectorToSVectorShr2(rec->u.spark.vel, (int *)arg3);
      rec->u.spark.unk1E = 0;
      rec->u.spark.life = 0x18;
      rec->u.spark.seed = GetRandomU32();
      rec->u.spark.r = 0xFF;
      rec->u.spark.g = 0xFF;
      rec->u.spark.b = 0;
      rec->u.spark.fx = 0x2E;
      rec->u.spark.unk11 = 4;
      rec->u.spark.unk10 = 0;
      break;
    }
    case 0x13: {
      d = ApproxDist2D(pos, g_anCameraPos);
      if (d > 0x7FFF) {
        break;
      }
      rec = (Emit *)AllocEmitListRecord(2);
      rec->type = type;
      rec->phase = 0;
      rec->unk03 = 1;
      VectorToSVectorShr2(rec->u.spark.pos, pos);
      rec->u.puff.vel = 0x20;
      rec->u.spark.life = 0x40;
      rec->u.spark.seed = GetRandomU32();
      if (d > 0x6000) {
        d = (0x8000 - d) >> 6;
      } else {
        d = 0x80;
      }
      rec->u.spark.r = d;
      rec->u.spark.g = d;
      rec->u.spark.b = d;
      rec->u.spark.fx = 0x2E;
      rec->u.spark.unk11 = 0x10;
      rec->u.spark.unk10 = 0;
      break;
    }
    case 0x1A:
      rec = (Emit *)AllocEmitListRecord(2);
      rec->type = type;
      rec->phase = 0;
      rec->unk03 = 1;
      VectorToSVectorShr2(rec->u.spark.pos, pos);
      rec->u.spark.life = 0xC;
      rec->u.spark.seed = GetRandomU32();
      c = arg3 * 8;
      rec->u.spark.r = c;
      rec->u.spark.g = c;
      rec->u.spark.b = c;
      rec->u.spark.fx = 0x2E;
      rec->u.spark.unk11 = 4;
      rec->u.spark.unk10 = 0;
      break;
    case 0x1B: {
      int w;
      int q;
      rec = (Emit *)AllocEmitListRecord(6);
      rec->type = type;
      rec->phase = 0;
      rec->unk03 = 1;
      VectorToSVectorShr2(rec->u.spark.pos, pos);
      rec->u.spark.life = ((arg3 >> 16) * 6) + 0xC;
      w = g_bSpyroPersistentEulerZ + (GetRandomU32() & 7) + 0x1D;
      q = (arg3 & 0xFFFF) * 8;
      rec->u.spark.r = q;
      rec->u.spark.g = q;
      rec->u.spark.b = q;
      rec->u.spark.seed = w;
      if ((GetRandomU32() & 7) == 0) {
        rec->u.spark.r = rec->u.spark.r + 0x10;
        rec->u.spark.g = rec->u.spark.g + 0x10;
        rec->u.spark.b = rec->u.spark.b + 0x10;
      }
      rec->u.spark.fx = 0x2E;
      rec->u.spark.unk11 = 4;
      rec->u.spark.unk10 = 0;
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
    }
  }
}
