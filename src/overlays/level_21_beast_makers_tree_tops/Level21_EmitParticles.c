/* Level21_EmitParticles (0x8008869C, level_21_beast_makers_tree_tops
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
 * actor's own matrix.  `effectArg` is per-effect: a velocity vector for 0/1, a
 * packed RGB for 0xC, a scale for 0x1A/0x1B, and a flag for 0x21/0x42.
 *
 * GENERATED from the matched donor library (cookbook A220 / the emit-spawn
 * generator): the arms below are lifted verbatim from the donors that already
 * carry them, emitted in jump-table ADDRESS order, which is source order.
 * This variant's classes: 0x0, 0x1, 0x2, 0x6, 0x9, 0xA, 0xC, 0x15, 0x18, 0x19, 0x21, 0x42, 0x46, 0x47, 0x4C, 0x4D, 0x4E, 0x4F, 0x50.
 *
 * Arm 0x18 is the family's one per-variant knob: `fx = 0x2E` must be a plain
 * literal, `depthBias`/`lift` must be written after it, and the 0x30 carrier must
 * be MULTI-SET or `li v0,48` is birthing-boosted and swaps with `sll v1,i,4`.
 * Here that carrier is `v` (arm 0x15's local promoted to function scope).
 */

#include "vecmath.h"
#include "actor_type.h"
#include "level_globals.h"
#include "level_emit_particles.h"
#include "emit_record.h"
#include "gamestate.h"
#include "level_id.h"


