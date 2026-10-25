/* Level20_EmitParticles (0x800861CC, level_20_beast_makers_misty_bog
 * overlay, 0xf64 bytes).
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
 * actor's own matrix.  `effectArg` is per-effect: a velocity vector for 0/1, a
 * packed RGB for 0xC, a scale for 0x1A/0x1B, and a flag for 0x21/0x42.
 *
 * The stone-hill variant: the town-square arm set plus 0x6, the actor-tracking
 * smoke plume (record class 3, which keeps a pointer to the actor's position
 * at +0x18 so the particle follows it).  `effectArg >> 24` is written inline at its
 * three 0xC-arm use sites so it stays a compiler temp and loop.c hoists it out
 * of the loop (scan_loop case 2); a named local would pin it inside the arm.
 *
 * Word-identical (bar branch displacements) to Level2_EmitParticles,
 * Level4_EmitParticles, Level10_EmitParticles and Level28_EmitParticles.
 *
 * Arm 0x18's store order is load-bearing: `depthBias = 8` and `lift = i * 32`
 * are written AFTER `fx = 0x2E` -- see the note at the end.
 */

#include "vecmath.h"
#include "actor_type.h"
#include "level_globals.h"
#include "level_emit_particles.h"
#include "emit_record.h"
#include "gamestate.h"