void Level21_EmitParticles(int count, int type, int *pos, int effectArg) {
  int v0[3];
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

  for (i = 0; i < count; i++) {
    k = i * 4;
    switch (type) {
    case EMIT_SPARK_VEL: {
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
    case EMIT_SPARK_VEL_DIM:
      rec = (Emit *)AllocEmitListRecord(2);
      rec->type = type;
      rec->phase = 0;
      rec->visible = 1;
      VectorToSVectorShr2(rec->u.spark.pos, pos);
      VectorToSVectorShr2(rec->u.spark.vel, (int *)effectArg);
      rec->u.spark.toggle = 0;
      if (g_nGamestate == GS_DRAGON_RESCUE) {
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
    case EMIT_SPARK_RISE: {
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
    case 0x6: {
      int *owner;
      int t1, t2, t3;
      int u1, u2, u3;
      owner = ((Actor *)pos)->state;
      rec = (Emit *)AllocEmitListRecord(3);
      rec->type = type;
      rec->phase = GetRandomU32() & 0xF;
      rec->visible = 1;
      VectorToSVectorShr2(rec->u.plume.pos, &((Actor *)pos)->posX);
      t1 = GetRandomU32() & 0x7E;
      u1 = rec->u.plume.pos[0] - 0x3F;
      rec->u.plume.pos[0] = u1 + t1;
      t2 = GetRandomU32() & 0x7E;
      u2 = rec->u.plume.pos[1] - 0x3F;
      rec->u.plume.pos[1] = u2 + t2;
      t3 = GetRandomU32() & 0x1FF;
      u3 = rec->u.plume.pos[2] - 0x100;
      rec->u.plume.src = &((Actor *)pos)->posX;
      rec->u.plume.pos[2] = u3 + t3;
      rec->u.plume.maxRadius = owner[1];
      rec->u.plume.rise = owner[0] >> 6;
      rec->u.plume.life = 0x20;
      rec->u.plume.seed = 0x14;
      if (((Actor *)pos)->subMode != 0) {
        switch (GetRandomU32() & 7) {
        case 0:
          rec->u.plume.r = 0x80;
          rec->u.plume.g = 0;
          rec->u.plume.b = 0;
          break;
        case 1:
          rec->u.plume.r = 0x80;
          rec->u.plume.g = 0x80;
          rec->u.plume.b = 0;
          break;
        case 2:
          rec->u.plume.r = 0;
          rec->u.plume.g = 0x80;
          rec->u.plume.b = 0;
          break;
        case 3:
          rec->u.plume.r = 0;
          rec->u.plume.g = 0x80;
          rec->u.plume.b = 0x80;
          break;
        case 4:
          rec->u.plume.r = 0;
          rec->u.plume.g = 0;
          rec->u.plume.b = 0x80;
          break;
        case 5:
          rec->u.plume.r = 0x80;
          rec->u.plume.g = 0;
          rec->u.plume.b = 0x80;
          break;
        case 6:
          rec->u.plume.r = 0x80;
          rec->u.plume.g = 0x40;
          rec->u.plume.b = 0;
          break;
        case 7:
          rec->u.plume.r = 0;
          rec->u.plume.g = 0x40;
          rec->u.plume.b = 0x80;
          break;
        }
      } else if (g_nActiveLevelId == LEVEL_DREAM_WEAVERS) {
        rec->u.plume.r = 0x80;
        rec->u.plume.g = 0x80;
        rec->u.plume.b = 0;
      } else {
        rec->u.plume.r = 0x80;
        rec->u.plume.g = 0x80;
        rec->u.plume.b = 0x80;
      }
      rec->u.plume.fx = 0x2C;
      rec->u.plume.depthBias = 0;
      rec->u.plume.texIndex = 0;
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
    case EMIT_ACTOR_SPARK: {
      rec = (Emit *)AllocEmitListRecord(2);
      rec->type = type;
      rec->visible = 1;
      rec->u.spark.depthBias = 4;
      if (((Actor *)pos)->type == ACTOR_TYPE_DRAGON_EGG) {
        CopyVector(v1, &((Actor *)pos)->posX);
      } else if (((Actor *)pos)->type == ACTOR_TYPE_KEY || ((Actor *)pos)->type == 0xB5) {
        if ((effectArg >> 24) < 6) {
          RotateVectorByMatrix(((Actor *)pos)->mtx, &g_anEmitSlotOffsets[(effectArg >> 24) * 3], v1);
          AddVector(v1, v1, &((Actor *)pos)->posX);
        } else {
          rec->cls = 5;
          rec->u.spark.depthBias = 0x16;
          CopyVector(v1, &g_anEmitSlotOffsets[(effectArg >> 24) * 3]);
        }
      } else if (((Actor *)pos)->type == ACTOR_TYPE_SPARX) {
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
    case 0x19: {
      rec = (Emit *)AllocEmitListRecord(2);
      rec->type = type;
      rec->phase = 0;
      rec->visible = 1;
      VectorToSVectorShr2(rec->u.spark.pos, pos);
      VectorToSVectorShr2(rec->u.spark.vel, pos + 3);
      rec->u.spark.toggle = 0;
      rec->u.spark.life = 0x18;
      rec->u.spark.seed = GetRandomU32();
      if (pos[6] >= 461) {
        rec->u.spark.r = 0xFF;
        rec->u.spark.g = 0;
        rec->u.spark.b = 0;
      } else if (pos[6] >= 401) {
        rec->u.spark.r = 0xF0;
        rec->u.spark.g = 0x60;
        rec->u.spark.b = 0;
      } else if (pos[6] >= 321) {
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
    case EMIT_TWINKLE: {
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
      if (effectArg == 1 || (effectArg == 0 && g_nSparxHealth >= 3)) {
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
    case EMIT_STREAK_BURST: {
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
    case EMIT_STREAK_BURST_WHITE: {
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
      if (g_nSpyroFlameVariant == 0) {
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
 * What breaks the tie is that `depthBias = 8` / `lift = i * 32` are NOT free
 * stragglers: their `li v0,8` / `sll v0,i,5` producers are chained to
 * `li v0,48` by v0 reuse, so sched2 keeps that chain (and its stores) in the
 * same relative slots no matter where the statements sit in the source.
 * Writing those two statements AFTER the fx store therefore raises their
 * LUIDs above the `li t0,46` -- putting the reload third in the straggler
 * sequence, exactly where the original has it -- while their stores stay put.
 * Measured: fx at each of source positions 0..7 gives 5,5,8,10,9,8,7,5 and
 * never both ends at once; moving depthBias/lift below fx gives MATCH.
 *
 * The `v` promotion (arm 0x15's block local carrying 0x30) is still required
 * and still load-bearing, and `fx = 0x2E` must stay a PLAIN LITERAL: a carrier
 * gives the constant two sets, which kills `reg_equiv_constant` and hands the
 * value a0 instead of leaving it to reload's spill register t0.
 *
 * Word-identical (bar branch displacements) to Level2_EmitParticles,
 * Level4_EmitParticles, Level10_EmitParticles and Level28_EmitParticles,
 * which are sed-clones of this file.  Level3_EmitParticles (3,940 B) is the
 * 985-insn variant of the same walk and carries the same arm 0x18.
 */