void Level20_EmitParticles(int count, int type, int *pos, int effectArg) {
  int v0[3];
  int v1[3];
  int v2[3];
  int v3[3];
  int v4[3];
  Emit *rec;
  int t;
  int v;
  int z;
  int i;
  int k;

  for (i = 0; i < count; i++) {
    k = i * 4;
    switch (type) {
    case 0x0: {
      rec = (Emit *)AllocEmitListRecord(2);
      rec->type = type;
      rec->phase = 0;
      rec->visible = 1;
      VectorToSVectorShr2(rec->u.spark.pos, pos);
      VectorToSVectorShr2(rec->u.spark.vel, (int *)effectArg);
      rec->u.spark.toggle = 0;
      rec->u.spark.life = 0x18;
      rec->u.spark.seed = GetRandomU32();
      rec->u.spark.r = 0x80;
      rec->u.spark.g = 0x80;
      rec->u.spark.b = 0x80;
      rec->u.spark.fx = 0x2E;
      rec->u.spark.depthBias = 4;
      rec->u.spark.texIndex = 0;
      break;
    }
    case 0x1:
      rec = (Emit *)AllocEmitListRecord(2);
      rec->type = type;
      rec->phase = 0;
      rec->visible = 1;
      VectorToSVectorShr2(rec->u.spark.pos, pos);
      VectorToSVectorShr2(rec->u.spark.vel, (int *)effectArg);
      rec->u.spark.toggle = 0;
      if (g_nGamestate == GS_GEM_PICKUP) {
        rec->u.spark.life = 8;
      } else {
        rec->u.spark.life = 0x10;
      }
      rec->u.spark.seed = GetRandomU32();
      rec->u.spark.r = 0x60;
      rec->u.spark.g = 0x60;
      rec->u.spark.b = 0x60;
      rec->u.spark.fx = 0x2E;
      rec->u.spark.depthBias = 4;
      rec->u.spark.texIndex = 0;
      break;
    case 0x2: {
      rec = (Emit *)AllocEmitListRecord(2);
      rec->type = type;
      rec->phase = 0;
      rec->visible = 1;
      VectorToSVectorShr2(rec->u.spark.pos, pos);
      v0[0] = (GetRandomU32() & 0x3F) - 0x20;
      v0[1] = (GetRandomU32() & 0x3F) - 0x20;
      v0[2] = 0x1E;
      VectorToSVectorShr2(rec->u.spark.vel, v0);
      rec->u.spark.toggle = 0;
      rec->u.spark.life = 0x20;
      rec->u.spark.seed = GetRandomU32();
      rec->u.spark.r = 0x80;
      rec->u.spark.g = 0x80;
      rec->u.spark.b = 0x80;
      rec->u.spark.fx = 0x2E;
      rec->u.spark.depthBias = 4;
      rec->u.spark.texIndex = 0;
      break;
    }
    case 0x9: {
      rec = (Emit *)AllocEmitListRecord(2);
      rec->type = type;
      rec->phase = 0;
      rec->visible = 1;
      VectorToSVectorShr2(rec->u.spark.pos, pos);
      VectorToSVectorShr2(rec->u.spark.vel, (int *)effectArg);
      rec->u.spark.toggle = 0;
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
      rec->u.spark.depthBias = 4;
      rec->u.spark.texIndex = 0;
      break;
    }
    case 0xA: {
      rec = (Emit *)AllocEmitListRecord(3);
      rec->type = type;
      rec->phase = GetRandomU32() & 0x1F;
      rec->visible = 1;
      VectorToSVectorShr2(rec->u.spark.pos, g_anSpyroWorldPos);
      rec->u.spark.life = 0x18;
      rec->u.spark.seed = 0xF;
      rec->u.spark.r = g_abEmitTrailColors[k];
      rec->u.spark.g = g_abEmitTrailColorsG[k];
      rec->u.spark.b = g_abEmitTrailColorsB[k];
      rec->u.spark.fx = 0x2C;
      rec->u.spark.depthBias = 4;
      rec->u.spark.texIndex = 0;
      rec->u.spark.vel[0] = ((count - i) << 12) / 5;
      break;
    }
    case 0xC: {
      rec = (Emit *)AllocEmitListRecord(2);
      rec->type = type;
      rec->visible = 1;
      rec->u.spark.depthBias = 4;
      if (((Actor *)pos)->type == 0x22) {
        CopyVector(v1, &((Actor *)pos)->posX);
      } else if (((Actor *)pos)->type == 0xAD || ((Actor *)pos)->type == 0xB5) {
        if ((effectArg >> 24) < 6) {
          RotateVectorByMatrix(((Actor *)pos)->mtx, &g_anEmitSlotOffsets[(effectArg >> 24) * 3], v1);
          AddVector(v1, v1, &((Actor *)pos)->posX);
        } else {
          rec->cls = 5;
          rec->u.spark.depthBias = 0x16;
          CopyVector(v1, &g_anEmitSlotOffsets[(effectArg >> 24) * 3]);
        }
      } else if (((Actor *)pos)->type == 0x78) {
        RotateVectorByMatrix(((Actor *)pos)->mtx, g_anSparxEmitOffset, v1);
        AddVector(v1, v1, &((Actor *)pos)->posX);
      } else {
        RotateVectorByMatrix(((Actor *)pos)->mtx, &g_anEmitStepOffsets[i * 3], v1);
        AddVector(v1, v1, &((Actor *)pos)->posX);
      }
      VectorToSVectorShr2(rec->u.spark.pos, v1);
      rec->u.spark.g = effectArg >> 8;
      rec->u.spark.r = effectArg;
      rec->u.spark.b = effectArg >> 16;
      rec->u.spark.fx = 0x2E;
      rec->u.spark.life = (GetRandomU32() & 7) + 0x10;
      rec->u.spark.seed = GetRandomU32();
      rec->u.spark.texIndex = 0;
      break;
    }
    case 0x15: {
      rec = (Emit *)AllocEmitListRecord(2);
      rec->type = type;
      rec->visible = 1;
      VectorToSVectorShr2(rec->u.ribbon.pos, pos);
      VectorToSVectorShr2(rec->u.ribbon.pos2, pos);
      if ((effectArg & 0x1F) < 0x10) {
        v = effectArg * 2;
      } else {
        v = effectArg * 2 - 0x80;
      }
      rec->u.ribbon.angle = v;
      rec->u.ribbon.radius = 8;
      rec->u.ribbon.r = ~((effectArg & 0xF) << 4);
      rec->u.ribbon.g = 0xFF;
      rec->u.ribbon.b = -0x80 - ((effectArg & 0xF) << 3);
      rec->u.ribbon.life = 0x30 - ((effectArg & 0xF) * 2);
      rec->u.ribbon.texIndex = 0;
      rec->u.ribbon.seed = effectArg * 2;
      rec->u.ribbon.fx = 0x2E;
      rec->u.ribbon.depthBias = 2;
      break;
    }
    case 0x18: {
      rec = (Emit *)AllocEmitListRecord(2);
      rec->type = type;
      rec->visible = 1;
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
      rec->u.band.depthBias = 8;
      rec->u.band.lift = i * 32;
      rec->u.band.texIndex = 0;
      rec->u.band.radius = 0x10;
      rec->u.band.angle = z;
      rec->u.band.segment = i;
      break;
    }
    case 0x21: {
      int ta, tb;
      int ua, ub;
      rec = (Emit *)AllocEmitListRecord(2);
      rec->type = type;
      rec->phase = 0;
      rec->visible = 1;
      if (effectArg != 0) {
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
      rec->u.spark.toggle = 0;
      rec->u.spark.life = 0x10;
      rec->u.spark.seed = GetRandomU32();
      rec->u.spark.r = 0x20;
      rec->u.spark.g = 0x20;
      rec->u.spark.b = 0x20;
      rec->u.spark.fx = 0x2E;
      rec->u.spark.depthBias = 4;
      rec->u.spark.texIndex = 0;
      break;
    }
    case 0x42: {
      rec = (Emit *)AllocEmitListRecord(0);
      rec->type = type;
      rec->phase = GetRandomU32() & 0xF;
      rec->visible = 1;
      rec->u.dust.pos[0] = (pos[0] >> 2) + (GetRandomU32() & 0xF) - 8;
      rec->u.dust.pos[1] = (pos[1] >> 2) + (GetRandomU32() & 0xF) - 8;
      rec->u.dust.pos[2] = (pos[2] >> 2) + (GetRandomU32() & 0xF) - 8;
      rec->u.dust.vel[0] = 0;
      rec->u.dust.vel[1] = 0;
      rec->u.dust.vel[2] = 0;
      rec->u.dust.life = 2;
      if (effectArg == 1 || (effectArg == 0 && g_nLevelReadyFlag >= 3)) {
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
      rec->visible = 1;
      v3[0] = (effectArg * ((int)(GetRandomU32() & 0xFFF) - 0x800)) >> 12;
      v3[1] = (effectArg * ((int)(GetRandomU32() & 0xFFF) - 0x800)) >> 12;
      v3[2] = (effectArg * (int)(GetRandomU32() & 0xFFF)) >> 12;
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
      rec->visible = 1;
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
      rec->visible = 1;
      VectorToSVectorShr2(rec->u.smear.pos, pos);
      VectorToSVectorShr2(rec->u.smear.pos2, pos);
      if ((effectArg & 0x1F) < 0x10) {
        w = effectArg * 2;
      } else {
        w = effectArg * 2 - 0x80;
      }
      rec->u.smear.angle = w;
      rec->u.smear.radius = 8;
      rec->u.smear.r = ~((effectArg & 0xF) << 4);
      rec->u.smear.g = 0xFF;
      rec->u.smear.b = -0x80 - ((effectArg & 0xF) << 3);
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
      rec->visible = 1;
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
      rec->visible = 1;
      VectorToSVectorShr2(rec->u.line.b, pos);
      AddVector(v3, pos, (int *)effectArg);
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
      rec->visible = 1;
      VectorToSVectorShr2(rec->u.line.a, pos);
      v4[0] = (effectArg * ((int)(GetRandomU32() & 0xFFF) - 0x800)) >> 12;
      v4[1] = (effectArg * ((int)(GetRandomU32() & 0xFFF) - 0x800)) >> 12;
      v4[2] = (effectArg * (int)(GetRandomU32() & 0xFFF)) >> 12;
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

/* MATCHED 2026-08-20-1; the 2026-08-04 / 2026-08-14-1 park is closed.  The
 * residue was arm 0x18's 4-insn knot, and both halves are statement order:
 *
 *  - `fx = 0x2E` must be a PLAIN LITERAL, not a carrier.  A carrier gives the
 *    constant two sets, which kills `reg_equiv_constant`, turns it into an
 *    ordinary allocno and hands it a0; as a literal it is left to reload's
 *    spill register, which is t0 -- what the original has.  (The old note's
 *    "find something that puts a0..a3 into regs_someone_prefers[]" was chasing
 *    a register that is never allocated at all.)
 *  - `depthBias = 8` and `lift = i * 32` must be written AFTER the fx store.
 *    Reload emits the `li t0,46` immediately before the store, so the `li`
 *    inherits the store's LUID, and sched2 emits low-priority stragglers in
 *    LUID order -- the `li` can never precede a straggler from an earlier
 *    source statement.  Moving the fx store up moves the store too (equal
 *    priority stores also come out in source order).  depthBias/lift are the way
 *    out because their `li v0,8` / `sll v0,i,5` producers are chained to
 *    `li v0,48` by v0 reuse, so sched2 holds those stores in place however the
 *    statements are ordered.  See cookbook A220.
 *  - Arm 0x15's `v` is promoted to function scope and carries 0x30 here, which
 *    is what puts `li v0,48` and `sll v1,i,4` in the original's first two
 *    slots.  With a single-set carrier (`t` used only in this arm) the `li` is
 *    birthing-boosted and the pair comes out swapped.
 *
 * Word-identical (bar branch displacements) to Level3_EmitParticles, of which
 * this file is a sed-clone; that file carries the full match note.  The same fix landed the five-overlay 1085-insn
 * group headed by Level1_EmitParticles (21,700 B).
 */
